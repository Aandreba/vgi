/*! \file */
#pragma once

#include <optional>
#include <ranges>

#include "memory.hpp"
#include "vulkan.hpp"

namespace vgi {
    /// @brief Physical device that can be used to perform hardware accelerated
    /// operations.
    class device {
        using props_type = vk::StructureChain<
                vk::PhysicalDeviceProperties2, vk::PhysicalDeviceVulkan11Properties,
                vk::PhysicalDeviceVulkan12Properties, vk::PhysicalDeviceVulkan13Properties>;

        using feats_type =
                vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features,
                                   vk::PhysicalDeviceVulkan12Features,
                                   vk::PhysicalDeviceVulkan13Features>;

        using queue_family_type = vk::StructureChain<vk::QueueFamilyProperties2>;

    public:
        device(const device&) = delete;
        device& operator=(const device&) = delete;

        /// @brief Move constructor of `device`
        /// @param other Value to be moved
        device(device&& other) noexcept :
            handle(std::move(other.handle)), props_chain(other.props_chain),
            feats_chain(other.feats_chain),
            queue_family_chains(std::move(other.queue_family_chains)) {}

        /// @brief Move assignment of `device`
        /// @param other Value to be moved
        device& operator=(device&& other) noexcept {
            if (this == &other) [[unlikely]]
                return *this;
            std::destroy_at(this);
            std::construct_at(this, std::move(other));
            return *this;
        }

        /// @brief Basic properties of the device
        inline const vk::PhysicalDeviceProperties& props() const noexcept {
            return this->props_chain.get<vk::PhysicalDeviceProperties2>().properties;
        }
        /// @brief Main features of the device
        inline const vk::PhysicalDeviceFeatures& feats() const noexcept {
            return this->feats_chain.get<vk::PhysicalDeviceFeatures2>().features;
        }

        /// @brief Iterator over all the queue family properties
        /// @return Iterator of `const vk::QueueFamilyProperties&`
        inline auto queue_families() const noexcept {
            return std::views::transform(
                    this->queue_family_chains,
                    [](const queue_family_type& chain) -> const vk::QueueFamilyProperties& {
                        return chain.get<vk::QueueFamilyProperties2>().queueFamilyProperties;
                    });
        }

        /// @brief Checks if the format supports the provided features for the tiling
        /// @param format Format for which to check the features
        /// @param features Features to check support for
        /// @param tiling Tiling for which to check the features
        /// @return `true` if all the features are supported on the specified tiling, `false`
        /// otherwise
        bool is_format_supported(vk::Format format, vk::FormatFeatureFlags features,
                                 vk::ImageTiling tiling = vk::ImageTiling::eOptimal) const noexcept;

        /// @brief Filters the provided range to only return the formats supported by this device
        /// @tparam R Range type
        /// @param candidates The format range to be filtered
        /// @param features Features to check support for
        /// @param tiling Tiling for which to check the features
        /// @return The filtered range
        template<std::ranges::input_range R>
            requires(std::convertible_to<std::ranges::range_value_t<R>, vk::Format> &&
                     std::ranges::view<R>)
        inline auto supported_formats(
                R&& candidates, vk::FormatFeatureFlags features,
                vk::ImageTiling tiling = vk::ImageTiling::eOptimal) const noexcept {
            return std::views::filter(
                    std::move(candidates), [=, this](std::ranges::range_value_t<R> format) {
                        return this->is_format_supported(static_cast<vk::Format>(format), features,
                                                         tiling);
                    });
        }

        /// @brief Type of device
        inline vk::PhysicalDeviceType type() const noexcept { return this->props().deviceType; }
        /// @brief Device name
        inline std::string_view name() const noexcept { return this->props().deviceName; }

        /// @brief Casts the device into it's underlying `vk::PhysicalDevice`
        inline operator vk::PhysicalDevice() const noexcept { return this->handle; }
        /// @brief Casts the device into it's underlying `VkPhysicalDevice`
        inline operator VkPhysicalDevice() const noexcept { return this->handle; }
        //! @cond Doxygen_Suppress
        inline const vk::PhysicalDevice* operator->() const noexcept { return &this->handle; }
        inline vk::PhysicalDevice* operator->() noexcept { return &this->handle; }
        inline const vk::PhysicalDevice& operator*() const noexcept { return this->handle; }
        inline vk::PhysicalDevice& operator*() noexcept { return this->handle; }
        //! @endcond

        /// @brief Retreive all the detected devices
        /// @return A list of all the devices detected by the host
        static std::span<const device> all();

    private:
        vk::PhysicalDevice handle;
        props_type props_chain;
        feats_type feats_chain;
        std::vector<queue_family_type> queue_family_chains;

        device(vk::PhysicalDevice handle);

        bool is_supported() const noexcept;
        std::optional<uint32_t> select_queue_family(vk::SurfaceKHR surface,
                                                    vk::SurfaceFormatKHR format, bool vsync) const;

        friend struct window;
    };
}  // namespace vgi
