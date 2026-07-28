#include <QByteArray>
#include <QDataStream>
#include <QDebug>
#include <QFile>
#include <QImage>
#include <QtConcurrent>
#include <algorithm>
#include <mutex>
#include <qlogging.h>
#include "core/vksnapshotreconstructor.h"
#include "core/vulkancontext.h"
#include "core/vulkanutils.h"

VkSnapshotReconstructor::VkSnapshotReconstructor(const VulkanHandles& handles)
    : m_handles(handles) {
}

VkSnapshotReconstructor::~VkSnapshotReconstructor() {
    cleanup();
}

void VkSnapshotReconstructor::cleanup() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto                                  df = m_handles.deviceFunctions;
    auto                                  dev = m_handles.device;

    if (!df || !dev) {
        return;
    }

    // If the global Vulkan context has already been cleaned up, the device handle
    // we hold is likely invalid. Avoid calling Vulkan functions to prevent crashing.
    if (VulkanContext::instance().getDevice() == VK_NULL_HANDLE ||
        df != VulkanContext::instance().getDeviceFunctions()) {
        return;
    }

    // Wait for the GPU to be idle before destroying resources
    df->vkDeviceWaitIdle(dev);

    VulkanUtils::destroyResource(df, dev, m_stateBuffer, &QVulkanDeviceFunctions::vkDestroyBuffer);
    VulkanUtils::freeMemory(df, dev, m_stateMemory);

    VulkanUtils::destroyResource(
        df, dev, m_pendingStateBuffer, &QVulkanDeviceFunctions::vkDestroyBuffer);
    VulkanUtils::freeMemory(df, dev, m_pendingStateMemory);

    VulkanUtils::destroyResource(
        df, dev, m_cachedBaseBuffer, &QVulkanDeviceFunctions::vkDestroyBuffer);
    VulkanUtils::freeMemory(df, dev, m_cachedBaseMemory);

    VulkanUtils::destroyResource(df, dev, m_uploadFence, &QVulkanDeviceFunctions::vkDestroyFence);
    VulkanUtils::destroyResource(df, dev, m_deltaFence, &QVulkanDeviceFunctions::vkDestroyFence);

    if (m_baseStagingBuffer) {
        if (m_baseStagingMapped)
            df->vkUnmapMemory(dev, m_baseStagingMemory);
        VulkanUtils::destroyResource(
            df, dev, m_baseStagingBuffer, &QVulkanDeviceFunctions::vkDestroyBuffer);
        VulkanUtils::freeMemory(df, dev, m_baseStagingMemory);
        m_baseStagingMapped = nullptr;
    }
    if (m_deltaStagingBuffer) {
        if (m_deltaStagingMapped)
            df->vkUnmapMemory(dev, m_deltaStagingMemory);
        VulkanUtils::destroyResource(
            df, dev, m_deltaStagingBuffer, &QVulkanDeviceFunctions::vkDestroyBuffer);
        VulkanUtils::freeMemory(df, dev, m_deltaStagingMemory);
        m_deltaStagingMapped = nullptr;
    }
    if (m_tileIndexBuffer) {
        if (m_tileIndexMapped)
            df->vkUnmapMemory(dev, m_tileIndexMemory);
        VulkanUtils::destroyResource(
            df, dev, m_tileIndexBuffer, &QVulkanDeviceFunctions::vkDestroyBuffer);
        VulkanUtils::freeMemory(df, dev, m_tileIndexMemory);
        m_tileIndexMapped = nullptr;
    }

    if (m_computeCmdBuffer) {
        df->vkFreeCommandBuffers(dev, m_handles.commandPool, 1, &m_computeCmdBuffer);
        m_computeCmdBuffer = VK_NULL_HANDLE;
    }

    if (m_descriptorSet != VK_NULL_HANDLE) {
        auto& ctx = VulkanContext::instance();
        df->vkFreeDescriptorSets(dev, ctx.getDescriptorPool(), 1, &m_descriptorSet);
        m_descriptorSet = VK_NULL_HANDLE;
    }
}

