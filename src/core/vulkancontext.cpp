#include <QDebug>
#include <QVulkanInstance>
#include <array>
#include "core/vulkancontext.h"
#include "core/vulkanutils.h"

VulkanContext& VulkanContext::instance() {
    static VulkanContext inst;
    return inst;
}

bool VulkanContext::initializeInstance() {
    if (m_instance)
        return true;

    m_instance = new QVulkanInstance();
    if (!m_instance->create()) {
        qCritical() << "[VulkanContext] Failed to create QVulkanInstance";
        delete m_instance;
        m_instance = nullptr;
        return false;
    }

    // Headless Device Initialization
    auto vf = m_instance->functions();

    // Pick a physical device
    uint32_t deviceCount = 0;
    vf->vkEnumeratePhysicalDevices(m_instance->vkInstance(), &deviceCount, nullptr);
    if (deviceCount == 0) {
        qCritical() << "[VulkanContext] No Vulkan physical devices found";
        return false;
    }

    std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
    vf->vkEnumeratePhysicalDevices(m_instance->vkInstance(), &deviceCount, physicalDevices.data());

    // Pick the first available device.
    m_physicalDevice = physicalDevices[0];

    // Find a queue family that supports compute/graphics
    uint32_t queueFamilyCount = 0;
    vf->vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vf->vkGetPhysicalDeviceQueueFamilyProperties(
        m_physicalDevice, &queueFamilyCount, queueFamilies.data());

    int foundFamily = -1;
    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT ||
            queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            foundFamily = i;
            break;
        }
    }

    if (foundFamily == -1) {
        qCritical() << "[VulkanContext] No suitable queue family found";
        return false;
    }
    m_queueFamilyIndex = foundFamily;

    // Create the logical device
    float                   queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = m_queueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;

    if (vf->vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr, &m_device) != VK_SUCCESS) {
        qCritical() << "[VulkanContext] Failed to create logical device";
        return false;
    }
    m_ownsDevice = true;

    // Get the queue and device functions
    m_instance->deviceFunctions(m_device)->vkGetDeviceQueue(
        m_device, m_queueFamilyIndex, 0, &m_queue);
    m_deviceFunctions = m_instance->deviceFunctions(m_device);

    // Prime internal resources and the reconstructor
    initializeInternalResources();

    qDebug() << "[VulkanContext] Headless device initialized successfully";
    return true;
}

void VulkanContext::setDevice(VkPhysicalDevice        physicalDevice,
                              VkDevice                device,
                              VkQueue                 queue,
                              uint32_t                queueFamilyIndex,
                              QVulkanDeviceFunctions *deviceFunctions) {
    if (m_device != VK_NULL_HANDLE) {
        VkDevice                oldDevice = m_device;
        QVulkanDeviceFunctions *oldDF = m_deviceFunctions;

        // 1. Update handles first so that listeners in notifyDeviceChanged()
        //    create resources using the NEW device.
        m_physicalDevice = physicalDevice;
        m_device = device;
        m_queue = queue;
        m_queueFamilyIndex = queueFamilyIndex;
        m_deviceFunctions = deviceFunctions;

        // 2. Clean up old resources using old handles.
        cleanupResources(oldDevice, oldDF);

        // 3. Finally, destroy the old device.
        if (m_ownsDevice) {
            oldDF->vkDestroyDevice(oldDevice, nullptr);
            m_ownsDevice = false;
        }
    } else {
        m_physicalDevice = physicalDevice;
        m_device = device;
        m_queue = queue;
        m_queueFamilyIndex = queueFamilyIndex;
        m_deviceFunctions = deviceFunctions;
    }

    // 4. Initialize internal resources (e.g., command pool) before notifying listeners.
    initializeInternalResources();

    // 5. Notify all listeners to recreate their resources using the now-initialized context.
    // This must happen after initializeInternalResources() because reconstructors need the command
    // pool.
    notifyDeviceChanged();
    emit deviceInitialized();
}

