#include "RenderPass.hpp"
#include <stdexcept>

RenderPass::RenderPass(VkDevice pDevice, VkSurfaceFormatKHR pSurfaceFormat)
    : device(pDevice)
    , surfaceFormat(pSurfaceFormat) {
    /* COLOR ATTACHMENT */
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = surfaceFormat.format;
    // Multisampling rasterizer Sameples needs same value
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT; //
    // -----------------------------------------------------
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    /* COLOR ATTACHMENT REF */
    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0; // Points to the first attachment in the render pass
    colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    /* SUBPASS DESCRIPTION */
    //-------------------------------------------------------------
    VkSubpassDescription subpass{};
    // vk_pipeline_bind_point_graphics rendering vertex, fragments or geometry rasterization
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    // colorAttachmentCount = 1. If you are doing advanced rendering (like deferred shading),
    // you might output to multiple textures simultaneously, raising that count.
    subpass.colorAttachmentCount = 1;                   // how many
    subpass.pColorAttachments    = &colorAttachmentRef; // where they are

    /* SUBPASS DEPENDANCY */
    //-------------------------------------------------------------
    VkSubpassDependency dependency{};
    // 1. Who is involved
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL; // Operations before this render pass
    dependency.dstSubpass = 0;                   // Our first (and only) subpass
    // 2.  when does the barrier trip?
    dependency.srcStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // Wait for previous frame's output
    dependency.dstStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // Block our subpass at this stage
    // 3. What memory operations need flushing?
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    dependency.dependencyFlags = 0;

    /* RENDERPASS INFO */
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments    = &colorAttachment;
    renderPassInfo.subpassCount    = 1;
    renderPassInfo.pSubpasses      = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies   = &dependency;

    if (vkCreateRenderPass(this->device, &renderPassInfo, nullptr, &this->renderPass) !=
        VK_SUCCESS) {
        throw std::runtime_error("Error: Failed to create RenderPass");
    }
};

RenderPass::~RenderPass() {
    vkDestroyRenderPass(device, this->renderPass, nullptr);
};

VkRenderPass RenderPass::handle() const {
    return this->renderPass;
};
