#pragma once

#include <vgi/buffer/index.hpp>
#include <vgi/buffer/vertex.hpp>
#include <vgi/pipeline.hpp>
#include <vgi/resource.hpp>
#include <vgi/window.hpp>

namespace vgi {
    /// @brief A vertex array-based geometry
    template<index T>
    struct mesh {
        /// @brief Buffer containint the mesh vertices
        vertex_buffer vertices;
        /// @brief Buffer containing the mesh indices
        index_buffer<T> indices;
        /// @brief Number of indices inside `indices`
        uint32_t index_count = 0;

        /// @brief Default constructor
        mesh() noexcept = default;

        /// @brief Creates a new mesh with the specified capacity
        /// @param parent Window that will create the mesh
        /// @param vertex_count Number of vertices that will fit inside the mesh
        /// @param index_count Number of indices that will fit inside the mesh
        mesh(const window& parent, vk::DeviceSize vertex_count, uint32_t index_count) :
            vertices(parent, vertex_count), indices(parent, index_count), index_count(index_count) {
        }

        /// @brief Move constructor
        /// @param other Object to move
        mesh(mesh&& other) noexcept :
            vertices(std::move(other.vertices)), indices(std::move(other.indices)),
            index_count(std::exchange(other.index_count, 0)) {}

        /// @brief Move assignment
        /// @param other Object to move
        mesh& operator=(mesh&& other) noexcept {
            if (this == &other) return *this;
            std::destroy_at(this);
            std::construct_at(this, std::move(other));
            return *this;
        }

        /// @brief Binds both the vertex and index buffers
        /// @param cmdbuf Command buffer into which the command is recorded.
        /// @param vertex_binding Index of the vertex input binding whose state is updated by the
        /// command
        /// @param vertex_offset Starting offset within the vertex buffer used in vertex buffer
        /// address calculations
        /// @param index_offset Starting offset within the index buffer used in index buffer address
        /// calculations
        void bind(vk::CommandBuffer cmdbuf, uint32_t vertex_binding = 0) const noexcept {
            this->vertices.bind(cmdbuf, vertex_binding);
            this->indices.bind(cmdbuf);
        }

        /// @brief Draws the mesh using the bounded properties of the command buffer
        /// @param cmdbuf Command buffer into which the command is recorded.
        /// @param instance_count Number of instances to draw
        /// @warning This method assumes that this mesh is the one currently bound. Call `bind` or
        /// `bind_and_draw` before calling this.
        void draw(vk::CommandBuffer cmdbuf, uint32_t instance_count = 1) const noexcept {
            cmdbuf.drawIndexed(this->index_count, instance_count, 0, 0, 0);
        }

        /// @brief Binds and draws the mesh.
        /// @details This is equivalent to calling `bind` and `draw` in sequence
        /// @sa vgi::mesh::bind
        /// @sa vgi::mesh::draw
        void bind_and_draw(vk::CommandBuffer cmdbuf, uint32_t instance_count = 1,
                           uint32_t vertex_binding = 0) const noexcept {
            this->bind(cmdbuf, vertex_binding);
            this->draw(cmdbuf, instance_count);
        }

        /// @brief Destroys the resource
        /// @param parent Window that created the resource
        void destroy(const window& parent) && {
            std::move(this->vertices).destroy(parent);
            std::move(this->indices).destroy(parent);
        }

        mesh(const mesh&) = delete;
        mesh& operator=(const mesh&) = delete;
    };
}  // namespace vgi
