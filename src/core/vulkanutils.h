#pragma once

#include <QString>
#include <QVulkanDeviceFunctions>
#include <vector>
#include <vulkan/vulkan.h>

namespace VulkanUtils {

struct BufferAllocation {
    VkBuffer       buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
};

/// @brief Finds a memory type index that matches the given filter and properties.
/// @param inst The Vulkan instance.
/// @param physicalDevice The physical device to query.
/// @param typeFilter Bitmask of compatible memory types.
/// @param properties Required memory property flags.
/// @return The index of the memory type, or UINT32_MAX if none is found.
uint32_t findMemoryType(QVulkanInstance      *inst,
                        VkPhysicalDevice      physicalDevice,
                        uint32_t              typeFilter,
                        VkMemoryPropertyFlags properties);

/// @brief Loads a SPIR-V shader file from resources.
/// @param path Path to the shader resource.
/// @return A vector containing the aligned SPIR-V code, or empty if loading failed.
std::vector<uint32_t> loadShader(const QString& path);

/// @brief Creates a VkShaderModule from SPIR-V code.
/// @param df Vulkan device functions.
/// @param dev The Vulkan device.
/// @param code The SPIR-V code.
/// @param name Name of the shader for logging/error reporting.
/// @return The created shader module, or VK_NULL_HANDLE on failure.
VkShaderModule createShaderModule(QVulkanDeviceFunctions      *df,
                                  VkDevice                     dev,
                                  const std::vector<uint32_t>& code,
                                  const QString&               name);

/// @brief Creates a Vulkan buffer, allocates memory for it, and binds them.
/// @param inst The Vulkan instance.
/// @param df Vulkan device functions.
/// @param dev The Vulkan device.
/// @param physicalDevice The physical device.
/// @param size Buffer size in bytes.
/// @param usage Buffer usage flags.
/// @param properties Required memory property flags.
/// @return A struct containing the buffer and memory handles.
BufferAllocation createBuffer(QVulkanInstance        *inst,
                              QVulkanDeviceFunctions *df,
                              VkDevice                dev,
                              VkPhysicalDevice        physicalDevice,
                              VkDeviceSize            size,
                              VkBufferUsageFlags      usage,
                              VkMemoryPropertyFlags   properties);

/// @brief Destroys a Vulkan resource and nulls the handle.
/// @tparam T The type of the Vulkan handle.
/// @param df Vulkan device functions.
/// @param dev The Vulkan device.
/// @param resource The resource handle to destroy.
/// @param destroyFunc Pointer to the member function of QVulkanDeviceFunctions used for
/// destruction.
template <typename T>
inline void destroyResource(
    QVulkanDeviceFunctions *df,
    VkDevice                dev,
    T&                      resource,
    void (QVulkanDeviceFunctions::*destroyFunc)(VkDevice, T, const VkAllocationCallbacks *)) {
    if (resource != VK_NULL_HANDLE) {
        (df->*destroyFunc)(dev, resource, nullptr);
        resource = VK_NULL_HANDLE;
    }
}

/// @brief Frees Vulkan device memory and nulls the handle.
/// @param df Vulkan device functions.
/// @param dev The Vulkan device.
/// @param memory The device memory handle to free.
inline void freeMemory(QVulkanDeviceFunctions *df, VkDevice dev, VkDeviceMemory& memory) {
    if (memory != VK_NULL_HANDLE) {
        df->vkFreeMemory(dev, memory, nullptr);
        memory = VK_NULL_HANDLE;
    }
}

} // namespace VulkanUtils
