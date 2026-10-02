#include "physicalDevice.hpp"
#include "vk_enum_string_helper.h"
#include <cstdint>
#include <print>
#include <vector>
#include <vulkan/vulkan.h>

/* NOTE: We never communicate to the GPU directly with the
 * physical device we do that through the (logical device)
 * the Physical device is just the description of hardware*/

/*PRINT FUNCTIONS*/
void printDevicePropertyResults(VkPhysicalDeviceProperties2 deviceProperties) {
    // Unpack major, minor, and patch numbers from the packed API version integer
    uint32_t major = VK_API_VERSION_MAJOR(deviceProperties.properties.apiVersion);
    uint32_t minor = VK_API_VERSION_MINOR(deviceProperties.properties.apiVersion);
    uint32_t patch = VK_API_VERSION_PATCH(deviceProperties.properties.apiVersion);

    std::println(" Device Name:\t{}", deviceProperties.properties.deviceName);
    // Uses vk_enum_string_helper.h
    std::println(" Device Type:\t{}",
                 string_VkPhysicalDeviceType(deviceProperties.properties.deviceType));
    std::println(" Driver Version: {}", deviceProperties.properties.driverVersion);
    std::println(" API Version:\t{}.{}.{}", major, minor, patch);
    std::println(" Vendor ID:\t{:#x}", deviceProperties.properties.vendorID);
    std::println(" Device ID:\t{:#x}\n", deviceProperties.properties.deviceID);
}

void printPhysicalDevices(VkInstance instance) {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(instance, &device_count, devices.data());
    std::println("\n------| Devices Found |------");
    for (const auto& device : devices) {
        VkPhysicalDeviceProperties2 properties2{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
            .pNext = nullptr};
        vkGetPhysicalDeviceProperties2(device, &properties2);
        std::println("{}", properties2.properties.deviceName);
    }
    std::println("------------------------------\n");
}

/*Physical Device*/
VkPhysicalDevice getPhysicalDevice(VkInstance instance) {
    uint32_t deviceCount = 0;
    VkResult result      = vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    // 1st check if driver function itself failed to load
    if (result != VK_SUCCESS) {
        std::println(stderr,
                     "Error: Failed to enumerate physical devices. VkResult: {}",
                     static_cast<int>(result));
        return VK_NULL_HANDLE;
    }
    if (deviceCount == 0) {
        std::println(stderr, "Error: No Physical devices with vulkan support were found");
        return VK_NULL_HANDLE;
    }

    // Physical Device
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    VkPhysicalDeviceProperties2 deviceProperties{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = nullptr};

    vkGetPhysicalDeviceProperties2(devices[0], &deviceProperties);

    printDevicePropertyResults(deviceProperties);

    return devices[0];
}
