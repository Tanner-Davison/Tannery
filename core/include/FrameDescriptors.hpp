#pragma once
#include "GraphicsContext.hpp"
#include "UniformData.hpp"
#include <memory>
#include <vector>

class FrameDescriptors {
  public:
    FrameDescriptors(const GraphicsContext& pContext, uint32_t pFrameCount);

    // Removed copy and move constructors
    FrameDescriptors(const FrameDescriptors&)            = delete;
    FrameDescriptors& operator=(const FrameDescriptors&) = delete;
    FrameDescriptors(FrameDescriptors&&)                 = delete;
    FrameDescriptors& operator=(FrameDescriptors&&)      = delete;

    ~FrameDescriptors();

    // Handles
    VkDescriptorSetLayout layoutHandle() const;
    VkDescriptorSet       setHandle(uint32_t pFrameIndex) const;

    // Writes this frame slot's uniform data (its fence must already have been waited on)
    void update(uint32_t frameIndex, const CameraUBO& data);

  private:
    void                                 destroy();
    VkDevice                             device = VK_NULL_HANDLE;
    VkDescriptorSetLayout                layout = VK_NULL_HANDLE;
    VkDescriptorPool                     pool   = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet>         sets;
    std::vector<std::unique_ptr<Buffer>> buffers;
};
