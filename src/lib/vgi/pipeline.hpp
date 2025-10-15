#pragma once

#include <initializer_list>
#include <optional>
#include <span>

#include "memory.hpp"
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
            handle(std::move(other.handle)), set_layout(std::move(other.set_layout)),
            layout(std::move(other.layout)), pool_sizes(std::move(other.pool_sizes)) {}

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
        /// @brief Casts to the undelying `vk::PipelineLayout`
        constexpr operator vk::PipelineLayout() const noexcept { return this->layout; }
        /// @brief Casts to the undelying `VkPipelineLayout`
        inline operator VkPipelineLayout() const noexcept { return this->layout; }
        /// @brief Casts to the undelying `vk::DescriptorSetLayout`
        constexpr operator vk::DescriptorSetLayout() const noexcept { return this->set_layout; }
        /// @brief Casts to the undelying `VkDescriptorSetLayout`
        inline operator VkDescriptorSetLayout() const noexcept { return this->set_layout; }

        /// @brief Destroys the pipeline
        /// @param parent Window used to create the pipeline
        void destroy(const window& parent) &&;

    protected:
        /// @brief Handle to the underlying `vk::Pipeline`
        vk::Pipeline handle = nullptr;

        /// @brief Default constructor
        pipeline() = default;

        /// @brief Creates a new pipeline with the provided bindings
        /// @param parent Window that will create the pipeline
        /// @param bindings Bindings that will be used througout the pipeline
        pipeline(const window& parent,
                 std::span<const vk::DescriptorSetLayoutBinding> bindings = {});

    private:
        vk::DescriptorSetLayout set_layout;
        vk::PipelineLayout layout;
        unique_span<vk::DescriptorPoolSize> pool_sizes;

        friend struct descriptor_pool;
    };

    /// @brief A pool of descriptor sets
    class descriptor_pool {
        using sets_type = std::array<vk::DescriptorSet, window::MAX_FRAMES_IN_FLIGHT>;

    public:
        /// @brief Default constructor
        descriptor_pool() = default;

        /// @brief Creates a new descriptor pool
        /// @param parent Window that will create the descriptor pool
        /// @param pipeline Pipeline to which the descriptor pool will be attached
        descriptor_pool(const window& parent, const pipeline& pipeline);

        /// @brief Move constructor
        /// @param other Object to be moved
        descriptor_pool(descriptor_pool&& other) noexcept :
            pool(std::move(other.pool)), sets(std::move(other.sets)) {}

        /// @brief Move assignment
        /// @param other Object to be moved
        descriptor_pool& operator=(descriptor_pool&& other) noexcept {
            if (this == &other) [[unlikely]]
                return *this;
            std::destroy_at(this);
            std::construct_at(this, std::move(other));
            return *this;
        }

        /// @brief Number of elements in the container
        consteval size_t size() const noexcept { return window::MAX_FRAMES_IN_FLIGHT; }
        /// @brief Iterator to the first decriptor set
        constexpr sets_type::const_iterator begin() const noexcept { return this->sets.cbegin(); }
        /// @brief Iterator past the last decriptor set
        constexpr sets_type::const_iterator end() const noexcept { return this->sets.cend(); }

        /// @brief Access specified descriptor set
        /// @param n Index of the descriptor set
        /// @return A reference to a `vk::DescriptorSet`
        const vk::DescriptorSet& operator[](uint32_t n) const noexcept {
            VGI_ASSERT(n < this->sets.size());
            return this->sets[n];
        }

        /// @brief Casts to the undelying `vk::DescriptorPool`
        constexpr operator vk::DescriptorPool() const noexcept { return this->pool; }
        /// @brief Casts to the undelying `VkDescriptorPool`
        inline operator VkDescriptorPool() const noexcept { return this->pool; }

        /// @brief Destroys the descriptor pool
        /// @param parent Window used to create the descriptor pool
        void destroy(const window& parent) &&;

    private:
        vk::DescriptorPool pool;
        sets_type sets;
    };

    /// @brief Information used to create a graphics pipeline
    struct graphics_pipeline_options {
        /// @brief Binding where the vertex information is located
        uint32_t vertex_binding = 0;
        /// @brief The topology of the vertex data
        vk::PrimitiveTopology topology = vk::PrimitiveTopology::eTriangleList;
        /// @brief The triangle rendering mode.
        vk::PolygonMode polygon_mode = vk::PolygonMode::eFill;
        /// @brief The triangle facing direction used for primitive culling
        vk::CullModeFlagBits cull_mode = vk::CullModeFlagBits::eNone;
        /// @brief Specifies the front-facing triangle orientation to be used for culling.
        vk::FrontFace fron_face = vk::FrontFace::eCounterClockwise;
        /// @brief Specifies the comparison operator to use in the depth testing
        vk::CompareOp depth_compare_op = vk::CompareOp::eNever;
        /// @brief Enables/Disables color blending
        bool color_blending = true;
        /// @brief The bindings used throughout the pipeline
        std::span<const vk::DescriptorSetLayoutBinding> bindings = {};
    };

    /// @brief A graphics pipeline, mainly used to rasterize images.
    struct graphics_pipeline : public pipeline {
        /// @brief Default constructor.
        graphics_pipeline() = default;

        /// @brief Creates a graphics pipeline
        /// @param parent Window that will create the pipeline
        /// @param vertex Vertex shader
        /// @param fragment Fragment shader
        /// @param options Options used to create the pipeline
        graphics_pipeline(const window& parent, const shader_stage& vertex,
                          const shader_stage& fragment,
                          const graphics_pipeline_options& options = {});
    };

    using graphics_pipeline_guard = resource_guard<graphics_pipeline>;
    using descriptor_pool_guard = resource_guard<descriptor_pool>;
}  // namespace vgi
