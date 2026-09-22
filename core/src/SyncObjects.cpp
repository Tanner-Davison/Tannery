#include "SyncObjects.hpp"
#include <stdexcept>
#include <vector>

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

SyncObjects::SyncObjects(VkDevice pDevice) : device(pDevice) {
    // Semaphore info struct
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    SemaphoreModuleGuard semGuard(this->device);

    // CREATE SEMAPHORES
    VkResult imageAvailableRes(
        vkCreateSemaphore(this->device, &semaphoreInfo, nullptr, &imageAvailableSemaphore));
    if (imageAvailableRes != VK_SUCCESS) {
        throw std::runtime_error("Failed to create the Image Available Semaphore");
    }
    semGuard.push_back(imageAvailableSemaphore);

    VkResult renderFinishedRes(
        vkCreateSemaphore(this->device, &semaphoreInfo, nullptr, &renderCompleteSemaphore));
    if (renderFinishedRes != VK_SUCCESS) {
        throw std::runtime_error("Failed to create the Render Finish Semaphore");
    }
    semGuard.push_back(renderCompleteSemaphore);

    // CREATE FENCE
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkResult fenceResult(vkCreateFence(this->device, &fenceInfo, nullptr, &this->fence));
    if (fenceResult != VK_SUCCESS) {
        throw std::runtime_error("Failed to create a fence for the vkCreateFence");
    }
    // RELEASE THE OWNERSHIP BACK TO SYNC OBJECTS
    semGuard.release();
};

VkSemaphore SyncObjects::getImageAvailableSemaphore() const {
    return this->imageAvailableSemaphore;
};

VkSemaphore SyncObjects::getRenderCompleteSemaphore() const {
    return this->renderCompleteSemaphore;
};

VkFence SyncObjects::getFence() const {
    return this->fence;
}

SyncObjects::~SyncObjects() {
    vkDestroySemaphore(this->device, imageAvailableSemaphore, nullptr);
    vkDestroySemaphore(this->device, renderCompleteSemaphore, nullptr);
    vkDestroyFence(this->device, this->fence, nullptr);
}