void VulkanContext::initializeInternalResources() {
    if (!m_device || !m_deviceFunctions) {
        qWarning() << "[VulkanContext] Cannot initialize internal resources: device not set";
        return;
    }

    auto df = m_deviceFunctions;
    auto dev = m_device;

    // Global Command Pool
    if (m_commandPool == VK_NULL_HANDLE) {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = m_queueFamilyIndex;

        if (df->vkCreateCommandPool(dev, &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
            qCritical() << "[VulkanContext] Failed to create global command pool";
        }
    }

    // Shared Descriptor Pool
    if (m_descriptorPool == VK_NULL_HANDLE) {
        VkDescriptorPoolSize poolSizes[2]{};
        poolSizes[0] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 20};
        poolSizes[1] = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10};

        VkDescriptorPoolCreateInfo dpci{};
        dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        dpci.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        dpci.maxSets = 30;
        dpci.poolSizeCount = 2;
        dpci.pPoolSizes = poolSizes;

        if (df->vkCreateDescriptorPool(dev, &dpci, nullptr, &m_descriptorPool) != VK_SUCCESS) {
            qCritical() << "[VulkanContext] Failed to create shared descriptor pool";
        }
    }

    // Compute Pipeline (Reconstruction)
    if (m_computePipeline == VK_NULL_HANDLE) {
        auto code = VulkanUtils::loadShader(":/shaders/snapshot_reconstruct.comp.spv");
        if (!code.empty()) {
            VkShaderModule shaderModule =
                VulkanUtils::createShaderModule(df, dev, code, "snapshot_reconstruct");
            if (shaderModule != VK_NULL_HANDLE) {
                VkDescriptorSetLayoutBinding bindings[3] = {};
                for (int i = 0; i < 3; ++i) {
                    bindings[i].binding = i;
                    bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                    bindings[i].descriptorCount = 1;
                    bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
                }

                VkDescriptorSetLayoutCreateInfo dslci{};
                dslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
                dslci.bindingCount = 3;
                dslci.pBindings = bindings;
                df->vkCreateDescriptorSetLayout(
                    dev, &dslci, nullptr, &m_computeDescriptorSetLayout);

                VkPipelineLayoutCreateInfo plci{};
                plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
                plci.setLayoutCount = 1;
                plci.pSetLayouts = &m_computeDescriptorSetLayout;
                df->vkCreatePipelineLayout(dev, &plci, nullptr, &m_computePipelineLayout);

                VkPipelineShaderStageCreateInfo stage{};
                stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
                stage.module = shaderModule;
                stage.pName = "main";

                VkComputePipelineCreateInfo cpci{};
                cpci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
                cpci.stage = stage;
                cpci.layout = m_computePipelineLayout;
                df->vkCreateComputePipelines(
                    dev, VK_NULL_HANDLE, 1, &cpci, nullptr, &m_computePipeline);

                df->vkDestroyShaderModule(dev, shaderModule, nullptr);
            }
        }
    }

    // Downsample Pipeline
    if (m_downsamplePipeline == VK_NULL_HANDLE) {
        auto downsampleCode = VulkanUtils::loadShader(":/shaders/snapshot_downsample.comp.spv");
        if (!downsampleCode.empty()) {
            VkShaderModule dsShaderModule =
                VulkanUtils::createShaderModule(df, dev, downsampleCode, "snapshot_downsample");
            if (dsShaderModule != VK_NULL_HANDLE) {
                VkDescriptorSetLayoutBinding dsBindings[2] = {};
                dsBindings[0].binding = 0;
                dsBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                dsBindings[0].descriptorCount = 1;
                dsBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
                dsBindings[1].binding = 1;
                dsBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                dsBindings[1].descriptorCount = 1;
                dsBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

                VkDescriptorSetLayoutCreateInfo dslciDs{};
                dslciDs.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
                dslciDs.bindingCount = 2;
                dslciDs.pBindings = dsBindings;
                df->vkCreateDescriptorSetLayout(
                    dev, &dslciDs, nullptr, &m_downsampleDescriptorSetLayout);

                VkPipelineLayoutCreateInfo plciDs{};
                plciDs.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
                plciDs.setLayoutCount = 1;
                plciDs.pSetLayouts = &m_downsampleDescriptorSetLayout;
                df->vkCreatePipelineLayout(dev, &plciDs, nullptr, &m_downsamplePipelineLayout);

                VkPipelineShaderStageCreateInfo stageDs{};
                stageDs.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                stageDs.stage = VK_SHADER_STAGE_COMPUTE_BIT;
                stageDs.module = dsShaderModule;
                stageDs.pName = "main";

                VkComputePipelineCreateInfo cpciDs{};
                cpciDs.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
                cpciDs.stage = stageDs;
                cpciDs.layout = m_downsamplePipelineLayout;
                df->vkCreateComputePipelines(
                    dev, VK_NULL_HANDLE, 1, &cpciDs, nullptr, &m_downsamplePipeline);

                df->vkDestroyShaderModule(dev, dsShaderModule, nullptr);
            }
        }
    }

    VulkanHandles handles;
    handles.physicalDevice = m_physicalDevice;
    handles.device = m_device;
    handles.queue = m_queue;
    handles.queueFamilyIndex = m_queueFamilyIndex;
    handles.deviceFunctions = m_deviceFunctions;
    handles.commandPool = m_commandPool;

    m_utilityReconstructor = std::make_unique<VkSnapshotReconstructor>(handles);
}

