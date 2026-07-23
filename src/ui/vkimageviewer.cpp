#include "ui/vkimageviewer.h"
#include "config/appsettings.h"
#include "core/viewstate.h"

#include <QAction>
#include <QDebug>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QGuiApplication>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QShowEvent>
#include <QUrl>
#include <QVBoxLayout>
#include <QVulkanFunctions>
#include <QWheelEvent>

#include <QFile>
#include <algorithm>
#include <array>
#include <cstring>
#include <vector>

class VkImageViewer;

// Safely destroys a Vulkan object if the handle is valid, then nulls it.
#define DESTROY_VK(df, dev, handle, destroyFn)                                                     \
    if (handle) {                                                                                  \
        (df)->destroyFn(dev, handle, nullptr);                                                     \
        handle = VK_NULL_HANDLE;                                                                   \
    }

struct alignas(4) UniformBufferObject {
    float    uViewport[2];     // widget size in pixels
    float    uFitImgSize[2];   // fitted image size in viewport pixels
    float    uFitImgOrigin[2]; // top-left corner of fitted image in viewport pixels
    float    uPanOffset[2];    // pan offset (center-shifted UV space)
    float    uFitScale;        // display-pixels / image-pixels (for LOD)
    float    uZoomLevel;       // display scale relative to fit (1.0 = 1:1)
    VkBool32 uGrayscale;       // `VK_TRUE` = grayscale
    VkBool32 uMirror;          // `VK_TRUE` = horizontally mirrored
};
static_assert(sizeof(UniformBufferObject) == 48, "UBO size mismatch with shader");

class VkImageViewerRenderer : public QVulkanWindowRenderer {
  public:
    VkImageViewerRenderer() = default;

    // QVulkanWindowRenderer lacks a window() accessor, so this is set by
    // VkImageViewer after construction.
    QVulkanWindow *m_vkWindow = nullptr;

    // Image state
    void setImage(const QImage& img, bool preserveView = false);
    void setZoom(float z) {
        m_zoom = z;
        m_uboDirty = true;
    }
    void setPan(const QPointF& p) {
        m_pan = p;
        m_uboDirty = true;
    }
    void setGrayscale(bool enabled) {
        m_grayscale = enabled;
        m_uboDirty = true;
    }
    void setMirror(bool enabled) {
        m_mirror = enabled;
        m_uboDirty = true;
    }
    bool grayscaleEnabled() const {
        return m_grayscale;
    }
    bool mirrorEnabled() const {
        return m_mirror;
    }
    float zoom() const {
        return m_zoom;
    }
    QPointF pan() const {
        return m_pan;
    }
    int imageWidth() const {
        return m_imageWidth;
    }
    int imageHeight() const {
        return m_imageHeight;
    }

    void setViewportSize(QSize s) {
        m_viewportSize = s;
        m_uboDirty = true;
    }

    void clear() {
        m_hasImage = false;
        m_sourceImage = QImage();
    }

  protected:
    void initResources() override;
    void initSwapChainResources() override;
    void releaseSwapChainResources() override;
    void releaseResources() override;
    void startNextFrame() override;

  private:
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props);
    void     createAndUploadTexture(VkCommandBuffer cmd, const QImage& image);
    void     recordMipChainGeneration(VkCommandBuffer cmd, int mipLevels, int width, int height);
    void     createViewAndUpdateDescriptors(int mipLevels);
    void     updateUniformBuffer();
    void
    updateDescriptors(VkDescriptorSet dstSet, VkBuffer ubo, VkSampler samp, VkImageView texView);

    // Resource creation helpers
    void createSamplers();
    void createShaderModules();
    void createDescriptorLayout();
    void createPipelineLayout();
    void createPipeline();
    void createDescriptorPoolAndSet();
    void createUniformBuffer();
    void createVertexBuffer();

    // Resource cleanup
    void cleanupOldTexture();

    // Image state
    bool    m_hasImage = false;
    float   m_zoom = 1.0f;
    QPointF m_pan;
    int     m_imageWidth = 0;
    int     m_imageHeight = 0;
    bool    m_grayscale = false;
    bool    m_mirror = false;
    bool    m_uploadPending = false; // Set when image needs upload to GPU
    QImage  m_sourceImage;

    // Qt Vulkan function wrappers
    QVulkanDeviceFunctions *m_devFuncs = nullptr;

    // Persistent (device-lifetime)
    VkShaderModule        m_vertModule = VK_NULL_HANDLE;
    VkShaderModule        m_fragModule = VK_NULL_HANDLE;
    VkPipeline            m_graphicsPipeline = VK_NULL_HANDLE;
    VkPipelineLayout      m_pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkSampler             m_samplerLinear = VK_NULL_HANDLE;
    VkSampler             m_samplerNearest = VK_NULL_HANDLE;
    VkBuffer              m_uniformBuffer = VK_NULL_HANDLE;
    VkDeviceMemory        m_uniformMemory = VK_NULL_HANDLE;
    void                 *m_uniformMapped = nullptr;
    VkDescriptorPool      m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet       m_descriptorSet = VK_NULL_HANDLE;

    // Texture
    VkImage        m_textureImage = VK_NULL_HANDLE;
    VkDeviceMemory m_textureMemory = VK_NULL_HANDLE;
    VkImageView    m_textureView = VK_NULL_HANDLE;

    // Staging buffer
    VkBuffer       m_stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_stagingMemory = VK_NULL_HANDLE;

    // Vertex buffer
    VkBuffer       m_vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_vertexMemory = VK_NULL_HANDLE;

    // Widget/container size
    QSize m_viewportSize;

    // UBO dirty flag — skip redundant writes when state hasn't changed
    bool m_uboDirty = false;

    // Last sampler bound to the descriptor set
    VkSampler m_activeSampler = VK_NULL_HANDLE;
};

