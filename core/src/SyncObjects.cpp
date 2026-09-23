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

    void push_back_vec(std::vector<VkSemaphore> pSemaphores) {
        for (const auto& semaphore : pSemaphores) {
            if (semaphore != VK_NULL_HANDLE) {
                semaphores.push_back(semaphore);
            }
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

    for (size_t i = 0; i < pImageCount; i++) {
        VkResult renderFinishedRes(vkCreateSemaphore(this->device,
                                                     &semaphoreInfo,
                                                     nullptr,
                                                     &renderCompleteSemaphores[i]));
        if (renderFinishedRes != VK_SUCCESS) {
            throw std::runtime_error("Failed to create the Render Finish Semaphore(s)");
        }
    };
    semGuard.push_back_vec(renderCompleteSemaphores);

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

std::vector<VkSemaphore> SyncObjects::getRenderCompleteSemaphores() const {
    return this->renderCompleteSemaphores;
};

VkFence SyncObjects::getFence() const {
    return this->fence;
}

SyncObjects::~SyncObjects() {
    vkDestroySemaphore(this->device, imageAvailableSemaphore, nullptr);
    for (auto& renderFinishedSemaphore : renderCompleteSemaphores) {
        vkDestroySemaphore(this->device, renderFinishedSemaphore, nullptr);
    }
    vkDestroyFence(this->device, this->fence, nullptr);
}
