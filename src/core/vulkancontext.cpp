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
    m_utilityQueueFamilyIndex = foundFamily;
    m_utilityQueueFamilyIndex = foundFamily;

    // Create the utility logical device
    float                   queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = m_utilityQueueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;

    if (vf->vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr, &m_utilityDevice) !=
        VK_SUCCESS) {
        qCritical() << "[VulkanContext] Failed to create utility logical device";
        return false;
    }

    // Get the utility queue and device functions
    m_utilityDeviceFunctions = m_instance->deviceFunctions(m_utilityDevice);
    m_utilityDeviceFunctions->vkGetDeviceQueue(
        m_utilityDevice, m_utilityQueueFamilyIndex, 0, &m_utilityQueue);

    // Create a dedicated command pool for the utility device
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = m_utilityQueueFamilyIndex;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (m_utilityDeviceFunctions->vkCreateCommandPool(
            m_utilityDevice, &poolInfo, nullptr, &m_utilityCommandPool) != VK_SUCCESS) {
        qCritical() << "[VulkanContext] Failed to create utility command pool";
        return false;
    }

    // Prime internal resources (on the utility device)
    initializeInternalResources();

    m_utilityReconstructor = createReconstructor();

    qDebug() << "[VulkanContext] Utility device initialized successfully";
    return true;
}

void VulkanContext::initializeInternalResources() {
    initializeComputeResources(m_utilityDevice, m_utilityDeviceFunctions, m_utilityResources);
}

std::shared_ptr<VkSnapshotReconstructor> VulkanContext::createReconstructor() {
    VulkanHandles handles;
    handles.physicalDevice = getPhysicalDevice();
    handles.device = getUtilityDevice();
    handles.queue = getUtilityQueue();
    handles.queueFamilyIndex = getUtilityQueueFamilyIndex();
    handles.deviceFunctions = getUtilityDeviceFunctions();
    handles.commandPool = getUtilityCommandPool();

    return std::make_shared<VkSnapshotReconstructor>(handles);
}

void VulkanContext::initializeComputeResources(VkDevice                dev,
                                               QVulkanDeviceFunctions *df,
                                               ComputeResources&       res) {
    if (!dev || !df)
        return;

    // Descriptor Pool
    if (res.descriptorPool == VK_NULL_HANDLE) {
        VkDescriptorPoolSize poolSizes[2]{};
        poolSizes[0] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 20};
        poolSizes[1] = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10};

        VkDescriptorPoolCreateInfo dpci{};
        dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        dpci.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        dpci.maxSets = 30;
        dpci.poolSizeCount = 2;
        dpci.pPoolSizes = poolSizes;

        if (df->vkCreateDescriptorPool(dev, &dpci, nullptr, &res.descriptorPool) != VK_SUCCESS) {
            qCritical() << "[VulkanContext] Failed to create compute descriptor pool";
        }
    }

    // Compute Pipeline (Reconstruction)
    if (res.computePipeline == VK_NULL_HANDLE) {
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
                    dev, &dslci, nullptr, &res.computeDescriptorSetLayout);

                VkPipelineLayoutCreateInfo plci{};
                plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
                plci.setLayoutCount = 1;
                plci.pSetLayouts = &res.computeDescriptorSetLayout;
                df->vkCreatePipelineLayout(dev, &plci, nullptr, &res.computePipelineLayout);

                VkPipelineShaderStageCreateInfo stage{};
                stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
                stage.module = shaderModule;
                stage.pName = "main";

                VkComputePipelineCreateInfo cpci{};
                cpci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
                cpci.stage = stage;
                cpci.layout = res.computePipelineLayout;
                df->vkCreateComputePipelines(
                    dev, VK_NULL_HANDLE, 1, &cpci, nullptr, &res.computePipeline);

                df->vkDestroyShaderModule(dev, shaderModule, nullptr);
            }
        }
    }

    // Downsample Pipeline
    if (res.downsamplePipeline == VK_NULL_HANDLE) {
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

                VkDescriptorSetLayoutCreateInfo dslci{};
                dslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
                dslci.bindingCount = 2;
                dslci.pBindings = dsBindings;
                df->vkCreateDescriptorSetLayout(
                    dev, &dslci, nullptr, &res.downsampleDescriptorSetLayout);

                VkPipelineLayoutCreateInfo plci{};
                plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
                plci.setLayoutCount = 1;
                plci.pSetLayouts = &res.downsampleDescriptorSetLayout;
                df->vkCreatePipelineLayout(dev, &plci, nullptr, &res.downsamplePipelineLayout);

                VkPipelineShaderStageCreateInfo stage{};
                stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
                stage.module = dsShaderModule;
                stage.pName = "main";

                VkComputePipelineCreateInfo cpci{};
                cpci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
                cpci.stage = stage;
                cpci.layout = res.downsamplePipelineLayout;
                df->vkCreateComputePipelines(
                    dev, VK_NULL_HANDLE, 1, &cpci, nullptr, &res.downsamplePipeline);

                df->vkDestroyShaderModule(dev, dsShaderModule, nullptr);
            }
        }
    }
}