class VkImageViewerWindow : public QVulkanWindow {
  public:
    explicit VkImageViewerWindow(QWindow *parent = nullptr) : QVulkanWindow(parent) {
        QSurfaceFormat format;
        format.setAlphaBufferSize(8);
        setFormat(format);
    }

    void setViewerRenderer(VkImageViewerRenderer *r) {
        m_viewerRenderer = r;
    }

  protected:
    QVulkanWindowRenderer *createRenderer() override {
        if (m_viewerRenderer) {
            m_viewerRenderer->m_vkWindow = this;
            qDebug() << "[VkImageViewer] createRenderer() called, m_vkWindow set";
        }
        return m_viewerRenderer;
    }

  private:
    VkImageViewerRenderer *m_viewerRenderer = nullptr;
};

uint32_t VkImageViewerRenderer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties memProps;
    m_vkWindow->vulkanInstance()->functions()->vkGetPhysicalDeviceMemoryProperties(
        m_vkWindow->physicalDevice(), &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props)
            return i;
    }
    qCritical() << "[VkImageViewer] findMemoryType: no suitable memory type found";
    return UINT32_MAX;
}

void VkImageViewerRenderer::initResources() {
    m_devFuncs = m_vkWindow->vulkanInstance()->deviceFunctions(m_vkWindow->device());

    createSamplers();
    createShaderModules();
    createDescriptorLayout();
    createPipelineLayout();
    createPipeline();
    createDescriptorPoolAndSet();
    createUniformBuffer();
    createVertexBuffer();

    // Update descriptor set now if a texture view already exists
    if (m_textureView != VK_NULL_HANDLE)
        updateDescriptors(m_descriptorSet, m_uniformBuffer, m_samplerLinear, m_textureView);

    // Mark UBO dirty so the first frame writes the current state into the
    // freshly mapped buffer.
    m_uboDirty = true;
}

void VkImageViewerRenderer::createSamplers() {
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
        return;
    }

    // Nearest-neighbor sampler (sharp pixels when zoomed in)
    VkSamplerCreateInfo siNearest = si;
    siNearest.magFilter = VK_FILTER_NEAREST;
    siNearest.minFilter = VK_FILTER_NEAREST;
    siNearest.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    result = df->vkCreateSampler(dev, &siNearest, nullptr, &m_samplerNearest);
    if (result != VK_SUCCESS) {
        qCritical() << "[VkImageViewer] vkCreateSampler (nearest) failed:" << result;
        df->vkDestroySampler(dev, m_samplerLinear, nullptr);
        m_samplerLinear = VK_NULL_HANDLE;
        return;
    }
}

void VkImageViewerRenderer::createShaderModules() {
    VkDevice                dev = m_vkWindow->device();
    QVulkanDeviceFunctions *df = m_devFuncs;

    auto loadShader = [](const char *path) -> std::vector<uint8_t> {
        QFile f(path);
        if (!f.open(QFile::ReadOnly)) {
            qCritical() << "[VkImageViewer] Failed to load shader resource:" << path;
            return {};
        }
        auto bytes = f.readAll();
        return std::vector<uint8_t>(bytes.constData(), bytes.constData() + bytes.size());
    };

    auto createModule = [df, dev](const std::vector<uint8_t>& code,
                                  const char                 *name) -> VkShaderModule {
        VkShaderModuleCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        ci.codeSize = code.size();
        ci.pCode = reinterpret_cast<const uint32_t *>(code.data());
        VkShaderModule m;
        VkResult       result = df->vkCreateShaderModule(dev, &ci, nullptr, &m);
        if (result != VK_SUCCESS)
            qCritical() << "[VkImageViewer] vkCreateShaderModule failed for" << name << ":"
                        << result;
        return m;
    };

    m_vertModule = createModule(loadShader(":/shaders/image_viewer.vert.spv"), "vertex");
    m_fragModule = createModule(loadShader(":/shaders/image_viewer.frag.spv"), "fragment");
}