bool VkSnapshotReconstructor::resetToBase(const QImage& base, const QString& checksum) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_isDirty = true; // State will change
    auto df = m_handles.deviceFunctions;
    auto dev = m_handles.device;

    if (!df || !dev || !m_handles.commandPool) {
        qWarning()
            << "[VkSnapshotReconstructor] Cannot reset to base: Vulkan context not initialized";
        return false;
    }

    uint32_t     width = base.width();
    uint32_t     height = base.height();
    VkDeviceSize size = static_cast<VkDeviceSize>(width) * height * 4;

    // Pending State Buffer Management
    if (!m_pendingStateBuffer || m_width != width || m_height != height) {
        if (m_isUploadingBase && m_uploadFence) {
            df->vkWaitForFences(dev, 1, &m_uploadFence, VK_TRUE, UINT64_MAX);
        }

        if (m_pendingStateBuffer) {
            df->vkDestroyBuffer(dev, m_pendingStateBuffer, nullptr);
            df->vkFreeMemory(dev, m_pendingStateMemory, nullptr);
        }

        auto alloc = VulkanUtils::createBuffer(VulkanContext::instance().getInstance(),
                                               df,
                                               dev,
                                               VulkanContext::instance().getPhysicalDevice(),
                                               size,
                                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (alloc.buffer == VK_NULL_HANDLE) {
            alloc = VulkanUtils::createBuffer(
                VulkanContext::instance().getInstance(),
                df,
                dev,
                VulkanContext::instance().getPhysicalDevice(),
                size,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        }

        if (alloc.buffer == VK_NULL_HANDLE) {
            qCritical() << "[VkSnapshotReconstructor] Failed to create pending state buffer";
            return false;
        }
        m_pendingStateBuffer = alloc.buffer;
        m_pendingStateMemory = alloc.memory;
    }

    // Identify if base image is cached
    bool isCached =
        (m_cachedBaseBuffer && m_lastBaseChecksum == checksum && m_stateBufferSize == size);

    if (!isCached) {
        QImage baseRGBA = base.format() == QImage::Format_ARGB32
                              ? base
                              : base.convertToFormat(QImage::Format_ARGB32);

        // Slow Path: Setup Staging
        if (!m_baseStagingBuffer || size > m_baseStagingCapacity) {
            if (m_baseStagingBuffer) {
                if (m_baseStagingMapped)
                    df->vkUnmapMemory(dev, m_baseStagingMemory);
                df->vkDestroyBuffer(dev, m_baseStagingBuffer, nullptr);
                df->vkFreeMemory(dev, m_baseStagingMemory, nullptr);
            }
            m_baseStagingCapacity = std::max(size, (VkDeviceSize)1024 * 1024 * 64);
            auto alloc = VulkanUtils::createBuffer(VulkanContext::instance().getInstance(),
                                                   df,
                                                   dev,
                                                   VulkanContext::instance().getPhysicalDevice(),
                                                   m_baseStagingCapacity,
                                                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            if (alloc.buffer == VK_NULL_HANDLE) {
                qCritical() << "[VkSnapshotReconstructor] Failed to create base staging buffer";
                return false;
            }
            m_baseStagingBuffer = alloc.buffer;
            m_baseStagingMemory = alloc.memory;
            df->vkMapMemory(
                dev, m_baseStagingMemory, 0, m_baseStagingCapacity, 0, &m_baseStagingMapped);
        }

        if (baseRGBA.bytesPerLine() == (int)width * 4) {
            memcpy(m_baseStagingMapped, baseRGBA.bits(), size);
        } else {
            for (int y = 0; y < (int)height; ++y) {
                memcpy(static_cast<uchar *>(m_baseStagingMapped) + y * width * 4,
                       baseRGBA.constScanLine(y),
                       width * 4);
            }
        }

        if (!updateCachedBase(size)) {
            qCritical() << "[VkSnapshotReconstructor] Failed to update GPU base cache";
            return false;
        }
    }

    // Record Command Buffer
    if (!m_computeCmdBuffer) {
        VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocInfo.commandPool = VulkanContext::instance().getCommandPool();
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;
        df->vkAllocateCommandBuffers(dev, &allocInfo, &m_computeCmdBuffer);
    }

    if (m_isApplyingDelta && m_deltaFence)
        df->vkWaitForFences(dev, 1, &m_deltaFence, VK_TRUE, UINT64_MAX);
    if (m_uploadFence)
        df->vkResetFences(dev, 1, &m_uploadFence);

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    df->vkBeginCommandBuffer(m_computeCmdBuffer, &beginInfo);

    VkBufferCopy copyRegion{};
    copyRegion.size = size;

    if (isCached) {
        df->vkCmdCopyBuffer(
            m_computeCmdBuffer, m_cachedBaseBuffer, m_pendingStateBuffer, 1, &copyRegion);
    } else {
        qDebug() << "[VkSnapshotReconstructor] Base cache MISS";
        df->vkCmdCopyBuffer(
            m_computeCmdBuffer, m_baseStagingBuffer, m_pendingStateBuffer, 1, &copyRegion);

        // Ensure Pending is written before we copy it to Cache
        VkBufferMemoryBarrier barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.buffer = m_pendingStateBuffer;
        barrier.offset = 0;
        barrier.size = size;
        df->vkCmdPipelineBarrier(m_computeCmdBuffer, 0, 0, 0, 0, nullptr, 1, &barrier, 0, nullptr);

        // Pending -> Cache
        df->vkCmdCopyBuffer(
            m_computeCmdBuffer, m_pendingStateBuffer, m_cachedBaseBuffer, 1, &copyRegion);
    }

    df->vkEndCommandBuffer(m_computeCmdBuffer);

    if (!m_uploadFence) {
        VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        df->vkCreateFence(dev, &fci, nullptr, &m_uploadFence);
    } else {
        df->vkResetFences(dev, 1, &m_uploadFence);
    }

    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_computeCmdBuffer;
    df->vkQueueSubmit(VulkanContext::instance().getQueue(), 1, &submitInfo, m_uploadFence);

    m_isUploadingBase = true;
    m_width = width;
    m_height = height;
    m_stateBufferSize = size;
    m_lastBaseChecksum = checksum;

    return true;
}

bool VkSnapshotReconstructor::reconstruct(const ReconstructionSequence& seq) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!resetToBase(seq.base, seq.baseChecksum))
        return false;

    for (const auto& delta : seq.deltas) {
        if (!applyDelta(delta))
            return false;
    }

    if (m_isUploadingBase) {
        auto df = m_handles.deviceFunctions;
        auto dev = m_handles.device;
        if (df && dev && m_uploadFence) {
            df->vkWaitForFences(dev, 1, &m_uploadFence, VK_TRUE, UINT64_MAX);
        }
        checkAndSwapBase();
    }

    return true;
}

