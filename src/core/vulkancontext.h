#ifndef VULKANCONTEXT_H
#define VULKANCONTEXT_H

#include <QObject>
#include <QVulkanInstance>
#include <memory>
#include <mutex>
#include <vulkan/vulkan.h>
#include "core/vksnapshotreconstructor.h"

static constexpr uint64_t FENCE_TIMEOUT_NS = 5000000000; // 5 seconds

/// @brief Group of Vulkan compute resources used for snapshot reconstruction.
struct ComputeResources {
    VkPipeline            computePipeline = VK_NULL_HANDLE;
    VkPipelineLayout      computePipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout computeDescriptorSetLayout = VK_NULL_HANDLE;

    VkPipeline            downsamplePipeline = VK_NULL_HANDLE;
    VkPipelineLayout      downsamplePipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout downsampleDescriptorSetLayout = VK_NULL_HANDLE;

    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
};

/// @brief Manages the lifecycle of the Vulkan instance, devices, and shared compute resources.
/// @details This class acts as a central hub for Vulkan handles and pipelines, supporting
/// both a background utility device for reconstruction and a UI device for rendering.
class VulkanContext : public QObject {
    Q_OBJECT
  public:
    /// @brief Default timeout for Vulkan fence waiting operations.
    static constexpr uint64_t FENCE_TIMEOUT_NS = 5000000000; // 5 seconds

    /// @brief Returns the singleton instance of the VulkanContext.
    static VulkanContext& instance();

    /// @brief Initializes the Vulkan instance and selects the physical device.
    /// @return True if initialization succeeded, false otherwise.
    bool initializeInstance();

    /// @brief Retrieves the handles associated with the UI device.
    VulkanHandles getUIHandles() const;

    /// @brief Configures the UI device and physical device used for rendering.
    /// @param device The Vulkan device handle.
    /// @param physicalDevice The physical device handle.
    /// @param instance The QVulkanInstance pointer.
    void setUIDevice(VkDevice device, VkPhysicalDevice physicalDevice, QVulkanInstance *instance);

    /// @brief Returns the device handle for the background utility device.
    VkDevice getUtilityDevice() const {
        return m_utilityDevice;
    }

    /// @brief Returns the device functions for the background utility device.
    QVulkanDeviceFunctions *getUtilityDeviceFunctions() const {
        return m_utilityDeviceFunctions;
    }

    /// @brief Returns the queue handle for the background utility device.
    VkQueue getUtilityQueue() const {
        return m_utilityQueue;
    }

    /// @brief Returns the command pool for the background utility device.
    VkCommandPool getUtilityCommandPool() const {
        return m_utilityCommandPool;
    }

    /// @brief Returns the queue family index for the background utility device.
    uint32_t getUtilityQueueFamilyIndex() const {
        return m_utilityQueueFamilyIndex;
    }

    /// @brief Notifies listeners that the Vulkan device has changed.
    void notifyDeviceChanged() {
        emit deviceChanged();
    }

    /// @brief Initializes internal Vulkan resources such as command pools and queues.
    void initializeInternalResources();

    /// @brief Creates the graphics pipeline used by the image viewer.
    /// @param dev The device to use for pipeline creation.
    /// @param df The device functions.
    /// @param renderPass The render pass associated with the graphics pipeline.
    void createGraphicsPipeline(VkDevice dev, QVulkanDeviceFunctions *df, VkRenderPass renderPass);

    /// @brief Cleans up all Vulkan resources managed by this context.
    void cleanupInstance();

    /// @brief Returns the underlying QVulkanInstance.
    QVulkanInstance *getInstance() const {
        return m_instance;
    }

    /// @brief Returns the selected physical device.
    VkPhysicalDevice getPhysicalDevice() const {
        return m_physicalDevice;
    }

    /// @brief Returns the descriptor pool for the specified device.
    /// @param dev The device for which the pool is requested.
    /// @return The corresponding VkDescriptorPool handle.
    VkDescriptorPool getDescriptorPool(VkDevice dev) const {
        return (dev == m_uiDevice) ? m_uiResources.descriptorPool
                                   : m_utilityResources.descriptorPool;
    }

    /// @brief Creates a new snapshot reconstructor instance.
    std::shared_ptr<VkSnapshotReconstructor> createReconstructor();

    /// @brief Returns the shared reconstructor used for background utility tasks.
    std::shared_ptr<VkSnapshotReconstructor> getUtilityReconstructor() const {
        return m_utilityReconstructor;
    }

    /// @brief Returns the compute pipeline for the specified device.
    VkPipeline getComputePipeline(VkDevice dev) const {
        return (dev == m_uiDevice) ? m_uiResources.computePipeline
                                   : m_utilityResources.computePipeline;
    }

    /// @brief Returns the compute pipeline layout for the specified device.
    VkPipelineLayout getComputePipelineLayout(VkDevice dev) const {
        return (dev == m_uiDevice) ? m_uiResources.computePipelineLayout
                                   : m_utilityResources.computePipelineLayout;
    }

    /// @brief Returns the compute descriptor set layout for the specified device.
    VkDescriptorSetLayout getComputeDescriptorSetLayout(VkDevice dev) const {
        return (dev == m_uiDevice) ? m_uiResources.computeDescriptorSetLayout
                                   : m_utilityResources.computeDescriptorSetLayout;
    }

    /// @brief Returns the downsample compute pipeline for the specified device.
    VkPipeline getDownsamplePipeline(VkDevice dev) const {
        return (dev == m_uiDevice) ? m_uiResources.downsamplePipeline
                                   : m_utilityResources.downsamplePipeline;
    }

    /// @brief Returns the downsample compute pipeline layout for the specified device.
    VkPipelineLayout getDownsamplePipelineLayout(VkDevice dev) const {
        return (dev == m_uiDevice) ? m_uiResources.downsamplePipelineLayout
                                   : m_utilityResources.downsamplePipelineLayout;
    }

    /// @brief Returns the downsample descriptor set layout for the specified device.
    VkDescriptorSetLayout getDownsampleDescriptorSetLayout(VkDevice dev) const {
        return (dev == m_uiDevice) ? m_uiResources.downsampleDescriptorSetLayout
                                   : m_utilityResources.downsampleDescriptorSetLayout;
    }

    /// @brief Returns the active graphics pipeline handle.
    VkPipeline getGraphicsPipeline() const {
        return m_graphicsPipeline;
    }

    /// @brief Returns the active graphics pipeline layout handle.
    VkPipelineLayout getGraphicsPipelineLayout() const {
        return m_graphicsPipelineLayout;
    }

    /// @brief Returns the active graphics descriptor set layout handle.
    VkDescriptorSetLayout getGraphicsDescriptorSetLayout() const {
        return m_graphicsDescriptorSetLayout;
    }

    /// @brief Initializes the compute resources for a specific device.
    /// @param dev The device to initialize.
    /// @param df The device functions.
    /// @param res The resource struct to populate.
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
    bool                                     m_ownsnDevice = false;
};

#endif // VULKANCONTEXT_H