void VkImageViewerRenderer::createDescriptorLayout() {
    VkDevice                dev = m_vkWindow->device();
    QVulkanDeviceFunctions *df = m_devFuncs;

    VkDescriptorSetLayoutBinding uboBinding{};
    uboBinding.binding = 0;
    uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBinding.descriptorCount = 1;
    uboBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding sampBinding{};
    sampBinding.binding = 1;
    sampBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sampBinding.descriptorCount = 1;
    sampBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding    bindings[] = {uboBinding, sampBinding};
    VkDescriptorSetLayoutCreateInfo dslci{};
    dslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dslci.bindingCount = 2;
    dslci.pBindings = bindings;
    df->vkCreateDescriptorSetLayout(dev, &dslci, nullptr, &m_descriptorSetLayout);
}

void VkImageViewerRenderer::createPipelineLayout() {
    VkDevice                dev = m_vkWindow->device();
    QVulkanDeviceFunctions *df = m_devFuncs;

    VkPipelineLayoutCreateInfo plci{};
    plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &m_descriptorSetLayout;
    df->vkCreatePipelineLayout(dev, &plci, nullptr, &m_pipelineLayout);
}

void VkImageViewerRenderer::createPipeline() {
    VkDevice                dev = m_vkWindow->device();
    QVulkanDeviceFunctions *df = m_devFuncs;

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = m_vertModule;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = m_fragModule;
    stages[1].pName = "main";

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = 4 * sizeof(float);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrs[2]{};
    attrs[0].location = 0;
    attrs[0].binding = 0;
    attrs[0].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[0].offset = 0;
    attrs[1].location = 1;
    attrs[1].binding = 0;
    attrs[1].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[1].offset = 2 * sizeof(float);

    VkPipelineVertexInputStateCreateInfo vii{};
    vii.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vii.vertexBindingDescriptionCount = 1;
    vii.pVertexBindingDescriptions = &binding;
    vii.vertexAttributeDescriptionCount = 2;
    vii.pVertexAttributeDescriptions = attrs;

    VkPipelineInputAssemblyStateCreateInfo iai{};
    iai.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    iai.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vsi{};
    vsi.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vsi.viewportCount = 1;
    vsi.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rsi{};
    rsi.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rsi.polygonMode = VK_POLYGON_MODE_FILL;
    rsi.lineWidth = 1.0f;
    rsi.cullMode = VK_CULL_MODE_NONE;
    rsi.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo msi{};
    msi.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msi.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState cbas{};
    cbas.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo cbsi{};
    cbsi.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cbsi.attachmentCount = 1;
    cbsi.pAttachments = &cbas;

    VkPipelineDepthStencilStateCreateInfo dssi{};
    dssi.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    dssi.depthTestEnable = VK_FALSE;
    dssi.depthWriteEnable = VK_FALSE;

    std::array<VkDynamicState, 2> dynStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dsci{};
    dsci.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dsci.dynamicStateCount = 2;
    dsci.pDynamicStates = dynStates.data();

    VkGraphicsPipelineCreateInfo gpi{};
    gpi.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gpi.stageCount = 2;
    gpi.pStages = stages;
    gpi.pVertexInputState = &vii;
    gpi.pInputAssemblyState = &iai;
    gpi.pViewportState = &vsi;
    gpi.pRasterizationState = &rsi;
    gpi.pMultisampleState = &msi;
    gpi.pColorBlendState = &cbsi;
    gpi.pDepthStencilState = &dssi;
    gpi.pDynamicState = &dsci;
    gpi.layout = m_pipelineLayout;
    gpi.renderPass = m_vkWindow->defaultRenderPass();
    gpi.subpass = 0;

    VkResult result =
        df->vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &gpi, nullptr, &m_graphicsPipeline);
    if (result != VK_SUCCESS)
        qCritical() << "[VkImageViewer] vkCreateGraphicsPipelines failed:" << result;

    // Clean up shader modules (no longer needed after pipeline creation)
    df->vkDestroyShaderModule(dev, m_vertModule, nullptr);
    df->vkDestroyShaderModule(dev, m_fragModule, nullptr);
}

