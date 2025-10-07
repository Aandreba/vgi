/*! \file */
#pragma once

#include <algorithm>
#include <cstddef>
#include <memory>
#include <new>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "defs.hpp"

namespace vgi {
    template<class T>
    struct unique_span_builder;

    /// A simple host buffer with constant size, usefull as an unresizable 'std::vector'
    template<class T>
    struct unique_span {
        //! @cond Doxygen_Suppress
        using element_type = T;
        using value_type = std::remove_cv_t<T>;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using pointer = T*;
        using const_pointer = const T*;
        using reference = T&;
        using const_reference = const T&;
        using iterator = pointer;
        using const_iterator = const_pointer;
        //! @endcond

        constexpr unique_span() noexcept = default;
        /// @brief Creates a new span that takes ownership of the provided pointer, destroying it's
        /// members and deallocating it when destroyed
        /// @param ptr Pointer to the elements
        /// @param len Number of elements stored
        constexpr unique_span(pointer VGI_RESTRICT ptr, size_t len) noexcept : ptr(ptr), len(len) {}

        // Make the struct non-copyable
        unique_span(const unique_span&) = delete;
        unique_span& operator=(const unique_span&) = delete;

        //! @cond Doxygen_Suppress
        constexpr unique_span(unique_span&& other) noexcept :
            ptr(std::exchange(other.ptr, nullptr)), len(std::exchange(other.len, 0)) {}

        constexpr unique_span& operator=(unique_span&& other) noexcept {
            if (this == &other) [[unlikely]]
                return *this;
            std::destroy_at(this);
            std::construct_at(this, std::move(other));
            return *this;
        }
        //! @endcond

        /// @return The number of elements stored by the span
        constexpr size_type size() const noexcept { return this->ptr ? this->len : 0; }
        /// @brief Checks whether the container is empty
        /// @return `true` if the span is empty, `false` otherwise
        constexpr bool empty() const noexcept { return this->size() == 0; }
        /// @brief Direct access to the underlying contiguous storage
        /// @return Pointer to the underlying element storage
        constexpr const_pointer data() const noexcept { return this->ptr; }
        /// @brief Direct access to the underlying contiguous storage
        /// @return Pointer to the underlying element storage
        constexpr pointer data() noexcept { return this->ptr; }

        /// @brief Access specified element with bounds checking
        /// @param pos Position of the element to return
        /// @return Reference to the requested element
        /// @throws `std::out_of_range` if `pos >= size()`
        constexpr reference at(size_type pos) {
            if (pos >= this->size()) throw std::out_of_range{"index out of bounds"};
            return this->ptr[pos];
        }
        /// @brief Access specified element with bounds checking
        /// @param pos Position of the element to return
        /// @return Reference to the requested element
        /// @throws `std::out_of_range` if `pos >= size()`
        constexpr const_reference at(size_type pos) const {
            if (pos >= this->size()) throw std::out_of_range{"index out of bounds"};
            return this->ptr[pos];
        }
        /// @brief Access specified element
        /// @param pos Position of the element to return
        /// @return Reference to the requested element
        constexpr reference operator[](size_type pos) {
            VGI_ASSERT(pos < this->size());
            return this->ptr[pos];
        }
        /// @brief Access specified element
        /// @param pos Position of the element to return
        /// @return Reference to the requested element
        constexpr const_reference operator[](size_type pos) const {
            VGI_ASSERT(pos < this->size());
            return this->ptr[pos];
        }
        /// @brief Access the first element
        /// @return Reference to the first element
        constexpr reference front() {
            VGI_ASSERT(!this->empty());
            return this->ptr[0];
        }
        /// @brief Access the first element
        /// @return Reference to the first element
        constexpr const_reference front() const {
            VGI_ASSERT(!this->empty());
            return this->ptr[0];
        }
        /// @brief Access the last element
        /// @return Reference to the last element
        constexpr reference back() {
            VGI_ASSERT(!this->empty());
            return this->ptr[this->len - 1];
        }
        /// @brief Access the last element
        /// @return Reference to the last element
        constexpr const_reference back() const {
            VGI_ASSERT(!this->empty());
            return this->ptr[this->len - 1];
        }

