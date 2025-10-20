#include "transfer.hpp"

#include <ranges>
#include <vgi/math.hpp>

#define VMA_CHECK(expr) ::vk::detail::resultCheck(static_cast<::vk::Result>(expr), __FUNCTION__)

namespace vgi {
    transfer_buffer::transfer_buffer(const window& parent, size_t byte_size) {
        VmaAllocationInfo info;
        auto [buffer, allocation] = parent.create_buffer(
                vk::BufferCreateInfo{
                        .size = byte_size,
                        .usage = vk::BufferUsageFlagBits::eTransferSrc |
                                 vk::BufferUsageFlagBits::eTransferDst,
                },
                VmaAllocationCreateInfo{
                        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
                                 VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
                        .usage = VMA_MEMORY_USAGE_AUTO,
                },
                &info);

        VGI_ASSERT(info.pMappedData != nullptr);
        this->buffer = buffer;
        this->allocation = allocation;
        this->bytes = std::span<std::byte>{static_cast<std::byte*>(info.pMappedData), byte_size};
    }

    size_t transfer_buffer::write_at(std::span<const std::byte> src, size_t byte_offset) {
        std::optional<size_t> end = math::check_add(byte_offset, src.size());
        if (!end || *end > this->bytes.size())
            throw std::out_of_range{"tried to write outside memory bounds"};
        std::copy_n(src.data(), src.size(), this->bytes.data() + byte_offset);
        return *end;
    }

    size_t transfer_buffer::write_at(vk::CommandBuffer cmdbuf, std::span<const std::byte> src,
                                     size_t src_offset, vk::Buffer dst, vk::DeviceSize dst_offset) {
        std::optional<size_t> end = math::check_add(src_offset, src.size());
        if (!end || *end > this->bytes.size())
            throw std::out_of_range{"tried to write outside memory bounds"};

        std::copy_n(src.data(), src.size(), this->bytes.data() + src_offset);
        cmdbuf.copyBuffer(
                this->buffer, dst,
                vk::BufferCopy{
                        .srcOffset = src_offset, .dstOffset = dst_offset, .size = src.size()});

        return *end;
    }

    void transfer_buffer::flush(const window& parent) {
        VMA_CHECK(vmaFlushAllocation(parent, this->allocation, 0, VK_WHOLE_SIZE));
    }
}  // namespace vgi
