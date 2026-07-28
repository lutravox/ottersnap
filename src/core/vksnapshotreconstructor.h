#pragma once

#include <mutex>
#include <vulkan/vulkan.h>
#include "core/vulkan_types.h"

/// @brief Handles GPU-accelerated reconstruction of images from a base image and deltas.
class VkSnapshotReconstructor {
  public:
    explicit VkSnapshotReconstructor(const VulkanHandles& handles);
    ~VkSnapshotReconstructor();

    /// @brief Performs cleanup of all GPU resources managed by the reconstructor.
    void cleanup();

    // Prevent copying
    VkSnapshotReconstructor(const VkSnapshotReconstructor&) = delete;
    VkSnapshotReconstructor& operator=(const VkSnapshotReconstructor&) = delete;

    /// @brief Reconstructs a snapshot from a base image and a series of deltas.
    /// @param seq The reconstruction sequence containing the base and deltas.
    /// @return True if reconstruction succeeded, false otherwise.
    bool reconstruct(const ReconstructionSequence& seq);

    /// @brief Resets the current state to a base image.
    /// @param base The base image to reset to.
    /// @param checksum The checksum of the base image for verification.
    /// @return True if reset succeeded, false otherwise.
    bool resetToBase(const QImage& base, const QString& checksum);

    /// @brief Applies a delta to the current state using the compute shader.
    /// @param delta The delta data to apply.
    /// @return True if delta was applied successfully, false otherwise.
    bool applyDelta(const QByteArray& delta);

    /// @brief Copies the current state buffer to a Vulkan image for rendering.
    /// @param cmd Command buffer to record the copy command into.
    /// @param targetImage The destination image.
    /// @param width Image width.
    /// @param height Image height.
    /// @return True if the copy was successfully recorded, false otherwise.
    bool copyToImage(VkCommandBuffer cmd, VkImage targetImage, uint32_t width, uint32_t height);

    /// @brief Reconstructs a snapshot and returns the result as a QImage.
    /// @param seq The reconstruction sequence containing the base and deltas.
    /// @param targetSize Optional target size for GPU-based downsampling.
    /// @return A QImage containing the reconstructed pixels, or an empty image on failure.
    QImage reconstructToImage(const ReconstructionSequence& seq,
                              QSize                         targetSize = QSize(),
                              VkSnapshotReconstructor      *worker = nullptr);

    /// @brief Checks if a pending base image upload has completed and swaps
    /// it into the active state.
    /// @return True if a swap occurred, false otherwise.
    bool checkAndSwapBase();

    /// @brief Ensures all pending delta operations are complete on the GPU.
    void waitForDeltas();

    /// @brief Returns true if the base image is currently being uploaded to
    /// the GPU.
    bool isUploadingBase() const {
        return m_isUploadingBase;
    }

    /// @brief Resets the internal reconstruction state, clearing active and
    /// cached buffers.
    void resetState();

    /// @brief Returns the current state buffer handle.
    /// @return The VkBuffer handle of the current state.
    VkBuffer stateBuffer() const {
        return m_stateBuffer;
    }

    uint32_t width() const {
        return m_width;
    }

    uint32_t height() const {
        return m_height;
    }

    /// @brief Returns true if the internal state has been updated and needs to be uploaded to the
    /// renderer.
    bool isDirty() const {
        return m_isDirty;
    }

    VkDeviceSize stateBufferSize() const {
        return m_stateBufferSize;
    }

  private:
    bool updateCachedBase(VkDeviceSize size);

    VulkanHandles        m_handles;
    std::recursive_mutex m_mutex;

    // Descriptor Set (Session-specific)
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;

    // State Buffer (The "Current Image")
    VkBuffer       m_stateBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_stateMemory = VK_NULL_HANDLE;
    VkDeviceSize   m_stateBufferSize = 0;

    // Cached Base Buffer
    VkBuffer       m_cachedBaseBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_cachedBaseMemory = VK_NULL_HANDLE;
    QString        m_lastBaseChecksum;

    // Pending State Buffer (For Async Uploads)
    VkBuffer       m_pendingStateBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_pendingStateMemory = VK_NULL_HANDLE;
    VkDeviceSize   m_pendingStateCapacity = 0;
    VkFence        m_uploadFence = VK_NULL_HANDLE;
    bool           m_isUploadingBase = false;

    // Delta Sync
    VkFence m_deltaFence = VK_NULL_HANDLE;
    bool    m_isApplyingDelta = false;

    bool m_isDirty = false; // Set true when state changes, false after copyToImage

    // Base Staging Buffer
    VkBuffer       m_baseStagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_baseStagingMemory = VK_NULL_HANDLE;
    VkDeviceSize   m_baseStagingCapacity = 0;
    void          *m_baseStagingMapped = nullptr;

    // Delta Staging Buffer (Pixels)
    VkBuffer       m_deltaStagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_deltaStagingMemory = VK_NULL_HANDLE;
    VkDeviceSize   m_deltaStagingBufferSize = 0;
    VkDeviceSize   m_deltaStagingCapacity = 0;
    void          *m_deltaStagingMapped = nullptr;

    // Tile Index Buffer (Metadata)
    VkBuffer       m_tileIndexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_tileIndexMemory = VK_NULL_HANDLE;
    VkDeviceSize   m_tileIndexBufferSize = 0;
    VkDeviceSize   m_tileIndexCapacity = 0;
    void          *m_tileIndexMapped = nullptr;

    struct PushConstants {
        uint32_t tileW;
        uint32_t tileH;
        uint32_t imageW;
        uint32_t imageH;
        uint32_t numTiles;
    };

    VkCommandBuffer m_computeCmdBuffer = VK_NULL_HANDLE;

    uint32_t m_width = 0;
    uint32_t m_height = 0;
};
