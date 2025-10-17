/*! \file */
#pragma once

#include <concepts>
#include <cstddef>
#include <glm/glm.hpp>
#include <initializer_list>
#include <span>
#include <type_traits>
#include <vgi/math.hpp>
#include <vgi/resource.hpp>
#include <vgi/vulkan.hpp>
#include <vgi/window.hpp>

namespace vgi {
    /// @brief A buffer used to store the vertices of a mesh
    struct transfer_buffer {
        /// @brief Creates a transfer buffer of the specified size
        /// @param parent Window that creates the buffer
        /// @param byte_size Number of bytes the buffer can store
        transfer_buffer(const window& parent, size_t byte_size);

        /// @brief Creates a new transfer buffer with the provided contents
        /// @param parent Window that creates the buffer
        /// @param src Contents to be written to the buffer
        template<class T>
            requires(std::is_trivially_copy_constructible_v<T> &&
                     std::is_trivially_destructible_v<T>)
        transfer_buffer(const window& parent, std::span<const T> src) :
            transfer_buffer(parent, src.size_bytes()) {
            this->write_at(src, 0);
            this->flush(parent);
        }

        /// @brief Creates a new transfer buffer with the provided contents
        /// @param parent Window that creates the buffer
        /// @param src Contents to be written to the buffer
        template<class T>
            requires(std::is_trivially_copy_constructible_v<T> &&
                     std::is_trivially_destructible_v<T>)
        transfer_buffer(const window& parent, std::initializer_list<T> src) :
            transfer_buffer(parent, std::span<const T>{src.begin(), src.size()}) {}

        transfer_buffer(const transfer_buffer&) = delete;
        transfer_buffer& operator=(const transfer_buffer&) = delete;

        /// @brief Move constructor operator
        /// @param other Value to be moved
        inline transfer_buffer(transfer_buffer&& other) noexcept :
            buffer(std::move(other.buffer)),
            allocation(std::exchange(other.allocation, VK_NULL_HANDLE)),
            bytes(std::exchange(other.bytes, {})) {}

        /// @brief Move assignment operator
        /// @param other Value to be moved
        inline transfer_buffer& operator=(transfer_buffer&& other) noexcept {
            if (this == &other) [[unlikely]]
                return *this;
            std::destroy_at(this);
            std::construct_at(this, std::move(other));
            return *this;
        }

        /// @brief Write to the buffer
        /// @param src Values to write
        /// @param byte_offset Buffer offset, in bytes
        /// @return The offset right after the copied memory, in bytes
        template<class T>
            requires(std::is_trivially_copy_constructible_v<T> &&
                     std::is_trivially_destructible_v<T>)
        inline size_t write_at(std::span<const T> src, size_t byte_offset) {
            return write_at(std::as_bytes(src), byte_offset);
        }

        /// @brief Write to the buffer
        /// @param src Values to write
        /// @param byte_offset Buffer offset, in bytes
        /// @return The offset right after the copied memory, in bytes
        template<class T>
            requires(std::is_trivially_copy_constructible_v<T> &&
                     std::is_trivially_destructible_v<T>)
        inline size_t write_at(std::initializer_list<T> src, size_t byte_offset) {
            return write_at<T>(std::span<const T>{src.begin(), src.size()}, byte_offset);
        }

        /// @brief Write to the buffer
        /// @param src Value to write
        /// @param byte_offset Buffer offset, in bytes
        /// @return The offset right after the copied memory, in bytes
        template<class T>
            requires(std::is_trivially_copy_constructible_v<T> &&
                     std::is_trivially_destructible_v<T>)
        inline size_t write_at(const T& src, size_t byte_offset) {
            return write_at<T>(std::span<const T>{std::addressof(src), 1}, byte_offset);
        }

        /// @brief Write to the buffer
        /// @param src Values to write
        /// @param offset Buffer offset, in units of `T`
        /// @return The offset right after the copied memory, in units of `T`
        template<class T>
            requires(std::is_trivially_copy_constructible_v<T> &&
                     std::is_trivially_destructible_v<T>)
        inline size_t write(std::span<const T> src, size_t offset) {
            std::optional<size_t> byte_offset = math::check_mul(offset, sizeof(T));
            if (!byte_offset) throw std::out_of_range{"tried to write outside memory bounds"};
            size_t new_offset = write_at<T>(src, byte_offset.value());
            VGI_ASSERT(new_offset % sizeof(T) == 0);
            return new_offset / sizeof(T);
        }

        /// @brief Write to the buffer
        /// @param src Values to write
        /// @param offset Buffer offset, in units of `T`
        /// @return The offset right after the copied memory, in units of `T`
        template<class T>
            requires(std::is_trivially_copy_constructible_v<T> &&
                     std::is_trivially_destructible_v<T>)
        inline size_t write(std::initializer_list<T> src, size_t offset) {
            return write<T>(std::span<const T>{src.begin(), src.size()}, offset);
        }

        /// @brief Write to the buffer
        /// @param src Value to write
        /// @param offset Buffer offset, in units of `T`
        /// @return The offset right after the copied memory, in units of `T`
        template<class T>
            requires(std::is_trivially_copy_constructible_v<T> &&
                     std::is_trivially_destructible_v<T>)
        inline size_t write(const T& src, size_t offset) {
            return write<T>(std::span<const T>{std::addressof(src), 1}, offset);
        }

        /// @brief Flush the cache back to device memory
        /// @param parent Window used to create the buffer
        void flush(const window& parent);

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
        /// @brief Casts to the underlying host-accessible memory
        constexpr operator std::span<std::byte>() const noexcept { return this->bytes; }
        /// @brief Provides access to the underlying host memory
        constexpr const std::span<std::byte>* operator->() noexcept { return &this->bytes; }
        /// @brief Provides access to the underlying host memory
        constexpr const std::span<std::byte>& operator*() noexcept { return this->bytes; }

    private:
        vk::Buffer buffer;
        VmaAllocation allocation = VK_NULL_HANDLE;
        std::span<std::byte> bytes;

        size_t write_at(std::span<const std::byte> src, size_t byte_offset);
    };

    /// @brief A guard that destroys the transfer buffer when dropped.
    using transfer_buffer_guard = resource_guard<transfer_buffer>;
}  // namespace vgi
