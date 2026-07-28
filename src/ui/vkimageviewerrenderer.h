#pragma once

#include <QElapsedTimer>
#include <QImage>
#include <QPointF>
#include <QSize>
#include <QVulkanDeviceFunctions>
#include <QVulkanWindowRenderer>
#include <mutex>
#include <vulkan/vulkan.h>
#include "core/imagesession.h"
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

    /// @brief Sets the source image to be displayed.
    /// @param img The image to display.
    /// @param preserveView If true, maintains the current zoom and pan levels.
    void setImage(const QImage& img, bool preserveView = false);

    /// @brief Triggers a reconstruction of the image from a snapshot sequence.
    void reconstruct(const ReconstructionSequence& seq);

    /// @brief Marks the uniform buffer as dirty, triggering an update on the next frame.
    void markUboDirty() {
        m_uboDirty = true;
    }

    bool grayscaleEnabled() const {
        return m_session ? m_session->grayscaleEnabled() : false;
    }

    bool mirrorEnabled() const {
        return m_session ? m_session->mirrorEnabled() : false;
    }

    /// @brief Updates the renderer's viewport size.
    /// @param s The new viewport dimensions.
    void setViewportSize(QSize s) {
        m_viewportSize = s;
        m_uboDirty = true;
    }

    /// @brief Clears the current image and resets the viewer state.
    void clear();

    /// @brief Sets the active reconstructor to use for rendering.
    void setReconstructor(std::shared_ptr<VkSnapshotReconstructor> reconstructor);
    void setSession(ImageSession *session);

  protected:
    void initResources() override;
    void initSwapChainResources() override;
    void releaseSwapChainResources() override;
    void releaseResources() override;
    void startNextFrame() override;

  private:
    void performUploads(VkCommandBuffer                          cmd,
                        std::shared_ptr<VkSnapshotReconstructor> reconstructor);
    /// @brief Uploads a QImage to the GPU as a Vulkan texture.
    /// @param cmd The command buffer to record the upload into.
    /// @param image The source image to upload.
    /// @return True if upload succeeded, false if it failed (e.g. OOM).
    bool createAndUploadTexture(VkCommandBuffer cmd, const QImage& image);
    int  createTexture(int width, int height);

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
    void createSamplers();
    void createShaderModules();
    void createDescriptorLayout();
    void createPipelineLayout();
    void createPipeline();
    void createDescriptorPoolAndSet();
    void createUniformBuffer();
    void createVertexBuffer();

    /// @brief Cleans up the old texture and its associated memory.
    void cleanupOldTexture();

    // Image state
    bool   m_hasImage = false;
    bool   m_uploadPending = false;         ///< Set when image needs upload to GPU
    bool   m_reconstructionPending = false; ///< Set when reconstruction is pending
    QImage m_sourceImage;

    ImageSession *m_session = nullptr;

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

    // Widget/container size
    QSize m_viewportSize;

    // UBO dirty flag — skip redundant writes when state hasn't changed
    bool m_uboDirty = false;

    // Last sampler bound to the descriptor set
    VkSampler m_activeSampler = VK_NULL_HANDLE;

    /// @brief The reconstructor currently being used by this renderer.
    std::shared_ptr<VkSnapshotReconstructor> m_activeReconstructor;
    std::mutex                               m_reconstructorMutex;
};
