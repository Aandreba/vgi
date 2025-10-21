#include "mesh.hpp"

#include <numbers>
#include <tuple>

namespace vgi {
    template<index T>
    size_t mesh<T>::plane_transfer_size(uint32_t points_x, uint32_t points_y) {
        points_x = (std::max)(points_x, UINT32_C(2));
        points_y = (std::max)(points_y, UINT32_C(2));

        std::optional<uint32_t> raw_point_count = math::check_mul(points_x, points_y);
        if (!raw_point_count) throw vgi_error{"too many vertices"};
        std::optional<T> vertex_count = math::check_cast<T>(*raw_point_count);
        if (!vertex_count) throw vgi_error{"too many vertices"};

        std::optional<uint32_t> index_count = math::check_mul(points_x - 1, points_y - 1);
        if (index_count) index_count = math::check_mul(UINT32_C(6), *index_count);
        if (!index_count) throw vgi_error{"too many indices"};

        return transfer_size(*vertex_count, *index_count);
    }

    template<index T>
    mesh<T> mesh<T>::load_plane(const window& parent, vk::CommandBuffer cmdbuf,
                                transfer_buffer& transfer, uint32_t points_x, uint32_t points_y,
                                const glm::vec4& color, size_t offset) {
        points_x = (std::max)(points_x, UINT32_C(2));
        points_y = (std::max)(points_y, UINT32_C(2));

        std::optional<uint32_t> raw_point_count = math::check_mul(points_x, points_y);
        if (!raw_point_count) throw vgi_error{"too many vertices"};
        std::optional<T> vertex_count = math::check_cast<T>(*raw_point_count);
        if (!vertex_count) throw vgi_error{"too many vertices"};
        std::optional<size_t> vertex_size = math::check_mul<size_t>(*vertex_count, sizeof(vertex));
        if (!vertex_size) throw vgi_error{"too many vertices"};

        std::optional<uint32_t> index_count = math::check_mul(points_x - 1, points_y - 1);
        if (index_count) index_count = math::check_mul(UINT32_C(6), *index_count);
        if (!index_count) throw vgi_error{"too many indices"};
        std::optional<size_t> index_size = math::check_mul<size_t>(*index_count, sizeof(T));
        if (!index_size) throw vgi_error{"too many indices"};

        const std::optional<size_t> start_index_offset = math::check_add(offset, *vertex_size);
        if (!start_index_offset) throw vgi_error{"out of memory"};
        size_t vertex_offset = offset;
        size_t index_offset = *start_index_offset;

        const float step_x = 1.0f / static_cast<float>(points_x - 1);
        const float step_y = 1.0f / static_cast<float>(points_y - 1);

        // Create top points
        for (uint32_t i = 0; i < points_x; ++i) {
            const float fi = static_cast<float>(i);
            vertex_offset = transfer.write_at(vertex{{step_x * fi - 0.5f, 0.5f, 0.0f},
                                                     color,
                                                     {step_x * fi, 0.0f},
                                                     {0.0f, 0.0f, 1.0f}},
                                              vertex_offset);
        }

        // Create remaining points
        for (uint32_t j = 1; j < points_y; ++j) {
            const float fj = static_cast<float>(j);
            const T upper_offset = static_cast<T>((j - 1) * points_x);
            const T lower_offset = static_cast<T>(j * points_x);

            for (uint32_t i = 0; i < points_x - 1; ++i) {
                const float fi = static_cast<float>(i);
                const T top_left = upper_offset + i;
                const T top_right = top_left + 1;
                const T bottom_left = lower_offset + i;
                const T bottom_right = bottom_left + 1;

                vertex_offset =
                        transfer.write_at(vertex{{step_x * fi - 0.5f, 0.5f - step_y * fj, 0.0f},
                                                 color,
                                                 {step_x * fi, step_y * fj},
                                                 {0.0f, 0.0f, 1.0f}},
                                          vertex_offset);

                index_offset = transfer.template write_at<T>(
                        {top_right, top_left, bottom_left, bottom_left, bottom_right, top_right},
                        index_offset);
            }

            // Add rightmost vertex
            const float fi = static_cast<float>(points_x - 1);
            vertex_offset = transfer.write_at(vertex{{step_x * fi - 0.5f, 0.5f - step_y * fj, 0.0f},
                                                     color,
                                                     {step_x * fi, step_y * fj},
                                                     {0.0f, 0.0f, 1.0f}},
                                              vertex_offset);
        }

        mesh result{parent, *vertex_count, *index_count};
        cmdbuf.copyBuffer(transfer, result.vertices, vk::BufferCopy{offset, 0, *vertex_size});
        cmdbuf.copyBuffer(transfer, result.indices,
                          vk::BufferCopy{*start_index_offset, 0, *index_size});

        return result;
    }

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
        if (raw_stacks < 2) throw vgi_error{"spheres require at least 2 stacks"};

