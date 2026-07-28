#include <QDebug>
#include <QFile>
#include <QVulkanInstance>
#include <cstring>
#include "core/vulkanutils.h"

std::vector<uint32_t> VulkanUtils::loadShader(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        qCritical() << "[VulkanUtils] Failed to load shader resource:" << path;
        return {};
    }

    QByteArray bytes = f.readAll();

    std::vector<uint32_t> code(bytes.size() / 4);
    std::memcpy(code.data(), bytes.constData(), bytes.size());
    return code;
}

VkShaderModule VulkanUtils::createShaderModule(QVulkanDeviceFunctions      *df,
                                               VkDevice                     dev,
                                               const std::vector<uint32_t>& code,
                                               const QString&               name) {
    if (code.empty()) {
        return VK_NULL_HANDLE;
    }

    VkShaderModuleCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = code.size() * sizeof(uint32_t);
    ci.pCode = code.data();

    VkShaderModule m;
    VkResult       result = df->vkCreateShaderModule(dev, &ci, nullptr, &m);
    if (result != VK_SUCCESS) {
        qCritical() << "[VulkanUtils] vkCreateShaderModule failed for" << name << ":" << result;
        return VK_NULL_HANDLE;
    }

    return m;
}

uint32_t VulkanUtils::findMemoryType(QVulkanInstance      *inst,
                                     VkPhysicalDevice      physicalDevice,
                                     uint32_t              typeFilter,
                                     VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProps;
    inst->functions()->vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) &&
            (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return UINT32_MAX;
}

VulkanUtils::BufferAllocation VulkanUtils::createBuffer(QVulkanInstance        *inst,
                                                        QVulkanDeviceFunctions *df,
                                                        VkDevice                dev,
                                                        VkPhysicalDevice        physicalDevice,
                                                        VkDeviceSize            size,
                                                        VkBufferUsageFlags      usage,
                                                        VkMemoryPropertyFlags   properties) {
    VkBufferCreateInfo bci{};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = size;
    bci.usage = usage;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer;
    if (df->vkCreateBuffer(dev, &bci, nullptr, &buffer) != VK_SUCCESS) {
        qCritical() << "[VulkanUtils] Failed to create buffer";
        return {};
    }

    VkMemoryRequirements memReq;
    df->vkGetBufferMemoryRequirements(dev, buffer, &memReq);

    uint32_t memType = findMemoryType(inst, physicalDevice, memReq.memoryTypeBits, properties);
    if (memType == UINT32_MAX) {
        qCritical() << "[VulkanUtils] Failed to find suitable memory type for buffer";
        df->vkDestroyBuffer(dev, buffer, nullptr);
        return {};
    }

    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = memReq.size;
    mai.memoryTypeIndex = memType;

    VkDeviceMemory memory;
    if (df->vkAllocateMemory(dev, &mai, nullptr, &memory) != VK_SUCCESS) {
        qCritical() << "[VulkanUtils] Failed to allocate buffer memory";
        df->vkDestroyBuffer(dev, buffer, nullptr);
        return {};
    }

    df->vkBindBufferMemory(dev, buffer, memory, 0);

    return {buffer, memory};
}
