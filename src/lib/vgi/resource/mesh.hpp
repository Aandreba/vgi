#pragma once

#include <ranges>
#include <vgi/buffer/index.hpp>
#include <vgi/buffer/transfer.hpp>
#include <vgi/buffer/vertex.hpp>
#include <vgi/math.hpp>
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

        /// @brief Creates a new mesh and sets up the command and transfer buffers for the upload
        /// @param parent Window that will create the mesh
        /// @param cmdbuf Command buffer used to register commands
        /// @param transfer Transfer buffer used to move the data from host to device memory
        /// @param vertices The vertex data to be uploaded
        /// @param indices The index data to be uploaded
        /// @param offset The offset from which the data will be written to the transfer buffer
        mesh(const window& parent, vk::CommandBuffer cmdbuf, transfer_buffer& transfer,
             std::span<const vertex> vertices, std::span<const T> indices, size_t offset = 0) :
            mesh(parent, vertices.size(), math::check_cast<uint32_t>(indices.size()).value()) {
            offset =
                    transfer.template write_at<vertex>(cmdbuf, vertices, offset, this->vertices, 0);
            transfer.template write_at<T>(cmdbuf, indices, offset, this->indices, 0);
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

        /// @brief Computes the minimum remaining space a transfer buffer must have to transfer the
        /// data to/from the mesh
        /// @param vertex_count The number of vertices the mesh will store
        /// @param index_count The number of indices the mesh will store
        /// @return The minimum remaining space a transfer buffer must have to transfer the data
        /// to/from the mesh
        static inline size_t transfer_size(size_t vertex_count, size_t index_count) {
            if (std::optional<uint32_t> index_count_32 = math::check_cast<uint32_t>(index_count)) {
                return transfer_size(vertex_count, *index_count_32);
            } else {
                throw std::runtime_error{"too many indices"};
            }
        }

        /// @brief Computes the minimum remaining space a transfer buffer must have to transfer the
        /// data to/from the mesh
        /// @param vertex_count The number of vertices the mesh will store
        /// @param index_count The number of indices the mesh will store
        /// @return The minimum remaining space a transfer buffer must have to transfer the data
        /// to/from the mesh
        static inline size_t transfer_size(size_t vertex_count, uint32_t index_count)
            requires(!std::is_same_v<size_t, uint32_t>)
        {
            auto vertex_size = math::check_mul<size_t>(vertex_count, sizeof(vertex));
            if (!vertex_size) throw std::runtime_error{"too many vertices"};
            auto index_size = math::check_mul<size_t>(index_count, sizeof(T));
            if (!index_size) throw std::runtime_error{"too many indices"};
            auto total_size = math::check_add(*vertex_size, *index_size);
            if (!total_size)
                throw std::runtime_error{"too much data to upload to the device at once"};
            return *total_size;
        }

        /// @brief Computes the minimum remaining space a transfer buffer must have to transfer the
        /// data to/from the mesh
        /// @param vertex_count The number of vertices the mesh will store
        /// @param index_count The number of indices the mesh will store
        /// @return The minimum remaining space a transfer buffer must have to transfer the data
        /// to/from the mesh, or `std::nullopt` if a transfer buffer of said size would be too
        /// large.
        static inline std::optional<size_t> try_transfer_size(size_t vertex_count,
                                                              size_t index_count) noexcept {
            if (std::optional<uint32_t> index_count_32 = math::check_cast<uint32_t>(index_count)) {
                return try_transfer_size(vertex_count, *index_count_32);
            } else {
                return std::nullopt;
            }
        }

        /// @brief Computes the minimum remaining space a transfer buffer must have to transfer the
        /// data to/from the mesh
        /// @param vertex_count The number of vertices the mesh will store
        /// @param index_count The number of indices the mesh will store
        /// @return The minimum remaining space a transfer buffer must have to transfer the data
        /// to/from the mesh, or `std::nullopt` if a transfer buffer of said size would be too
        /// large.
        static inline std::optional<size_t> try_transfer_size(size_t vertex_count,
                                                              uint32_t index_count) noexcept
            requires(!std::is_same_v<size_t, uint32_t>)
        {
            auto vertex_size = math::check_mul<size_t>(vertex_count, sizeof(vertex));
            auto index_size = math::check_mul<size_t>(index_count, sizeof(T));
            if (vertex_size && index_size) return math::check_add(*vertex_size, *index_size);
            return std::nullopt;
        }

        /// @brief Creates a new mesh, creates a transfer buffer with the required size and sets up
        /// the command for the upload
        /// @param parent Window that will create the mesh
        /// @param cmdbuf Command buffer used to register commands
        /// @param transfer Transfer buffer used to move the data from host to device memory
        /// @param vertices The vertex data to be uploaded
        /// @param indices The index data to be uploaded
        /// @param min_size The minimum size of the transfer buffer
        static inline std::pair<mesh, transfer_buffer_guard> upload(
                const window& parent, vk::CommandBuffer cmdbuf, std::span<const vertex> vertices,
                std::span<const T> indices, size_t min_size = 0) {
            vgi::transfer_buffer_guard transfer{
                    parent, (std::max)(transfer_size(vertices.size(), indices.size()), min_size)};
            mesh result{parent, cmdbuf, transfer, vertices, indices};
            return std::make_pair<mesh, transfer_buffer_guard>(std::move(result),
                                                               std::move(transfer));
        }

        /// @brief Creates a mesh and uploads the data to the device, waiting for the upload to
        /// complete.
        /// @param parent Window that will create the mesh
        /// @param cmdbuf Command buffer used to register commands
        /// @param transfer Transfer buffer used to move the data from host to device memory
        /// @param vertices The vertex data to be uploaded
        /// @param indices The index data to be uploaded
        /// @param min_size The minimum size of the transfer buffer
        /// @warning If possible, avoid using this method and instead upload multiple meshes
        /// simultaneously.
        static inline mesh upload_and_wait(window& parent, std::span<const vertex> vertices,
                                           std::span<const T> indices) {
            command_buffer cmdbuf{parent};
            auto [mesh, transfer] = upload(parent, cmdbuf, vertices, indices);
            std::move(cmdbuf).submit_and_wait();
            return std::move(mesh);
        }

        mesh(const mesh&) = delete;
        mesh& operator=(const mesh&) = delete;
    };
}  // namespace vgi
