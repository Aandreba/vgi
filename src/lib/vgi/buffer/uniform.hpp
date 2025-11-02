/*! \file */
#pragma once

#include <concepts>
#include <glm/glm.hpp>
#include <span>
#include <type_traits>
#include <vgi/math.hpp>
#include <vgi/resource.hpp>
#include <vgi/vulkan.hpp>
#include <vgi/window.hpp>

namespace vgi {
    template<class T>
    concept std140_scalar = same_as_any<T, bool, int32_t, uint32_t, float>;
    template<class T>
    concept uniform = std::is_trivially_copyable_v<T> && std::is_trivially_destructible_v<T>;

    /// @brief A buffer used to store uniform objects
    template<uniform T>
    struct uniform_buffer {
        /// @brief Default constructor.
        uniform_buffer() = default;

        /// @brief Creates a new uniform buffer
        /// @param parent Window that creates the buffer
        /// @param size Number of uniform objects the buffer can store
        explicit uniform_buffer(const window& parent, vk::DeviceSize size = 1) {
            std::optional<vk::DeviceSize> byte_size =
                    math::check_mul<vk::DeviceSize>(sizeof(T), size);
            if (!byte_size) throw vgi_error{"too many uniform objects"};

            auto [buffer, allocation] = parent.create_buffer(
                    vk::BufferCreateInfo{
                            .size = byte_size.value(),
                            .usage = vk::BufferUsageFlagBits::eTransferSrc |
                                     vk::BufferUsageFlagBits::eTransferDst |
                                     vk::BufferUsageFlagBits::eUniformBuffer,
                    },
                    VmaAllocationCreateInfo{
                            .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
                                     VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                            .usage = VMA_MEMORY_USAGE_AUTO,
                    });

            this->buffer = buffer;
            this->allocation = allocation;
        }

        uniform_buffer(const uniform_buffer&) = delete;
        uniform_buffer& operator=(const uniform_buffer&) = delete;

        /// @brief Move constructor
        /// @param other Object to be moved
        uniform_buffer(uniform_buffer&& other) noexcept :
            buffer(std::move(other.buffer)),
            allocation(std::exchange(other.allocation, VK_NULL_HANDLE)) {}

        /// @brief Move assignment
        /// @param other Object to be moved
        uniform_buffer& operator=(uniform_buffer&& other) noexcept {
            if (this == &other) return *this;
            std::destroy_at(this);
            std::construct_at(this, std::move(other));
            return *this;
        }

        /// @brief Upload uniform objects to the GPU
        /// @param parent Window used to create the buffer
        /// @param src Elements to be uploaded
        /// @param offset Offset within the buffer where to write copied data
        inline void write(const window& parent, std::span<const T> src, vk::DeviceSize offset = 0) {
            std::optional<vk::DeviceSize> byte_offset =
                    math::check_mul<vk::DeviceSize>(sizeof(T), offset);
            if (!byte_offset) throw vgi_error{"offset is too large"};

            vk::detail::resultCheck(static_cast<vk::Result>(vmaCopyMemoryToAllocation(
                                            parent, src.data(), this->allocation,
                                            byte_offset.value(), src.size_bytes())),
                                    __FUNCTION__);
        }

        /// @brief Upload uniform object to the GPU
        /// @param parent Window used to create the buffer
        /// @param src Element to be uploaded
        /// @param offset Offset within the buffer where to write copied data
        inline void write(const window& parent, const T& src, vk::DeviceSize offset = 0) {
            return write(parent, std::span<const T>(std::addressof(src), 1), offset);
        }

        /// @brief Download uniform objects from the GPU
        /// @param parent Window used to create the buffer
        /// @param dest Host memory where data will be written
        /// @param offset Offset within the buffer where to read copied data
        inline void read(const window& parent, std::span<T> dest, vk::DeviceSize offset = 0) {
            std::optional<vk::DeviceSize> byte_offset =
                    math::check_mul<vk::DeviceSize>(sizeof(T), offset);
            if (!byte_offset) throw vgi_error{"offset is too large"};

            vk::detail::resultCheck(static_cast<vk::Result>(vmaCopyAllocationToMemory(
                                            parent, this->allocation, byte_offset.value(),
                                            dest.data(), dest.size_bytes())),
                                    __FUNCTION__);
        }

