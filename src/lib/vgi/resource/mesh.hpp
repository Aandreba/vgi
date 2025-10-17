#pragma once

#include <vgi/buffer/index.hpp>
#include <vgi/buffer/vertex.hpp>
#include <vgi/pipeline.hpp>
#include <vgi/resource.hpp>
#include <vgi/window.hpp>

namespace vgi {
    /// @brief A vertex array-based geometry
    template<index T>
    struct mesh : public shared_resource {
        /// @brief Buffer containint the mesh vertices
        vertex_buffer vertices;
        /// @brief Buffer containing the mesh indices
        index_buffer<T> indices;
        /// @brief Number of indices inside `indices`
        uint32_t index_count;

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
            cmdbuf.drawIndexed(this->index_count, 0, 0, 0);
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

        void destroy(const window& parent) && override {
            std::move(this->vertices).destroy(parent);
            std::move(this->indices).destroy(parent);
        }
    };

    /// @brief An instance of `vgi::mesh`
    template<index T>
    struct mesh_instance {
        using mesh_type = mesh<T>;

        res<mesh_type> mesh;
        descriptor_pool desc_pool;

        /// @brief Binds and draws the mesh.
        /// @warning Each mesh instance can only be drawn **once** per each frame.
        /// @sa vgi::mesh_instance::bind
        /// @sa vgi::mesh_instance::draw
        [[nodiscard]] bool draw(vk::CommandBuffer cmdbuf, const graphics_pipeline& pipeline,
                                uint32_t current_frame, uint32_t set_offset = 0,
                                uint32_t vertex_binding = 0) const noexcept {
            res_lock<mesh_type> mesh = this->mesh.lock();
            if (!mesh) return false;

            cmdbuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipeline, set_offset,
                                      this->desc_pool[current_frame], {});
            cmdbuf.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
            mesh->bind(vertex_binding);
            mesh->draw();
            return true;
        }

        void destroy(const window& parent) && { std::move(desc_pool).destroy(parent); }
    };
}  // namespace vgi
