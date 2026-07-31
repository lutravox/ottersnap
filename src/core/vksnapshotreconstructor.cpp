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

VkSnapshotReconstructor::VkSnapshotReconstructor(const VulkanHandles& handles,
                                                 VulkanContext       *context)
    : m_handles(handles), m_context(context) {
}

VkSnapshotReconstructor::~VkSnapshotReconstructor() {
    cleanup();
}

void VkSnapshotReconstructor::cleanup() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto                                  df = m_handles.deviceFunctions;
    auto                                  dev = m_handles.device;

    if (df == nullptr || dev == VK_NULL_HANDLE) {
        return;
    }

    // Wait for the GPU to be idle before destroying resources.
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
        df->vkFreeDescriptorSets(dev, m_context->getDescriptorPool(dev), 1, &m_descriptorSet);
        m_descriptorSet = VK_NULL_HANDLE;
    }

    if (m_downsampleDescriptorSet != VK_NULL_HANDLE) {
        df->vkFreeDescriptorSets(
            dev, m_context->getDescriptorPool(dev), 1, &m_downsampleDescriptorSet);
        m_downsampleDescriptorSet = VK_NULL_HANDLE;
    }

    if (m_downsampleCmdBuffer) {
        df->vkFreeCommandBuffers(dev, m_handles.commandPool, 1, &m_downsampleCmdBuffer);
        m_downsampleCmdBuffer = VK_NULL_HANDLE;
    }

    VulkanUtils::destroyResource(
        df, dev, m_downsampleFence, &QVulkanDeviceFunctions::vkDestroyFence);
    VulkanUtils::destroyResource(
        df, dev, m_downsampleBuffer, &QVulkanDeviceFunctions::vkDestroyBuffer);
    VulkanUtils::freeMemory(df, dev, m_downsampleMemory);

    m_downsampleBufferSize = 0;

    m_handles.device = VK_NULL_HANDLE;
    m_handles.deviceFunctions = nullptr;
}

bool VkSnapshotReconstructor::resetToBase(const QImage& base, const QString& checksum) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_isDirty = true; // State will change
    auto df = m_handles.deviceFunctions;
    auto dev = m_handles.device;

    if (!df || !dev || !m_handles.commandPool) {
        qWarning()
            << "[VkSnapshotReconstructor] Cannot reset to base: Vulkan context not initialized";
        m_isUploadingBase = false;
        return false;
    }

    uint32_t     width = base.width();
    uint32_t     height = base.height();
    VkDeviceSize size = static_cast<VkDeviceSize>(width) * height * 4;

    if (!ensurePendingStateBuffer(width, height, size)) {
        return false;
    }

    bool isCached =
        (m_cachedBaseBuffer && m_lastBaseChecksum == checksum && m_stateBufferSize == size);
    if (!isCached && !prepareBaseStaging(base, width, height, size)) {
        return false;
    }

    if (!recordBaseUploadCommands(isCached, size)) {
        return false;
    }

    if (!m_uploadFence) {
        VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        df->vkCreateFence(dev, &fci, nullptr, &m_uploadFence);
    } else {
        df->vkResetFences(dev, 1, &m_uploadFence);
    }

    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_computeCmdBuffer;
    df->vkQueueSubmit(m_handles.queue, 1, &submitInfo, m_uploadFence);

    m_isUploadingBase = true;
    if (m_descriptorSet == VK_NULL_HANDLE) {
        auto&                       ctx = m_context;
        VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocInfo.descriptorPool = ctx->getDescriptorPool(dev);
        allocInfo.descriptorSetCount = 1;

        VkDescriptorSetLayout layouts[] = {ctx->getComputeDescriptorSetLayout(dev)};
        allocInfo.pSetLayouts = layouts;

        VkDescriptorSet pSet = nullptr;
        if (df->vkAllocateDescriptorSets(dev, &allocInfo, &pSet) != VK_SUCCESS) {
            qCritical()
                << "[VkSnapshotReconstructor] Failed to allocate descriptor set in resetToBase";
            m_isUploadingBase = false;
            return false;
        }
        m_descriptorSet = pSet;
    }

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

    if (m_isUploadingBase) {
        auto df = m_handles.deviceFunctions;
        auto dev = m_handles.device;
        if (df && dev && m_uploadFence) {
            if (df->vkWaitForFences(
                    dev, 1, &m_uploadFence, VK_TRUE, VulkanContext::FENCE_TIMEOUT_NS) !=
                VK_SUCCESS) {
                qCritical() << "[VkSnapshotReconstructor] Timeout waiting for base upload fence in "
                               "reconstruct";
                cancelBaseUpload();
                return false;
            }
        }
        checkAndSwapBase();
    }

    for (const auto& delta : seq.deltas) {
        if (!applyDelta(delta))
            return false;
    }

    // Ensure all deltas are applied before returning.
    if (m_deltaFence) {
        auto df = m_handles.deviceFunctions;
        auto dev = m_handles.device;
        if (df->vkWaitForFences(dev, 1, &m_deltaFence, VK_TRUE, VulkanContext::FENCE_TIMEOUT_NS) !=
            VK_SUCCESS) {
            qCritical()
                << "[VkSnapshotReconstructor] Timeout waiting for final delta fence in reconstruct";
            return false;
        }
    }

    return true;
}