void VkImageViewerRenderer::createDescriptorPoolAndSet() {
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
    dsa.pSetLayouts = &m_descriptorSetLayout;
    VkResult result = df->vkAllocateDescriptorSets(dev, &dsa, &m_descriptorSet);
    if (result != VK_SUCCESS)
        qCritical() << "[VkImageViewer] vkAllocateDescriptorSets failed:" << result;
}

void VkImageViewerRenderer::createUniformBuffer() {
    VkDevice                dev = m_vkWindow->device();
    QVulkanDeviceFunctions *df = m_devFuncs;

    VkDeviceSize bufSize = sizeof(UniformBufferObject);

    auto cleanupOnError = [this, df, dev]() {
        if (m_uniformMapped) {
            df->vkUnmapMemory(dev, m_uniformMemory);
            m_uniformMapped = nullptr;
        }
        if (m_uniformMemory) {
            df->vkFreeMemory(dev, m_uniformMemory, nullptr);
            m_uniformMemory = VK_NULL_HANDLE;
        }
        if (m_uniformBuffer) {
            df->vkDestroyBuffer(dev, m_uniformBuffer, nullptr);
            m_uniformBuffer = VK_NULL_HANDLE;
        }
    };

    VkBufferCreateInfo bci{};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = bufSize;
    bci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkResult result = df->vkCreateBuffer(dev, &bci, nullptr, &m_uniformBuffer);
    if (result != VK_SUCCESS) {
        qCritical() << "[VkImageViewer] vkCreateBuffer (UBO) failed:" << result;
        return;
    }

    VkMemoryRequirements memReq;
    df->vkGetBufferMemoryRequirements(dev, m_uniformBuffer, &memReq);

    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = memReq.size;
    mai.memoryTypeIndex =
        findMemoryType(memReq.memoryTypeBits,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    result = df->vkAllocateMemory(dev, &mai, nullptr, &m_uniformMemory);
    if (result != VK_SUCCESS) {
        qCritical() << "[VkImageViewer] vkAllocateMemory (UBO) failed:" << result;
        cleanupOnError();
        return;
    }
    result = df->vkBindBufferMemory(dev, m_uniformBuffer, m_uniformMemory, 0);
    if (result != VK_SUCCESS) {
        qCritical() << "[VkImageViewer] vkBindBufferMemory (UBO) failed:" << result;
        cleanupOnError();
        return;
    }
    result = df->vkMapMemory(dev, m_uniformMemory, 0, VK_WHOLE_SIZE, 0, &m_uniformMapped);
    if (result != VK_SUCCESS) {
        qCritical() << "[VkImageViewer] vkMapMemory (UBO) failed:" << result;
        cleanupOnError();
        return;
    }
}

void VkImageViewerRenderer::createVertexBuffer() {
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
        if (m_vertexMemory) {
            df->vkFreeMemory(dev, m_vertexMemory, nullptr);
            m_vertexMemory = VK_NULL_HANDLE;
        }
        if (m_vertexBuffer) {
            df->vkDestroyBuffer(dev, m_vertexBuffer, nullptr);
            m_vertexBuffer = VK_NULL_HANDLE;
        }
    };

    VkBufferCreateInfo vbci{};
    vbci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vbci.size = vbSize;
    vbci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    vbci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkResult result = df->vkCreateBuffer(dev, &vbci, nullptr, &m_vertexBuffer);
    if (result != VK_SUCCESS) {
        qCritical() << "[VkImageViewer] vkCreateBuffer (vertex) failed:" << result;
        return;
    }

    VkMemoryRequirements vbMemReq;
    df->vkGetBufferMemoryRequirements(dev, m_vertexBuffer, &vbMemReq);

    VkMemoryAllocateInfo vbMai{};
    vbMai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    vbMai.allocationSize = vbMemReq.size;
    vbMai.memoryTypeIndex =
        findMemoryType(vbMemReq.memoryTypeBits,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    result = df->vkAllocateMemory(dev, &vbMai, nullptr, &m_vertexMemory);
    if (result != VK_SUCCESS) {
        qCritical() << "[VkImageViewer] vkAllocateMemory (vertex) failed:" << result;
        cleanupOnError();
        return;
    }
    result = df->vkBindBufferMemory(dev, m_vertexBuffer, m_vertexMemory, 0);
    if (result != VK_SUCCESS) {
        qCritical() << "[VkImageViewer] vkBindBufferMemory (vertex) failed:" << result;
        cleanupOnError();
        return;
    }

    void *vbData;
    result = df->vkMapMemory(dev, m_vertexMemory, 0, vbSize, 0, &vbData);
    if (result != VK_SUCCESS) {
        qCritical() << "[VkImageViewer] vkMapMemory (vertex) failed:" << result;
        cleanupOnError();
        return;
    }
    std::memcpy(vbData, quadVertices, vbSize);
    df->vkUnmapMemory(dev, m_vertexMemory);
}

void VkImageViewerRenderer::initSwapChainResources() {
    if (m_textureView == VK_NULL_HANDLE && !m_sourceImage.isNull()) {
        m_uploadPending = true;
    }
}

void VkImageViewerRenderer::releaseSwapChainResources() {
    // Nothing to do here.
}

void VkImageViewerRenderer::cleanupOldTexture() {
    VkDevice dev = m_vkWindow->device();
    DESTROY_VK(m_devFuncs, dev, m_textureView, vkDestroyImageView);
    DESTROY_VK(m_devFuncs, dev, m_textureImage, vkDestroyImage);
    DESTROY_VK(m_devFuncs, dev, m_textureMemory, vkFreeMemory);
    DESTROY_VK(m_devFuncs, dev, m_stagingBuffer, vkDestroyBuffer);
    DESTROY_VK(m_devFuncs, dev, m_stagingMemory, vkFreeMemory);
}

void VkImageViewerRenderer::recordMipChainGeneration(VkCommandBuffer cmd,
                                                     int             mipLevels,
                                                     int             width,
                                                     int             height) {
    VkDevice                dev = m_vkWindow->device();
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
    vii.format = VK_FORMAT_R8G8B8A8_UNORM;
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
    updateDescriptors(m_descriptorSet, m_uniformBuffer, m_samplerLinear, m_textureView);
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
    DESTROY_VK(m_devFuncs, dev, m_uniformBuffer, vkDestroyBuffer);
    DESTROY_VK(m_devFuncs, dev, m_uniformMemory, vkFreeMemory);

    // Vertex buffer
    DESTROY_VK(m_devFuncs, dev, m_vertexBuffer, vkDestroyBuffer);
    DESTROY_VK(m_devFuncs, dev, m_vertexMemory, vkFreeMemory);

    // Descriptors
    DESTROY_VK(m_devFuncs, dev, m_descriptorPool, vkDestroyDescriptorPool);

    // Persistent resources
    DESTROY_VK(m_devFuncs, dev, m_samplerLinear, vkDestroySampler);
    DESTROY_VK(m_devFuncs, dev, m_samplerNearest, vkDestroySampler);
    DESTROY_VK(m_devFuncs, dev, m_graphicsPipeline, vkDestroyPipeline);
    DESTROY_VK(m_devFuncs, dev, m_pipelineLayout, vkDestroyPipelineLayout);
    DESTROY_VK(m_devFuncs, dev, m_descriptorSetLayout, vkDestroyDescriptorSetLayout);
}

void VkImageViewerRenderer::updateUniformBuffer() {
    if (!m_uniformMapped || !m_uboDirty)
        return;
    m_uboDirty = false;

    float vpW = static_cast<float>(m_viewportSize.width());
    float vpH = static_cast<float>(m_viewportSize.height());

    // Compute image-fit parameters
    float imgAspect = (m_imageWidth > 0 && m_imageHeight > 0)
                          ? static_cast<float>(m_imageWidth) / m_imageHeight
                          : 1.0f;
    float scaleX = vpW / (imgAspect * vpH);
    float fit = std::min(scaleX, 1.0f);
    float fitImgW = fit * imgAspect * vpH;
    float fitImgH = fit * vpH;
    float originX = (vpW - fitImgW) * 0.5f;
    float originY = (vpH - fitImgH) * 0.5f;
    float fitScale = (m_imageWidth > 0) ? fitImgW / m_imageWidth : 1.0f;

    auto *ubo = static_cast<UniformBufferObject *>(m_uniformMapped);
    ubo->uViewport[0] = vpW;
    ubo->uViewport[1] = vpH;
    ubo->uFitImgSize[0] = fitImgW;
    ubo->uFitImgSize[1] = fitImgH;
    ubo->uFitImgOrigin[0] = originX;
    ubo->uFitImgOrigin[1] = originY;
    ubo->uPanOffset[0] = m_pan.x();
    ubo->uPanOffset[1] = m_pan.y();
    ubo->uFitScale = fitScale;
    ubo->uZoomLevel = m_zoom;
    ubo->uGrayscale = m_grayscale;
    ubo->uMirror = m_mirror ? VK_TRUE : VK_FALSE;
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

void VkImageViewerRenderer::createAndUploadTexture(VkCommandBuffer cmd, const QImage& image) {
    VkDevice                dev = m_vkWindow->device();
    QVulkanDeviceFunctions *df = m_devFuncs;

    // Clean up the old texture. The previous frame has already completed by the
    // time this is called, so no extra synchronization is needed.
    cleanupOldTexture();

    // Compute full mip chain length
    int maxDim = std::max(image.width(), image.height());
    int mipLevels = 0;
    while ((1u << mipLevels) <= static_cast<unsigned>(maxDim)) {
        mipLevels++;
    }
    mipLevels = std::max(1, mipLevels);

    // On error, clean up any resources created so far.
    auto cleanupOnError = [this, df, dev]() {
        DESTROY_VK(df, dev, m_textureView, vkDestroyImageView);
        DESTROY_VK(df, dev, m_stagingBuffer, vkDestroyBuffer);
        DESTROY_VK(df, dev, m_stagingMemory, vkFreeMemory);
        DESTROY_VK(df, dev, m_textureImage, vkDestroyImage);
        DESTROY_VK(df, dev, m_textureMemory, vkFreeMemory);
    };

    VkImageCreateInfo ici{};
    ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format = VK_FORMAT_R8G8B8A8_UNORM;
    ici.extent = {static_cast<uint32_t>(image.width()), static_cast<uint32_t>(image.height()), 1};
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
        return;
    }

    // Allocate device-local memory
    VkMemoryRequirements memReq;
    df->vkGetImageMemoryRequirements(dev, m_textureImage, &memReq);

    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = memReq.size;
    mai.memoryTypeIndex =
        findMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    result = df->vkAllocateMemory(dev, &mai, nullptr, &m_textureMemory);
    if (result != VK_SUCCESS) {
        qCritical() << "[VkImageViewer] vkAllocateMemory (texture) failed:" << result;
        cleanupOnError();
        return;
    }
    result = df->vkBindImageMemory(dev, m_textureImage, m_textureMemory, 0);
    if (result != VK_SUCCESS) {
        qCritical() << "[VkImageViewer] vkBindImageMemory failed:" << result;
        cleanupOnError();
        return;
    }

    // Create staging buffer and upload image data
    VkDeviceSize imageSize = static_cast<VkDeviceSize>(image.width()) * image.height() * 4;

    VkBufferCreateInfo bci{};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = imageSize;
    bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    result = df->vkCreateBuffer(dev, &bci, nullptr, &m_stagingBuffer);
    if (result != VK_SUCCESS) {
        qCritical() << "[VkImageViewer] vkCreateBuffer (staging) failed:" << result;
        cleanupOnError();
        return;
    }

    VkMemoryRequirements bufMemReq;
    df->vkGetBufferMemoryRequirements(dev, m_stagingBuffer, &bufMemReq);

    VkMemoryAllocateInfo bufMai{};
    bufMai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    bufMai.allocationSize = bufMemReq.size;
    bufMai.memoryTypeIndex =
        findMemoryType(bufMemReq.memoryTypeBits,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    result = df->vkAllocateMemory(dev, &bufMai, nullptr, &m_stagingMemory);
    if (result != VK_SUCCESS) {
        qCritical() << "[VkImageViewer] vkAllocateMemory (staging) failed:" << result;
        cleanupOnError();
        return;
    }
    result = df->vkBindBufferMemory(dev, m_stagingBuffer, m_stagingMemory, 0);
    if (result != VK_SUCCESS) {
        qCritical() << "[VkImageViewer] vkBindBufferMemory failed:" << result;
        cleanupOnError();
        return;
    }

    // Copy image pixels into staging buffer
    void *data;
    result = df->vkMapMemory(dev, m_stagingMemory, 0, imageSize, 0, &data);
    if (result != VK_SUCCESS) {
        qCritical() << "[VkImageViewer] vkMapMemory failed:" << result;
        cleanupOnError();
        return;
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

    // Create image view
    createViewAndUpdateDescriptors(mipLevels);
}

void VkImageViewerRenderer::setImage(const QImage& img, bool preserveView) {
    if (img.isNull())
        return;

    if (!preserveView) {
        // Resetting view
        m_zoom = 1.0f;
        m_pan = QPointF(0, 0);
    }
    m_imageWidth = img.width();
    m_imageHeight = img.height();
    m_hasImage = true;

    // Convert to the premultiplied RGBA format the shader expects, skipping
    // the conversion if already correct.
    QImage rgba = img.format() == QImage::Format_RGBA8888_Premultiplied
                      ? QImage(img)
                      : img.convertToFormat(QImage::Format_RGBA8888_Premultiplied);

    // Store a persistent copy so the image can be re-uploaded after resources
    // are released.
    m_sourceImage = std::move(rgba);
    m_uploadPending = true;
    m_uboDirty = true;
}

void VkImageViewerRenderer::startNextFrame() {
    // Guard: resources may have been destroyed by releaseResources() if
    // this was a pending requestUpdate() that arrived after window destruction.
    if (m_graphicsPipeline == VK_NULL_HANDLE)
        return;

    VkCommandBuffer cmd = m_vkWindow->currentCommandBuffer();

    // Upload the image if pending
    if (m_uploadPending) {
        createAndUploadTexture(cmd, m_sourceImage);
        m_uploadPending = false;
    }

    // Skip rendering when there's no image to display, but still present
    // an empty frame so Qt's swapchain lifecycle stays in sync.
    if (!m_hasImage) {
        m_vkWindow->frameReady();
        m_vkWindow->requestUpdate();
        return;
    }

    // If the texture view is null despite having an image, the source data
    // was likely lost. Skip this frame.
    if (m_textureView == VK_NULL_HANDLE) {
        qWarning() << "[VkImageViewer] startNextFrame: texture view null despite "
                      "having image";
        m_vkWindow->frameReady();
        m_vkWindow->requestUpdate();
        return;
    }

    // Update uniform buffer
    updateUniformBuffer();

    // Viewport
    QSize sz = m_vkWindow->swapChainImageSize();

    // Switch sampler: nearest-neighbor when zoom >= 1:1
    VkSampler activeSampler = (m_zoom >= 1.0f) ? m_samplerNearest : m_samplerLinear;
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

    // Begin render pass
    VkClearValue cv[2]{};
    cv[0].color.float32[0] = 0.12f;
    cv[0].color.float32[1] = 0.12f;
    cv[0].color.float32[2] = 0.12f;
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

    // Bind pipeline & descriptors
    m_devFuncs->vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
    m_devFuncs->vkCmdBindDescriptorSets(
        cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);

    // Bind vertex buffer
    VkDeviceSize vbOffset = 0;
    m_devFuncs->vkCmdBindVertexBuffers(cmd, 0, 1, &m_vertexBuffer, &vbOffset);

    // Draw fullscreen quad
    m_devFuncs->vkCmdDraw(cmd, 6, 1, 0, 0);

    m_devFuncs->vkCmdEndRenderPass(cmd);

    // Present the frame and schedule the next one.
    m_vkWindow->frameReady();
    m_vkWindow->requestUpdate();
}

VkImageViewer::VkImageViewer(QWidget *parent) : QWidget(parent) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    {
        QFile qss(":/qss/vkimageviewer.qss");
        if (qss.open(QIODevice::ReadOnly | QIODevice::Text))
            setStyleSheet(QString::fromUtf8(qss.readAll()));
    }

    m_vulkanInstance = new QVulkanInstance();
    m_vulkanInstance->create();

    m_renderer = new VkImageViewerRenderer();
    auto *myVkWindow = new VkImageViewerWindow();
    myVkWindow->setViewerRenderer(m_renderer);
    myVkWindow->setVulkanInstance(m_vulkanInstance);
    m_vulkanWindow = myVkWindow;

    m_container = QWidget::createWindowContainer(m_vulkanWindow, this);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_container);

    // Intercept events on the embedded QVulkanWindow
    m_vulkanWindow->installEventFilter(this);
}

VkImageViewer::~VkImageViewer() {
    delete m_vulkanInstance;
}

void VkImageViewer::setNotificationCallback(IEffectsRenderer::EffectChangedCallback callback) {
    m_notificationCallback = callback;
}

bool VkImageViewer::eventFilter(QObject *obj, QEvent *event) {
    if (obj != m_vulkanWindow && obj != m_container)
        return QObject::eventFilter(obj, event);

    switch (event->type()) {
        // Offer Image Drop
        case QEvent::DragEnter: {
            if (obj == m_vulkanWindow) {
                auto *de = static_cast<QDragEnterEvent *>(event);
                if (de->mimeData()->hasUrls()) {
                    de->acceptProposedAction();
                    return true;
                }
            }
            break;
        }
        // Open Image on Drop
        case QEvent::Drop: {
            if (obj == m_vulkanWindow) {
                auto            *dp = static_cast<QDropEvent *>(event);
                const QMimeData *mimeData = dp->mimeData();
                if (mimeData && mimeData->hasUrls()) {
                    for (const QUrl& url : mimeData->urls()) {
                        QString path = url.toLocalFile();
                        if (!path.isEmpty()) {
                            emit imageOpenRequested(path);
                        }
                    }
                    dp->acceptProposedAction();
                    return true;
                }
            }
            break;
        }
        case QEvent::MouseButtonPress: {
            if (obj == m_vulkanWindow) {
                auto *me = static_cast<QMouseEvent *>(event);
                if (me->button() == Qt::LeftButton) {
                    m_isDragging = true;
                    m_lastMousePos = me->position().toPoint();
                    setFocus();
                    return true;
                } else if (me->button() == Qt::RightButton) {
                    //  Context Menu
                    QMenu menu(this);

                    auto *grayscaleAction = menu.addAction(tr("Grayscale"));
                    grayscaleAction->setCheckable(true);
                    grayscaleAction->setChecked(m_renderer->grayscaleEnabled());

                    auto *mirrorAction = menu.addAction(tr("Mirror"));
                    mirrorAction->setCheckable(true);
                    mirrorAction->setChecked(m_renderer->mirrorEnabled());

                    QAction *selectedAction = menu.exec(me->globalPosition().toPoint());

                    if (selectedAction == grayscaleAction) {
                        bool enabled = grayscaleAction->isChecked();
                        emit grayscaleToggled(enabled);
                        if (m_notificationCallback) {
                            m_notificationCallback(enabled, m_renderer->mirrorEnabled());
                        }
                        return true;
                    } else if (selectedAction == mirrorAction) {
                        bool enabled = mirrorAction->isChecked();
                        emit mirrorToggled(enabled);
                        if (m_notificationCallback) {
                            m_notificationCallback(m_renderer->grayscaleEnabled(), enabled);
                        }
                        return true;
                    }
                }
            }
            break;
        }
        case QEvent::MouseButtonRelease: {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton) {
                if (m_isDragging) {
                    m_isDragging = false;
                } else {
                    emit imageClicked();
                }
                return true;
            }
            break;
        }
        // Pan
        case QEvent::MouseMove: {
            auto *me = static_cast<QMouseEvent *>(event);
            if (!(me->buttons() & Qt::LeftButton)) {
                m_isDragging = false;
            }

            if (m_isDragging && m_hasImage) {
                QPoint delta = me->position().toPoint() - m_lastMousePos;
                emit   panRequested(delta.x(), delta.y());
            }
            m_lastMousePos = me->position().toPoint();
            return false;
        }
        // Zoom
        case QEvent::Wheel: {
            auto *we = static_cast<QWheelEvent *>(event);
            if (!m_hasImage)
                return QObject::eventFilter(obj, event);

            emit zoomRequested(we->angleDelta().y() >= 0, we->modifiers() & Qt::ControlModifier);
            return true;
        }
    }

    return QObject::eventFilter(obj, event);
}

