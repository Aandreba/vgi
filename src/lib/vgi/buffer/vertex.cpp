#include "vertex.hpp"

#include <vgi/math.hpp>

namespace vgi {
    vertex_buffer::vertex_buffer(const window& parent, vk::DeviceSize size) {
        std::optional<vk::DeviceSize> byte_size =
                math::check_mul<vk::DeviceSize>(sizeof(vertex), size);
        if (!byte_size) throw vgi_error{"too many vertices"};

        auto [buffer, allocation] = parent.create_buffer(
                vk::BufferCreateInfo{
                        .size = byte_size.value(),
                        .usage = vk::BufferUsageFlagBits::eTransferSrc |
                                 vk::BufferUsageFlagBits::eTransferDst |
                                 vk::BufferUsageFlagBits::eVertexBuffer,
                },
                VmaAllocationCreateInfo{
                        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                });

        this->buffer = buffer;
        this->allocation = allocation;
    }
}  // namespace vgi
