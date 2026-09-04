#pragma once

#include <QColor>
#include <QElapsedTimer>
#include <QPointF>
#include <QQuickRhiItem>
#include <QQuickRhiItemRenderer>
#include <QSize>
#include <QVulkanDeviceFunctions>
#include <QVulkanInstance>
#include <mutex>
#include <optional>
#include <rhi/qrhi.h>
#include <vulkan/vulkan.h>
#include "core/viewer_interfaces.h"
#include "core/vksnapshotreconstructor.h"

/// @struct UniformBufferObject
/// @brief Layout of the uniform buffer used for image display.
struct alignas(4) UniformBufferObject {
    float    uViewport[2];     // widget size in pixels
    float    uFitImgSize[2];   // fitted image size in viewport pixels
    float    uFitImgOrigin[2]; // top-left corner of fitted image in viewport pixels
    float    uPanOffset[2];    // pan offset (center-shifted UV space)
    float    uFitScale;        // display-pixels / image-pixels (for LOD)
    float    uZoomLevel;       // display scale relative to fit (1.0 = 1:1)
    VkBool32 uGrayscale;       // `VK_TRUE` = grayscale
    VkBool32 uMirror;          // `VK_TRUE` = horizontally mirrored
};

/// @class VkImageViewerRenderer
/// @brief Handles the Vulkan rendering pipeline for displaying image snapshots.
class VkImageViewerRenderer : public QQuickRhiItemRenderer {
  public:
    VkImageViewerRenderer() = default;
    ~VkImageViewerRenderer() override;

    /// @brief Updates the render state.
    /// @param state The new state.
    /// @param generation The request ID associated with this state change.
    /// @return True if the state was updated, false if the request was stale.
    bool setRenderState(RenderState state, uint32_t generation) {
        if (generation < m_currentGeneration) {
            return false;
        }
        m_currentGeneration = generation;
        m_state = state;
        return true;
    }

    /// @brief Sets the source image to be displayed.
    /// @param img The image to display.
    /// @param generation The request ID associated with this image.
    void setImage(const QImage& img, uint32_t generation);

    /// @brief Triggers a reconstruction of the image from a snapshot sequence.
    /// @return True if reconstruction was successful, false otherwise.
    bool reconstruct(const ReconstructionSequence& seq, uint32_t generation);

    /// @brief Marks the uniform buffer as dirty, triggering an update on the next frame.
    void markUboDirty() {
        m_uboDirty = true;
    }

    bool grayscaleEnabled() const {
        return m_params.grayscale;
    }

    /// @brief Returns whether horizontal mirroring is enabled.
    bool mirrorEnabled() const {
        return m_params.mirror;
    }

    /// @brief Updates the renderer's viewport size.
    /// @param s The new viewport dimensions.
    void setViewportSize(QSize s) {
        m_viewportSize = s;
        m_uboDirty = true;
    }

    /// @brief Returns the current rendering state.
    RenderState renderState() const {
        return m_state;
    }

    /// @brief Clears the current image and resets the viewer state.
    void clear();

    /// @brief Sets the active reconstructor to use for rendering.
    void setReconstructor(const std::shared_ptr<VkSnapshotReconstructor>& reconstructor);

    RenderParams m_params;

  protected:
    /// @brief Initializes resources (called before each on-demand render).
    void initialize(QRhiCommandBuffer *cb) override;
    /// @brief Synchronizes per-render state with the item.
    void synchronize(QQuickRhiItem *item) override;
    /// @brief Renders the frame (raw-Vulkan draw into the item's render target).
    void render(QRhiCommandBuffer *cb) override;

  private:
    /// @brief One-time resource creation (guarded by m_resourcesReady).
    bool ensureResources();
    /// @brief Releases all allocated Vulkan resources.
    void releaseResources();
    /// @brief Returns the item's render pass (for pipeline creation).
    VkRenderPass itemRenderPass() const;

  private:
    /// @brief Records image-upload commands (copy + mip generation) into a primary
    /// command buffer that runs outside the render pass.
    /// @return True if commands were recorded and must be submitted, false if the
    /// upload was deferred or failed.
    bool performUploads(VkCommandBuffer                                 cmd,
                        const std::shared_ptr<VkSnapshotReconstructor>& reconstructor);
    /// @brief Allocates the dedicated primary command buffer and fence used for uploads.
    void ensurePrepareCmd();
    /// @brief Submits the prepare command buffer and waits for it to finish.
    void submitAndWaitPrepare();
    /// @brief Uploads a QImage to the GPU as a Vulkan texture.
    /// @param cmd The command buffer to record the upload into.
    /// @param image The source image to upload.
    /// @return True if upload succeeded, false if it failed (e.g. OOM).
    bool createAndUploadTexture(VkCommandBuffer cmd, const QImage& image);
    /// @brief Creates a Vulkan image and allocates memory for it.
    /// @return The number of mip levels created, or 0 if failed.
    int createTexture(int width, int height);