void VkImageViewer::setViewState(const ViewState& state) {
    m_currentViewState = state;
    if (m_renderer) {
        m_renderer->setZoom(state.zoom());
        m_renderer->setPan(state.pan());
    }
    emit zoomChanged(state.percentage());
}

void VkImageViewer::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);

    QSize sz = m_container->size();
    if (!sz.isEmpty()) {
        m_renderer->setViewportSize(sz);
        emit viewportResized(sz.width(), sz.height());
    }

    m_vulkanWindow->requestUpdate();
}

void VkImageViewer::setImage(const QImage& image, bool preserveView) {
    m_hasImage = !image.isNull();
    if (!m_hasImage)
        return;

    // Initial state is now managed by the ViewController.
    // We only forward the image to the renderer.
    m_renderer->setImage(image, preserveView);

    QSize sz = m_container->size();
    if (!sz.isEmpty()) {
        m_renderer->setViewportSize(sz);
    }
}

void VkImageViewer::clear() {
    m_hasImage = false;
    m_renderer->clear();
}

void VkImageViewer::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    if (!m_hasImage)
        return;
    QSize sz = m_container->size();
    if (sz.isEmpty())
        return;
    m_renderer->setViewportSize(sz);
    emit viewportResized(sz.width(), sz.height());
}

void VkImageViewer::setGrayscale(bool enabled) {
    m_renderer->setGrayscale(enabled);
}

void VkImageViewer::setMirror(bool enabled) {
    m_renderer->setMirror(enabled);
}
