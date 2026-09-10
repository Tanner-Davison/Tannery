#include "Pipeline.hpp"

Pipeline::Pipeline(VkDevice                     pDevice,
                   VkRenderPass                 pRenderPass,
                   VkExtent2D                   pExtent,
                   const std::filesystem::path& pFilePathOne,
                   const std::filesystem::path& pFilepathTwo)
    : device(pDevice) {

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
