#include "Pipeline.hpp"
#include "shaderModule.hpp"
#include <stdexcept>

/* SHADER MODULE SCOPED GUARD */
namespace {
struct ShaderModuleGuard {
    VkDevice       device;
    VkShaderModule module;
    ShaderModuleGuard(VkDevice pDevice, VkShaderModule pModule)
        : device(pDevice)
        , module(pModule) {};
    // copy constructors
    ShaderModuleGuard(const ShaderModuleGuard&)            = delete;
    ShaderModuleGuard& operator=(const ShaderModuleGuard&) = delete;

    // destructor
    ~ShaderModuleGuard() {
        vkDestroyShaderModule(device, module, nullptr);
    }
};
} // namespace

Pipeline::Pipeline(VkDevice                     pDevice,
                   VkRenderPass                 pRenderPass,
                   VkExtent2D                   pExtent,
                   const std::filesystem::path& pVertPath,
                   const std::filesystem::path& pFragPath)
    : device(pDevice) {
    std::vector<char> vertCode   = readFile(pVertPath);
    VkShaderModule    vertModule = createShaderModule(this->device, vertCode);
    ShaderModuleGuard vertGuard{this->device, vertModule};

    /* Vert Stage Info */
    VkPipelineShaderStageCreateInfo vertStageInfo{}; // Vert Struct
    vertStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStageInfo.stage  = VK_SHADER_STAGE_VERTEX_BIT;
    vertStageInfo.module = vertModule;
    vertStageInfo.pName  = "main";

    /* Frag Stage Info */
    std::vector<char> fragCode   = readFile(pFragPath);
    VkShaderModule    fragModule = createShaderModule(this->device, fragCode);
    ShaderModuleGuard fragGuard{this->device, fragModule};

    VkPipelineShaderStageCreateInfo fragStageInfo{}; // Frag Struct
    fragStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStageInfo.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStageInfo.module = fragModule;
    fragStageInfo.pName  = "main";

    /* Vertex Input & Assembly Info*/
    VkPipelineShaderStageCreateInfo shaderStages[] = {vertStageInfo, fragStageInfo};

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount   = 0;
    vertexInputInfo.pVertexBindingDescriptions      = nullptr;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    vertexInputInfo.pVertexAttributeDescriptions    = nullptr;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    /* VIEWPORT */
    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(pExtent.width);
    viewport.height   = static_cast<float>(pExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    /* SCISSOR */
    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = pExtent; // covers the entire framebuffer - no clipping

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports    = &viewport;
    viewportState.scissorCount  = 1;
    viewportState.pScissors     = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizerState{};
    rasterizerState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizerState.depthClampEnable        = VK_FALSE;
    rasterizerState.rasterizerDiscardEnable = VK_FALSE;
    rasterizerState.polygonMode             = VK_POLYGON_MODE_FILL;
    rasterizerState.lineWidth               = 1.0f;
    rasterizerState.cullMode                = VK_CULL_MODE_NONE;
    rasterizerState.frontFace               = VK_FRONT_FACE_CLOCKWISE;
    rasterizerState.depthBiasEnable         = VK_FALSE;

    /*MULIT SAMPLE ANTI ALIASING (MSAA)*/
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    // needs to agree with Render Passes color attachment-----
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    //----------------------------------------------------------
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    /*COLOR  BLENDING*/
    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable   = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments    = &colorBlendAttachment;

    /* PIPLINE LAYOUT */
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pSetLayouts    = nullptr;

    if (vkCreatePipelineLayout(this->device,
                               &pipelineLayoutInfo,
                               nullptr,
                               &this->pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Error: could not create vkCreatePipelineLayout");

        vkDestroyShaderModule(this->device, vertModule, nullptr);
        vkDestroyShaderModule(this->device, fragModule, nullptr);
    }
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount          = 2;
    pipelineInfo.pStages             = shaderStages;
    pipelineInfo.pVertexInputState   = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizerState;
    pipelineInfo.pMultisampleState   = &multisampling;
    pipelineInfo.pColorBlendState    = &colorBlending;
    pipelineInfo.pDepthStencilState  = nullptr;
    pipelineInfo.pDynamicState       = nullptr;
    pipelineInfo.layout              = this->pipelineLayout;
    pipelineInfo.renderPass          = pRenderPass;
    pipelineInfo.subpass             = 0; // hardcoded since we only have 1 subpass

    if (vkCreateGraphicsPipelines(this->device,
                                  VK_NULL_HANDLE,
                                  1,
                                  &pipelineInfo,
                                  nullptr,
                                  &this->pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Error: Could Not Create vkCreateGraphicsPipelines");
    }
    vkDestroyShaderModule(this->device, vertModule, nullptr);
    vkDestroyShaderModule(this->device, fragModule, nullptr);
};

Pipeline::~Pipeline() {
    vkDestroyPipeline(this->device, this->pipeline, nullptr);
    vkDestroyPipelineLayout(this->device, this->pipelineLayout, nullptr);
};

VkPipeline Pipeline::pipelineHandle() const {
    return this->pipeline;
};

VkPipelineLayout Pipeline::pipelineLayoutHandle() const {
    return this->pipelineLayout;
};
