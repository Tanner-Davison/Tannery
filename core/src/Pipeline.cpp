#include "Pipeline.hpp"

Pipeline::Pipeline(VkDevice pDevice)
    : device(pDevice) {

    };

Pipeline::~Pipeline() {
    vkDestroyPipeline(this->device, this->pipeline, nullptr);
    vkDestroyPipelineLayout(this->device, this->pipelineLayout, nullptr);
};

VkPipeline Pipeline::handle() const {
    return this->pipeline;
};