bool VkSnapshotReconstructor::applyDelta(const QByteArray& delta) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_isDirty = true;

    // Initialize resources and descriptor set
    auto& ctx = VulkanContext::instance();

    if (!ctx.getComputePipeline()) {
        return false;
    }

    auto df = m_handles.deviceFunctions;
    auto dev = m_handles.device;

    if (!df || !dev || !m_handles.commandPool) {
        qWarning()
            << "[VkSnapshotReconstructor] Cannot apply delta: Vulkan context not initialized";
        return false;
    }

    // Ensure we have a descriptor set allocated for this session
    if (m_descriptorSet == VK_NULL_HANDLE) {
        VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocInfo.descriptorPool = ctx.getDescriptorPool();
        allocInfo.descriptorSetCount = 1;

        // Provide the layout for the descriptor set
        VkDescriptorSetLayout layouts[] = {ctx.getComputeDescriptorSetLayout()};
        allocInfo.pSetLayouts = layouts;

        VkDescriptorSet pSet = nullptr;
        if (df->vkAllocateDescriptorSets(dev, &allocInfo, &pSet) != VK_SUCCESS) {
            qCritical() << "[VkSnapshotReconstructor] Failed to allocate descriptor set";
            return false;
        }
        m_descriptorSet = pSet;
    }

    if (m_isUploadingBase) {
        if (m_uploadFence) {
            df->vkWaitForFences(dev, 1, &m_uploadFence, VK_TRUE, UINT64_MAX);
        }
        checkAndSwapBase();
    }

    if (delta.isEmpty())
        return false;

    QDataStream stream(delta);
    stream.setByteOrder(QDataStream::LittleEndian);

    uint32_t version;
    stream >> version;
    if (version != 1) {
        qWarning() << "[VkSnapshotReconstructor] Unsupported delta format version:" << version;
        return false;
    }

    uint32_t tileW, tileH, numTiles;
    stream >> tileW >> tileH >> numTiles;
    if (numTiles == 0)
        return true;

    // Decompress Tiles in Parallel
    struct TileData {
        uint32_t   index;
        QByteArray compressed;
    };
    QVector<TileData> tiles;
    tiles.reserve(numTiles);
    for (uint32_t i = 0; i < numTiles; ++i) {
        uint32_t   idx;
        QByteArray compressed;
        stream >> idx >> compressed;
        tiles.append({idx, compressed});
    }

    QVector<QByteArray> decompressed = QtConcurrent::blockingMapped(
        tiles, [](const TileData& td) { return qUncompress(td.compressed); });

    // Pack decompressed pixels and collect indices
    QByteArray        packedPixels;
    QVector<uint32_t> tileIndices;
    QVector<uint32_t> tileOffsets;
    tileIndices.reserve(numTiles);
    tileOffsets.reserve(numTiles);

    for (uint32_t i = 0; i < numTiles; ++i) {
        if (decompressed[i].isEmpty())
            continue;
        tileIndices.append(tiles[i].index);
        tileOffsets.append(packedPixels.size() / sizeof(uint32_t));
        packedPixels.append(decompressed[i]);
    }

    if (tileIndices.isEmpty())
        return true;

    // Update Staging Buffer
    if (m_deltaFence) {
        df->vkWaitForFences(dev, 1, &m_deltaFence, VK_TRUE, UINT64_MAX);
        df->vkResetFences(dev, 1, &m_deltaFence);
    }

    if (!m_deltaStagingBuffer || packedPixels.size() > m_deltaStagingCapacity) {
        if (m_deltaStagingBuffer) {
            if (m_deltaStagingMapped)
                df->vkUnmapMemory(dev, m_deltaStagingMemory);
            df->vkDestroyBuffer(dev, m_deltaStagingBuffer, nullptr);
            df->vkFreeMemory(dev, m_deltaStagingMemory, nullptr);
        }

        m_deltaStagingCapacity =
            std::max((size_t)packedPixels.size(), (size_t)1024 * 1024 * 16); // Min 16MB
        auto alloc = VulkanUtils::createBuffer(VulkanContext::instance().getInstance(),
                                               df,
                                               dev,
                                               m_handles.physicalDevice,
                                               m_deltaStagingCapacity,
                                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (alloc.buffer == VK_NULL_HANDLE) {
            qCritical() << "[VkSnapshotReconstructor] Failed to create delta staging buffer";
            return false;
        }
        m_deltaStagingBuffer = alloc.buffer;
        m_deltaStagingMemory = alloc.memory;

        df->vkMapMemory(
            dev, m_deltaStagingMemory, 0, m_deltaStagingCapacity, 0, &m_deltaStagingMapped);

        VkDescriptorBufferInfo bufferInfo{m_deltaStagingBuffer, 0, m_deltaStagingCapacity};
        VkWriteDescriptorSet   write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = m_descriptorSet;
        write.dstBinding = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &bufferInfo;
        df->vkUpdateDescriptorSets(dev, 1, &write, 0, nullptr);
    }
    memcpy(m_deltaStagingMapped, packedPixels.data(), packedPixels.size());
    m_deltaStagingBufferSize = packedPixels.size();

    // Update Tile Index Buffer size if needed
    size_t indexBufferSize = tileIndices.size() * sizeof(uint32_t) * 2;
    if (!m_tileIndexBuffer || indexBufferSize > m_tileIndexCapacity) {
        if (m_tileIndexBuffer) {
            if (m_tileIndexMapped)
                df->vkUnmapMemory(dev, m_tileIndexMemory);
            df->vkDestroyBuffer(dev, m_tileIndexBuffer, nullptr);
            df->vkFreeMemory(dev, m_tileIndexMemory, nullptr);
        }

        m_tileIndexCapacity = std::max((size_t)indexBufferSize, (size_t)1024 * 64); // Min 64KB
        auto alloc = VulkanUtils::createBuffer(VulkanContext::instance().getInstance(),
                                               df,
                                               dev,
                                               m_handles.physicalDevice,
                                               m_tileIndexCapacity,
                                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (alloc.buffer == VK_NULL_HANDLE) {
            qCritical() << "[VkSnapshotReconstructor] Failed to create tile index buffer";
            return false;
        }
        m_tileIndexBuffer = alloc.buffer;
        m_tileIndexMemory = alloc.memory;

        df->vkMapMemory(dev, m_tileIndexMemory, 0, m_tileIndexCapacity, 0, &m_tileIndexMapped);

        VkDescriptorBufferInfo bufferInfo{m_tileIndexBuffer, 0, m_tileIndexCapacity};
        VkWriteDescriptorSet   write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = m_descriptorSet;
        write.dstBinding = 2;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &bufferInfo;
        df->vkUpdateDescriptorSets(dev, 1, &write, 0, nullptr);
    }
    uint32_t *ptr = static_cast<uint32_t *>(m_tileIndexMapped);
    for (size_t i = 0; i < tileIndices.size(); ++i) {
        ptr[i * 2] = tileIndices[i];
        ptr[i * 2 + 1] = tileOffsets[i];
    }
    m_tileIndexBufferSize = indexBufferSize;

    // Dispatch Compute Shader
    if (!m_computeCmdBuffer) {
        VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocInfo.commandPool = VulkanContext::instance().getCommandPool();
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;
        df->vkAllocateCommandBuffers(dev, &allocInfo, &m_computeCmdBuffer);
    }

    // Ensure previous operations on this command buffer are complete
    if (m_isUploadingBase && m_uploadFence) {
        df->vkWaitForFences(dev, 1, &m_uploadFence, VK_TRUE, UINT64_MAX);
    }
    m_isApplyingDelta = false;

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    df->vkBeginCommandBuffer(m_computeCmdBuffer, &beginInfo);
    df->vkCmdBindPipeline(
        m_computeCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, ctx.getComputePipeline());
    df->vkCmdBindDescriptorSets(m_computeCmdBuffer,
                                VK_PIPELINE_BIND_POINT_COMPUTE,
                                ctx.getComputePipelineLayout(),
                                0,
                                1,
                                &m_descriptorSet,
                                0,
                                nullptr);

    PushConstants pcs;
    pcs.tileW = tileW;
    pcs.tileH = tileH;
    pcs.imageW = m_width;
    pcs.imageH = m_height;
    pcs.numTiles = tileIndices.size();
    df->vkCmdPushConstants(m_computeCmdBuffer,
                           ctx.getComputePipelineLayout(),
                           VK_SHADER_STAGE_COMPUTE_BIT,
                           0,
                           sizeof(pcs),
                           &pcs);

    uint32_t totalPixels = tileIndices.size() * tileW * tileH;
    uint32_t groupCount = (totalPixels + 255) / 256;
    df->vkCmdDispatch(m_computeCmdBuffer, groupCount, 1, 1);
    df->vkEndCommandBuffer(m_computeCmdBuffer);

    // Create fence for this delta if it does not exist
    if (!m_deltaFence) {
        VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        df->vkCreateFence(dev, &fci, nullptr, &m_deltaFence);
    }

    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_computeCmdBuffer;
    df->vkQueueSubmit(VulkanContext::instance().getQueue(), 1, &submitInfo, m_deltaFence);

    m_isApplyingDelta = true;

    return true;
}

bool VkSnapshotReconstructor::copyToImage(VkCommandBuffer cmd,
                                          VkImage         targetImage,
                                          uint32_t        width,
                                          uint32_t        height) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_stateBuffer) {
        qDebug() << "[VkSnapshotReconstructor] Null state buffer in copyToImage";
        return false;
    }

    auto df = VulkanContext::instance().getDeviceFunctions();

    VkBufferMemoryBarrier bufBarrier{};
    bufBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    bufBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    bufBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    bufBarrier.buffer = m_stateBuffer;
    bufBarrier.offset = 0;
    bufBarrier.size = m_stateBufferSize;

    df->vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0,
                             0,
                             nullptr,
                             1,
                             &bufBarrier,
                             0,
                             nullptr);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    df->vkCmdCopyBufferToImage(
        cmd, m_stateBuffer, targetImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    m_isDirty = false;

    return true;
}

