#include "ui/vkimageviewerrenderer.h"
#include "config/appsettings.h"
#include "core/vulkancontext.h"
#include "core/vulkanutils.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QFile>
#include <algorithm>
#include <cstring>

// Push constant structure for the indicator
struct IndicatorConstants {
    float offset[2];
    float size[2];
    float color[4];
};

void VkImageViewerRenderer::setReconstructor(
    const std::shared_ptr<VkSnapshotReconstructor>& reconstructor) {
    std::lock_guard<std::mutex> lock(m_reconstructorMutex);

    if (m_activeReconstructor == reconstructor)
        return;

    m_activeReconstructor = reconstructor;

    setRenderState(RenderState::Empty, m_currentGeneration);
    m_textureView = VK_NULL_HANDLE;
    m_uboDirty = true;
}

void VkImageViewerRenderer::clear() {
    std::lock_guard<std::mutex> lock(m_reconstructorMutex);
    m_activeReconstructor = nullptr;
    setRenderState(RenderState::Empty, m_currentGeneration);
    m_sourceImage = QImage();
    m_textureView = VK_NULL_HANDLE;
    m_uboDirty = true;
}

void VkImageViewerRenderer::setIndicator(QPoint pos, QColor color, bool visible) {
    m_indicatorPos = pos;
    m_indicatorColor = color;
    m_indicatorVisible = visible;
}

void VkImageViewerRenderer::initResources() {
    VulkanContext::instance().setUIDevice(
        m_vkWindow->device(), m_vkWindow->physicalDevice(), m_vkWindow->vulkanInstance());

    m_devFuncs = m_vkWindow->vulkanInstance()->deviceFunctions(m_vkWindow->device());

    if (!createSamplers())
        return;
    VulkanContext::instance().createGraphicsPipeline(
        m_vkWindow->device(), m_devFuncs, m_vkWindow->defaultRenderPass());
    if (!createDescriptorPoolAndSet())
        return;
    if (!createUniformBuffer())
        return;
    if (!createVertexBuffer())
        return;

    // Create indicator pipeline
    createIndicatorPipeline();

    m_activeSampler = m_samplerLinear;

    // Update descriptor set now if a texture view already exists
    if (m_textureView != VK_NULL_HANDLE)
        updateDescriptors(m_descriptorSet, m_uniformBuffer, m_samplerLinear, m_textureView);

    // Mark UBO dirty so the first frame writes the current state into the
    // freshly mapped buffer
    m_uboDirty = true;

    // Execute any reconstruction that was deferred because m_vkWindow was null
    if (m_pendingReconstruction) {
        reconstruct(*m_pendingReconstruction, m_pendingGeneration);
        m_pendingReconstruction.reset();
    }
}

