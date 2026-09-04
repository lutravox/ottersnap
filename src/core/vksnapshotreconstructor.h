#pragma once

#include "core/snapshotreconstructor.h"
#include "core/vulkan_types.h"
#include <mutex>

class VulkanContext;

/// @brief Handles GPU-accelerated reconstruction of images from a base image and deltas.
class VkSnapshotReconstructor : public ISnapshotReconstructor {
  public:
    explicit VkSnapshotReconstructor(const VulkanHandles& handles, VulkanContext *context);
    ~VkSnapshotReconstructor();

    bool reconstruct(const ReconstructionSequence& seq) override;
    bool resetToBase(const QImage& base, const QString& checksum) override;
    bool applyDelta(const DeltaEntry& delta) override;
    QImage reconstructToImage(const ReconstructionSequence& seq, QSize targetSize = QSize()) override;
    QRgb samplePixel(int x, int y) override;
    QImage currentState() const override;

    /// @brief Performs cleanup of all GPU resources managed by the reconstructor.
    void cleanup();

    VulkanHandles getHandles() const {
        return m_handles;
    }

    // Prevent copying
    VkSnapshotReconstructor(const VkSnapshotReconstructor&) = delete;
    VkSnapshotReconstructor& operator=(const VkSnapshotReconstructor&) = delete;

    /// @brief Copies the current state buffer to a Vulkan image for rendering.
    /// @param cmd Command buffer to record the copy command into.
    /// @param targetImage The destination image.
    /// @param width Image width.
    /// @param height Image height.
    /// @return True if the copy was successfully recorded, false otherwise.
    bool
    copyToVulkanImage(VkCommandBuffer cmd, VkImage targetImage, uint32_t width, uint32_t height);

    /// @brief Checks if a pending base image upload has completed and swaps
    /// it into the active state.
    /// @return True if a swap occurred, false otherwise.
    bool checkAndSwapBase();

    /// @brief Ensures all pending delta operations are complete on the GPU.
    /// @return True if all pending operations completed, false if a timeout occurred.
    bool waitForDeltas();

    /// @brief Returns true if the base image is currently being uploaded to
    /// the GPU.
    bool isUploadingBase() const;

    /// @brief Resets the uploading base status.
    void cancelBaseUpload();

    /// @brief Resets the delta application status.
    void cancelDeltaApplication();

    /// @brief Resets the internal reconstruction state, clearing active and
    /// cached buffers.
    void resetState();

    void updateStateBufferBinding();

    /// @brief Returns the current state buffer handle.
    /// @return The VkBuffer handle of the current state.
    VkBuffer stateBuffer() const;

    /// @brief Returns the size of the current state buffer.
    /// @return The size in bytes.
    VkDeviceSize stateBufferSize() const;

    /// @brief Returns the width of the current reconstructed image in pixels.
    uint32_t width() const;

    /// @brief Returns the height of the current reconstructed image in pixels.
    uint32_t height() const;

    /// @brief Returns true if the internal state has been updated and needs to be uploaded to the
    /// renderer.
    bool isDirty() const;


  private:
    struct DownsampledBuffer {
        VkBuffer       buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        uint32_t       width = 0;
        uint32_t       height = 0;
    };

    struct UncompressedTile {
        uint32_t   index;
        QByteArray pixels;
    };

    bool updateCachedBase(VkDeviceSize size);
    bool ensurePendingStateBuffer(uint32_t width, uint32_t height, VkDeviceSize size);
    bool prepareBaseStaging(const QImage& base, uint32_t width, uint32_t height, VkDeviceSize size);
    bool recordBaseUploadCommands(bool isCached, VkDeviceSize size);
    bool parseAndDecompressDelta(const DeltaEntry&  delta,
                                 uint32_t&          tileW,
                                 uint32_t&          tileH,
                                 QByteArray&        packedPixels,
                                 QVector<uint32_t>& tileIndices,
                                 QVector<uint32_t>& tileOffsets);
    bool updateStagingResources(const QByteArray&        packedPixels,
                                const QVector<uint32_t>& tileIndices,
                                const QVector<uint32_t>& tileOffsets);
    bool recordDeltaCommands(uint32_t tileW, uint32_t tileH, uint32_t numTiles);
    DownsampledBuffer performDownsample(
        VkBuffer srcBuffer, VkDeviceSize srcSize, uint32_t srcW, uint32_t srcH, QSize targetSize);
    QImage copyToQImage(VkBuffer buffer, VkDeviceSize size, uint32_t width, uint32_t height) const;

    VulkanHandles                m_handles;
    VulkanContext               *m_context = nullptr;
    mutable std::recursive_mutex m_mutex;
    bool                         m_hasValidState = false;

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

    bool m_isDirty = false; // Set true when state changes, false after copyToVulkanImage

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

    VkCommandBuffer m_uploadCmdBuffer = VK_NULL_HANDLE;
    VkCommandBuffer m_deltaCmdBuffer = VK_NULL_HANDLE;
    VkCommandBuffer m_sampleCmdBuffer = VK_NULL_HANDLE;

    // Persistent staging buffer for sampling single pixels
    VkBuffer       m_sampleStagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_sampleStagingMemory = VK_NULL_HANDLE;

    // Downsample Resources
    VkBuffer        m_downsampleBuffer = VK_NULL_HANDLE;
    VkDeviceMemory  m_downsampleMemory = VK_NULL_HANDLE;
    VkDeviceSize    m_downsampleBufferSize = 0;
    VkCommandBuffer m_downsampleCmdBuffer = VK_NULL_HANDLE;
    VkFence         m_downsampleFence = VK_NULL_HANDLE;
    VkDescriptorSet m_downsampleDescriptorSet = VK_NULL_HANDLE;

    uint32_t m_width = 0;
    uint32_t m_height = 0;
};