void VkSnapshotReconstructor::waitForDeltas() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto                                  df = VulkanContext::instance().getDeviceFunctions();
    auto                                  dev = VulkanContext::instance().getDevice();

    if (!df || !dev)
        return;

    // 1. Wait for base upload (critical for cache MISS path)
    if (m_isUploadingBase && m_uploadFence) {
        df->vkWaitForFences(dev, 1, &m_uploadFence, VK_TRUE, UINT64_MAX);
    }

    // 2. Wait for delta applications
    if (m_isApplyingDelta && m_deltaFence) {
        df->vkWaitForFences(dev, 1, &m_deltaFence, VK_TRUE, UINT64_MAX);
        m_isApplyingDelta = false;
    }
}

void VkSnapshotReconstructor::resetState() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto                                  df = VulkanContext::instance().getDeviceFunctions();
    auto                                  dev = VulkanContext::instance().getDevice();
    if (!df || !dev)
        return;

    // Destroy active state buffer
    if (m_stateBuffer) {
        df->vkDestroyBuffer(dev, m_stateBuffer, nullptr);
        df->vkFreeMemory(dev, m_stateMemory, nullptr);
        m_stateBuffer = VK_NULL_HANDLE;
        m_stateMemory = VK_NULL_HANDLE;
        m_stateBufferSize = 0;
    }

    // Destroy cached base buffer
    if (m_cachedBaseBuffer) {
        df->vkDestroyBuffer(dev, m_cachedBaseBuffer, nullptr);
        df->vkFreeMemory(dev, m_cachedBaseMemory, nullptr);
        m_cachedBaseBuffer = VK_NULL_HANDLE;
        m_cachedBaseMemory = VK_NULL_HANDLE;
        m_lastBaseChecksum = QString();
    }

    // Clear pending state
    if (m_pendingStateBuffer) {
        df->vkDestroyBuffer(dev, m_pendingStateBuffer, nullptr);
        df->vkFreeMemory(dev, m_pendingStateMemory, nullptr);
        m_pendingStateBuffer = VK_NULL_HANDLE;
        m_pendingStateMemory = VK_NULL_HANDLE;
    }

    m_isUploadingBase = false;
    m_isApplyingDelta = false;
}

