/*! \file */
#pragma once

#include <concepts>
#include <glm/glm.hpp>
#include <span>
#include <type_traits>
#include <vgi/math.hpp>
#include <vgi/pipeline.hpp>
#include <vgi/resource.hpp>
#include <vgi/vulkan.hpp>
#include <vgi/window.hpp>

namespace vgi {
    template<class T>
    concept storage = std::is_trivially_copyable_v<T> && std::is_trivially_destructible_v<T>;

    /// @brief A buffer used to store objects
    template<storage T>
    struct storage_buffer {
        /// @brief Default constructor.
        storage_buffer() = default;

        /// @brief Creates a new buffer
        /// @param parent Window that creates the buffer
        /// @param size Number of objects the buffer can store
        explicit storage_buffer(const window& parent, vk::DeviceSize size = 1) : size(size) {
            auto byte_size = math::check_mul<vk::DeviceSize>(sizeof(T), size);
            if (byte_size) {
                byte_size =
                        math::check_mul<vk::DeviceSize>(*byte_size, window::MAX_FRAMES_IN_FLIGHT);
            }

            if (!byte_size) throw vgi_error{"too many objects"};
            auto [buffer, allocation] = parent.create_buffer(
                    vk::BufferCreateInfo{
                            .size = byte_size.value(),
                            .usage = vk::BufferUsageFlagBits::eTransferSrc |
                                     vk::BufferUsageFlagBits::eTransferDst |
                                     vk::BufferUsageFlagBits::eStorageBuffer,
                    },
                    VmaAllocationCreateInfo{
                            .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
                                     VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
                            .usage = VMA_MEMORY_USAGE_AUTO,
                    });

            this->buffer = buffer;
            this->allocation = allocation;
        }

        /// @brief Move constructor
        /// @param other Object to be moved
        storage_buffer(storage_buffer&& other) noexcept :
            buffer(std::move(other.buffer)),
            allocation(std::exchange(other.allocation, VK_NULL_HANDLE)),
            size(std::exchange(other.size, 0)) {}

        /// @brief Move assignment
        /// @param other Object to be moved
        storage_buffer& operator=(storage_buffer&& other) noexcept {
            if (this == &other) return *this;
            std::destroy_at(this);
            std::construct_at(this, std::move(other));
            return *this;
        }

        /// @brief Updates a descriptor pool's bindings so that they use this buffer
        /// @param parent Window used to create the descriptor pool and the buffer
        /// @param pool Descriptor pool to update
        /// @param binding Slot to which bind the buffer
        void update_descriptors(const window& parent, descriptor_pool& pool,
                                uint32_t binding) const {
            const vk::DeviceSize stride = static_cast<vk::DeviceSize>(this->size) *
                                          static_cast<vk::DeviceSize>(sizeof(T));

            for (uint32_t i = 0; i < pool.size(); ++i) {
                const vk::DescriptorBufferInfo buf_info{
                        .buffer = this->buffer,
                        .offset = stride * static_cast<vk::DeviceSize>(i),
                        .range = stride,
                };

                parent->updateDescriptorSets(
                        vk::WriteDescriptorSet{
                                .dstSet = pool[i],
                                .dstBinding = binding,
                                .descriptorCount = 1,
                                .descriptorType = vk::DescriptorType::eStorageBuffer,
                                .pBufferInfo = &buf_info,
                        },
                        {});
            }
        }

        /// @brief Upload objects to the GPU
        /// @param parent Window used to create the buffer
        /// @param src Elements to be uploaded
        /// @param current_frame Frame for which the buffer information will be updated
        /// @param offset Offset within the buffer where to write copied data
        inline void write(const window& parent, std::span<const T> src, uint32_t current_frame,
                          vk::DeviceSize offset = 0) {
            VGI_ASSERT(offset + src.size() <= this->size);
            auto byte_offset = math::check_mul<vk::DeviceSize>(current_frame, this->size);
            if (byte_offset) byte_offset = math::check_add<vk::DeviceSize>(*byte_offset, offset);
            if (byte_offset) byte_offset = math::check_mul<vk::DeviceSize>(*byte_offset, sizeof(T));
            if (!byte_offset) throw vgi_error{"offset is too large"};

            vk::detail::resultCheck(static_cast<vk::Result>(vmaCopyMemoryToAllocation(
                                            parent, src.data(), this->allocation,
                                            byte_offset.value(), src.size_bytes())),
                                    __FUNCTION__);
        }

        /// @brief Upload object to the GPU
        /// @param parent Window used to create the buffer
        /// @param src Element to be uploaded
        /// @param current_frame Frame for which the buffer information will be updated
        /// @param offset Offset within the buffer where to write copied data
        inline void write(const window& parent, const T& src, uint32_t current_frame,
                          vk::DeviceSize offset = 0) {
            return write(parent, std::span<const T>(std::addressof(src), 1), current_frame, offset);
        }

        /// @brief Destroys the buffer
        /// @param parent Window used to create the buffer
        inline void destroy(const window& parent) && noexcept {
            vmaDestroyBuffer(parent, this->buffer, this->allocation);
        }

        /// @brief Casts to the underlying `vk::Buffer`
        constexpr operator vk::Buffer() const noexcept { return this->buffer; }
        /// @brief Casts to the underlying `VkBuffer`
        inline operator VkBuffer() const noexcept { return this->buffer; }
        /// @brief Casts to the underlying `VmaAllocation`
        constexpr operator VmaAllocation() const noexcept { return this->allocation; }

        storage_buffer(const storage_buffer&) = delete;
        storage_buffer& operator=(const storage_buffer&) = delete;

    private:
        vk::Buffer buffer;
        VmaAllocation allocation = VK_NULL_HANDLE;
        vk::DeviceSize size;
    };

    /// @brief A guard that destroys the storage buffer when dropped.
    template<storage T>
    using storage_buffer_guard = resource_guard<storage_buffer<T>>;
}  // namespace vgi
