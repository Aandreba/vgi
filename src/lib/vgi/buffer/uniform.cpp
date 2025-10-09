#include "uniform.hpp"

namespace vgi {
    template<>
    uniform_buffer<std::byte>::uniform_buffer(const window& parent, size_t size) {
        auto [buffer, allocation] = parent.create_buffer(
                vk::BufferCreateInfo{
                        .size = size,
                        .usage = vk::BufferUsageFlagBits::eTransferSrc |
                                 vk::BufferUsageFlagBits::eTransferDst |
                                 vk::BufferUsageFlagBits::eUniformBuffer,
                },
                VmaAllocationCreateInfo{
                        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                        .usage = VMA_MEMORY_USAGE_AUTO,
                });

        this->buffer = buffer;
        this->allocation = allocation;
    }
}  // namespace vgi