bool VkSnapshotReconstructor::checkAndSwapBase() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_isUploadingBase)
        return false;

    auto df = m_handles.deviceFunctions;
    auto dev = m_handles.device;

    if (df->vkGetFenceStatus(dev, m_uploadFence) == VK_SUCCESS) {
        // Ensure any pending delta operations are finished before we swap
        waitForDeltas();

        // Swap active state buffer with the pending one
        if (m_stateBuffer) {
            df->vkDestroyBuffer(dev, m_stateBuffer, nullptr);
            df->vkFreeMemory(dev, m_stateMemory, nullptr);
        }

        m_stateBuffer = m_pendingStateBuffer;
        m_stateMemory = m_pendingStateMemory;

        // Reset pending buffer to null so it can be recreated on next resetToBase
        m_pendingStateBuffer = VK_NULL_HANDLE;
        m_pendingStateMemory = VK_NULL_HANDLE;
        m_isUploadingBase = false;

        // Update the descriptor set to point to the new active buffer
        if (m_descriptorSet != VK_NULL_HANDLE) {
            VkDescriptorBufferInfo bufferInfo{m_stateBuffer, 0, m_stateBufferSize};
            VkWriteDescriptorSet   write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            write.dstSet = m_descriptorSet;
            write.dstBinding = 0;
            write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            write.descriptorCount = 1;
            write.pBufferInfo = &bufferInfo;
            df->vkUpdateDescriptorSets(dev, 1, &write, 0, nullptr);
        }
        m_isUploadingBase = false;
        return true;
    }
    return false;
}

