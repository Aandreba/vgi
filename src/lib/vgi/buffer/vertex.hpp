/*! \file */
#pragma once

#include <concepts>
#include <glm/glm.hpp>
#include <type_traits>
#include <vgi/resource.hpp>
#include <vgi/vulkan.hpp>
#include <vgi/window.hpp>

namespace vgi {
    /// @brief Represents a meshes' vertex on device memory
    struct vertex {
        /// @brief Position
        glm::vec3 origin;
        /// @brief RGBA color
        glm::vec4 color;
        /// @brief Texture coordinates
        glm::vec2 tex;
        /// @brief Normal vector
        glm::vec3 normal;

        //! @cond Doxygen_Supress
        constexpr static vk::VertexInputBindingDescription input_binding(
                uint32_t binding = 0,
                vk::VertexInputRate input_rate = vk::VertexInputRate::eVertex) noexcept {
            return vk::VertexInputBindingDescription{
                    .binding = binding,
                    .stride = sizeof(vertex),
                    .inputRate = input_rate,
            };
        }

        constexpr static std::array<vk::VertexInputAttributeDescription, 4> input_attributes(
                uint32_t binding = 0) noexcept {
            return {{{
                             .location = 0,
                             .binding = binding,
                             .format = vk::Format::eR32G32B32Sfloat,
                             .offset = offsetof(vertex, origin),
                     },
                     {
                             .location = 1,
                             .binding = binding,
                             .format = vk::Format::eR32G32B32A32Sfloat,
                             .offset = offsetof(vertex, color),
                     },
                     {
                             .location = 2,
                             .binding = binding,
                             .format = vk::Format::eR32G32Sfloat,
                             .offset = offsetof(vertex, tex),
                     },
                     {
                             .location = 3,
                             .binding = binding,
                             .format = vk::Format::eR32G32B32Sfloat,
                             .offset = offsetof(vertex, normal),
                     }}};
        }
    };
    //! @endcond

    /// @brief A buffer used to store the vertices of a mesh
    struct vertex_buffer {
        /// @brief Default constructor.
        vertex_buffer() = default;
        /// @brief Creates a vertex buffer of the specified size
        /// @param parent Window that creates the buffer
        /// @param size Number of vertices the buffer can store
        vertex_buffer(const window& parent, vk::DeviceSize size);
        vertex_buffer(const vertex_buffer&) = delete;
        vertex_buffer& operator=(const vertex_buffer&) = delete;

        /// @brief Move constructor operator
        /// @param other Value to be moved
        inline vertex_buffer(vertex_buffer&& other) noexcept :
            buffer(std::move(other.buffer)),
            allocation(std::exchange(other.allocation, VK_NULL_HANDLE)) {}

        /// @brief Move assignment operator
        /// @param other Value to be moved
        inline vertex_buffer& operator=(vertex_buffer&& other) noexcept {
            if (this == &other) [[unlikely]]
                return *this;
            std::destroy_at(this);
            std::construct_at(this, std::move(other));
            return *this;
        }

        /// @brief Destroys the buffer
        /// @param parent Window used to create the buffer
        inline void destroy(const window& parent) && {
            vmaDestroyBuffer(parent, this->buffer, this->allocation);
        }

        /// @brief Casts to the underlying `vk::Buffer`
        constexpr operator vk::Buffer() const noexcept { return this->buffer; }
        /// @brief Casts to the underlying `VkBuffer`
        inline operator VkBuffer() const noexcept { return this->buffer; }
        /// @brief Casts to the underlying `VmaAllocation`
        constexpr operator VmaAllocation() const noexcept { return this->allocation; }

    private:
        vk::Buffer buffer;
        VmaAllocation allocation = VK_NULL_HANDLE;
    };

    /// @brief A guard that destroys the vertex buffer when dropped.
    using vertex_buffer_guard = resource_guard<vertex_buffer>;
}  // namespace vgi
