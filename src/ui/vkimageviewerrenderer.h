#pragma once

#include <QColor>
#include <QElapsedTimer>
#include <QPointF>
#include <QSize>
#include <QVulkanDeviceFunctions>
#include <QVulkanWindowRenderer>
#include <mutex>
#include <optional>
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
class VkImageViewerRenderer : public QVulkanWindowRenderer {
  public:
    VkImageViewerRenderer() = default;

    /// @brief The window associated with this renderer.
    QVulkanWindow *m_vkWindow = nullptr;

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

    /// @brief Sets the indicator's state.
    /// @param pos Position in pixels relative to the viewer.
    /// @param color Color of the indicator.
    /// @param visible Whether it should be rendered.
    void setIndicator(QPoint pos, QColor color, bool visible);

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
    /// @brief Initializes Vulkan resources (samplers, buffers, etc.).
    void initResources() override;
    /// @brief Prepares resources specifically tied to the swapchain.
    void initSwapChainResources() override;
    /// @brief Releases swapchain-specific resources.
    void releaseSwapChainResources() override;
    /// @brief Releases all allocated Vulkan resources.
    void releaseResources() override;
    /// @brief Renders the next frame to the swapchain.
    void startNextFrame() override;

  private:
    /// @brief Handles uploading image data to the GPU.
    void performUploads(VkCommandBuffer                                 cmd,
                        const std::shared_ptr<VkSnapshotReconstructor>& reconstructor);
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

    /// @brief Creates the pipeline for rendering the cluster indicator.
    void createIndicatorPipeline();

    // Render state
    RenderState m_state = RenderState::Empty;
    uint32_t    m_currentGeneration = 0;
    QImage      m_sourceImage;

    // Qt Vulkan function wrappers
    QVulkanDeviceFunctions *m_devFuncs = nullptr;

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

    // Indicator state
    static constexpr float m_indicatorSize = 40.0f;
    QPoint                 m_indicatorPos;
    QColor                 m_indicatorColor;
    bool                   m_indicatorVisible = false;

    // Indicator pipeline
    VkPipeline       m_indicatorPipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_indicatorPipelineLayout = VK_NULL_HANDLE;

    // Widget/container size
    QSize m_viewportSize;

    // UBO dirty flag — skip redundant writes when state hasn't changed
    bool m_uboDirty = false;

    // Last sampler bound to the descriptor set
    VkSampler m_activeSampler = VK_NULL_HANDLE;

    /// @brief The reconstructor currently being used by this renderer.
    std::shared_ptr<VkSnapshotReconstructor> m_activeReconstructor;
    std::mutex                               m_reconstructorMutex;

    /// @brief Reconstruction sequence deferred until m_vkWindow is ready.
    std::optional<ReconstructionSequence> m_pendingReconstruction;
    uint32_t                              m_pendingGeneration = 0;
};
