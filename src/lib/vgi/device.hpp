/*! \file */
#pragma once

#include <optional>
#include <vulkan/vulkan.hpp>

#include "memory.hpp"

namespace vgi {
    /// @brief Physical device that can be used to perform hardware accelerated
    /// operations.
    struct device {
        device(const device&) = delete;
        device& operator=(const device&) = delete;

        /// @brief Basic properties of the device
        inline const vk::PhysicalDeviceProperties& properties() const noexcept {
            return this->props.get<vk::PhysicalDeviceProperties2>().properties;
        }
        /// @brief Main features of the device
        inline const vk::PhysicalDeviceFeatures& features() const noexcept {
            return this->props.get<vk::PhysicalDeviceFeatures2>().features;
        }

        /// @brief Type of device
        inline vk::PhysicalDeviceType type() const noexcept {
            return this->properties().deviceType;
        }
        /// @brief Device name
        inline std::string name() const noexcept { return this->properties().deviceName; }
        /// @brief Device name, as a UTF-8 encoded string
        inline std::u8string_view name_utf8() const noexcept {
            std::string_view name = this->properties().deviceName;
            return std::u8string_view{reinterpret_cast<const char8_t*>(name.data()), name.size()};
        }

        /// @brief Casts the device into it's underlying `vk::PhysicalDevice`
        inline operator vk::PhysicalDevice() const noexcept { return this->handle; }
        //! @cond Doxygen_Suppress
        inline const vk::PhysicalDevice* operator->() const noexcept { return &this->handle; }
        inline vk::PhysicalDevice* operator->() noexcept { return &this->handle; }
        inline const vk::PhysicalDevice& operator*() const noexcept { return this->handle; }
        inline vk::PhysicalDevice& operator*() noexcept { return this->handle; }
        //! @endcond

        /// @brief Retreive all the devices detected
        /// @return A list of all the devices detected by the host
        static std::span<const device> all();

    private:
        using props_type = vk::StructureChain<
                vk::PhysicalDeviceProperties2, vk::PhysicalDeviceVulkan11Properties,
                vk::PhysicalDeviceVulkan12Properties, vk::PhysicalDeviceVulkan13Properties>;

        using feats_type =
                vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features,
                                   vk::PhysicalDeviceVulkan12Features,
                                   vk::PhysicalDeviceVulkan13Features>;

        using queue_family_type = vk::StructureChain<vk::QueueFamilyProperties2>;

        vk::PhysicalDevice handle;
        props_type props;
        feats_type feats;
        std::vector<queue_family_type> queue_families;

        device(vk::PhysicalDevice handle);

        std::optional<uint32_t> select_queue_family(vk::SurfaceFormatKHR format) const;
    };
}  // namespace vgi
