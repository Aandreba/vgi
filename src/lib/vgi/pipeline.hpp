#pragma once

#include <initializer_list>
#include <optional>
#include <span>

#include "pipeline/shader.hpp"
#include "resource.hpp"
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
            for (uint32_t i = 0; i < window::MAX_FRAMES_IN_FLIGHT; ++i) {
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

    protected:
        /// @brief Handle to the underlying `vk::Pipeline`
        vk::Pipeline handle = nullptr;

        /// @brief Creates a new pipeline with the provided bindings
        /// @param parent Window that will create the pipeline
        /// @param bindings Bindings that will be used througout the pipeline
        pipeline(const window& parent,
                 std::span<const vk::DescriptorSetLayoutBinding> bindings = {});

        /// @brief Layout of the pipeline's bindings
        constexpr vk::PipelineLayout layout() const noexcept { return this->pipeline_layout; }

    private:
        vk::DescriptorPool pool;
        vk::DescriptorSetLayout set_layout;
        vk::PipelineLayout pipeline_layout;
        vk::DescriptorSet sets[window::MAX_FRAMES_IN_FLIGHT];
    };

    struct graphics_pipeline_options {
        uint32_t vertex_binding = 0;
        vk::PrimitiveTopology topology = vk::PrimitiveTopology::eTriangleList;
        vk::PolygonMode polygon_mode = vk::PolygonMode::eFill;
        vk::CullModeFlagBits cull_mode = vk::CullModeFlagBits::eNone;
        vk::FrontFace fron_face = vk::FrontFace::eCounterClockwise;
        vk::CompareOp depth_compare_op = vk::CompareOp::eNever;
        vk::ColorComponentFlags color_blend_mask = {};
        std::span<const vk::DescriptorSetLayoutBinding> bindings = {};
    };

    /// @brief A graphics pipeline, mainly used to rasterize images.
    struct graphics_pipeline : public pipeline {
        graphics_pipeline(const window& parent, const shader_stage& vertex,
                          const shader_stage& fragment,
                          const graphics_pipeline_options& options = {});
    };

    using graphics_pipeline_guard = resource_guard<graphics_pipeline>;
}  // namespace vgi