void VkImageViewerRenderer::createIndicatorPipeline() {
    VkDevice                dev = m_vkWindow->device();
    QVulkanDeviceFunctions *df = m_devFuncs;

    auto loadShader = [](const QString& path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            qCritical() << "[VkImageViewer] Failed to open shader file:" << path;
            return QByteArray();
        }
        return file.readAll();
    };

    QByteArray vertCode = loadShader(":/shaders/cluster_indicator.vert.spv");
    QByteArray fragCode = loadShader(":/shaders/cluster_indicator.frag.spv");

    if (vertCode.isEmpty() || fragCode.isEmpty()) {
        qCritical() << "[VkImageViewer] Failed to load indicator shaders";
        return;
    }

    VkShaderModuleCreateInfo vci{};
    vci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vci.codeSize = vertCode.size();
    vci.pCode = reinterpret_cast<const uint32_t *>(vertCode.data());
    VkShaderModule vertMod;
    df->vkCreateShaderModule(dev, &vci, nullptr, &vertMod);

    vci.codeSize = fragCode.size();
    vci.pCode = reinterpret_cast<const uint32_t *>(fragCode.data());
    VkShaderModule fragMod;
    df->vkCreateShaderModule(dev, &vci, nullptr, &fragMod);

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertMod;
    stages[0].pName = "main";

    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragMod;
    stages[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(float) * 4;
    VkVertexInputAttributeDescription attr[2]{};
    attr[0] = {0, 0, VK_FORMAT_R32G32_SFLOAT, 0};
    attr[1] = {1, 0, VK_FORMAT_R32G32_SFLOAT, sizeof(float) * 2};
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &binding;
    vi.vertexAttributeDescriptionCount = 2;
    vi.pVertexAttributeDescriptions = attr;

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vs{};
    vs.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vs.viewportCount = 1;
    vs.scissorCount = 1;

    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo ds{};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    ds.dynamicStateCount = 2;
    ds.pDynamicStates = dynamicStates;

    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.lineWidth = 1.0f;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState cb[1]{};
    cb[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    cb[0].blendEnable = VK_TRUE;
    cb[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    cb[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cb[0].colorBlendOp = VK_BLEND_OP_ADD;
    cb[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    cb[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cb[0].alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo cbs{};
    cbs.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cbs.logicOpEnable = VK_FALSE;
    cbs.attachmentCount = 1;
    cbs.pAttachments = cb;

    VkPipelineLayoutCreateInfo pli{};
    pli.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pli.pushConstantRangeCount = 1;
    VkPushConstantRange pcr{};
    pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pcr.offset = 0;
    pcr.size = sizeof(IndicatorConstants);
    pli.pPushConstantRanges = &pcr;
    df->vkCreatePipelineLayout(dev, &pli, nullptr, &m_indicatorPipelineLayout);

    VkGraphicsPipelineCreateInfo gp{};
    gp.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gp.stageCount = 2;
    gp.pStages = stages;
    gp.pVertexInputState = &vi;
    gp.pInputAssemblyState = &ia;
    gp.pViewportState = &vs;
    gp.pRasterizationState = &rs;
    gp.pMultisampleState = &ms;
    gp.pColorBlendState = &cbs;
    gp.pDynamicState = &ds;
    gp.layout = m_indicatorPipelineLayout;
    gp.renderPass = m_vkWindow->defaultRenderPass();
    gp.subpass = 0;

    df->vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &gp, nullptr, &m_indicatorPipeline);

    df->vkDestroyShaderModule(dev, vertMod, nullptr);
    df->vkDestroyShaderModule(dev, fragMod, nullptr);
}

bool VkImageViewerRenderer::createSamplers() {
    VkDevice                dev = m_vkWindow->device();
    QVulkanDeviceFunctions *df = m_devFuncs;

    VkSamplerCreateInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    si.magFilter = VK_FILTER_LINEAR;
    si.minFilter = VK_FILTER_LINEAR;
    si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    si.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.anisotropyEnable = VK_FALSE;
    si.maxAnisotropy = 1.0f;
    si.minLod = 0.0f;
    si.maxLod = 100.0f;
    VkResult result = df->vkCreateSampler(dev, &si, nullptr, &m_samplerLinear);
    if (result != VK_SUCCESS) {
        qCritical() << "[VkImageViewer] vkCreateSampler (linear) failed:" << result;
        return false;
    }

    // Nearest-neighbor sampler (sharp pixels when zoomed in)
    VkSamplerCreateInfo siNearest = si;
    siNearest.magFilter = VK_FILTER_NEAREST;
    siNearest.minFilter = VK_FILTER_NEAREST;
    siNearest.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    result = df->vkCreateSampler(dev, &siNearest, nullptr, &m_samplerNearest);
    if (result != VK_SUCCESS) {
        qCritical() << "[VkImageViewer] vkCreateSampler (nearest) failed:" << result;
        VulkanUtils::destroyResource(
            df, dev, m_samplerLinear, &QVulkanDeviceFunctions::vkDestroySampler);
        return false;
    }
    return true;
}

// Removed helper methods: createShaderModules, createDescriptorLayout, createPipelineLayout,
// createPipeline as they are now managed by VulkanContext.

bool VkImageViewerRenderer::createDescriptorPoolAndSet() {
    VkDevice                dev = m_vkWindow->device();
    QVulkanDeviceFunctions *df = m_devFuncs;

    VkDescriptorPoolSize poolSizes[2]{};
    poolSizes[0] = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1};
    poolSizes[1] = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};

    VkDescriptorPoolCreateInfo dpci{};
    dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dpci.poolSizeCount = 2;
    dpci.pPoolSizes = poolSizes;
    dpci.maxSets = 1;
    df->vkCreateDescriptorPool(dev, &dpci, nullptr, &m_descriptorPool);

    VkDescriptorSetAllocateInfo dsa{};
    dsa.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dsa.descriptorPool = m_descriptorPool;
    dsa.descriptorSetCount = 1;
    VkDescriptorSetLayout layout = VulkanContext::instance().getGraphicsDescriptorSetLayout();
    dsa.pSetLayouts = &layout;
    VkResult result = df->vkAllocateDescriptorSets(dev, &dsa, &m_descriptorSet);
    if (result != VK_SUCCESS) {
        qCritical() << "[VkImageViewer] vkAllocateDescriptorSets failed:" << result;
        VulkanUtils::destroyResource(
            df, dev, m_descriptorPool, &QVulkanDeviceFunctions::vkDestroyDescriptorPool);
        return false;
    }
    return true;
}

bool VkImageViewerRenderer::createUniformBuffer() {
    VkDevice                dev = m_vkWindow->device();
    QVulkanDeviceFunctions *df = m_devFuncs;

    VkDeviceSize bufSize = sizeof(UniformBufferObject);

    auto cleanupOnError = [this, df, dev]() {
        if (m_uniformMapped) {
            df->vkUnmapMemory(dev, m_uniformMemory);
            m_uniformMapped = nullptr;
        }
        VulkanUtils::freeMemory(df, dev, m_uniformMemory);
        VulkanUtils::destroyResource(
            df, dev, m_uniformBuffer, &QVulkanDeviceFunctions::vkDestroyBuffer);
    };

    auto alloc = VulkanUtils::createBuffer(m_vkWindow->vulkanInstance(),
                                           df,
                                           dev,
                                           m_vkWindow->physicalDevice(),
                                           bufSize,
                                           VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (alloc.buffer == VK_NULL_HANDLE) {
        qCritical() << "[VkImageViewer] Failed to create uniform buffer";
        return false;
    }
    m_uniformBuffer = alloc.buffer;
    m_uniformMemory = alloc.memory;

    VkResult result = df->vkMapMemory(dev, m_uniformMemory, 0, VK_WHOLE_SIZE, 0, &m_uniformMapped);
    if (result != VK_SUCCESS) {
        qCritical() << "[VkImageViewer] vkMapMemory (UBO) failed:" << result;
        cleanupOnError();
        return false;
    }
    return true;
}

bool VkImageViewerRenderer::createVertexBuffer() {
    VkDevice                dev = m_vkWindow->device();
    QVulkanDeviceFunctions *df = m_devFuncs;

    static const float quadVertices[] = {
        //   x,    y,   u,   v
        -1.0f, -1.0f, 0.0f, 0.0f, // bottom-left
        1.0f,  -1.0f, 1.0f, 0.0f, // bottom-right
        -1.0f, 1.0f,  0.0f, 1.0f, // top-left
        1.0f,  -1.0f, 1.0f, 0.0f, // bottom-right
        1.0f,  1.0f,  1.0f, 1.0f, // top-right
        -1.0f, 1.0f,  0.0f, 1.0f, // top-left
    };
    VkDeviceSize vbSize = sizeof(quadVertices);

    auto cleanupOnError = [this, df, dev]() {
        VulkanUtils::freeMemory(df, dev, m_vertexMemory);
        VulkanUtils::destroyResource(
            df, dev, m_vertexBuffer, &QVulkanDeviceFunctions::vkDestroyBuffer);
    };

    VkBufferCreateInfo vbci{};
    vbci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vbci.size = vbSize;
    vbci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    vbci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkResult result = df->vkCreateBuffer(dev, &vbci, nullptr, &m_vertexBuffer);
    if (result != VK_SUCCESS) {
        qCritical() << "[VkImageViewer] vkCreateBuffer (vertex) failed:" << result;
        return false;
    }

    VkMemoryRequirements vbMemReq;
    df->vkGetBufferMemoryRequirements(dev, m_vertexBuffer, &vbMemReq);

    VkMemoryAllocateInfo vbMai{};
    vbMai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    vbMai.allocationSize = vbMemReq.size;
    vbMai.memoryTypeIndex = VulkanUtils::findMemoryType(m_vkWindow->vulkanInstance(),
                                                        m_vkWindow->physicalDevice(),
                                                        vbMemReq.memoryTypeBits,
                                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    result = df->vkAllocateMemory(dev, &vbMai, nullptr, &m_vertexMemory);
    if (result != VK_SUCCESS) {
        qCritical() << "[VkImageViewer] vkAllocateMemory (vertex) failed:" << result;
        cleanupOnError();
        return false;
    }
    result = df->vkBindBufferMemory(dev, m_vertexBuffer, m_vertexMemory, 0);
    if (result != VK_SUCCESS) {
        qCritical() << "[VkImageViewer] vkBindBufferMemory (vertex) failed:" << result;
        cleanupOnError();
        return false;
    }

    void *vbData = nullptr;
    result = df->vkMapMemory(dev, m_vertexMemory, 0, vbSize, 0, &vbData);
    if (result != VK_SUCCESS) {
        qCritical() << "[VkImageViewer] vkMapMemory (vertex) failed:" << result;
        cleanupOnError();
        return false;
    }
    std::memcpy(vbData, quadVertices, vbSize);
    df->vkUnmapMemory(dev, m_vertexMemory);
    return true;
}

void VkImageViewerRenderer::initSwapChainResources() {
    if (m_textureView == VK_NULL_HANDLE && !m_sourceImage.isNull()) {
        setRenderState(RenderState::Loading, m_currentGeneration);
    }
}

void VkImageViewerRenderer::releaseSwapChainResources() {
    // Nothing to do here.
}

void VkImageViewerRenderer::cleanupOldTexture() {
    VkDevice dev = m_vkWindow->device();
    VulkanUtils::destroyResource(
        m_devFuncs, dev, m_textureView, &QVulkanDeviceFunctions::vkDestroyImageView);
    VulkanUtils::destroyResource(
        m_devFuncs, dev, m_textureImage, &QVulkanDeviceFunctions::vkDestroyImage);
    VulkanUtils::freeMemory(m_devFuncs, dev, m_textureMemory);
    VulkanUtils::destroyResource(
        m_devFuncs, dev, m_stagingBuffer, &QVulkanDeviceFunctions::vkDestroyBuffer);
    VulkanUtils::freeMemory(m_devFuncs, dev, m_stagingMemory);

    m_textureView = VK_NULL_HANDLE;
    m_textureImage = VK_NULL_HANDLE;
    m_textureMemory = VK_NULL_HANDLE;
    m_stagingBuffer = VK_NULL_HANDLE;
    m_stagingMemory = VK_NULL_HANDLE;
    m_currentTextureWidth = 0;
    m_currentTextureHeight = 0;
}

void VkImageViewerRenderer::recordMipChainGeneration(VkCommandBuffer cmd,
                                                     int             mipLevels,
                                                     int             width,
                                                     int             height) {
    QVulkanDeviceFunctions *df = m_devFuncs;

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = m_textureImage;

    int curW = width;
    int curH = height;
    for (int i = 0; i < mipLevels - 1; ++i) {
        int nextW = std::max(1, curW >> 1);
        int nextH = std::max(1, curH >> 1);

        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, static_cast<uint32_t>(i), 1, 0, 1};
        df->vkCmdPipelineBarrier(cmd,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 0,
                                 nullptr,
                                 1,
                                 &barrier);

        VkImageBlit blit{};
        blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, static_cast<uint32_t>(i), 0, 1};
        blit.srcOffsets[1] = {curW, curH, 1};
        blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, static_cast<uint32_t>(i + 1), 0, 1};
        blit.dstOffsets[1] = {nextW, nextH, 1};
        df->vkCmdBlitImage(cmd,
                           m_textureImage,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           m_textureImage,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1,
                           &blit,
                           VK_FILTER_LINEAR);

        curW = nextW;
        curH = nextH;
    }
}

void VkImageViewerRenderer::createViewAndUpdateDescriptors(int mipLevels) {
    VkDevice                dev = m_vkWindow->device();
    QVulkanDeviceFunctions *df = m_devFuncs;

    VkImageViewCreateInfo vii{};
    vii.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vii.image = m_textureImage;
    vii.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vii.format = VK_FORMAT_B8G8R8A8_UNORM;
    vii.components = {VK_COMPONENT_SWIZZLE_IDENTITY,
                      VK_COMPONENT_SWIZZLE_IDENTITY,
                      VK_COMPONENT_SWIZZLE_IDENTITY,
                      VK_COMPONENT_SWIZZLE_IDENTITY};
    vii.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, static_cast<uint32_t>(mipLevels), 0, 1};

    VkResult result = df->vkCreateImageView(dev, &vii, nullptr, &m_textureView);
    if (result != VK_SUCCESS) {
        qCritical() << "[VkImageViewer] vkCreateImageView failed:" << result;
        return;
    }
    VkSampler sampler = (m_activeSampler != VK_NULL_HANDLE) ? m_activeSampler : m_samplerLinear;
    updateDescriptors(m_descriptorSet, m_uniformBuffer, sampler, m_textureView);
}

