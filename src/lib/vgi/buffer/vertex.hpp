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
    };

    /// @brief A buffer used to store the vertices of a mesh
    struct vertex_buffer {
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
        constexpr operator VkBuffer() const noexcept { return this->buffer; }
        /// @brief Casts to the underlying `VmaAllocation`
        constexpr operator VmaAllocation() const noexcept { return this->allocation; }

    protected:
        vk::Buffer buffer;
        VmaAllocation allocation = VK_NULL_HANDLE;
    };

    using vertex_buffer_guard = resource_guard<vertex_buffer>;
}  // namespace vgi
