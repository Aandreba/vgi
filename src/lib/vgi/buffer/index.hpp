#pragma once

#include <concepts>
#include <cstdint>
#include <vgi/math.hpp>
#include <vgi/resource.hpp>
#include <vgi/vulkan.hpp>
#include <vgi/window.hpp>

namespace vgi {
    template<class T>
    struct index_traits;

    /// @brief Specializes `vgi::index_traits` for `std::uint16_t`
    template<>
    struct index_traits<uint16_t> {
        /// @brief Specifies the index type that will be passed to
        /// [`vkCmdBindIndexBuffer`](https://registry.khronos.org/vulkan/specs/latest/man/html/vkCmdBindIndexBuffer.html)
        constexpr static inline const vk::IndexType type = vk::IndexType::eUint16;
    };

    /// @brief Specializes `vgi::index_traits` for `std::uint32_t`
    template<>
    struct index_traits<uint32_t> {
        /// @brief Specifies the index type that will be passed to
        /// [`vkCmdBindIndexBuffer`](https://registry.khronos.org/vulkan/specs/latest/man/html/vkCmdBindIndexBuffer.html)
        constexpr static inline const vk::IndexType type = vk::IndexType::eUint32;
    };

    /// @brief Specifies a type can be used as the value of an index buffer
    template<class T>
    concept index = requires {
        { index_traits<T>::type } -> std::same_as<const vk::IndexType&>;
    };

    /// @brief A buffer used to store the indices of a mesh
    template<index T>
    struct index_buffer {
        /// @brief Maximum number of indices a buffer can be made to store
        constexpr static inline const vk::DeviceSize MAX_SIZE = UINT32_MAX / sizeof(T);

        /// @brief Default constructor.
        index_buffer() = default;

        /// @brief Creates an index buffer of the specified size
        /// @param parent Window that creates the buffer
        /// @param size Number of indices the buffer can store
        index_buffer(const window& parent, vk::DeviceSize size) {
            std::optional<vk::DeviceSize> byte_size =
                    math::check_mul<vk::DeviceSize>(sizeof(T), size);
            if (!byte_size) throw std::runtime_error{"too many indices"};

            auto [buffer, allocation] = parent.create_buffer(
                    vk::BufferCreateInfo{
                            .size = byte_size.value(),
                            .usage = vk::BufferUsageFlagBits::eTransferSrc |
                                     vk::BufferUsageFlagBits::eTransferDst |
                                     vk::BufferUsageFlagBits::eIndexBuffer,
                    },
                    VmaAllocationCreateInfo{
                            .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                    });

            this->buffer = buffer;
            this->allocation = allocation;
        }

        index_buffer(const index_buffer&) = delete;
        index_buffer& operator=(const index_buffer&) = delete;

        /// @brief Move constructor operator
        /// @param other Value to be moved
        inline index_buffer(index_buffer&& other) noexcept :
            buffer(std::move(other.buffer)),
            allocation(std::exchange(other.allocation, VK_NULL_HANDLE)) {}

        /// @brief Move assignment operator
        /// @param other Value to be moved
        inline index_buffer& operator=(index_buffer&& other) noexcept {
            if (this == &other) [[unlikely]]
                return *this;
            std::destroy_at(this);
            std::construct_at(this, std::move(other));
            return *this;
        }

        /// @brief Binds the index buffer to a command buffer
        /// @param cmdbuf Command buffer into which the command is recorded.
        /// @param offset Starting offset within buffer used in index buffer address
        /// calculations.
        inline void bind(vk::CommandBuffer cmdbuf, vk::DeviceSize offset = 0) const noexcept {
            VGI_ASSERT(offset <= MAX_SIZE);
            cmdbuf.bindIndexBuffer(this->buffer, offset * sizeof(T), index_traits<T>::type);
        }

        /// @brief Destroys the buffer
        /// @param parent Window used to create the buffer
        inline void destroy(const window& parent) && {
            vmaDestroyBuffer(parent, this->buffer, this->allocation);
        }

        /// @brief Casts to the underlying `vk::Buffer`
        constexpr operator vk::Buffer() const noexcept { return this->buffer; }
        /// @brief Casts to the underlying `VkBuffer`
        inline operator VkBuffer() const noexcept { return this->buffer; }
        /// @brief Casts to the underlying `VmaAllocation`
        constexpr operator VmaAllocation() const noexcept { return this->allocation; }

    private:
        vk::Buffer buffer;
        VmaAllocation allocation = VK_NULL_HANDLE;
    };

    /// @brief A guard that destroys the vertex buffer when dropped.
    template<index T>
    using index_buffer_guard = resource_guard<index_buffer<T>>;
}  // namespace vgi
