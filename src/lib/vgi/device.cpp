#include "device.hpp"

#include "log.hpp"

namespace vgi {
    extern vk::Instance instance;
    unique_span<device> all_devices;

    device::device(vk::PhysicalDevice handle) :
        handle(handle),
        props_chain(handle.getProperties2<
                    vk::PhysicalDeviceProperties2, vk::PhysicalDeviceVulkan11Properties,
                    vk::PhysicalDeviceVulkan12Properties, vk::PhysicalDeviceVulkan13Properties>()),
        feats_chain(
                handle.getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features,
                                    vk::PhysicalDeviceVulkan12Features,
                                    vk::PhysicalDeviceVulkan13Features>()),
        queue_family_chains(handle.getQueueFamilyProperties2<queue_family_type>()) {
        this->queue_family_chains.shrink_to_fit();
    }

    std::optional<uint32_t> device::select_queue_family(vk::SurfaceKHR surface,
                                                        vk::SurfaceFormatKHR format,
                                                        bool vsync) const {
        // Check if the desired format is supported by this device-surface combination
        auto formats = this->handle.getSurfaceFormatsKHR(surface);
        if (std::ranges::find(formats, format) == formats.end()) {
            log_warn("Device '{}' does not support the display format {} with colorspace {}",
                     this->name(), vk::to_string(format.format), vk::to_string(format.colorSpace));
            return std::nullopt;
        }

        // Check if disabling vsync is supported by this device-surface combination
        if (!vsync) {
            auto present_modes = this->handle.getSurfacePresentModesKHR(surface);
            if (std::ranges::find(present_modes, vk::PresentModeKHR::eMailbox) ==
                present_modes.end()) {
                log_warn("Device '{}' does not support disabling vsync", this->name());
                return std::nullopt;
            }
        }

        uint32_t i = 0;
        for (const auto& info: this->queue_families()) {
            // If this queue family cannot present to the screen, skip it
            if (!this->handle.getSurfaceSupportKHR(i, surface)) {
                i++;
                continue;
            }

            // Check if this queue family supports both graphics and compute commands
            constexpr vk::QueueFlags flag_mask =
                    vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute;
            if ((info.queueFlags & flag_mask) != flag_mask) {
                i++;
                continue;
            }

            return i;
        }

        return std::nullopt;
    }

    std::span<const device> device::all() {
        if (!all_devices) [[unlikely]] {
            std::vector<vk::PhysicalDevice> logicals = instance.enumeratePhysicalDevices();
            std::vector<device> devices;
            devices.reserve(logicals.size());

            for (vk::PhysicalDevice logical: logicals) {
                device& info = devices.emplace_back(device{logical});
                if (info.props().apiVersion < VK_API_VERSION_1_3) {
                    log_warn("Device '{}' only supports up to Vulkan {}.{}", info.name(),
                             VK_API_VERSION_MAJOR(info.props().apiVersion),
                             VK_API_VERSION_MINOR(info.props().apiVersion));
                    devices.pop_back();
                }
            }

            all_devices = unique_span<device>{std::move(devices)};
        }
        return all_devices;
    }
}  // namespace vgi