bool VkSnapshotReconstructor::parseAndDecompressDelta(const QByteArray&  delta,
                                                      uint32_t&          tileW,
                                                      uint32_t&          tileH,
                                                      QByteArray&        packedPixels,
                                                      QVector<uint32_t>& tileIndices,
                                                      QVector<uint32_t>& tileOffsets) {
    QDataStream stream(delta);
    stream.setByteOrder(QDataStream::LittleEndian);

    uint32_t version = 0;
    stream >> version;
    if (version != 1) {
        qWarning() << "[VkSnapshotReconstructor] Unsupported delta format version:" << version;
        return false;
    }

    uint32_t numTiles = 0;
    stream >> tileW >> tileH >> numTiles;

    if (numTiles == 0) {
        return true;
    }

    if (tileW == 0 || tileH == 0 || tileW > 4096 || tileH > 4096) {
        qCritical() << "[VkSnapshotReconstructor] Delta corruption: invalid tile dimensions ("
                    << tileW << "x" << tileH << ")";
        return false;
    }

    if (numTiles > 1000000) {
        qCritical() << "[VkSnapshotReconstructor] Delta corruption: numTiles too large ("
                    << numTiles << ")";
        return false;
    }

    struct TileData {
        uint32_t   index;
        QByteArray compressed;
    };
    QVector<TileData> tiles;
    tiles.reserve(numTiles);
    for (uint32_t i = 0; i < numTiles; ++i) {
        uint32_t   idx = 0;
        QByteArray compressed;
        stream >> idx >> compressed;
        tiles.append({idx, compressed});
    }

    QVector<UncompressedTile> uncompressed =
        QtConcurrent::blockingMapped(tiles, [](const TileData& td) {
            return UncompressedTile{td.index, qUncompress(td.compressed)};
        });

    packedPixels.clear();
    tileIndices.clear();
    tileOffsets.clear();
    tileIndices.reserve(numTiles);
    tileOffsets.reserve(numTiles);

    for (const auto& tile : uncompressed) {
        if (tile.pixels.isEmpty()) {
            qCritical() << "[VkSnapshotReconstructor] Delta corruption: Tile index" << tile.index
                        << "decompression failed. Aborting applyDelta.";
            return false;
        }
        tileIndices.append(tile.index);
        tileOffsets.append(packedPixels.size() / sizeof(uint32_t));
        packedPixels.append(tile.pixels);
    }

    return true;
}

