#pragma once

#include <concepts>

namespace vgi {
    struct window;
    struct frame;
    struct shared_resource;
    template<std::derived_from<shared_resource> R>
    struct res_lock;
    template<std::derived_from<shared_resource> R>
    struct res;
}  // namespace vgi
