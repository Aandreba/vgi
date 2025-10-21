#include "mesh.hpp"

#include <numbers>
#include <tuple>

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

    template<index T>
    static T sphere_vertex_count(uint32_t raw_slices, uint32_t raw_stacks) {
        constexpr T const_v = 2;
        if (raw_stacks < 2) throw std::runtime_error{"spheres require at least 2 stacks"};

        std::optional<T> opt_slices = math::check_cast<T>(raw_slices);
        if (!opt_slices) throw std::runtime_error{"too many slices"};
        std::optional<T> opt_stacks = math::check_cast<T>(raw_stacks);
        if (!opt_stacks) throw std::runtime_error{"too many stacks"};
        T slices = *opt_slices;
        T stacks = *opt_stacks;

        std::optional<T> stage13_v = math::check_mul<T>(2, slices);
        if (!stage13_v) throw std::runtime_error{"too many slices"};
        std::optional<T> stage2_v = math::check_mul<T>(2, *stage13_v);
        if (!stage2_v) throw std::runtime_error{"too many slices"};
        stage2_v = math::check_mul<T>(stacks - 2, *stage2_v);
        if (!stage2_v) throw std::runtime_error{"too many stacks"};

        std::optional<T> vertex_count = math::check_add(*stage13_v, *stage2_v);
        if (vertex_count) vertex_count = math::check_add(*vertex_count, *stage13_v);
        if (vertex_count) vertex_count = math::check_add(*vertex_count, const_v);
        if (!vertex_count) throw std::runtime_error{"too many vertices"};
        return *vertex_count;
    }

    static uint32_t sphere_index_count(uint32_t slices, uint32_t stacks) {
        constexpr uint32_t const_i = 0;
        if (stacks < 2) throw std::runtime_error{"spheres require at least 2 stacks"};

        std::optional<uint32_t> stage13_i = math::check_mul<uint32_t>(3, slices);
        if (!stage13_i) throw std::runtime_error{"too many slices"};
        std::optional<uint32_t> stage2_i = math::check_mul<uint32_t>(2, *stage13_i);
        if (!stage2_i) throw std::runtime_error{"too many slices"};
        stage2_i = math::check_mul<uint32_t>(stacks - 2, *stage2_i);
        if (!stage2_i) throw std::runtime_error{"too many stacks"};

        std::optional<uint32_t> index_count = math::check_add(*stage13_i, *stage2_i);
        if (index_count) index_count = math::check_add(*index_count, *stage13_i);
        if (index_count) index_count = math::check_add(*index_count, const_i);
        if (!index_count) throw std::runtime_error{"too many indices"};
        return *index_count;
    }

    template<index T>
    size_t mesh<T>::sphere_transfer_size(uint32_t slices, uint32_t stacks) {
        return transfer_size(sphere_vertex_count<T>(slices, stacks),
                             sphere_index_count(slices, stacks));
    }

    /// Packed sine and cosine data. Since we're going to be accessing sine and cosine for the same
    /// value every time, better have them close in memory so that they can better cached.
    struct alignas(8) sincost {
        float sin;
        float cos;
    };

    static std::unique_ptr<sincost[]> build_circle_table(size_t size, bool dir) {
        const float angle = (2.0f * std::numbers::pi_v<float>) /
                            (size == 0 ? 1.0f : (static_cast<float>(size) * (dir ? 1.0f : -1.0f)));

        std::optional<size_t> buf_size = math::check_add<size_t>(size, 1);
        if (!buf_size) throw std::runtime_error{"too many elements"};

        std::unique_ptr<sincost[]> buf = std::make_unique_for_overwrite<sincost[]>(*buf_size);
        for (size_t i = 0; i <= size; ++i) {
            const float j = angle * static_cast<float>(i);
#if defined(__GLIBC__) || defined(__BIONIC__)
            ::sincosf(j, &buf[i].sin, &buf[i].cos);
#else
            buf[i] = {std::sin(j), std::cos(j)};
#endif
        }

        return buf;
    }

    template<index T>
    mesh<T> mesh<T>::load_sphere(const window& parent, vk::CommandBuffer cmdbuf,
                                 transfer_buffer& transfer, uint32_t slices, uint32_t stacks,
                                 const glm::vec4& color, size_t offset) {
        size_t vertex_count = sphere_vertex_count(slices, stacks);
        uint32_t index_count = sphere_index_count(slices, stacks);
        std::optional<size_t> sincos2_size = math::check_add<size_t>(stacks, stacks);
        if (!sincos2_size) throw std::runtime_error{"too many stacks"};

        std::unique_ptr<sincost[]> sincost1 = build_circle_table(slices, false);
        std::unique_ptr<sincost[]> sincost2 = build_circle_table(*sincos2_size, true);

        size_t vertex_offset = 0;
        size_t index_offset = 0;

        return {};
    }

    template size_t mesh<uint16_t>::sphere_transfer_size(uint32_t, uint32_t);
    template size_t mesh<uint32_t>::sphere_transfer_size(uint32_t, uint32_t);

    template mesh<uint16_t> mesh<uint16_t>::load_cube(const window&, vk::CommandBuffer,
                                                      transfer_buffer&, const glm::vec4&, size_t);
    template mesh<uint32_t> mesh<uint32_t>::load_cube(const window&, vk::CommandBuffer,
                                                      transfer_buffer&, const glm::vec4&, size_t);

    template mesh<uint16_t> mesh<uint16_t>::load_sphere(const window&, vk::CommandBuffer,
                                                        transfer_buffer&, uint32_t, uint32_t,
                                                        const glm::vec4&, size_t);
    template mesh<uint32_t> mesh<uint32_t>::load_sphere(const window&, vk::CommandBuffer,
                                                        transfer_buffer&, uint32_t, uint32_t,
                                                        const glm::vec4&, size_t);
}  // namespace vgi