bool VkSnapshotReconstructor::updateCachedBase(VkDeviceSize size) {
    auto df = m_handles.deviceFunctions;
    auto dev = m_handles.device;

    if (m_cachedBaseBuffer && m_stateBufferSize == size) {
        return true;
    }

    if (m_cachedBaseBuffer) {
        df->vkDestroyBuffer(dev, m_cachedBaseBuffer, nullptr);
        df->vkFreeMemory(dev, m_cachedBaseMemory, nullptr);
        m_cachedBaseBuffer = VK_NULL_HANDLE;
        m_cachedBaseMemory = VK_NULL_HANDLE;
    }

    auto alloc = VulkanUtils::createBuffer(VulkanContext::instance().getInstance(),
                                           df,
                                           dev,
                                           VulkanContext::instance().getPhysicalDevice(),
                                           size,
                                           VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                               VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // Failed to allocate memory on GPU, fallback to CPU memory
    if (alloc.buffer == VK_NULL_HANDLE) {
        alloc = VulkanUtils::createBuffer(
            VulkanContext::instance().getInstance(),
            df,
            dev,
            VulkanContext::instance().getPhysicalDevice(),
            size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    }

    if (alloc.buffer == VK_NULL_HANDLE) {
        return false;
    }
    m_cachedBaseBuffer = alloc.buffer;
    m_cachedBaseMemory = alloc.memory;
    return true;
}

bool VkSnapshotReconstructor::isUploadingBase() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_isUploadingBase;
}

VkBuffer VkSnapshotReconstructor::stateBuffer() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_stateBuffer;
}