void VkImageViewerRenderer::releaseResources() {
    VkDevice dev = m_vkWindow->device();

    // Texture
    cleanupOldTexture();

    // Uniform buffer
    if (m_uniformMapped) {
        m_devFuncs->vkUnmapMemory(dev, m_uniformMemory);
        m_uniformMapped = nullptr;
    }
    VulkanUtils::destroyResource(
        m_devFuncs, dev, m_uniformBuffer, &QVulkanDeviceFunctions::vkDestroyBuffer);
    VulkanUtils::freeMemory(m_devFuncs, dev, m_uniformMemory);

    // Vertex buffer
    VulkanUtils::destroyResource(
        m_devFuncs, dev, m_vertexBuffer, &QVulkanDeviceFunctions::vkDestroyBuffer);
    VulkanUtils::freeMemory(m_devFuncs, dev, m_vertexMemory);

    // Descriptors
    VulkanUtils::destroyResource(
        m_devFuncs, dev, m_descriptorPool, &QVulkanDeviceFunctions::vkDestroyDescriptorPool);

    // Persistent resources
    VulkanUtils::destroyResource(
        m_devFuncs, dev, m_samplerLinear, &QVulkanDeviceFunctions::vkDestroySampler);
    VulkanUtils::destroyResource(
        m_devFuncs, dev, m_samplerNearest, &QVulkanDeviceFunctions::vkDestroySampler);

    // Indicator resources
    VulkanUtils::destroyResource(
        m_devFuncs, dev, m_indicatorPipeline, &QVulkanDeviceFunctions::vkDestroyPipeline);
    VulkanUtils::destroyResource(m_devFuncs,
                                 dev,
                                 m_indicatorPipelineLayout,
                                 &QVulkanDeviceFunctions::vkDestroyPipelineLayout);

    // Cleanup reconstructor resources
    std::lock_guard<std::mutex> lock(m_reconstructorMutex);
    if (m_activeReconstructor) {
        m_activeReconstructor->cleanup();
    }
}