        /// @brief Download uniform objects from the GPU
        /// @param parent Window used to create the buffer
        /// @param src Host memory where data will be written
        /// @param offset Offset within the buffer where to read copied data
        inline void read(const window& parent, T& src, vk::DeviceSize offset = 0) {
            return read(parent, std::span<T>(std::addressof(src), 1), offset);
        }

        /// @brief Download uniform objects from the GPU
        /// @param parent Window used to create the buffer
        /// @param offset Offset within the buffer where to read copied data
        inline T read(const window& parent, vk::DeviceSize offset = 0) {
            std::optional<vk::DeviceSize> byte_offset =
                    math::check_mul<vk::DeviceSize>(sizeof(T), offset);
            if (!byte_offset) throw vgi_error{"offset is too large"};

            std::byte bytes[sizeof(T)];
            vk::detail::resultCheck(
                    static_cast<vk::Result>(vmaCopyAllocationToMemory(
                            parent, this->allocation, byte_offset.value(), bytes, sizeof(T))),
                    __FUNCTION__);

            return std::bit_cast<T>(bytes);
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

    /// @brief A guard that destroys the uniform buffer when dropped.
    template<uniform T>
    using uniform_buffer_guard = resource_guard<uniform_buffer<T>>;

    /// @brief Helper class to properly align the values of a uniform buffer object.
    /// @details Vulkan uniform buffers follow the [`std140` memory
    /// layout](https://ptgmedia.pearsoncmg.com/images/9780321552624/downloads/0321552628_AppL.pdf),
    /// which has some weird alignment rules. To help with programability, this struct can be used
    /// to automatically align the fields of a struct to the alignment and size that would be used
    /// with the std140 layout.
    template<class T>
    struct std140;

    //! @cond Doxygen_Suppress
    template<std140_scalar T>
    class std140<T> {
        using value_type = T;
        using bytes_type = std::array<std::byte, sizeof(T)>;
        bytes_type bytes;

    public:
        constexpr std140() noexcept = default;
        constexpr std140(const value_type& value) noexcept :
            bytes(std::bit_cast<bytes_type>(value)) {}

        constexpr std140& operator=(const value_type& other) noexcept {
            this->bytes = std::bit_cast<bytes_type>(other);
        }

        constexpr operator value_type() const noexcept {
            return std::bit_cast<value_type>(this->bytes);
        }
    };

    template<std140_scalar T>
    class std140<glm::vec<2, T, glm::qualifier::highp>> {
        using value_type = glm::vec<2, T, glm::qualifier::highp>;
        using bytes_type = std::array<std::byte, 2 * sizeof(T)>;
        bytes_type bytes;

    public:
        constexpr std140() noexcept = default;
        constexpr std140(const value_type& value) noexcept :
            bytes(std::bit_cast<bytes_type>(value)) {}

        constexpr std140& operator=(const value_type& other) noexcept {
            this->bytes = std::bit_cast<bytes_type>(other);
        }

        constexpr operator value_type() const noexcept {
            return std::bit_cast<value_type>(this->bytes);
        }
    };

    template<std140_scalar T>
    class std140<glm::vec<3, T, glm::qualifier::highp>> {
        using value_type = glm::vec<3, T, glm::qualifier::highp>;
        using bytes_type = std::array<std::byte, 3 * sizeof(T)>;
        bytes_type bytes;
        std::byte padding[sizeof(T)];

    public:
        constexpr std140() noexcept = default;
        constexpr std140(const value_type& value) noexcept :
            bytes(std::bit_cast<bytes_type>(value)) {}

        constexpr std140& operator=(const value_type& other) noexcept {
            this->bytes = std::bit_cast<bytes_type>(other);
        }

        constexpr operator value_type() const noexcept {
            return std::bit_cast<value_type>(this->bytes);
        }
    };

    template<std140_scalar T>
    class std140<glm::vec<4, T, glm::qualifier::highp>> {
        using value_type = glm::vec<4, T, glm::qualifier::highp>;
        using bytes_type = std::array<std::byte, 4 * sizeof(T)>;
        bytes_type bytes;

    public:
        constexpr std140() noexcept = default;
        constexpr std140(const value_type& value) noexcept :
            bytes(std::bit_cast<bytes_type>(value)) {}

        constexpr std140& operator=(const value_type& other) noexcept {
            this->bytes = std::bit_cast<bytes_type>(other);
            return *this;
        }

        constexpr operator value_type() const noexcept {
            return std::bit_cast<value_type>(this->bytes);
        }
    };

    template<std140_scalar T, size_t N>
    class std140<T[N]> {
        using element_type = T;
        using value_type = std::array<element_type, N>;
        using storage_type = std::array<std140<element_type>, N>;

        struct alignas(sizeof(std140<glm::vec4>)) inner {
            storage_type data;
        };

        union {
            storage_type data;
            std::byte padding[sizeof(inner)];
        };

    public:
        constexpr std140() noexcept = default;
        constexpr std140(const value_type& value) noexcept :
            data(std::bit_cast<storage_type>(value)) {}

        constexpr std140& operator=(const value_type& other) noexcept {
            this->data = std::bit_cast<value_type>(other);
            return *this;
        }

        constexpr const std140<element_type>& operator[](size_t n) const noexcept {
            return this->data[n];
        }
        constexpr std140<element_type>& operator[](size_t n) noexcept { return this->data[n]; }

        constexpr operator value_type() const noexcept {
            return std::bit_cast<storage_type>(this->data);
        }
    };

    template<std140_scalar T, size_t M, size_t N>
    class std140<glm::vec<M, T, glm::qualifier::highp>[N]> {
        using element_type = glm::vec<M, T, glm::qualifier::highp>;
        using value_type = std::array<element_type, N>;
        using storage_type = std::array<std140<element_type>, N>;

        struct alignas(sizeof(std140<glm::vec4>)) inner {
            storage_type data;
        };

        union {
            storage_type data;
            std::byte padding[sizeof(inner)];
        };

    public:
        constexpr std140() noexcept = default;
        constexpr std140(const value_type& value) noexcept {
            for (size_t i = 0; i < N; ++i) {
                this->data[i] = value[i];
            }
        }

        constexpr std140& operator=(const value_type& other) noexcept {
            for (size_t i = 0; i < N; ++i) {
                this->data[i] = other[i];
            }
            return *this;
        }

        constexpr const std140<element_type>& operator[](size_t n) const noexcept {
            return this->data[n];
        }
        constexpr std140<element_type>& operator[](size_t n) noexcept { return this->data[n]; }

        constexpr operator value_type() const noexcept {
            value_type result;
            for (size_t i = 0; i < N; ++i) {
                result[i] = this->data[i];
            }
            return result;
        }
    };

    template<std140_scalar T, size_t C, size_t R>
    class std140<glm::mat<C, R, T, glm::qualifier::highp>> {
        using value_type = glm::mat<C, R, T, glm::qualifier::highp>;
        using storage_type = std140<glm::vec<R, T, glm::qualifier::highp>[C]>;

        storage_type data;

    public:
        constexpr std140() noexcept = default;
        constexpr std140(const value_type& value) noexcept {
            for (size_t i = 0; i < C; ++i) {
                this->data[i] = value[i];
            }
        }

        constexpr std140& operator=(const value_type& other) noexcept {
            for (size_t i = 0; i < C; ++i) {
                this->data[i] = other[i];
            }
            return *this;
        }

        constexpr operator value_type() const noexcept {
            value_type value;
            for (size_t i = 0; i < C; ++i) {
                value[i] = this->data[i];
            }
            return value;
        }
    };

    template<std140_scalar T, size_t C, size_t R, size_t N>
    class std140<glm::mat<C, R, T, glm::qualifier::highp>[N]> {
        using value_type = std::array<glm::mat<C, R, T, glm::qualifier::highp>, N>;
        using storage_type = std140<glm::vec<R, T, glm::qualifier::highp>[N * C]>;
        storage_type data;

    public:
        constexpr std140() noexcept = default;
        constexpr std140(const value_type& value) noexcept {
            for (size_t i = 0; i < N; ++i) {
                for (size_t j = 0; j < C; ++j) {
                    this->data[i * C + j] = value[i][j];
                }
            }
            return *this;
        }

        constexpr operator value_type() const noexcept {
            value_type value;
            for (size_t i = 0; i < N; ++i) {
                for (size_t j = 0; j < C; ++j) {
                    value[i][j] = this->data[i * C + j];
                }
            }
            return value;
        }
    };
    //! @endcond

}  // namespace vgi