uint32_t VkSnapshotReconstructor::width() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_width;
}

uint32_t VkSnapshotReconstructor::height() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_height;
}

bool VkSnapshotReconstructor::isDirty() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_isDirty;
}

VkDeviceSize VkSnapshotReconstructor::stateBufferSize() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_stateBufferSize;
}

QImage VkSnapshotReconstructor::reconstructToImage(const ReconstructionSequence& seq,
                                                   QSize                         targetSize,
                                                   VkSnapshotReconstructor      *worker) {
    std::unique_ptr<VkSnapshotReconstructor> localWorker = nullptr;
    VkSnapshotReconstructor                 *activeWorker = worker;

    if (!activeWorker) {
        localWorker = std::make_unique<VkSnapshotReconstructor>(m_handles);
        activeWorker = localWorker.get();
    }

    if (!activeWorker->reconstruct(seq)) {
        return QImage();
    }

    auto df = m_handles.deviceFunctions;
    auto dev = m_handles.device;
    auto pool = m_handles.commandPool;

    if (!df || !dev || !pool) {
        qDebug() << "[VkSnapshotReconstructor] Null handles in reconstructToImage";
        return QImage();
    }

    VkBuffer     srcBuffer = activeWorker->stateBuffer();
    VkDeviceSize srcBufferSize = activeWorker->stateBufferSize();
    uint32_t     srcW = activeWorker->width();
    uint32_t     srcH = activeWorker->height();

    if (srcBuffer == VK_NULL_HANDLE || srcBufferSize == 0) {
        qDebug() << "[VkSnapshotReconstructor] Null srcBuffer in reconstructToImage";
        return QImage();
    }

    VkBuffer     finalBuffer = srcBuffer;
    VkDeviceSize finalBufferSize = srcBufferSize;
    uint32_t     finalW = srcW;
    uint32_t     finalH = srcH;

    // GPU Downsampling Pass
    if (!targetSize.isEmpty() &&
        ((int)srcW > targetSize.width() || (int)srcH > targetSize.height())) {
        auto& ctx = VulkanContext::instance();

        VkDescriptorSet             ds;
        VkDescriptorSetAllocateInfo dsai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        dsai.descriptorPool = ctx.getDescriptorPool();
        dsai.descriptorSetCount = 1;
        VkDescriptorSetLayout layout = ctx.getDownsampleDescriptorSetLayout();
        dsai.pSetLayouts = &layout;
        df->vkAllocateDescriptorSets(dev, &dsai, &ds);

        VkDescriptorBufferInfo bufferInfos[2] = {};
        bufferInfos[0].buffer = srcBuffer;
        bufferInfos[0].offset = 0;
        bufferInfos[0].range = srcBufferSize;

        finalW = targetSize.width();
        finalH = targetSize.height();
        finalBufferSize = (VkDeviceSize)finalW * finalH * 4;

        auto downsampleAlloc = VulkanUtils::createBuffer(
            VulkanContext::instance().getInstance(),
            df,
            dev,
            VulkanContext::instance().getPhysicalDevice(),
            finalBufferSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        if (downsampleAlloc.buffer == VK_NULL_HANDLE)
            return QImage();
        finalBuffer = downsampleAlloc.buffer;

        bufferInfos[1].buffer = finalBuffer;
        bufferInfos[1].offset = 0;
        bufferInfos[1].range = finalBufferSize;

        VkWriteDescriptorSet writes[2] = {};
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = ds;
        writes[0].dstBinding = 0;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[0].descriptorCount = 1;
        writes[0].pBufferInfo = &bufferInfos[0];

        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = ds;
        writes[1].dstBinding = 1;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[1].descriptorCount = 1;
        writes[1].pBufferInfo = &bufferInfos[1];

        df->vkUpdateDescriptorSets(dev, 2, writes, 0, nullptr);

        VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocInfo.commandPool = pool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;
        VkCommandBuffer cmd;
        df->vkAllocateCommandBuffers(dev, &allocInfo, &cmd);

        VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        df->vkBeginCommandBuffer(cmd, &beginInfo);

        struct PushConstants {
            uint32_t w, h, tw, th;
        } pcs = {srcW, srcH, finalW, finalH};

        df->vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, ctx.getDownsamplePipeline());
        df->vkCmdBindDescriptorSets(cmd,
                                    VK_PIPELINE_BIND_POINT_COMPUTE,
                                    ctx.getDownsamplePipelineLayout(),
                                    0,
                                    1,
                                    &ds,
                                    0,
                                    nullptr);
        df->vkCmdPushConstants(cmd,
                               ctx.getDownsamplePipelineLayout(),
                               VK_SHADER_STAGE_COMPUTE_BIT,
                               0,
                               sizeof(pcs),
                               &pcs);

        df->vkCmdDispatch(cmd, (finalW + 15) / 16, (finalH + 15) / 16, 1);
        df->vkEndCommandBuffer(cmd);

        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd;

        VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        VkFence           fence;
        df->vkCreateFence(dev, &fci, nullptr, &fence);

        df->vkQueueSubmit(VulkanContext::instance().getQueue(), 1, &submit, fence);
        df->vkWaitForFences(dev, 1, &fence, VK_TRUE, UINT64_MAX);
        df->vkDestroyFence(dev, fence, nullptr);

        df->vkFreeCommandBuffers(dev, pool, 1, &cmd);
        df->vkFreeDescriptorSets(dev, ctx.getDescriptorPool(), 1, &ds);
    }

    // Now copy result
    auto stagingAlloc = VulkanUtils::createBuffer(VulkanContext::instance().getInstance(),
                                                  df,
                                                  dev,
                                                  VulkanContext::instance().getPhysicalDevice(),
                                                  finalBufferSize,
                                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (stagingAlloc.buffer == VK_NULL_HANDLE)
        return QImage();

    VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocInfo.commandPool = pool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    VkCommandBuffer copyCmd;
    df->vkAllocateCommandBuffers(dev, &allocInfo, &copyCmd);

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    df->vkBeginCommandBuffer(copyCmd, &beginInfo);

    VkBufferCopy copyRegion = {0, 0, finalBufferSize};
    df->vkCmdCopyBuffer(copyCmd, finalBuffer, stagingAlloc.buffer, 1, &copyRegion);
    df->vkEndCommandBuffer(copyCmd);

    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &copyCmd;

    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence           fence;
    df->vkCreateFence(dev, &fci, nullptr, &fence);

    df->vkQueueSubmit(VulkanContext::instance().getQueue(), 1, &submit, fence);
    df->vkWaitForFences(dev, 1, &fence, VK_TRUE, UINT64_MAX);
    df->vkDestroyFence(dev, fence, nullptr);

    void *data = nullptr;
    df->vkMapMemory(dev, stagingAlloc.memory, 0, finalBufferSize, 0, &data);
    QImage result =
        QImage((const uchar *)data, finalW, finalH, finalW * 4, QImage::Format_ARGB32).copy();
    df->vkUnmapMemory(dev, stagingAlloc.memory);

    df->vkFreeCommandBuffers(dev, pool, 1, &copyCmd);
    VulkanUtils::destroyResource(
        df, dev, stagingAlloc.buffer, &QVulkanDeviceFunctions::vkDestroyBuffer);
    VulkanUtils::freeMemory(df, dev, stagingAlloc.memory);

    return result;
}
