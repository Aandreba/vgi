#include "mesh.hpp"

namespace vgi {
    template<index T>
    mesh<T> mesh<T>::load_cube(const window& parent, vk::CommandBuffer cmdbuf,
                               transfer_buffer& transfer, const glm::vec4& color, size_t offset) {
        constexpr float size = 0.5f;
        const auto v0 = [&](float nx, float ny, float nz) constexpr noexcept {
            return vgi::vertex{{size, size, size}, color, {0.0f, 0.0f}, {nx, ny, nz}};
        };
        const auto v1 = [&](float nx, float ny, float nz) constexpr noexcept {
            return vgi::vertex{{-size, size, size}, color, {1.0f, 0.0f}, {nx, ny, nz}};
        };
        const auto v2 = [&](float nx, float ny, float nz) constexpr noexcept {
            return vgi::vertex{{-size, -size, size}, color, {1.0f, 1.0f}, {nx, ny, nz}};
        };
        const auto v3 = [&](float nx, float ny, float nz) constexpr noexcept {
            return vgi::vertex{{size, -size, size}, color, {0.0f, 1.0f}, {nx, ny, nz}};
        };
        const auto v4 = [&](float nx, float ny, float nz) constexpr noexcept {
            return vgi::vertex{{size, -size, -size}, color, {0.0f, 0.0f}, {nx, ny, nz}};
        };
        const auto v5 = [&](float nx, float ny, float nz) constexpr noexcept {
            return vgi::vertex{{size, size, -size}, color, {1.0f, 0.0f}, {nx, ny, nz}};
        };
        const auto v6 = [&](float nx, float ny, float nz) constexpr noexcept {
            return vgi::vertex{{-size, size, -size}, color, {0.0f, 0.0f}, {nx, ny, nz}};
        };
        const auto v7 = [&](float nx, float ny, float nz) constexpr noexcept {
            return vgi::vertex{{-size, -size, -size}, color, {1.0f, 0.0f}, {nx, ny, nz}};
        };

        const vgi::vertex vertices[] = {// Front face
                                        v0(0.0f, 0.0f, 1.0f), v1(0.0f, 0.0f, 1.0f),
                                        v2(0.0f, 0.0f, 1.0f), v3(0.0f, 0.0f, 1.0f),
                                        // Right face
                                        v0(1.0f, 0.0f, 0.0f), v3(1.0f, 0.0f, 0.0f),
                                        v4(1.0f, 0.0f, 0.0f), v5(1.0f, 0.0f, 0.0f),
                                        // Top face
                                        v0(0.0f, 1.0f, 0.0f), v5(0.0f, 1.0f, 0.0f),
                                        v6(0.0f, 1.0f, 0.0f), v1(0.0f, 1.0f, 0.0f),
                                        // Left face
                                        v1(-1.0f, 0.0f, 0.0f), v6(-1.0f, 0.0f, 0.0f),
                                        v7(-1.0f, 0.0f, 0.0f), v2(-1.0f, 0.0f, 0.0f),
                                        // Bottom face
                                        v7(0.0f, -1.0f, 0.0f), v4(0.0f, -1.0f, 0.0f),
                                        v3(0.0f, -1.0f, 0.0f), v2(0.0f, -1.0f, 0.0f),
                                        // Back face
                                        v4(0.0f, 0.0f, -1.0f), v7(0.0f, 0.0f, -1.0f),
                                        v6(0.0f, 0.0f, -1.0f), v5(0.0f, 0.0f, -1.0f)};

        constexpr T indices[] = {0,  1,  2,  2,  3,  0,  // v0-v1-v2-v3 (front)
                                 4,  5,  6,  6,  7,  4,  // v0-v3-v4-v5 (right)
                                 8,  9,  10, 10, 11, 8,  // v0-v5-v6-v1 (top)
                                 12, 13, 14, 14, 15, 12,  // v1-v6-v7-v2 (left)
                                 16, 17, 18, 18, 19, 16,  // v7-v4-v3-v2 (bottom)
                                 20, 21, 22, 22, 23, 20};  // v4-v7-v6-v5 (back)

        return mesh{parent, cmdbuf, transfer, vertices, indices, offset};
    }

    template mesh<uint16_t> mesh<uint16_t>::load_cube(const window&, vk::CommandBuffer,
                                                      transfer_buffer&, const glm::vec4&, size_t);
    template mesh<uint32_t> mesh<uint32_t>::load_cube(const window&, vk::CommandBuffer,
                                                      transfer_buffer&, const glm::vec4&, size_t);
}  // namespace vgi
