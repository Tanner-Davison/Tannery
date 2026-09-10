#include "Pipeline.hpp"
#include "shaderModule.hpp"

Pipeline::Pipeline(VkDevice                     pDevice,
                   VkRenderPass                 pRenderPass,
                   VkExtent2D                   pExtent,
                   const std::filesystem::path& pVertPath,
                   const std::filesystem::path& pFragPath)
    : device(pDevice) {
    std::vector<char> vertCode   = readFile(pVertPath);
    VkShaderModule    vertModule = createShaderModule(this->device, vertCode);

    VkPipelineShaderStageCreateInfo vertStageInfo{};
    vertStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStageInfo.stage  = VK_SHADER_STAGE_VERTEX_BIT;
    vertStageInfo.module = vertModule;
    vertStageInfo.pName  = "main";

    std::vector<char> fragCode   = readFile(pFragPath);
    VkShaderModule    fragModule = createShaderModule(this->device, fragCode);

    VkPipelineShaderStageCreateInfo fragStageInfo{};
    fragStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStageInfo.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStageInfo.module = fragModule;
    fragStageInfo.pName  = "main";
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
