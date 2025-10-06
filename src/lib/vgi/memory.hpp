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
    // A simple host buffer with constant size, usefull as an unresizable 'std::vector'
    template<class T>
    struct unique_span {
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

        constexpr unique_span() noexcept = default;
        constexpr unique_span(pointer VGI_RESTRICT ptr, size_t len) noexcept : ptr(ptr), len(len) {}

        // Make the struct non-copyable
        unique_span(const unique_span&) = delete;
        unique_span& operator=(const unique_span&) = delete;

        // Make the struct moveable
        constexpr unique_span(unique_span&& other) noexcept :
            ptr(std::exchange(other.ptr, nullptr)), len(std::exchange(other.len, 0)) {}

        constexpr unique_span& operator=(unique_span&& other) noexcept {
            if (this == &other) [[unlikely]]
                return *this;
            std::destroy_at(this);
            std::construct_at(this, std::move(other));
            return *this;
        }

        // Basic methods
        constexpr size_type size() const noexcept { return this->ptr ? this->len : 0; }
        constexpr bool empty() const noexcept { return this->size() == 0; }
        constexpr const_pointer data() const noexcept { return this->ptr; }
        constexpr pointer data() noexcept { return this->ptr; }

        // Element access
        constexpr reference at(size_type pos) {
            if (pos >= this->size()) throw std::out_of_range{"index out of bounds"};
            return this->ptr[pos];
        }
        constexpr const_reference at(size_type pos) const {
            if (pos >= this->size()) throw std::out_of_range{"index out of bounds"};
            return this->ptr[pos];
        }
        constexpr reference operator[](size_type pos) {
            VGI_ASSERT(pos < this->size());
            return this->ptr[pos];
        }
        constexpr const_reference operator[](size_type pos) const {
            VGI_ASSERT(pos < this->size());
            return this->ptr[pos];
        }
        constexpr reference front() {
            VGI_ASSERT(!this->empty());
            return this->ptr[0];
        }
        constexpr const_reference front() const {
            VGI_ASSERT(!this->empty());
            return this->ptr[0];
        }
        constexpr reference back() {
            VGI_ASSERT(!this->empty());
            return this->ptr[this->len - 1];
        }
        constexpr const_reference back() const {
            VGI_ASSERT(!this->empty());
            return this->ptr[this->len - 1];
        }

        // Iterator methods
        constexpr iterator begin() noexcept { return this->ptr; }
        constexpr const_iterator begin() const noexcept { return this->ptr; }
        constexpr const_iterator cbegin() const noexcept { return this->ptr; }

        constexpr iterator end() noexcept { return this->ptr + this->size(); }
        constexpr const_iterator end() const noexcept { return this->ptr + this->size(); }
        constexpr const_iterator cend() const noexcept { return this->ptr + this->size(); }

        // Misc.
        constexpr void swap(unique_span& other) noexcept {
            std::swap(this->ptr, other.ptr);
            std::swap(this->len, other.len);
        }

        // Conversions
        constexpr operator std::span<const T>() const noexcept {
            return std::span<const T>(this->ptr, this->size());
        }
        constexpr operator std::span<T>() noexcept { return std::span<T>(this->ptr, this->size()); }

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
    };

    template<class T>
    constexpr void swap(unique_span<T>& lhs,
                        unique_span<T>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
        lhs.swap(rhs);
    }

    // Allocate a `unique_span` of `T` without initializing the values
    template<class T>
    inline unique_span<T> make_unique_span_for_overwrite(size_t n) {
        VGI_ASSERT(n <= SIZE_MAX / sizeof(T));

        T* VGI_RESTRICT ptr = static_cast<T*>(
                ::operator new(sizeof(T) * n, static_cast<std::align_val_t>(alignof(T))));

        VGI_ASSERT(ptr != nullptr);
        return unique_span<T>{ptr, n};
    }
}  // namespace vgi

using vgi::swap;
