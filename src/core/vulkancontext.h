#ifndef VULKANCONTEXT_H
#define VULKANCONTEXT_H

#include <QObject>
#include <QVulkanInstance>
#include <memory>
#include <mutex>
#include <vulkan/vulkan.h>
#include "core/vksnapshotreconstructor.h"

static constexpr uint64_t FENCE_TIMEOUT_NS = 5000000000; // 5 seconds

/// @brief VulkanContext manages the lifecycle of the Vulkan instance and device.
///
/// It provides a centralized point of access for core Vulkan handles, enabling
/// components like VkSnapshotReconstructor to operate independently of the UI.
struct ComputeResources {
    VkPipeline            computePipeline = VK_NULL_HANDLE;
    VkPipelineLayout      computePipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout computeDescriptorSetLayout = VK_NULL_HANDLE;

    VkPipeline            downsamplePipeline = VK_NULL_HANDLE;
    VkPipelineLayout      downsamplePipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout downsampleDescriptorSetLayout = VK_NULL_HANDLE;

    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
};

class VulkanContext : public QObject {
    Q_OBJECT
  public:
    static constexpr uint64_t FENCE_TIMEOUT_NS = 5000000000; // 5 seconds

    static VulkanContext& instance();

    bool          initializeInstance();
    VulkanHandles getUIHandles() const;
    void setUIDevice(VkDevice device, VkPhysicalDevice physicalDevice, QVulkanInstance *instance);

    VkDevice getUtilityDevice() const {
        return m_utilityDevice;
    }
    QVulkanDeviceFunctions *getUtilityDeviceFunctions() const {
        return m_utilityDeviceFunctions;
    }
    VkQueue getUtilityQueue() const {
        return m_utilityQueue;
    }
    VkCommandPool getUtilityCommandPool() const {
        return m_utilityCommandPool;
    }
    uint32_t getUtilityQueueFamilyIndex() const {
        return m_utilityQueueFamilyIndex;
    }

    void notifyDeviceChanged() {
        emit deviceChanged();
    }
    void initializeInternalResources();
    void createGraphicsPipeline(VkDevice dev, QVulkanDeviceFunctions *df, VkRenderPass renderPass);
    void cleanupInstance();

    QVulkanInstance *getInstance() const {
        return m_instance;
    }
    VkPhysicalDevice getPhysicalDevice() const {
        return m_physicalDevice;
    }

    VkDescriptorPool getDescriptorPool(VkDevice dev) const {
        return (dev == m_uiDevice) ? m_uiResources.descriptorPool
                                   : m_utilityResources.descriptorPool;
    }

    std::shared_ptr<VkSnapshotReconstructor> createReconstructor();
    std::shared_ptr<VkSnapshotReconstructor> getUtilityReconstructor() const {
        return m_utilityReconstructor;
    }

    VkPipeline getComputePipeline(VkDevice dev) const {
        return (dev == m_uiDevice) ? m_uiResources.computePipeline
                                   : m_utilityResources.computePipeline;
    }

    VkPipelineLayout getComputePipelineLayout(VkDevice dev) const {
        return (dev == m_uiDevice) ? m_uiResources.computePipelineLayout
                                   : m_utilityResources.computePipelineLayout;
    }

    VkDescriptorSetLayout getComputeDescriptorSetLayout(VkDevice dev) const {
        return (dev == m_uiDevice) ? m_uiResources.computeDescriptorSetLayout
                                   : m_utilityResources.computeDescriptorSetLayout;
    }

    VkPipeline getDownsamplePipeline(VkDevice dev) const {
        return (dev == m_uiDevice) ? m_uiResources.downsamplePipeline
                                   : m_utilityResources.downsamplePipeline;
    }

    VkPipelineLayout getDownsamplePipelineLayout(VkDevice dev) const {
        return (dev == m_uiDevice) ? m_uiResources.downsamplePipelineLayout
                                   : m_utilityResources.downsamplePipelineLayout;
    }

    VkDescriptorSetLayout getDownsampleDescriptorSetLayout(VkDevice dev) const {
        return (dev == m_uiDevice) ? m_uiResources.downsampleDescriptorSetLayout
                                   : m_utilityResources.downsampleDescriptorSetLayout;
    }

    VkPipeline getGraphicsPipeline() const {
        return m_graphicsPipeline;
    }
    VkPipelineLayout getGraphicsPipelineLayout() const {
        return m_graphicsPipelineLayout;
    }
    VkDescriptorSetLayout getGraphicsDescriptorSetLayout() const {
        return m_graphicsDescriptorSetLayout;
    }

    void
    initializeComputeResources(VkDevice dev, QVulkanDeviceFunctions *df, ComputeResources& res);

  signals:
    void deviceInitialized();
    void deviceChanged();

  private:
    VulkanContext() = default;
    ~VulkanContext();
    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    mutable std::recursive_mutex m_mutex;
    QVulkanInstance             *m_instance = nullptr;
    VkPhysicalDevice             m_physicalDevice = VK_NULL_HANDLE;

    // Utility Device
    VkDevice                m_utilityDevice = VK_NULL_HANDLE;
    VkQueue                 m_utilityQueue = VK_NULL_HANDLE;
    uint32_t                m_utilityQueueFamilyIndex = 0;
    VkCommandPool           m_utilityCommandPool = VK_NULL_HANDLE;
    QVulkanDeviceFunctions *m_utilityDeviceFunctions = nullptr;

    // UI Device
    VkPhysicalDevice        m_uiPhysicalDevice = VK_NULL_HANDLE;
    VkDevice                m_uiDevice = VK_NULL_HANDLE;
    VkQueue                 m_uiQueue = VK_NULL_HANDLE;
    uint32_t                m_uiQueueFamilyIndex = 0;
    VkCommandPool           m_uiCommandPool = VK_NULL_HANDLE;
    QVulkanDeviceFunctions *m_uiDeviceFunctions = nullptr;

    // Compute Resources
    ComputeResources m_utilityResources;
    ComputeResources m_uiResources;

    // Shared Graphics Pipeline
    VkPipeline            m_graphicsPipeline = VK_NULL_HANDLE;
    VkPipelineLayout      m_graphicsPipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_graphicsDescriptorSetLayout = VK_NULL_HANDLE;

    std::shared_ptr<VkSnapshotReconstructor> m_utilityReconstructor;
    bool                                     m_ownsDevice = false;
};

#endif // VULKANCONTEXT_H
