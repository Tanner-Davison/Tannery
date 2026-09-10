#pragma once
#include <filesystem>
#include <vulkan/vulkan_core.h>

class Pipeline {
  public:
    Pipeline(VkDevice                     pDevice,
             VkRenderPass                 pRenderPass,
             VkExtent2D                   pExtent,
             const std::filesystem::path& pFilePathOne,
             const std::filesystem::path& pFilepathTwo);

    ~Pipeline();

    VkPipeline handle() const;

  private:
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline       pipeline       = VK_NULL_HANDLE;
    VkDevice         device         = VK_NULL_HANDLE;
};