bool VkSnapshotReconstructor::updateStagingResources(const QByteArray&        packedPixels,
                                                     const QVector<uint32_t>& tileIndices,
                                                     const QVector<uint32_t>& tileOffsets) {
    auto df = m_handles.deviceFunctions;
    auto dev = m_handles.device;

    // Update Delta Staging Buffer
    if (!m_deltaStagingBuffer ||
        static_cast<VkDeviceSize>(packedPixels.size()) > m_deltaStagingCapacity) {
        if (m_deltaStagingBuffer) {
            if (m_deltaStagingMapped)
                df->vkUnmapMemory(dev, m_deltaStagingMemory);
            df->vkDestroyBuffer(dev, m_deltaStagingBuffer, nullptr);
            df->vkFreeMemory(dev, m_deltaStagingMemory, nullptr);
        }

        m_deltaStagingCapacity = std::max((size_t)packedPixels.size(), (size_t)1024 * 1024 * 16);
        auto alloc = VulkanUtils::createBuffer(m_context->getInstance(),
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

    // Update Tile Index Buffer
    size_t indexBufferSize = tileIndices.size() * sizeof(uint32_t) * 2;
    if (!m_tileIndexBuffer || indexBufferSize > m_tileIndexCapacity) {
        if (m_tileIndexBuffer) {
            if (m_tileIndexMapped)
                df->vkUnmapMemory(dev, m_tileIndexMemory);
            df->vkDestroyBuffer(dev, m_tileIndexBuffer, nullptr);
            df->vkFreeMemory(dev, m_tileIndexMemory, nullptr);
        }

        m_tileIndexCapacity = std::max((size_t)indexBufferSize, (size_t)1024 * 64);
        auto alloc = VulkanUtils::createBuffer(m_context->getInstance(),
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
    for (size_t i = 0; i < static_cast<size_t>(tileIndices.size()); ++i) {
        ptr[static_cast<uint32_t>(i * 2)] = tileIndices[i];
        ptr[static_cast<uint32_t>(i * 2 + 1)] = tileOffsets[i];
    }
    m_tileIndexBufferSize = indexBufferSize;

    return true;
}

bool VkSnapshotReconstructor::recordDeltaCommands(uint32_t tileW,
                                                  uint32_t tileH,
                                                  uint32_t numTiles) {
    auto df = m_handles.deviceFunctions;
    auto dev = m_handles.device;

    if (!m_computeCmdBuffer) {
        VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocInfo.commandPool = m_handles.commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;
        df->vkAllocateCommandBuffers(dev, &allocInfo, &m_computeCmdBuffer);
    }

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    df->vkBeginCommandBuffer(m_computeCmdBuffer, &beginInfo);

    df->vkCmdBindPipeline(
        m_computeCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_context->getComputePipeline(dev));
    df->vkCmdBindDescriptorSets(m_computeCmdBuffer,
                                VK_PIPELINE_BIND_POINT_COMPUTE,
                                m_context->getComputePipelineLayout(dev),
                                0,
                                1,
                                &m_descriptorSet,
                                0,
                                nullptr);

    PushConstants pcs{};
    pcs.tileW = tileW;
    pcs.tileH = tileH;
    pcs.imageW = m_width;
    pcs.imageH = m_height;
    pcs.numTiles = numTiles;
    df->vkCmdPushConstants(m_computeCmdBuffer,
                           m_context->getComputePipelineLayout(dev),
                           VK_SHADER_STAGE_COMPUTE_BIT,
                           0,
                           sizeof(pcs),
                           &pcs);

    uint32_t totalPixels = numTiles * tileW * tileH;
    uint32_t groupCount = (totalPixels + 255) / 256;
    df->vkCmdDispatch(m_computeCmdBuffer, groupCount, 1, 1);
    df->vkEndCommandBuffer(m_computeCmdBuffer);

    if (!m_deltaFence) {
        VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        df->vkCreateFence(dev, &fci, nullptr, &m_deltaFence);
    }

    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_computeCmdBuffer;
    df->vkQueueSubmit(m_handles.queue, 1, &submitInfo, m_deltaFence);

    m_isApplyingDelta = true;
    return true;
}

bool VkSnapshotReconstructor::applyDelta(const QByteArray& delta) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_isDirty = true;

    auto df = m_handles.deviceFunctions;
    auto dev = m_handles.device;

    if (!df || !dev || !m_handles.commandPool) {
        qWarning()
            << "[VkSnapshotReconstructor] Cannot apply delta: Vulkan context not initialized";
        return false;
    }

    if (!m_context->getComputePipeline(dev)) {
        qWarning()
            << "[VkSnapshotReconstructor] Cannot apply delta: Compute pipeline not initialized";
        return false;
    }

    // Ensure binding 0 points to the current active state buffer.
    updateStateBufferBinding();

    if (m_isUploadingBase) {
        if (m_deltaFence) {
            if (df->vkWaitForFences(
                    dev, 1, &m_deltaFence, VK_TRUE, VulkanContext::FENCE_TIMEOUT_NS) !=
                VK_SUCCESS) {
                qCritical()
                    << "[VkSnapshotReconstructor] Timeout waiting for delta fence in applyDelta";
                return false;
            }
        }
        checkAndSwapBase();
    }

    uint32_t          tileW = 0, tileH = 0;
    QByteArray        packedPixels;
    QVector<uint32_t> tileIndices, tileOffsets;

    if (!parseAndDecompressDelta(delta, tileW, tileH, packedPixels, tileIndices, tileOffsets)) {
        return false;
    }

    if (tileIndices.isEmpty()) {
        return true;
    }

    if (m_deltaFence) {
        if (df->vkWaitForFences(dev, 1, &m_deltaFence, VK_TRUE, VulkanContext::FENCE_TIMEOUT_NS) !=
            VK_SUCCESS) {
            qCritical() << "[VkSnapshotReconstructor] Timeout waiting for delta fence during "
                           "staging update";
            return false;
        }
        df->vkResetFences(dev, 1, &m_deltaFence);
    }

    if (!updateStagingResources(packedPixels, tileIndices, tileOffsets)) {
        return false;
    }

    if (m_isUploadingBase && m_uploadFence) {
        if (df->vkWaitForFences(dev, 1, &m_uploadFence, VK_TRUE, VulkanContext::FENCE_TIMEOUT_NS) !=
            VK_SUCCESS) {
            qCritical()
                << "[VkSnapshotReconstructor] Timeout waiting for base upload fence in applyDelta";
            return false;
        }
    }

    return recordDeltaCommands(tileW, tileH, tileIndices.size());
}

bool VkSnapshotReconstructor::copyToVulkanImage(VkCommandBuffer cmd,
                                                VkImage         targetImage,
                                                uint32_t        width,
                                                uint32_t        height) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_stateBuffer) {
        qDebug() << "[VkSnapshotReconstructor] Null state buffer in copyToVulkanImage";
        return false;
    }

    auto df = m_handles.deviceFunctions;

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

bool VkSnapshotReconstructor::waitForDeltas() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto                                  df = m_handles.deviceFunctions;
    auto                                  dev = m_handles.device;

    if (!df || !dev)
        return false;

    // Wait for base upload
    if (m_isUploadingBase && m_uploadFence) {
        if (df->vkWaitForFences(dev, 1, &m_uploadFence, VK_TRUE, VulkanContext::FENCE_TIMEOUT_NS) !=
            VK_SUCCESS) {
            qCritical() << "[VkSnapshotReconstructor] Timeout waiting for base upload fence in "
                           "waitForDeltas";
            cancelBaseUpload();
            return false;
        }
    }

    if (m_isApplyingDelta && m_deltaFence) {
        if (df->vkWaitForFences(dev, 1, &m_deltaFence, VK_TRUE, VulkanContext::FENCE_TIMEOUT_NS) !=
            VK_SUCCESS) {
            qCritical()
                << "[VkSnapshotReconstructor] Timeout waiting for delta fence in waitForDeltas";
            cancelDeltaApplication();
            return false;
        }
        cancelDeltaApplication();
    }

    return true;
}

void VkSnapshotReconstructor::resetState() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto                                  df = m_handles.deviceFunctions;
    auto                                  dev = m_handles.device;
    if (!df || !dev)
        return;

    // Destroy active state buffer
    if (m_stateBuffer) {
        VulkanUtils::destroyResource(
            df, dev, m_stateBuffer, &QVulkanDeviceFunctions::vkDestroyBuffer);
        VulkanUtils::freeMemory(df, dev, m_stateMemory);
        m_stateBuffer = VK_NULL_HANDLE;
        m_stateMemory = VK_NULL_HANDLE;
        m_stateBufferSize = 0;
    }

    // Destroy cached base buffer
    if (m_cachedBaseBuffer) {
        VulkanUtils::destroyResource(
            df, dev, m_cachedBaseBuffer, &QVulkanDeviceFunctions::vkDestroyBuffer);
        VulkanUtils::freeMemory(df, dev, m_cachedBaseMemory);
        m_cachedBaseBuffer = VK_NULL_HANDLE;
        m_cachedBaseMemory = VK_NULL_HANDLE;
        m_lastBaseChecksum = QString();
    }

    // Clear pending state
    if (m_pendingStateBuffer) {
        VulkanUtils::destroyResource(
            df, dev, m_pendingStateBuffer, &QVulkanDeviceFunctions::vkDestroyBuffer);
        VulkanUtils::freeMemory(df, dev, m_pendingStateMemory);
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
        if (!waitForDeltas()) {
            return false;
        }

        // Swap active state buffer with the pending one
        if (m_stateBuffer) {
            VulkanUtils::destroyResource(
                df, dev, m_stateBuffer, &QVulkanDeviceFunctions::vkDestroyBuffer);
            VulkanUtils::freeMemory(df, dev, m_stateMemory);
        }

        m_stateBuffer = m_pendingStateBuffer;
        m_stateMemory = m_pendingStateMemory;

        // Reset pending buffer to null so it can be recreated on next resetToBase
        m_pendingStateBuffer = VK_NULL_HANDLE;
        m_pendingStateMemory = VK_NULL_HANDLE;
        m_isUploadingBase = false;

        // Update the descriptor set to point to the new active buffer
        updateStateBufferBinding();
        return true;
    }
    return false;
}