void VulkanContext::createGraphicsPipeline(VkDevice                dev,
                                           QVulkanDeviceFunctions *df,
                                           VkRenderPass            renderPass) {
    if (!dev || !df)
        return;
    if (m_graphicsPipeline != VK_NULL_HANDLE)
        return;

    auto df_ = df;
    auto dev_ = dev;

    VkShaderModule vertModule = VulkanUtils::createShaderModule(
        df_, dev_, VulkanUtils::loadShader(":/shaders/image_viewer.vert.spv"), "vertex");
    VkShaderModule fragModule = VulkanUtils::createShaderModule(
        df_, dev_, VulkanUtils::loadShader(":/shaders/image_viewer.frag.spv"), "fragment");

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

void VulkanContext::cleanupInstance() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (m_utilityReconstructor) {
        m_utilityReconstructor->cleanup();
        m_utilityReconstructor.reset();
    }

    // Cleanup Utility device resources
    if (m_utilityDevice && m_utilityDeviceFunctions) {
        auto df = m_utilityDeviceFunctions;
        auto dev = m_utilityDevice;

        VulkanUtils::destroyResource(df,
                                     dev,
                                     m_utilityResources.computePipeline,
                                     &QVulkanDeviceFunctions::vkDestroyPipeline);
        VulkanUtils::destroyResource(df,
                                     dev,
                                     m_utilityResources.computePipelineLayout,
                                     &QVulkanDeviceFunctions::vkDestroyPipelineLayout);
        VulkanUtils::destroyResource(df,
                                     dev,
                                     m_utilityResources.computeDescriptorSetLayout,
                                     &QVulkanDeviceFunctions::vkDestroyDescriptorSetLayout);
        VulkanUtils::destroyResource(df,
                                     dev,
                                     m_utilityResources.downsamplePipeline,
                                     &QVulkanDeviceFunctions::vkDestroyPipeline);
        VulkanUtils::destroyResource(df,
                                     dev,
                                     m_utilityResources.downsamplePipelineLayout,
                                     &QVulkanDeviceFunctions::vkDestroyPipelineLayout);
        VulkanUtils::destroyResource(df,
                                     dev,
                                     m_utilityResources.downsampleDescriptorSetLayout,
                                     &QVulkanDeviceFunctions::vkDestroyDescriptorSetLayout);
        VulkanUtils::destroyResource(df,
                                     dev,
                                     m_utilityResources.descriptorPool,
                                     &QVulkanDeviceFunctions::vkDestroyDescriptorPool);
        VulkanUtils::destroyResource(
            df, dev, m_utilityCommandPool, &QVulkanDeviceFunctions::vkDestroyCommandPool);
        df->vkDestroyDevice(dev, nullptr);
    }

    if (m_instance) {
        delete m_instance;
        m_instance = nullptr;
    }
}

VulkanHandles VulkanContext::getUIHandles() const {
    VulkanHandles handles;
    handles.physicalDevice = m_uiPhysicalDevice;
    handles.device = m_uiDevice;
    handles.queue = m_uiQueue;
    handles.queueFamilyIndex = m_uiQueueFamilyIndex;
    handles.deviceFunctions = m_uiDeviceFunctions;
    handles.commandPool = m_uiCommandPool;
    return handles;
}

void VulkanContext::setUIDevice(VkDevice         device,
                                VkPhysicalDevice physicalDevice,
                                QVulkanInstance *instance) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (m_uiDevice == device)
        return;

    // Cleanup previous UI resources
    if (m_uiDeviceFunctions && m_uiCommandPool != VK_NULL_HANDLE) {
        m_uiDeviceFunctions->vkDestroyCommandPool(m_uiDevice, m_uiCommandPool, nullptr);
    }

    m_uiDevice = device;
    m_uiPhysicalDevice = physicalDevice;
    m_uiDeviceFunctions = instance->deviceFunctions(device);

    initializeComputeResources(m_uiDevice, m_uiDeviceFunctions, m_uiResources);

    // Find a suitable queue family (graphics/compute)
    auto     vf = instance->functions();
    uint32_t queueFamilyCount = 0;
    vf->vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> properties(queueFamilyCount);
    vf->vkGetPhysicalDeviceQueueFamilyProperties(
        physicalDevice, &queueFamilyCount, properties.data());

    m_uiQueueFamilyIndex = 0;
    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        if (properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            m_uiQueueFamilyIndex = i;
            break;
        }
    }

    m_uiDeviceFunctions->vkGetDeviceQueue(m_uiDevice, m_uiQueueFamilyIndex, 0, &m_uiQueue);

    // Create a command pool for the UI device
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = m_uiQueueFamilyIndex;

    if (m_uiDeviceFunctions->vkCreateCommandPool(
            m_uiDevice, &poolInfo, nullptr, &m_uiCommandPool) != VK_SUCCESS) {
        qCritical() << "[VulkanContext] Failed to create UI command pool";
    }
}

VulkanContext::~VulkanContext() {
    cleanupInstance();
}