        /// @return Iterator to the beginning
        constexpr iterator begin() noexcept { return this->ptr; }
        /// @return Iterator to the beginning
        constexpr const_iterator begin() const noexcept { return this->ptr; }
        /// @return Iterator to the beginning
        constexpr const_iterator cbegin() const noexcept { return this->ptr; }

        /// @return Iterator to the end
        constexpr iterator end() noexcept { return this->ptr + this->size(); }
        /// @return Iterator to the end
        constexpr const_iterator end() const noexcept { return this->ptr + this->size(); }
        /// @return Iterator to the end
        constexpr const_iterator cend() const noexcept { return this->ptr + this->size(); }

        /// @brief Swaps the contents
        /// @param other Container to exchange the contents with
        constexpr void swap(unique_span& other) noexcept {
            std::swap(this->ptr, other.ptr);
            std::swap(this->len, other.len);
        }

        /// @brief Returns a non-owning view to the container's data
        constexpr operator std::span<const T>() const noexcept {
            return std::span<const T>(this->ptr, this->size());
        }
        /// @brief Returns a non-owning view to the container's data
        constexpr operator std::span<T>() noexcept { return std::span<T>(this->ptr, this->size()); }
        /// @brief Checks whether the container is initialized
        constexpr explicit operator bool() const noexcept { return static_cast<bool>(this->ptr); }

        // Destructor
        ~unique_span() noexcept(std::is_nothrow_destructible_v<T>) {
            if (!this->ptr) return;
            if constexpr (!std::is_trivially_destructible_v<T>) {
                std::destroy_n(this->ptr, this->len);
            }
            ::operator delete(this->ptr, sizeof(T) * this->len,
                              static_cast<std::align_val_t>(alignof(T)));
        }

    private:
        pointer VGI_RESTRICT ptr = nullptr;
        size_t len = 0;

        friend struct unique_span_builder<T>;
    };

    /// @brief Allocate a `unique_span` without initializing the values
    /// @tparam T Type of the elements to allocate
    /// @param n Number of elements to allocate
    /// @return A container with `n` uninitialized elements of `T`
    template<class T>
    inline unique_span<T> make_unique_span_for_overwrite(size_t n) {
        VGI_ASSERT(n <= SIZE_MAX / sizeof(T));

        T* VGI_RESTRICT ptr = static_cast<T*>(
                ::operator new(sizeof(T) * n, static_cast<std::align_val_t>(alignof(T))));

        VGI_ASSERT(ptr != nullptr);
        return unique_span<T>{ptr, n};
    }

    /// @brief Helper class to initialize a `unique_span`
    /// @tparam T Element type
    template<class T>
    class unique_span_builder {
        union entry {
            char none[0];
            T some;
            ~entry() {}
        };

        unique_span<entry> entries;
        size_t offset;

    public:
        unique_span_builder(size_t n) :
            entries(make_unique_span_for_overwrite<entry>()), offset(0) {}

        T* next() noexcept {
            VGI_ASSERT(this->entries);
            if (this->offset >= this->entries.size()) return nullptr;
            return std::addressof(this->entries[this->offset++].some);
        }

        void pop_back() noexcept(std::is_nothrow_destructible_v<T>) {
            VGI_ASSERT(this->entries);
            VGI_ASSERT(this->offset > 0);
            std::destroy_at(std::addressof(this->entries[this->offset--].some));
        }

        unique_span<T> build() && {
            VGI_ASSERT(this->entries);
            VGI_ASSERT(this->offset == this->entries.size());
            return unique_span<T>{
                    reinterpret_cast<T * VGI_RESTRICT>(std::exchange(this->entries.ptr, nullptr)),
                    std::exchange(this->entries.len, 0)};
        }

        ~unique_span_builder() noexcept {
            if (!this->entries) return;
            if constexpr (!std::is_trivially_destructible_v<T>) {
                std::destroy_n(reinterpret_cast<T*>(this->entries.data()), this->offset);
            }
        }
    };

    /// @brief Specializes the `std::swap` algorithm for `vgi::unique_span`. Swaps the contents of
    /// `lhs` and `rhs`. Calls `lhs.swap(rhs)`
    /// @param lhs container whose contents to swap
    /// @param rhs container whose contents to swap
    template<class T>
    constexpr void swap(unique_span<T>& lhs,
                        unique_span<T>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
        lhs.swap(rhs);
    }
}  // namespace vgi

using vgi::swap;
