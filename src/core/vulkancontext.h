#ifndef VULKANCONTEXT_H
#define VULKANCONTEXT_H

#include <QObject>
#include <QVulkanInstance>
#include <memory>
#include <vulkan/vulkan.h>
#include "core/vksnapshotreconstructor.h"

/// @brief VulkanContext manages the lifecycle of the Vulkan instance and device.
///
/// It provides a centralized point of access for core Vulkan handles, enabling
/// components like VkSnapshotReconstructor to operate independently of the UI.
class VulkanContext : public QObject {
    Q_OBJECT
  public:
    static VulkanContext& instance();

    /// @brief Initializes the Vulkan instance.
    bool initializeInstance();

    /// @brief Sets the device handles, typically called by a QVulkanWindow.
    void setDevice(VkPhysicalDevice        physicalDevice,
                   VkDevice                device,
                   VkQueue                 queue,
                   uint32_t                queueFamilyIndex,
                   QVulkanDeviceFunctions *deviceFunctions);

    /// @brief Notifies listeners that the Vulkan device has changed.
    void notifyDeviceChanged() {
        emit deviceChanged();
    }

    /// @brief Initializes internal resources (e.g., global command pool) after device is set.
    void initializeInternalResources();

    /// @brief Creates the shared graphics pipeline. Must be called once a RenderPass is available.
    void createGraphicsPipeline(VkRenderPass renderPass);

    /// @brief Cleans up GPU resources (buffers, pipelines) that depend on the device.
    /// This should be called before the VkDevice is destroyed.
    void cleanupResources(VkDevice dev = VK_NULL_HANDLE, QVulkanDeviceFunctions *df = nullptr);

    /// @brief Cleans up the Vulkan instance.
    void cleanupInstance();

    QVulkanInstance *getInstance() const {
        return m_instance;
    }
    VkPhysicalDevice getPhysicalDevice() const {
        return m_physicalDevice;
    }
    VkDevice getDevice() const {
        return m_device;
    }
    VkQueue getQueue() const {
        return m_queue;
    }
    uint32_t getQueueFamilyIndex() const {
        return m_queueFamilyIndex;
    }
    VkCommandPool getCommandPool() const {
        return m_commandPool;
    }

    VkDescriptorPool getDescriptorPool() const {
        return m_descriptorPool;
    }
    QVulkanDeviceFunctions *getDeviceFunctions() const {
        return m_deviceFunctions;
    }

    /// @brief Returns the shared reconstructor used for background tasks (e.g., thumbnails,
    /// exports).
    VkSnapshotReconstructor *getUtilityReconstructor() const {
        return m_utilityReconstructor.get();
    }

    VkPipeline getComputePipeline() const {
        return m_computePipeline;
    }

    VkPipelineLayout getComputePipelineLayout() const {
        return m_computePipelineLayout;
    }

    VkDescriptorSetLayout getComputeDescriptorSetLayout() const {
        return m_computeDescriptorSetLayout;
    }

    VkPipeline getDownsamplePipeline() const {
        return m_downsamplePipeline;
    }

    VkPipelineLayout getDownsamplePipelineLayout() const {
        return m_downsamplePipelineLayout;
    }

    VkDescriptorSetLayout getDownsampleDescriptorSetLayout() const {
        return m_downsampleDescriptorSetLayout;
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

    /// @brief Clears the device handles. Called when the Vulkan device is destroyed.
    void clearDevice() {
        m_device = VK_NULL_HANDLE;
        m_deviceFunctions = nullptr;
    }

  signals:
    void deviceInitialized();
    void deviceChanged();

  private:
    VulkanContext() = default;
    ~VulkanContext();
    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    QVulkanInstance        *m_instance = nullptr;
    VkPhysicalDevice        m_physicalDevice = VK_NULL_HANDLE;
    VkDevice                m_device = VK_NULL_HANDLE;
    VkQueue                 m_queue = VK_NULL_HANDLE;
    uint32_t                m_queueFamilyIndex = 0;
    VkCommandPool           m_commandPool = VK_NULL_HANDLE;
    QVulkanDeviceFunctions *m_deviceFunctions = nullptr;

    // Shared Compute Pipeline
    VkPipeline            m_computePipeline = VK_NULL_HANDLE;
    VkPipelineLayout      m_computePipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_computeDescriptorSetLayout = VK_NULL_HANDLE;

    // Shared Downsample Pipeline
    VkPipeline            m_downsamplePipeline = VK_NULL_HANDLE;
    VkPipelineLayout      m_downsamplePipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_downsampleDescriptorSetLayout = VK_NULL_HANDLE;

    // Shared Graphics Pipeline
    VkPipeline            m_graphicsPipeline = VK_NULL_HANDLE;
    VkPipelineLayout      m_graphicsPipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_graphicsDescriptorSetLayout = VK_NULL_HANDLE;

    // Shared Descriptor Pool
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;

    std::unique_ptr<VkSnapshotReconstructor> m_utilityReconstructor;

    bool m_ownsDevice = false;
};

#endif // VULKANCONTEXT_H