    /// @brief Records commands to generate a mipmap chain for the texture.
    /// @param cmd The command buffer to record into.
    /// @param mipLevels Number of mip levels to generate.
    /// @param width Initial width of the image.
    /// @param height Initial height of the image.
    void recordMipChainGeneration(VkCommandBuffer cmd, int mipLevels, int width, int height);

    /// @brief Creates a VkImageView and updates the descriptor sets.
    /// @param mipLevels Number of mip levels in the image.
    void createViewAndUpdateDescriptors(int mipLevels);

    /// @brief Updates the Uniform Buffer Object with current view state.
    void updateUniformBuffer();

    /// @brief Updates the descriptor set for the current frame.
    /// @param dstSet The descriptor set to update.
    /// @param ubo The uniform buffer handle.
    /// @param samp The sampler handle.
    /// @param texView The image view handle.
    void
    updateDescriptors(VkDescriptorSet dstSet, VkBuffer ubo, VkSampler samp, VkImageView texView);

    // Resource creation helpers
    /// @brief Creates the linear and nearest-neighbor samplers.
    /// @return True if success, false otherwise.
    bool createSamplers();
    /// @brief Creates the descriptor pool and allocates the descriptor set.
    /// @return True if success, false otherwise.
    bool createDescriptorPoolAndSet();
    /// @brief Creates and maps the uniform buffer.
    /// @return True if success, false otherwise.
    bool createUniformBuffer();
    /// @brief Creates and populates the vertex buffer for the fullscreen quad.
    /// @return True if success, false otherwise.
    bool createVertexBuffer();

    /// @brief Cleans up the old texture and its associated memory.
    void cleanupOldTexture();
    /// @brief Moves the current texture handles into a "retired" slot without
    /// destroying them (a prior frame's draw may still be sampling them).
    void retireOldTexture();
    /// @brief Destroys the retired texture (safe once the in-flight work has
    /// completed, e.g. after the prepare fence is signaled).
    void destroyRetiredTexture();

    // Render state
    RenderState m_state = RenderState::Empty;
    uint32_t    m_currentGeneration = 0;
    QImage      m_sourceImage;

    // Qt Vulkan function wrappers
    QVulkanDeviceFunctions *m_devFuncs = nullptr;

    // QRHI device (obtained from rhi()->nativeHandles() in initialize())
    QRhi            *m_rhi = nullptr;
    VkDevice         m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physDev = VK_NULL_HANDLE;
    QVulkanInstance *m_instance = nullptr;
    bool             m_resourcesReady = false;

    // Upload/prepare command buffer (runs outside the render pass) + fence
    VkCommandBuffer m_prepareCmd = VK_NULL_HANDLE;
    VkFence         m_prepareFence = VK_NULL_HANDLE;

    // Retired texture awaiting delayed destruction (previous frame may still sample it)
    VkImage        m_retiredImage = VK_NULL_HANDLE;
    VkDeviceMemory m_retiredMemory = VK_NULL_HANDLE;
    VkImageView    m_retiredView = VK_NULL_HANDLE;

    // Persistent (device-lifetime)
    VkSampler        m_samplerLinear = VK_NULL_HANDLE;
    VkSampler        m_samplerNearest = VK_NULL_HANDLE;
    VkBuffer         m_uniformBuffer = VK_NULL_HANDLE;
    VkDeviceMemory   m_uniformMemory = VK_NULL_HANDLE;
    void            *m_uniformMapped = nullptr;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet  m_descriptorSet = VK_NULL_HANDLE;

    // Texture
    VkImage        m_textureImage = VK_NULL_HANDLE;
    VkDeviceMemory m_textureMemory = VK_NULL_HANDLE;
    VkImageView    m_textureView = VK_NULL_HANDLE;
    int            m_currentTextureWidth = 0;
    int            m_currentTextureHeight = 0;

    // Staging buffer
    VkBuffer       m_stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_stagingMemory = VK_NULL_HANDLE;

    // Vertex buffer
    VkBuffer       m_vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_vertexMemory = VK_NULL_HANDLE;

    // Widget/container size
    QSize m_viewportSize;

    // UBO dirty flag — skip redundant writes when state hasn't changed
    bool m_uboDirty = false;

    // Last sampler bound to the descriptor set
    VkSampler m_activeSampler = VK_NULL_HANDLE;

    /// @brief The reconstructor currently being used by this renderer.
    std::shared_ptr<VkSnapshotReconstructor> m_activeReconstructor;
    std::mutex                               m_reconstructorMutex;

    /// @brief Reconstruction sequence deferred until the device is ready.
    std::optional<ReconstructionSequence> m_pendingReconstruction;
    uint32_t                              m_pendingGeneration = 0;
};