void VulkanContext::createGraphicsPipeline(VkRenderPass renderPass) {
    if (!m_device || !m_deviceFunctions)
        return;
    if (m_graphicsPipeline != VK_NULL_HANDLE)
        return;

    auto df = m_deviceFunctions;
    auto dev = m_device;

    VkShaderModule vertModule = VulkanUtils::createShaderModule(
        df, dev, VulkanUtils::loadShader(":/shaders/image_viewer.vert.spv"), "vertex");
    VkShaderModule fragModule = VulkanUtils::createShaderModule(
        df, dev, VulkanUtils::loadShader(":/shaders/image_viewer.frag.spv"), "fragment");

    VkDescriptorSetLayoutBinding bindings[2] = {};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo dslci{};
    dslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dslci.bindingCount = 2;
    dslci.pBindings = bindings;
    df->vkCreateDescriptorSetLayout(dev, &dslci, nullptr, &m_graphicsDescriptorSetLayout);

    VkPipelineLayoutCreateInfo plci{};
    plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &m_graphicsDescriptorSetLayout;
    df->vkCreatePipelineLayout(dev, &plci, nullptr, &m_graphicsPipelineLayout);

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertModule;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragModule;
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
    gpi.layout = m_graphicsPipelineLayout;
    gpi.renderPass = renderPass;
    gpi.subpass = 0;

    df->vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &gpi, nullptr, &m_graphicsPipeline);

    df->vkDestroyShaderModule(dev, vertModule, nullptr);
    df->vkDestroyShaderModule(dev, fragModule, nullptr);
}
void VulkanContext::cleanupResources(VkDevice dev, QVulkanDeviceFunctions *df) {
    if (!dev || !df)
        return;

    auto df_ = df;
    auto dev_ = dev;

    if (m_utilityReconstructor) {
        m_utilityReconstructor->cleanup();
        m_utilityReconstructor.reset();
    }

    VulkanUtils::destroyResource(
        df_, dev_, m_computePipeline, &QVulkanDeviceFunctions::vkDestroyPipeline);
    VulkanUtils::destroyResource(
        df_, dev_, m_computePipelineLayout, &QVulkanDeviceFunctions::vkDestroyPipelineLayout);
    VulkanUtils::destroyResource(df_,
                                 dev_,
                                 m_computeDescriptorSetLayout,
                                 &QVulkanDeviceFunctions::vkDestroyDescriptorSetLayout);

    VulkanUtils::destroyResource(
        df_, dev_, m_downsamplePipeline, &QVulkanDeviceFunctions::vkDestroyPipeline);
    VulkanUtils::destroyResource(
        df_, dev_, m_downsamplePipelineLayout, &QVulkanDeviceFunctions::vkDestroyPipelineLayout);
    VulkanUtils::destroyResource(df_,
                                 dev_,
                                 m_downsampleDescriptorSetLayout,
                                 &QVulkanDeviceFunctions::vkDestroyDescriptorSetLayout);

    VulkanUtils::destroyResource(
        df_, dev_, m_graphicsPipeline, &QVulkanDeviceFunctions::vkDestroyPipeline);
    VulkanUtils::destroyResource(
        df_, dev_, m_graphicsPipelineLayout, &QVulkanDeviceFunctions::vkDestroyPipelineLayout);
    VulkanUtils::destroyResource(df_,
                                 dev_,
                                 m_graphicsDescriptorSetLayout,
                                 &QVulkanDeviceFunctions::vkDestroyDescriptorSetLayout);

    VulkanUtils::destroyResource(
        df_, dev_, m_descriptorPool, &QVulkanDeviceFunctions::vkDestroyDescriptorPool);

    VulkanUtils::destroyResource(
        df_, dev_, m_commandPool, &QVulkanDeviceFunctions::vkDestroyCommandPool);

    m_computePipeline = VK_NULL_HANDLE;
    m_computePipelineLayout = VK_NULL_HANDLE;
    m_computeDescriptorSetLayout = VK_NULL_HANDLE;
    m_downsamplePipeline = VK_NULL_HANDLE;
    m_downsamplePipelineLayout = VK_NULL_HANDLE;
    m_downsampleDescriptorSetLayout = VK_NULL_HANDLE;
    m_graphicsPipeline = VK_NULL_HANDLE;
    m_graphicsPipelineLayout = VK_NULL_HANDLE;
    m_graphicsDescriptorSetLayout = VK_NULL_HANDLE;
    m_descriptorPool = VK_NULL_HANDLE;
    m_commandPool = VK_NULL_HANDLE;
}

void VulkanContext::cleanupInstance() {
    if (m_instance) {
        delete m_instance;
        m_instance = nullptr;
    }
}

VulkanContext::~VulkanContext() {
    cleanupResources();
    if (m_ownsDevice && m_device != VK_NULL_HANDLE) {
        if (m_deviceFunctions) {
            m_deviceFunctions->vkDestroyDevice(m_device, nullptr);
        }
    }
    cleanupInstance();
}
