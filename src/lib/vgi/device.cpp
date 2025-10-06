#include "device.hpp"

namespace vgi {
    std::vector<device> all_devices;

    device::device(vk::PhysicalDevice handle) :
        handle(handle),
        props(handle.getProperties2<
                vk::PhysicalDeviceProperties2, vk::PhysicalDeviceVulkan11Properties,
                vk::PhysicalDeviceVulkan12Properties, vk::PhysicalDeviceVulkan13Properties,
                VULKAN_HPP_DEFAULT_DISPATCHER_TYPE>(VULKAN_HPP_DEFAULT_DISPATCHER)),
        feats(handle.getFeatures2(VULKAN_HPP_DEFAULT_DISPATCHER)),
        queue_families(handle.getQueueFamilyProperties2<queue_family_type>()) {
        this->queue_families.shrink_to_fit();
    }

    std::span<const device> device::all() {
        if (!all_devices.empty()) [[likely]] {
            return all_devices;
        }

        // TODO
        return {};
    }
}  // namespace vgi
