#pragma once

#include <ranges>
#include <vgi/buffer/index.hpp>
#include <vgi/buffer/transfer.hpp>
#include <vgi/buffer/vertex.hpp>
#include <vgi/cmdbuf.hpp>
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
        /// @param vertices The vertex data to be uploaded
        /// @param indices The index data to be uploaded
        /// @warning If possible, avoid using this method and instead upload multiple meshes
        /// simultaneously.
        static inline mesh upload_and_wait(window& parent, std::span<const vertex> vertices,
                                           std::span<const T> indices) {
            command_buffer cmdbuf{parent};
            auto [mesh, transfer] = upload(parent, cmdbuf, vertices, indices);
            std::move(cmdbuf).submit_and_wait();
            return std::move(mesh);
        }

        /// @brief Computes the size required for a transfer buffer to hold a cube's mesh data
        static size_t cube_transfer_size() { return 24 * sizeof(vertex) + 36 * sizeof(T); }

        /// @brief Computes the size required for a transfer buffer to hold a sphere's mesh data
        /// @param slices Number of slices that form the sphere
        /// @param stacks Number of stacks that form the sphere
        static size_t sphere_transfer_size(uint32_t slices, uint32_t stacks);

        /// @brief Loads a solid cube as a mesh
        /// @param parent Window that will create the mesh
        /// @param cmdbuf Command buffer used to register commands
        /// @param transfer Transfer buffer used to move the data from host to device memory
        /// @param color Color value of the cube's vertices
        /// @param offset The offset from which the data will be written to the transfer buffer
        /// @author Andreas Umbach <marvin@dataway.ch>
        /// @author Enric Marti <enric.marti@uab.cat>
        static mesh load_cube(const window& parent, vk::CommandBuffer cmdbuf,
                              transfer_buffer& transfer, const glm::vec4& color = glm::vec4{1.0f},
                              size_t offset = 0);


        /// @brief Loads a solid sphere as a mesh
        /// @param parent Window that will create the mesh
        /// @param cmdbuf Command buffer used to register commands
        /// @param transfer Transfer buffer used to move the data from host to device memory
        /// @param slices Number of slices that form the sphere
        /// @param stacks Number of stacks that form the sphere
        /// @param color Color value of the cube's vertices
        /// @param offset The offset from which the data will be written to the transfer buffer
        static mesh load_sphere(const window& parent, vk::CommandBuffer cmdbuf,
                                transfer_buffer& transfer, uint32_t slices, uint32_t stacks,
                                const glm::vec4& color = glm::vec4{1.0f}, size_t offset = 0);

        /// @brief Loads a solid cube as a mesh
        /// @param parent Window that will create the mesh
        /// @param cmdbuf Command buffer used to register commands
        /// @param color Color value of the cube's vertices
        /// @param min_size The minimum size of the transfer buffer
        /// @author Andreas Umbach <marvin@dataway.ch>
        /// @author Enric Marti <enric.marti@uab.cat>
        static std::pair<mesh, transfer_buffer_guard> load_cube(
                const window& parent, vk::CommandBuffer cmdbuf,
                const glm::vec4& color = glm::vec4{1.0f}, size_t min_size = 0) {
            vgi::transfer_buffer_guard transfer{parent, (std::max)(cube_transfer_size(), min_size)};
            mesh result = load_cube(parent, cmdbuf, transfer, color, min_size);
            return std::make_pair<mesh, transfer_buffer_guard>(std::move(result),
                                                               std::move(transfer));
        }

        /// @brief Loads a solid cube as a mesh
        /// @param parent Window that will create the mesh
        /// @param cmdbuf Command buffer used to register commands
        /// @param slices Number of slices that form the sphere
        /// @param stacks Number of stacks that form the sphere
        /// @param color Color value of the cube's vertices
        /// @param min_size The minimum size of the transfer buffer
        static std::pair<mesh, transfer_buffer_guard> load_sphere(
                const window& parent, vk::CommandBuffer cmdbuf, uint32_t slices, uint32_t stacks,
                const glm::vec4& color = glm::vec4{1.0f}, size_t min_size = 0) {
            vgi::transfer_buffer_guard transfer{
                    parent, (std::max)(sphere_transfer_size(slices, stacks), min_size)};
            mesh result = load_sphere(parent, cmdbuf, transfer, slices, stacks, color, min_size);
            return std::make_pair<mesh, transfer_buffer_guard>(std::move(result),
                                                               std::move(transfer));
        }

        /// @brief Loads a solid cube as a mesh
        /// @param parent Window that will create the mesh
        /// @param color Color value of the cube's vertices
        /// @warning If possible, avoid using this method and instead upload multiple meshes
        /// simultaneously.
        /// @author Andreas Umbach <marvin@dataway.ch>
        /// @author Enric Marti <enric.marti@uab.cat>
        static mesh load_cube_and_wait(window& parent, const glm::vec4& color = glm::vec4{1.0f}) {
            command_buffer cmdbuf{parent};
            auto [mesh, transfer] = load_cube(parent, cmdbuf, color);
            std::move(cmdbuf).submit_and_wait();
            return std::move(mesh);
        }

        /// @brief Loads a solid cube as a mesh
        /// @param parent Window that will create the mesh
        /// @param slices Number of slices that form the sphere
        /// @param stacks Number of stacks that form the sphere
        /// @param color Color value of the cube's vertices
        /// @warning If possible, avoid using this method and instead upload multiple meshes
        /// simultaneously.
        static mesh load_sphere_and_wait(window& parent, uint32_t slices, uint32_t stacks,
                                         const glm::vec4& color = glm::vec4{1.0f}) {
            command_buffer cmdbuf{parent};
            auto [mesh, transfer] = load_sphere(parent, slices, stacks, cmdbuf, color);
            std::move(cmdbuf).submit_and_wait();
            return std::move(mesh);
        }

        mesh(const mesh&) = delete;
        mesh& operator=(const mesh&) = delete;
    };
}  // namespace vgi