        std::optional<T> opt_slices = math::check_cast<T>(raw_slices);
        if (!opt_slices) throw vgi_error{"too many slices"};
        std::optional<T> opt_stacks = math::check_cast<T>(raw_stacks);
        if (!opt_stacks) throw vgi_error{"too many stacks"};
        T slices = *opt_slices;
        T stacks = *opt_stacks;

        std::optional<T> stage13_v = math::check_mul<T>(2, slices);
        if (!stage13_v) throw vgi_error{"too many slices"};
        std::optional<T> stage2_v = math::check_mul<T>(2, *stage13_v);
        if (!stage2_v) throw vgi_error{"too many slices"};
        stage2_v = math::check_mul<T>(stacks - 2, *stage2_v);
        if (!stage2_v) throw vgi_error{"too many stacks"};

        std::optional<T> vertex_count = math::check_add(*stage13_v, *stage2_v);
        if (vertex_count) vertex_count = math::check_add(*vertex_count, *stage13_v);
        if (vertex_count) vertex_count = math::check_add(*vertex_count, const_v);
        if (!vertex_count) throw vgi_error{"too many vertices"};
        return *vertex_count;
    }

    static uint32_t sphere_index_count(uint32_t slices, uint32_t stacks) {
        constexpr uint32_t const_i = 0;
        if (stacks < 2) throw vgi_error{"spheres require at least 2 stacks"};

        std::optional<uint32_t> stage13_i = math::check_mul<uint32_t>(3, slices);
        if (!stage13_i) throw vgi_error{"too many slices"};
        std::optional<uint32_t> stage2_i = math::check_mul<uint32_t>(2, *stage13_i);
        if (!stage2_i) throw vgi_error{"too many slices"};
        stage2_i = math::check_mul<uint32_t>(stacks - 2, *stage2_i);
        if (!stage2_i) throw vgi_error{"too many stacks"};

        std::optional<uint32_t> index_count = math::check_add(*stage13_i, *stage2_i);
        if (index_count) index_count = math::check_add(*index_count, *stage13_i);
        if (index_count) index_count = math::check_add(*index_count, const_i);
        if (!index_count) throw vgi_error{"too many indices"};
        return *index_count;
    }

    template<index T>
    size_t mesh<T>::sphere_transfer_size(uint32_t slices, uint32_t stacks) {
        return transfer_size(sphere_vertex_count<T>(slices, stacks),
                             sphere_index_count(slices, stacks));
    }

    /// Packed sine and cosine data. Since we're going to be accessing sine and cosine for the same
    /// value every time, better have them close in memory so that they can better cached.
    struct alignas(8) sin_cos {
        float sin;
        float cos;
    };

    static std::unique_ptr<sin_cos[]> build_circle_table(size_t size, bool dir) {
        const float angle = (2.0f * std::numbers::pi_v<float>) /
                            (size == 0 ? 1.0f : (static_cast<float>(size) * (dir ? 1.0f : -1.0f)));

        std::optional<size_t> buf_size = math::check_add<size_t>(size, 1);
        if (!buf_size) throw vgi_error{"too many elements"};

        std::unique_ptr<sin_cos[]> buf = std::make_unique_for_overwrite<sin_cos[]>(*buf_size);
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
        T vertex_count = sphere_vertex_count<T>(slices, stacks);
        uint32_t index_count = sphere_index_count(slices, stacks);
        std::optional<size_t> sincos2_size = math::check_add<size_t>(stacks, stacks);
        if (!sincos2_size) throw vgi_error{"too many stacks"};

        std::unique_ptr<sin_cos[]> sincost1 = build_circle_table(slices, false);
        std::unique_ptr<sin_cos[]> sincost2 = build_circle_table(*sincos2_size, true);
        const auto sint1 = [&](size_t i) noexcept -> float { return sincost1[i].sin; };
        const auto cost1 = [&](size_t i) noexcept -> float { return sincost1[i].cos; };
        const auto sint2 = [&](size_t i) noexcept -> float { return sincost2[i].sin; };
        const auto cost2 = [&](size_t i) noexcept -> float { return sincost2[i].cos; };
        const auto point_at = [&](float x, float y, float z) noexcept -> vertex {
            return vertex{{x, y, z}, color, {y - x, z - x}, {x, y, z}};
        };

        std::optional<size_t> index_size = math::check_mul<size_t>(index_count, sizeof(T));
        if (!index_size) throw vgi_error{"too many indices"};
        std::optional<size_t> vertex_size = math::check_mul<size_t>(vertex_count, sizeof(vertex));
        if (!vertex_size) throw vgi_error{"too many vertices"};
        std::optional<size_t> start_index_offset = math::check_add<size_t>(offset, *vertex_size);
        if (!start_index_offset) throw vgi_error{"out of memory"};

        size_t vertex_offset = offset;
        size_t index_offset = *start_index_offset;

        float z0 = 1.0f;
        float z1 = cost2(1);
        float r0 = 0.0f;
        float r1 = sint2(1);
        T index = 0;

        index += 1;
        vertex_offset = transfer.write_at(
                vertex{{0.0f, 0.0f, 1.0f}, color, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}}, vertex_offset);

        for (int64_t j = slices - 1; j >= 0; --j) {
            index_offset = transfer.template write_at<T>({0, index, static_cast<T>(index + 1)},
                                                         index_offset);
            vertex_offset = transfer.template write_at<vertex>(
                    {point_at(cost1(j + 1) * r1, sint1(j + 1) * r1, z1),
                     point_at(cost1(j) * r1, sint1(j) * r1, z1)},
                    vertex_offset);
            index += 2;
        }

        for (uint32_t i = 1; i < stacks - 1; ++i) {
            z0 = z1;
            z1 = cost2(i + 1);
            r0 = r1;
            r1 = sint2(i + 1);

            for (uint32_t j = 0; j < slices; ++j) {
                index_offset = transfer.template write_at<T>(
                        {index, static_cast<T>(index + 1), static_cast<T>(index + 3),
                         static_cast<T>(index + 3), static_cast<T>(index + 2), index},
                        index_offset);
                vertex_offset = transfer.template write_at<vertex>(
                        {
                                point_at(cost1(j) * r1, sint1(j) * r1, z1),
                                point_at(cost1(j) * r0, sint1(j) * r0, z0),
                                point_at(cost1(j + 1) * r1, sint1(j + 1) * r1, z1),
                                point_at(cost1(j + 1) * r0, sint1(j + 1) * r0, z0),
                        },
                        vertex_offset);
                index += 4;
            }
        }

        z0 = z1;
        r0 = r1;

        vertex_offset = transfer.write_at(
                vertex{{0.0f, 0.0f, -1.0f}, color, {0.0f, -1.0f}, {0.0f, 0.0f, -1.0f}},
                vertex_offset);

        const T sub_index = index;
        index += 1;

        for (uint32_t j = 0; j < slices; ++j) {
            index_offset = transfer.template write_at<T>(
                    {sub_index, index, static_cast<T>(index + 1)}, index_offset);
            vertex_offset = transfer.template write_at<vertex>(
                    {point_at(cost1(j) * r0, sint1(j) * r0, z0),
                     point_at(cost1(j + 1) * r0, sint1(j + 1) * r0, z0)},
                    vertex_offset);
            index += 2;
        }

        mesh result{parent, vertex_count, index_count};
        cmdbuf.copyBuffer(transfer, result.vertices, vk::BufferCopy{offset, 0, *vertex_size});
        cmdbuf.copyBuffer(transfer, result.indices,
                          vk::BufferCopy{*start_index_offset, 0, *index_size});

        return result;
    }

    template size_t mesh<uint16_t>::plane_transfer_size(uint32_t, uint32_t);
    template size_t mesh<uint32_t>::plane_transfer_size(uint32_t, uint32_t);

    template size_t mesh<uint16_t>::sphere_transfer_size(uint32_t, uint32_t);
    template size_t mesh<uint32_t>::sphere_transfer_size(uint32_t, uint32_t);

    template mesh<uint16_t> mesh<uint16_t>::load_plane(const window&, vk::CommandBuffer,
                                                       transfer_buffer&, uint32_t, uint32_t,
                                                       const glm::vec4&, size_t);
    template mesh<uint32_t> mesh<uint32_t>::load_plane(const window&, vk::CommandBuffer,
                                                       transfer_buffer&, uint32_t, uint32_t,
                                                       const glm::vec4&, size_t);

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
