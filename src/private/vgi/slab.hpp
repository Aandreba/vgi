#pragma once

#include <cstddef>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>
#include <vgi/defs.hpp>

namespace vgi::priv {
    template<class T>
    class slab {
        using entry_type = std::variant<T, size_t>;
        std::vector<entry_type> entries;
        size_t next = 0;

    public:
        template<class... Args>
            requires(std::is_constructible_v<T, Args...>)
        inline size_t emplace(Args&&... args) {
            const size_t key = this->next;
            if (key == this->entries.size()) {
                this->entries.emplace_back(std::in_place_index<0>, std::forward<Args>(args)...);
                this->next++;
            } else {
                VGI_ASSERT(this->entries[key].index() == 1);
                const size_t next = std::get<1>(this->entries[key]);
                this->entries[key].template emplace<0>(std::forward<Args>(args)...);
                this->next = next;
            }
            return key;
        }

        inline const T& at(size_t key) const {
            if (key >= this->entries.size()) throw std::out_of_range{"invalid slab key"};
            return std::get<0>(this->entries[key]);
        }

        inline T& at(size_t key) {
            if (key >= this->entries.size()) throw std::out_of_range{"invalid slab key"};
            return std::get<0>(this->entries[key]);
        }

        inline const T& operator[](size_t key) const {
            VGI_ASSERT(key < this->entries.size() && this->entries[key].index() == 0);
            const T* ptr = std::get_if<0>(std::addressof(this->entries[key]));
            [[assume(ptr != nullptr)]];
            return *ptr;
        }

        inline T& operator[](size_t key) {
            VGI_ASSERT(key < this->entries.size() && this->entries[key].index() == 0);
            T* ptr = std::get_if<0>(std::addressof(this->entries[key]));
            [[assume(ptr != nullptr)]];
            return *ptr;
        }

        [[nodiscard]] inline bool try_remove(size_t key) noexcept(
                std::is_nothrow_destructible_v<T>) {
            if (key >= this->entries.size()) return false;
            if (this->entries[key].index() != 0) return false;
            this->entries[key].template emplace<1>(this->next);
            this->next = key;
            return true;
        }

        inline void remove(size_t key) noexcept(std::is_nothrow_destructible_v<T>) {
            bool removed = try_remove(key);
            VGI_ASSERT(removed);
        }

        inline auto keys() noexcept {
            return std::views::iota(static_cast<size_t>(0), this->entries.size()) |
                   std::views::filter([&](size_t i) { return this->entries[i].index() == 0; });
        }

        inline auto values() noexcept {
            return this->entries |
                   std::views::filter([](const entry_type& entry) { return entry.index() == 0; }) |
                   std::views::transform([](entry_type& entry) -> T& {
                       // This can be better optimized by the compiler than `std::get`
                       T* ptr = std::get_if<0>(std::addressof(entry));
                       [[assume(ptr != nullptr)]];
                       return *ptr;
                   });
        }

        inline auto values() const noexcept {
            return this->entries |
                   std::views::filter([](const entry_type& entry) { return entry.index() == 0; }) |
                   std::views::transform([](const entry_type& entry) -> const T& {
                       // This can be better optimized by the compiler than `std::get`
                       const T* ptr = std::get_if<0>(std::addressof(entry));
                       [[assume(ptr != nullptr)]];
                       return *ptr;
                   });
        }

        inline auto iter() noexcept {
            return std::views::iota(static_cast<size_t>(0), this->entries.size()) |
                   std::views::filter([&](size_t i) { return this->entries[i].index() == 0; }) |
                   std::views::transform([&](size_t i) {
                       // This can be better optimized by the compiler than `std::get`
                       T* ptr = std::get_if<0>(std::addressof(this->entries[i]));
                       [[assume(ptr != nullptr)]];
                       return std::pair<size_t, T&>(i, *ptr);
                   });
        }

        inline auto iter() const noexcept {
            return std::views::iota(static_cast<size_t>(0), this->entries.size()) |
                   std::views::filter([&](size_t i) { return this->entries[i].index() == 0; }) |
                   std::views::transform([&](size_t i) {
                       // This can be better optimized by the compiler than `std::get`
                       const T* ptr = std::get_if<0>(std::addressof(this->entries[i]));
                       [[assume(ptr != nullptr)]];
                       return std::pair<size_t, const T&>(i, *ptr);
                   });
        }
    };
}  // namespace vgi::priv
