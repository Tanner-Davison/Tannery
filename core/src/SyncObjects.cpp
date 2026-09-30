#include "SyncObjects.hpp"
#include <format>
#include <stdexcept>
#include <vector>
#include <vk_enum_string_helper.h>

namespace {
struct SemaphoreModuleGuard {
    VkDevice                 device;
    std::vector<VkSemaphore> semaphores;

    explicit SemaphoreModuleGuard(VkDevice pDevice) : device(pDevice) {
        semaphores.reserve(2);
    };

    // Member function to add new semaphores to vector
    void push_back(VkSemaphore semaphore) {
        if (semaphore != VK_NULL_HANDLE) {
            semaphores.push_back(semaphore);
        }
    }

    // Release Ownership
    void release() {
        semaphores.clear();
    }

    // Copy Constructors Removed
    SemaphoreModuleGuard(const SemaphoreModuleGuard&)            = delete;
    SemaphoreModuleGuard& operator=(const SemaphoreModuleGuard&) = delete;

    // Destructor
    ~SemaphoreModuleGuard() {
        for (const auto& semaphore : semaphores) {
            vkDestroySemaphore(this->device, semaphore, nullptr);
        }
    }
};
} // namespace

SyncObjects::SyncObjects(VkDevice pDevice, uint32_t pImageCount) : device(pDevice) {
    renderCompleteSemaphores.resize(pImageCount);
    imagesAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    SemaphoreModuleGuard semGuard(this->device);

    // Semaphore info struct
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for (uint32_t i = 0; i < imagesAvailableSemaphores.size(); ++i) {
        // IMAGE AVAILABLE SEMAPHORE
        VkResult imageAvailableRes(vkCreateSemaphore(this->device,
                                                     &semaphoreInfo,
                                                     nullptr,
                                                     &imagesAvailableSemaphores[i]));
        if (imageAvailableRes != VK_SUCCESS) {
            throw std::runtime_error(
                std::format("Failed to create the Image Available Semaphore. VkError: {}",
                            string_VkResult(imageAvailableRes)));
        }

        semGuard.push_back(imagesAvailableSemaphores[i]);
    }

    for (size_t i = 0; i < pImageCount; i++) {
        VkResult renderFinishedRes(vkCreateSemaphore(this->device,
                                                     &semaphoreInfo,
                                                     nullptr,
                                                     &renderCompleteSemaphores[i]));
        if (renderFinishedRes != VK_SUCCESS) {
            throw std::runtime_error(
                std::format("Failed to create the Render Finish Semaphore(s), VkError: {}",
                            string_VkResult(renderFinishedRes)));
        }

        semGuard.push_back(renderCompleteSemaphores[i]);
    };

    // CREATE FENCE
    fences.resize(MAX_FRAMES_IN_FLIGHT);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VkResult fenceResult(
            vkCreateFence(this->device, &fenceInfo, nullptr, &this->fences[i]));
        if (fenceResult != VK_SUCCESS) {
            throw std::runtime_error(
                std::format("Failed to create a fence for the vkCreateFence fences[{}]: {}",
                            i,
                            string_VkResult(fenceResult)));
        }
    }
    // RELEASE THE OWNERSHIP BACK TO THIS- SYNC OBJECTS CLASS
    semGuard.release();
};

VkSemaphore SyncObjects::getImageAvailableSemaphore(uint32_t currentFrameIdex) const {
    return this->imagesAvailableSemaphores.at(currentFrameIdex);
};

VkSemaphore SyncObjects::getRenderCompleteSemaphore(uint32_t imageIndex) const {
    return this->renderCompleteSemaphores.at(imageIndex);
};

VkFence SyncObjects::getFence(uint32_t currentFrameIndex) const {
    return this->fences.at(currentFrameIndex);
}

SyncObjects::~SyncObjects() {
    for (const auto& semaphore : imagesAvailableSemaphores) {
        vkDestroySemaphore(this->device, semaphore, nullptr);
    }
    for (auto& renderFinishedSemaphore : renderCompleteSemaphores) {
        vkDestroySemaphore(this->device, renderFinishedSemaphore, nullptr);
    }
    for (const auto& fence : this->fences) {
        vkDestroyFence(this->device, fence, nullptr);
    }
}
