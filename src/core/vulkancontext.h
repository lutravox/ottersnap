#ifndef VULKANCONTEXT_H
#define VULKANCONTEXT_H

#include <QObject>
#include <QVulkanInstance>
#include <memory>
#include <mutex>
#include <vulkan/vulkan.h>
#include "core/vulkan_types.h"

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
/// @details This class acts as a central hub for Vulkan handles and pipelines for the
/// UI device used for rendering and GPU reconstruction.
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

    /// @brief Notifies listeners that the Vulkan device has changed.
    void notifyDeviceChanged() {
        emit deviceChanged();
    }

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

    /// @brief Returns the descriptor pool for the UI device.
    VkDescriptorPool getDescriptorPool() const {
        return m_uiResources.descriptorPool;
    }

    /// @brief Returns the compute pipeline for the UI device.
    VkPipeline getComputePipeline() const {
        return m_uiResources.computePipeline;
    }

    /// @brief Returns the compute pipeline layout for the UI device.
    VkPipelineLayout getComputePipelineLayout() const {
        return m_uiResources.computePipelineLayout;
    }

    /// @brief Returns the compute descriptor set layout for the UI device.
    VkDescriptorSetLayout getComputeDescriptorSetLayout() const {
        return m_uiResources.computeDescriptorSetLayout;
    }

    /// @brief Returns the downsample compute pipeline for the UI device.
    VkPipeline getDownsamplePipeline() const {
        return m_uiResources.downsamplePipeline;
    }

    /// @brief Returns the downsample compute pipeline layout for the UI device.
    VkPipelineLayout getDownsamplePipelineLayout() const {
        return m_uiResources.downsamplePipelineLayout;
    }

    /// @brief Returns the downsample descriptor set layout for the UI device.
    VkDescriptorSetLayout getDownsampleDescriptorSetLayout() const {
        return m_uiResources.downsampleDescriptorSetLayout;
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

    // UI Device
    VkPhysicalDevice        m_uiPhysicalDevice = VK_NULL_HANDLE;
    VkDevice                m_uiDevice = VK_NULL_HANDLE;
    VkQueue                 m_uiQueue = VK_NULL_HANDLE;
    uint32_t                m_uiQueueFamilyIndex = 0;
    VkCommandPool           m_uiCommandPool = VK_NULL_HANDLE;
    QVulkanDeviceFunctions *m_uiDeviceFunctions = nullptr;

    // Compute Resources (UI device)
    ComputeResources m_uiResources;

    // Shared Graphics Pipeline
    VkPipeline            m_graphicsPipeline = VK_NULL_HANDLE;
    VkPipelineLayout      m_graphicsPipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_graphicsDescriptorSetLayout = VK_NULL_HANDLE;

    bool m_ownsnDevice = false;
};

#endif // VULKANCONTEXT_H
