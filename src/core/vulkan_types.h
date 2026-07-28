#pragma once

#include <QByteArray>
#include <QImage>
#include <QString>
#include <QVector>
#include <QVulkanDeviceFunctions>
#include <QVulkanInstance>
#include <vulkan/vulkan.h>

/// @brief A simple container for core Vulkan handles.
struct VulkanHandles {
    VkPhysicalDevice        physicalDevice = VK_NULL_HANDLE;
    VkDevice                device = VK_NULL_HANDLE;
    VkQueue                 queue = VK_NULL_HANDLE;
    uint32_t                queueFamilyIndex = 0;
    QVulkanDeviceFunctions *deviceFunctions = nullptr;
    VkCommandPool           commandPool = VK_NULL_HANDLE;

    bool isValid() const {
        return device != VK_NULL_HANDLE && deviceFunctions != nullptr &&
               commandPool != VK_NULL_HANDLE;
    }
};

/// @brief The reconstruction sequence containing the base and deltas.
struct ReconstructionSequence {
    int                 baseIdx;
    QImage              base;
    QString             baseChecksum;
    QVector<QByteArray> deltas;
};