void VkImageViewerRenderer::updateUniformBuffer() {
    if (!m_uniformMapped)
        return;

    float vpW = m_params.viewportWidth;
    float vpH = m_params.viewportHeight;

    if (vpW == 0 || vpH == 0)
        return;

    float imgAspect = (m_params.imageWidth > 0 && m_params.imageHeight > 0)
                          ? m_params.imageWidth / m_params.imageHeight
                          : 1.0f;
    float scaleX = vpW / (imgAspect * vpH);
    float fit = std::min(scaleX, 1.0f);
    float fitImgW = fit * imgAspect * vpH;
    float fitImgH = fit * vpH;
    float originX = (vpW - fitImgW) * 0.5f;
    float originY = (vpH - fitImgH) * 0.5f;
    float fitScale = (m_params.imageWidth > 0) ? fitImgW / m_params.imageWidth : 1.0f;

    auto *ubo = static_cast<UniformBufferObject *>(m_uniformMapped);
    ubo->uViewport[0] = vpW;
    ubo->uViewport[1] = vpH;
    ubo->uFitImgSize[0] = fitImgW;
    ubo->uFitImgSize[1] = fitImgH;
    ubo->uFitImgOrigin[0] = originX;
    ubo->uFitImgOrigin[1] = originY;
    ubo->uPanOffset[0] = m_params.panX;
    ubo->uPanOffset[1] = m_params.panY;
    ubo->uFitScale = fitScale;
    ubo->uZoomLevel = m_params.zoom;
    ubo->uGrayscale = m_params.grayscale ? VK_TRUE : VK_FALSE;
    ubo->uMirror = m_params.mirror ? VK_TRUE : VK_FALSE;
}