void VkSnapshotReconstructor::updateStateBufferBinding() {
    if (m_descriptorSet == VK_NULL_HANDLE)
        return;

    auto df = m_handles.deviceFunctions;
    auto dev = m_handles.device;
    if (!df || !dev)
        return;

    VkDescriptorBufferInfo bufferInfo{m_stateBuffer, 0, m_stateBufferSize};
    VkWriteDescriptorSet   write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = m_descriptorSet;
    write.dstBinding = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.descriptorCount = 1;
    write.pBufferInfo = &bufferInfo;
    df->vkUpdateDescriptorSets(dev, 1, &write, 0, nullptr);
}

bool VkSnapshotReconstructor::updateCachedBase(VkDeviceSize size) {
    auto df = m_handles.deviceFunctions;
    auto dev = m_handles.device;

    if (m_cachedBaseBuffer && m_stateBufferSize == size) {
        return true;
    }

    if (m_cachedBaseBuffer) {
        VulkanUtils::destroyResource(
            df, dev, m_cachedBaseBuffer, &QVulkanDeviceFunctions::vkDestroyBuffer);
        VulkanUtils::freeMemory(df, dev, m_cachedBaseMemory);
        m_cachedBaseBuffer = VK_NULL_HANDLE;
        m_cachedBaseMemory = VK_NULL_HANDLE;
    }

    auto alloc = VulkanUtils::createBuffer(m_context->getInstance(),
                                           df,
                                           dev,
                                           m_handles.physicalDevice,
                                           size,
                                           VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                               VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (alloc.buffer == VK_NULL_HANDLE) {
        alloc = VulkanUtils::createBuffer(
            m_context->getInstance(),
            df,
            dev,
            m_handles.physicalDevice,
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

bool VkSnapshotReconstructor::ensurePendingStateBuffer(uint32_t     width,
                                                       uint32_t     height,
                                                       VkDeviceSize size) {
    auto df = m_handles.deviceFunctions;
    auto dev = m_handles.device;
    if (!df || !dev)
        return false;

    if (!m_pendingStateBuffer || m_width != width || m_height != height) {
        if (m_isUploadingBase && m_uploadFence) {
            if (df->vkWaitForFences(
                    dev, 1, &m_uploadFence, VK_TRUE, VulkanContext::FENCE_TIMEOUT_NS) !=
                VK_SUCCESS) {
                qCritical() << "[VkSnapshotReconstructor] Timeout waiting for base upload fence";
                cancelBaseUpload();
                return false;
            }
        }

        if (m_pendingStateBuffer) {
            VulkanUtils::destroyResource(
                df, dev, m_pendingStateBuffer, &QVulkanDeviceFunctions::vkDestroyBuffer);
            VulkanUtils::freeMemory(df, dev, m_pendingStateMemory);
        }

        auto alloc = VulkanUtils::createBuffer(m_context->getInstance(),
                                               df,
                                               dev,
                                               m_handles.physicalDevice,
                                               size,
                                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (alloc.buffer == VK_NULL_HANDLE) {
            alloc = VulkanUtils::createBuffer(
                m_context->getInstance(),
                df,
                dev,
                m_handles.physicalDevice,
                size,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        }

        if (alloc.buffer == VK_NULL_HANDLE) {
            qCritical() << "[VkSnapshotReconstructor] Failed to create pending state buffer";
            m_isUploadingBase = false;
            return false;
        }
        m_pendingStateBuffer = alloc.buffer;
        m_pendingStateMemory = alloc.memory;
    }
    return true;
}

bool VkSnapshotReconstructor::prepareBaseStaging(const QImage& base,
                                                 uint32_t      width,
                                                 uint32_t      height,
                                                 VkDeviceSize  size) {
    auto df = m_handles.deviceFunctions;
    auto dev = m_handles.device;
    if (!df || !dev)
        return false;

    QImage baseRGBA =
        base.format() == QImage::Format_ARGB32 ? base : base.convertToFormat(QImage::Format_ARGB32);

    if (!m_baseStagingBuffer || size > m_baseStagingCapacity) {
        if (m_baseStagingBuffer) {
            if (m_baseStagingMapped)
                df->vkUnmapMemory(dev, m_baseStagingMemory);
            VulkanUtils::destroyResource(
                df, dev, m_baseStagingBuffer, &QVulkanDeviceFunctions::vkDestroyBuffer);
            VulkanUtils::freeMemory(df, dev, m_baseStagingMemory);
        }
        m_baseStagingCapacity = std::max(size, (VkDeviceSize)1024 * 1024 * 64);
        auto alloc = VulkanUtils::createBuffer(m_context->getInstance(),
                                               df,
                                               dev,
                                               m_handles.physicalDevice,
                                               m_baseStagingCapacity,
                                               VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (alloc.buffer == VK_NULL_HANDLE) {
            qCritical() << "[VkSnapshotReconstructor] Failed to create base staging buffer";
            m_isUploadingBase = false;
            return false;
        }
        m_baseStagingBuffer = alloc.buffer;
        m_baseStagingMemory = alloc.memory;
        df->vkMapMemory(
            dev, m_baseStagingMemory, 0, m_baseStagingCapacity, 0, &m_baseStagingMapped);
    }

    if (baseRGBA.bytesPerLine() == static_cast<int>(width) * 4) {
        memcpy(m_baseStagingMapped, baseRGBA.bits(), size);
    } else {
        for (int y = 0; y < static_cast<int>(height); ++y) {
            memcpy(static_cast<uchar *>(m_baseStagingMapped) + static_cast<size_t>(y) * width * 4,
                   baseRGBA.constScanLine(y),
                   static_cast<size_t>(width) * 4);
        }
    }

    if (!updateCachedBase(size)) {
        qCritical() << "[VkSnapshotReconstructor] Failed to update GPU base cache";
        m_isUploadingBase = false;
        return false;
    }
    return true;
}

bool VkSnapshotReconstructor::recordBaseUploadCommands(bool isCached, VkDeviceSize size) {
    auto df = m_handles.deviceFunctions;
    auto dev = m_handles.device;
    if (!df || !dev || !m_handles.commandPool)
        return false;

    if (!m_computeCmdBuffer) {
        VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocInfo.commandPool = m_handles.commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;
        df->vkAllocateCommandBuffers(dev, &allocInfo, &m_computeCmdBuffer);
        if (m_computeCmdBuffer == VK_NULL_HANDLE) {
            qCritical() << "[VkSnapshotReconstructor] Failed to allocate compute command buffer";
            return false;
        }
    }

    if (m_isApplyingDelta && m_deltaFence) {
        if (df->vkWaitForFences(dev, 1, &m_deltaFence, VK_TRUE, VulkanContext::FENCE_TIMEOUT_NS) !=
            VK_SUCCESS) {
            qCritical() << "[VkSnapshotReconstructor] Timeout waiting for delta fence";
            cancelDeltaApplication();
            return false;
        }
    }
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

        VkBufferMemoryBarrier barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.buffer = m_pendingStateBuffer;
        barrier.offset = 0;
        barrier.size = size;
        df->vkCmdPipelineBarrier(m_computeCmdBuffer, 0, 0, 0, 0, nullptr, 1, &barrier, 0, nullptr);

        df->vkCmdCopyBuffer(
            m_computeCmdBuffer, m_pendingStateBuffer, m_cachedBaseBuffer, 1, &copyRegion);
    }

    df->vkEndCommandBuffer(m_computeCmdBuffer);
    return true;
}

void VkSnapshotReconstructor::cancelBaseUpload() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_isUploadingBase = false;
}

void VkSnapshotReconstructor::cancelDeltaApplication() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_isApplyingDelta = false;
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

VkSnapshotReconstructor::DownsampledBuffer VkSnapshotReconstructor::performDownsample(
    VkBuffer srcBuffer, VkDeviceSize srcSize, uint32_t srcW, uint32_t srcH, QSize targetSize) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto                                  df = m_handles.deviceFunctions;
    auto                                  dev = m_handles.device;
    auto                                  pool = m_handles.commandPool;
    auto                                 *ctx = m_context;
    DownsampledBuffer                     result;

    if (targetSize.isEmpty() ||
        ((int)srcW <= targetSize.width() && (int)srcH <= targetSize.height())) {
        result.buffer = srcBuffer;
        result.width = srcW;
        result.height = srcH;
        return result;
    }

    result.width = targetSize.width();
    result.height = targetSize.height();
    VkDeviceSize finalBufferSize = (VkDeviceSize)result.width * result.height * 4;

    // Manage Cached Buffer
    if (m_downsampleBuffer == VK_NULL_HANDLE || m_downsampleBufferSize < finalBufferSize) {
        if (m_downsampleBuffer) {
            VulkanUtils::destroyResource(
                df, dev, m_downsampleBuffer, &QVulkanDeviceFunctions::vkDestroyBuffer);
            VulkanUtils::freeMemory(df, dev, m_downsampleMemory);
        }

        auto alloc = VulkanUtils::createBuffer(
            ctx->getInstance(),
            df,
            dev,
            m_handles.physicalDevice,
            finalBufferSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        if (alloc.buffer == VK_NULL_HANDLE) {
            qCritical() << "[VkSnapshotReconstructor] Failed to allocate downsample buffer";
            return result;
        }
        m_downsampleBuffer = alloc.buffer;
        m_downsampleMemory = alloc.memory;
        m_downsampleBufferSize = finalBufferSize;
    }
    result.buffer = m_downsampleBuffer;
    result.memory = m_downsampleMemory;

    // Manage Descriptor Set
    if (m_downsampleDescriptorSet == VK_NULL_HANDLE) {
        VkDescriptorSetAllocateInfo dsai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        dsai.descriptorPool = ctx->getDescriptorPool(dev);
        dsai.descriptorSetCount = 1;
        VkDescriptorSetLayout layout = ctx->getDownsampleDescriptorSetLayout(dev);
        dsai.pSetLayouts = &layout;
        if (df->vkAllocateDescriptorSets(dev, &dsai, &m_downsampleDescriptorSet) != VK_SUCCESS) {
            qCritical() << "[VkSnapshotReconstructor] Failed to allocate downsample descriptor set";
            return result;
        }
    }

    VkDescriptorBufferInfo bufferInfos[2] = {};
    bufferInfos[0].buffer = srcBuffer;
    bufferInfos[0].offset = 0;
    bufferInfos[0].range = srcSize;
    bufferInfos[1].buffer = m_downsampleBuffer;
    bufferInfos[1].offset = 0;
    bufferInfos[1].range = finalBufferSize;

    VkWriteDescriptorSet writes[2] = {};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = m_downsampleDescriptorSet;
    writes[0].dstBinding = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[0].descriptorCount = 1;
    writes[0].pBufferInfo = &bufferInfos[0];
    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = m_downsampleDescriptorSet;
    writes[1].dstBinding = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[1].descriptorCount = 1;
    writes[1].pBufferInfo = &bufferInfos[1];

    df->vkUpdateDescriptorSets(dev, 2, writes, 0, nullptr);

    // Manage Command Buffer
    if (!m_downsampleCmdBuffer) {
        VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocInfo.commandPool = pool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;
        df->vkAllocateCommandBuffers(dev, &allocInfo, &m_downsampleCmdBuffer);
    } else {
        df->vkResetCommandBuffer(m_downsampleCmdBuffer, 0);
    }

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    df->vkBeginCommandBuffer(m_downsampleCmdBuffer, &beginInfo);

    struct PushConstants {
        uint32_t w, h, tw, th;
    } pcs = {srcW, srcH, result.width, result.height};
    df->vkCmdBindPipeline(
        m_downsampleCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, ctx->getDownsamplePipeline(dev));
    df->vkCmdBindDescriptorSets(m_downsampleCmdBuffer,
                                VK_PIPELINE_BIND_POINT_COMPUTE,
                                ctx->getDownsamplePipelineLayout(dev),
                                0,
                                1,
                                &m_downsampleDescriptorSet,
                                0,
                                nullptr);
    df->vkCmdPushConstants(m_downsampleCmdBuffer,
                           ctx->getDownsamplePipelineLayout(dev),
                           VK_SHADER_STAGE_COMPUTE_BIT,
                           0,
                           sizeof(pcs),
                           &pcs);
    df->vkCmdDispatch(
        m_downsampleCmdBuffer, (result.width + 15) / 16, (result.height + 15) / 16, 1);
    df->vkEndCommandBuffer(m_downsampleCmdBuffer);

    // Manage Fence
    if (!m_downsampleFence) {
        VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        df->vkCreateFence(dev, &fci, nullptr, &m_downsampleFence);
    } else {
        df->vkResetFences(dev, 1, &m_downsampleFence);
    }

    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &m_downsampleCmdBuffer;
    df->vkQueueSubmit(m_handles.queue, 1, &submit, m_downsampleFence);

    VkResult res =
        df->vkWaitForFences(dev, 1, &m_downsampleFence, VK_TRUE, VulkanContext::FENCE_TIMEOUT_NS);
    if (res != VK_SUCCESS) {
        qCritical() << "[VkSnapshotReconstructor] Timeout waiting for downsample fence";
    }

    return result;
}

QImage VkSnapshotReconstructor::copyToQImage(VkBuffer     buffer,
                                             VkDeviceSize size,
                                             uint32_t     width,
                                             uint32_t     height) {
    auto df = m_handles.deviceFunctions;
    auto dev = m_handles.device;
    auto pool = m_handles.commandPool;

    auto stagingAlloc = VulkanUtils::createBuffer(m_context->getInstance(),
                                                  df,
                                                  dev,
                                                  m_handles.physicalDevice,
                                                  size,
                                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (stagingAlloc.buffer == VK_NULL_HANDLE) {
        qCritical() << "[VkSnapshotReconstructor] copyToQImage: Failed to create staging buffer";
        return QImage();
    }

    VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocInfo.commandPool = pool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    VkCommandBuffer copyCmd = VK_NULL_HANDLE;
    df->vkAllocateCommandBuffers(dev, &allocInfo, &copyCmd);
    if (copyCmd == VK_NULL_HANDLE) {
        qCritical()
            << "[VkSnapshotReconstructor] copyToQImage: Failed to allocate copy command buffer";
        VulkanUtils::destroyResource(
            df, dev, stagingAlloc.buffer, &QVulkanDeviceFunctions::vkDestroyBuffer);
        VulkanUtils::freeMemory(df, dev, stagingAlloc.memory);
        return QImage();
    }

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    df->vkBeginCommandBuffer(copyCmd, &beginInfo);

    VkBufferCopy copyRegion = {0, 0, size};
    df->vkCmdCopyBuffer(copyCmd, buffer, stagingAlloc.buffer, 1, &copyRegion);
    df->vkEndCommandBuffer(copyCmd);

    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &copyCmd;

    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence           fence = VK_NULL_HANDLE;
    df->vkCreateFence(dev, &fci, nullptr, &fence);

    df->vkQueueSubmit(m_handles.queue, 1, &submit, fence);

    VkResult res = df->vkWaitForFences(dev, 1, &fence, VK_TRUE, VulkanContext::FENCE_TIMEOUT_NS);
    df->vkDestroyFence(dev, fence, nullptr);
    df->vkFreeCommandBuffers(dev, pool, 1, &copyCmd);

    if (res != VK_SUCCESS) {
        VulkanUtils::destroyResource(
            df, dev, stagingAlloc.buffer, &QVulkanDeviceFunctions::vkDestroyBuffer);
        VulkanUtils::freeMemory(df, dev, stagingAlloc.memory);
        return QImage();
    }

    void *data = nullptr;
    df->vkMapMemory(dev, stagingAlloc.memory, 0, size, 0, &data);
    QImage result =
        QImage(static_cast<const uchar *>(data), width, height, width * 4, QImage::Format_ARGB32)
            .copy();
    df->vkUnmapMemory(dev, stagingAlloc.memory);

    VulkanUtils::destroyResource(
        df, dev, stagingAlloc.buffer, &QVulkanDeviceFunctions::vkDestroyBuffer);
    VulkanUtils::freeMemory(df, dev, stagingAlloc.memory);

    return result;
}

QImage VkSnapshotReconstructor::reconstructToImage(const ReconstructionSequence& seq,
                                                   QSize                         targetSize,
                                                   VkSnapshotReconstructor      *worker) {
    std::unique_lock<std::recursive_mutex>   lock(m_mutex);
    std::unique_ptr<VkSnapshotReconstructor> localWorker = nullptr;
    VkSnapshotReconstructor                 *activeWorker = worker;

    if (!activeWorker) {
        localWorker = std::make_unique<VkSnapshotReconstructor>(m_handles, m_context);
        activeWorker = localWorker.get();
    }

    if (!activeWorker->reconstruct(seq)) {
        activeWorker->cancelBaseUpload();
        return QImage();
    }

    VkBuffer     srcBuffer = activeWorker->stateBuffer();
    VkDeviceSize srcBufferSize = activeWorker->stateBufferSize();
    uint32_t     srcW = activeWorker->width();
    uint32_t     srcH = activeWorker->height();

    if (srcBuffer == VK_NULL_HANDLE || srcBufferSize == 0) {
        return QImage();
    }

    DownsampledBuffer downsample =
        performDownsample(srcBuffer, srcBufferSize, srcW, srcH, targetSize);
    if (downsample.buffer == VK_NULL_HANDLE) {
        return QImage();
    }

    QImage result = copyToQImage(downsample.buffer,
                                 (VkDeviceSize)downsample.width * downsample.height * 4,
                                 downsample.width,
                                 downsample.height);

    return result;
}
