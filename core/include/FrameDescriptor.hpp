#pragma once
#include "GraphicsContext.hpp"
#include "UniformData.hpp"
#include <memory>
#include <vector>

class FrameDescriptor {
  public:
    FrameDescriptor(const GraphicsContext& pContext, uint32_t pFrameCount);

    // Removed copy and move constructors
    FrameDescriptor(const FrameDescriptor&)           = delete;
    FrameDescriptor operator=(const FrameDescriptor&) = delete;
    FrameDescriptor(FrameDescriptor&&)                = delete;
    FrameDescriptor& operator=(FrameDescriptor&&)     = delete;

    ~FrameDescriptor();

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
