#pragma once

#include <initializer_list>
#include <span>

#include "vulkan.hpp"
#include "window.hpp"

namespace vgi {
    /// @brief A pipeline used to execute user code on the acceleration device
    /// @warning This object cannot be directly constructed, for that you must use either
    /// `vgi::graphics_pipeline` or `vgi::compute_pipeline`
    struct pipeline {
        pipeline(const pipeline&) = delete;
        pipeline& operator=(const pipeline&) = delete;

        /// @brief Move constructor
        /// @param other Object to be moved
        pipeline(pipeline&& other) noexcept :
            handle(std::move(other.handle)), pool(std::move(other.pool)),
            set_layout(std::move(other.set_layout)),
            pipeline_layout(std::move(other.pipeline_layout)) {
            for (uint32_t i = 0; i < window::max_frames_in_flight; ++i) {
                this->sets[i] = std::move(other.sets[i]);
            }
        }

        /// @brief Move assignment
        /// @param other Object to be moved
        pipeline& operator=(pipeline&& other) noexcept {
            if (this == &other) [[unlikely]]
                return *this;
            std::destroy_at(this);
            std::construct_at(this, std::move(other));
            return *this;
        }

        /// @brief Casts to the undelying `vk::Pipeline`
        constexpr operator vk::Pipeline() const noexcept { return this->handle; }
        /// @brief Casts to the undelying `VkPipeline`
        inline operator VkPipeline() const noexcept { return this->handle; }

        /// @brief Destroys the pipeline
        /// @param parent Window used to create the pipeline
        void destroy(const window& parent) &&;

        ~pipeline() noexcept;

    protected:
        /// @brief Handle to the underlying `vk::Pipeline`
        vk::Pipeline handle = nullptr;

        /// @brief Creates a new pipeline with the provided bindings
        /// @param parent Window that will create the pipeline
        /// @param bindings Bindings that will be used througout the pipeline
        pipeline(const window& parent,
                 std::span<const vk::DescriptorSetLayoutBinding> bindings = {});

    private:
        vk::DescriptorPool pool;
        vk::DescriptorSetLayout set_layout;
        vk::PipelineLayout pipeline_layout;
        vk::DescriptorSet sets[window::max_frames_in_flight];
    };

    /// @brief A graphics pipeline, mainly used to rasterize images.
    struct graphics_pipeline : public pipeline {
        /// @brief Creates a new graphics pipeline with the provided bindings
        /// @param parent Window that will create the pipeline
        /// @param create_info Information about the properties and capabilities of the pipeline
        /// @param bindings Bindings that will be used througout the pipeline
        graphics_pipeline(const window& parent, const vk::GraphicsPipelineCreateInfo& create_info,
                          std::span<const vk::DescriptorSetLayoutBinding> bindings = {});
    };
}  // namespace vgi