void VkImageViewerRenderer::updateDescriptors(VkDescriptorSet dstSet,
                                              VkBuffer        ubo,
                                              VkSampler       samp,
                                              VkImageView     texView) {
    VkDescriptorBufferInfo bufInfo{};
    bufInfo.buffer = ubo;
    bufInfo.offset = 0;
    bufInfo.range = VK_WHOLE_SIZE;

    VkDescriptorImageInfo imgInfo{};
    imgInfo.sampler = samp;
    imgInfo.imageView = texView;
    imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet writes[2];
    memset(writes, 0, sizeof(writes));

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = dstSet;
    writes[0].dstBinding = 0;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].descriptorCount = 1;
    writes[0].pBufferInfo = &bufInfo;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = dstSet;
    writes[1].dstBinding = 1;
    writes[1].dstArrayElement = 0;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].descriptorCount = 1;
    writes[1].pImageInfo = &imgInfo;

    m_devFuncs->vkUpdateDescriptorSets(m_vkWindow->device(), 2, writes, 0, nullptr);
}

int VkImageViewerRenderer::createTexture(int width, int height) {
    VkDevice                dev = m_vkWindow->device();
    QVulkanDeviceFunctions *df = m_devFuncs;

    cleanupOldTexture();

    int maxDim = std::max(width, height);
    int mipLevels = 0;
    while ((1u << mipLevels) <= static_cast<unsigned>(maxDim)) {
        mipLevels++;
    }
    mipLevels = std::max(1, mipLevels);

    VkImageCreateInfo ici{};
    ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format = VK_FORMAT_B8G8R8A8_UNORM;
    ici.extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
    ici.mipLevels = static_cast<uint32_t>(mipLevels);
    ici.arrayLayers = 1;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                VK_IMAGE_USAGE_SAMPLED_BIT;
    ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkResult result = df->vkCreateImage(dev, &ici, nullptr, &m_textureImage);
    if (result != VK_SUCCESS) {
        qCritical() << "[VkImageViewer] vkCreateImage failed:" << result;
        return 0;
    }

    VkMemoryRequirements memReq;
    df->vkGetImageMemoryRequirements(dev, m_textureImage, &memReq);

    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = memReq.size;
    mai.memoryTypeIndex = VulkanUtils::findMemoryType(m_vkWindow->vulkanInstance(),
                                                      m_vkWindow->physicalDevice(),
                                                      memReq.memoryTypeBits,
                                                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    result = df->vkAllocateMemory(dev, &mai, nullptr, &m_textureMemory);
    if (result != VK_SUCCESS) {
        qCritical() << "[VkImageViewer] vkAllocateMemory (texture) failed:" << result;
        return 0;
    }
    result = df->vkBindImageMemory(dev, m_textureImage, m_textureMemory, 0);
    if (result != VK_SUCCESS) {
        qCritical() << "[VkImageViewer] vkBindImageMemory failed:" << result;
        return 0;
    }

    createViewAndUpdateDescriptors(mipLevels);
    m_currentTextureWidth = width;
    m_currentTextureHeight = height;
    return mipLevels;
}

bool VkImageViewerRenderer::createAndUploadTexture(VkCommandBuffer cmd, const QImage& image) {
    VkDevice                dev = m_vkWindow->device();
    QVulkanDeviceFunctions *df = m_devFuncs;

    int mipLevels = createTexture(image.width(), image.height());
    if (mipLevels == 0)
        return false;

    // On error, clean up any resources created so far.
    auto cleanupOnError = [this, df, dev]() {
        VulkanUtils::destroyResource(
            df, dev, m_textureView, &QVulkanDeviceFunctions::vkDestroyImageView);
        VulkanUtils::destroyResource(
            df, dev, m_stagingBuffer, &QVulkanDeviceFunctions::vkDestroyBuffer);
        VulkanUtils::freeMemory(df, dev, m_stagingMemory);
        VulkanUtils::destroyResource(
            df, dev, m_textureImage, &QVulkanDeviceFunctions::vkDestroyImage);
        VulkanUtils::freeMemory(df, dev, m_textureMemory);
    };

    // Create staging buffer and upload image data
    VkDeviceSize imageSize = static_cast<VkDeviceSize>(image.width()) * image.height() * 4;

    VkBufferCreateInfo bci{};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = imageSize;
    bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkResult result = df->vkCreateBuffer(dev, &bci, nullptr, &m_stagingBuffer);
    if (result != VK_SUCCESS) {
        qCritical() << "[VkImageViewer] vkCreateBuffer (staging) failed:" << result;
        cleanupOnError();
        return false;
    }

    VkMemoryRequirements bufMemReq;
    df->vkGetBufferMemoryRequirements(dev, m_stagingBuffer, &bufMemReq);

    VkMemoryAllocateInfo bufMai{};
    bufMai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    bufMai.allocationSize = bufMemReq.size;
    bufMai.memoryTypeIndex = VulkanUtils::findMemoryType(m_vkWindow->vulkanInstance(),
                                                         m_vkWindow->physicalDevice(),
                                                         bufMemReq.memoryTypeBits,
                                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    result = df->vkAllocateMemory(dev, &bufMai, nullptr, &m_stagingMemory);
    if (result != VK_SUCCESS) {
        qCritical() << "[VkImageViewer] vkAllocateMemory (staging) failed:" << result;
        cleanupOnError();
        return false;
    }
    result = df->vkBindBufferMemory(dev, m_stagingBuffer, m_stagingMemory, 0);
    if (result != VK_SUCCESS) {
        qCritical() << "[VkImageViewer] vkBindBufferMemory failed:" << result;
        cleanupOnError();
        return false;
    }

    // Copy image pixels into staging buffer
    void *data = nullptr;
    result = df->vkMapMemory(dev, m_stagingMemory, 0, imageSize, 0, &data);
    if (result != VK_SUCCESS) {
        qCritical() << "[VkImageViewer] vkMapMemory failed:" << result;
        cleanupOnError();
        return false;
    }

    uint   rowBytes = image.width() * 4;
    uchar *dstRow = static_cast<uchar *>(data);
    for (int y = 0; y < image.height(); ++y) {
        std::memcpy(dstRow, image.constScanLine(y), rowBytes);
        dstRow += rowBytes;
    }
    df->vkUnmapMemory(dev, m_stagingMemory);

    // Transition image
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.image = m_textureImage;
    barrier.subresourceRange = {
        VK_IMAGE_ASPECT_COLOR_BIT, 0, static_cast<uint32_t>(mipLevels), 0, 1};
    df->vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_HOST_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0,
                             0,
                             nullptr,
                             0,
                             nullptr,
                             1,
                             &barrier);

    // Transfer image from staging buffer to texture
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {
        static_cast<uint32_t>(image.width()), static_cast<uint32_t>(image.height()), 1};
    df->vkCmdCopyBufferToImage(
        cmd, m_stagingBuffer, m_textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Generate mip levels via blit
    recordMipChainGeneration(cmd, mipLevels, image.width(), image.height());

    if (mipLevels > 1) {
        // Levels used as blit sources
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.subresourceRange = {
            VK_IMAGE_ASPECT_COLOR_BIT, 0, static_cast<uint32_t>(mipLevels - 1), 0, 1};
        df->vkCmdPipelineBarrier(cmd,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 0,
                                 nullptr,
                                 1,
                                 &barrier);

        // Last level (only a destination)
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.subresourceRange = {
            VK_IMAGE_ASPECT_COLOR_BIT, static_cast<uint32_t>(mipLevels - 1), 1, 0, 1};
        df->vkCmdPipelineBarrier(cmd,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 0,
                                 nullptr,
                                 1,
                                 &barrier);
    } else {
        // No mipmaps needed
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        df->vkCmdPipelineBarrier(cmd,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 0,
                                 nullptr,
                                 1,
                                 &barrier);
    }
    return true;
}

void VkImageViewerRenderer::setImage(const QImage& img, uint32_t generation) {
    if (img.isNull())
        return;

    if (!setRenderState(RenderState::Loading, generation))
        return;

    // Invalidate old texture view
    m_textureView = VK_NULL_HANDLE;

    // Convert to the BGRA format the renderer expects, skipping
    // the conversion if already correct.
    QImage rgba = img.format() == QImage::Format_ARGB32
                      ? QImage(img)
                      : img.convertToFormat(QImage::Format_ARGB32);

    // Store a persistent copy so the image can be re-uploaded after resources
    // are released
    m_sourceImage = std::move(rgba);
    m_uboDirty = true;
}

bool VkImageViewerRenderer::reconstruct(const ReconstructionSequence& seq, uint32_t generation) {
    // Defer reconstruction until the Vulkan window is ready (e.g. during session restore)
    if (!m_vkWindow) {
        m_pendingReconstruction = seq;
        m_pendingGeneration = generation;
        return true; // We've deferred it, so it's "successful" in terms of request handling
    }

    if (!setRenderState(RenderState::Reconstructing, generation))
        return false;

    std::shared_ptr<VkSnapshotReconstructor> reconstructor;
    {
        std::lock_guard<std::mutex> lock(m_reconstructorMutex);
        reconstructor = m_activeReconstructor;
    }

    if (!reconstructor) {
        qWarning() << "[VkImageViewerRenderer] reconstruct: Null reconstructor";
        setRenderState(RenderState::Empty, m_currentGeneration);
        return false;
    }

    m_sourceImage = seq.base;
    m_uboDirty = true;

    return reconstructor->reconstruct(seq);
}

void VkImageViewerRenderer::performUploads(
    VkCommandBuffer cmd, const std::shared_ptr<VkSnapshotReconstructor>& reconstructor) {
    if (m_state == RenderState::Empty || m_state == RenderState::Ready)
        return;

    if (m_state == RenderState::Reconstructing) {
        if (!reconstructor) {
            qCritical() << "[VkImageViewerRenderer] Reconstruction pending but no "
                           "reconstructor active!";
            setRenderState(RenderState::Empty, m_currentGeneration);
            return;
        }

        // Try to promote pending base buffer to active state buffer
        reconstructor->checkAndSwapBase();

        if (reconstructor->isUploadingBase()) {
            qDebug() << "[VkImageViewerRenderer] performUploads: Base image uploading, deferring";
            return;
        }

        if (reconstructor->stateBuffer() == VK_NULL_HANDLE) {
            qDebug() << "[VkImageViewerRenderer] performUploads: State buffer not yet allocated, "
                        "deferring";
            return;
        }

        VkImageLayout oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (m_textureImage == VK_NULL_HANDLE || m_textureView == VK_NULL_HANDLE ||
            (m_params.imageWidth != m_currentTextureWidth ||
             m_params.imageHeight != m_currentTextureHeight)) {
            if (createTexture(m_params.imageWidth, m_params.imageHeight) == 0) {
                qCritical()
                    << "[VkImageViewerRenderer] Failed to create texture for reconstruction";
                setRenderState(RenderState::Empty, m_currentGeneration);
                return;
            }
            oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        }


        // Global memory barrier to ensure compute writes are visible
        VkMemoryBarrier memBarrier{};
        memBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        memBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        memBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        if (!reconstructor->waitForDeltas()) {
            qWarning() << "[VkImageViewerRenderer] Timeout waiting for deltas during upload";
            setRenderState(RenderState::Empty, m_currentGeneration);
            return;
        }

        // Transition image to TRANSFER_DST_OPTIMAL for the copy
        VkImageMemoryBarrier imgBarrier{};
        imgBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imgBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        imgBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imgBarrier.oldLayout = oldLayout;
        imgBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imgBarrier.image = m_textureImage;
        imgBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        m_devFuncs->vkCmdPipelineBarrier(cmd,
                                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                                         0,
                                         1,
                                         &memBarrier,
                                         0,
                                         nullptr,
                                         1,
                                         &imgBarrier);

        // Copy reconstructed buffer to image
        reconstructor->copyToVulkanImage(
            cmd, m_textureImage, m_params.imageWidth, m_params.imageHeight);

        // Regenerate mip chain from the updated level 0
        int maxDim = std::max(m_params.imageWidth, m_params.imageHeight);
        int mipLevels = 0;
        while ((1u << mipLevels) <= static_cast<unsigned>(maxDim)) {
            mipLevels++;
        }
        mipLevels = std::max(1, mipLevels);
        recordMipChainGeneration(cmd, mipLevels, m_params.imageWidth, m_params.imageHeight);

        // Final transition to SHADER_READ_ONLY_OPTIMAL
        if (mipLevels > 1) {
            imgBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            imgBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            imgBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            imgBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imgBarrier.subresourceRange = {
                VK_IMAGE_ASPECT_COLOR_BIT, 0, static_cast<uint32_t>(mipLevels - 1), 0, 1};
            m_devFuncs->vkCmdPipelineBarrier(cmd,
                                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                             0,
                                             0,
                                             nullptr,
                                             0,
                                             nullptr,
                                             1,
                                             &imgBarrier);

            imgBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imgBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imgBarrier.subresourceRange = {
                VK_IMAGE_ASPECT_COLOR_BIT, static_cast<uint32_t>(mipLevels - 1), 1, 0, 1};
            m_devFuncs->vkCmdPipelineBarrier(cmd,
                                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                             0,
                                             0,
                                             nullptr,
                                             0,
                                             nullptr,
                                             1,
                                             &imgBarrier);
        } else {
            imgBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imgBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            imgBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imgBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imgBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            m_devFuncs->vkCmdPipelineBarrier(cmd,
                                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                             0,
                                             0,
                                             nullptr,
                                             0,
                                             nullptr,
                                             1,
                                             &imgBarrier);
        }
    } else if (m_state == RenderState::Loading) {
        if (!createAndUploadTexture(cmd, m_sourceImage)) {
            qCritical() << "[VkImageViewerRenderer] Regular upload failed (OOM?)";
            setRenderState(RenderState::Empty, m_currentGeneration);
            return;
        }
    }

    if (m_textureView != VK_NULL_HANDLE) {
        setRenderState(RenderState::Ready, m_currentGeneration);
    } else {
        setRenderState(RenderState::Empty, m_currentGeneration);
    }
}

void VkImageViewerRenderer::startNextFrame() {
    if (!m_vkWindow || VulkanContext::instance().getGraphicsPipeline() == VK_NULL_HANDLE) {
        return;
    }

    std::shared_ptr<VkSnapshotReconstructor> reconstructor;
    {
        std::lock_guard<std::mutex> lock(m_reconstructorMutex);
        reconstructor = m_activeReconstructor;
    }

    if (reconstructor) {
        if (reconstructor->checkAndSwapBase() || reconstructor->isDirty()) {
            setRenderState(RenderState::Reconstructing, m_currentGeneration);
        }
    }

    VkCommandBuffer cmd = m_vkWindow->currentCommandBuffer();

    performUploads(cmd, reconstructor);

    updateUniformBuffer();

    if (m_state != RenderState::Ready) {
        m_vkWindow->frameReady();
        m_vkWindow->requestUpdate();
        return;
    }

    if (m_textureView == VK_NULL_HANDLE) {
        m_vkWindow->frameReady();
        m_vkWindow->requestUpdate();
        return;
    }

    QSize     sz = m_vkWindow->swapChainImageSize();
    float     currentZoom = m_params.zoom;
    VkSampler activeSampler = (currentZoom >= 1.0f) ? m_samplerNearest : m_samplerLinear;

    if (activeSampler != m_activeSampler) {
        updateDescriptors(m_descriptorSet, m_uniformBuffer, activeSampler, m_textureView);
        m_activeSampler = activeSampler;
    }

    VkViewport vp{};
    vp.width = static_cast<float>(sz.width());
    vp.height = static_cast<float>(sz.height());
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;
    m_devFuncs->vkCmdSetViewport(cmd, 0, 1, &vp);

    VkRect2D sc{};
    sc.extent = {static_cast<uint32_t>(sz.width()), static_cast<uint32_t>(sz.height())};
    m_devFuncs->vkCmdSetScissor(cmd, 0, 1, &sc);

    VkClearValue cv[2]{};
    QColor       bgColor = AppSettings::backgroundColor();
    cv[0].color.float32[0] = bgColor.redF();
    cv[0].color.float32[1] = bgColor.greenF();
    cv[0].color.float32[2] = bgColor.blueF();
    cv[0].color.float32[3] = 1.0f;
    cv[1].depthStencil.depth = 1.0f;

    VkRenderPassBeginInfo rpi{};
    rpi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpi.renderPass = m_vkWindow->defaultRenderPass();
    rpi.framebuffer = m_vkWindow->currentFramebuffer();
    rpi.renderArea = sc;
    rpi.clearValueCount = 2;
    rpi.pClearValues = cv;

    m_devFuncs->vkCmdBeginRenderPass(cmd, &rpi, VK_SUBPASS_CONTENTS_INLINE);

    m_devFuncs->vkCmdBindPipeline(
        cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, VulkanContext::instance().getGraphicsPipeline());
    m_devFuncs->vkCmdBindDescriptorSets(cmd,
                                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        VulkanContext::instance().getGraphicsPipelineLayout(),
                                        0,
                                        1,
                                        &m_descriptorSet,
                                        0,
                                        nullptr);

    VkDeviceSize vbOffset = 0;
    m_devFuncs->vkCmdBindVertexBuffers(cmd, 0, 1, &m_vertexBuffer, &vbOffset);
    m_devFuncs->vkCmdDraw(cmd, 6, 1, 0, 0);

    if (m_indicatorVisible && m_indicatorPipeline != VK_NULL_HANDLE) {
        m_devFuncs->vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_indicatorPipeline);
        float              ndcX = (2.0f * m_indicatorPos.x() / sz.width()) - 1.0f;
        float              ndcy = (2.0f * m_indicatorPos.y() / sz.height()) - 1.0f;
        IndicatorConstants constants{};
        constants.offset[0] = ndcX;
        constants.offset[1] = ndcy;
        constants.size[0] = m_indicatorSize / (2.0f * sz.width());
        constants.size[1] = m_indicatorSize / (2.0f * sz.height());
        constants.color[0] = m_indicatorColor.redF();
        constants.color[1] = m_indicatorColor.greenF();
        constants.color[2] = m_indicatorColor.blueF();
        constants.color[3] = m_indicatorColor.alphaF();
        m_devFuncs->vkCmdPushConstants(cmd,
                                       m_indicatorPipelineLayout,
                                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                       0,
                                       sizeof(IndicatorConstants),
                                       &constants);
        m_devFuncs->vkCmdBindVertexBuffers(cmd, 0, 1, &m_vertexBuffer, &vbOffset);
        m_devFuncs->vkCmdDraw(cmd, 6, 1, 0, 0);
    }

    m_devFuncs->vkCmdEndRenderPass(cmd);
    m_vkWindow->frameReady();
    m_vkWindow->requestUpdate();
}
