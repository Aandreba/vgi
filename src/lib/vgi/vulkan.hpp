#pragma once

#ifdef __APPLE__
#define VK_ENABLE_BETA_EXTENSIONS
#endif
#include <vulkan/vulkan.hpp>
#define VMA_VULKAN_VERSION 1003000
#include <vma/vk_mem_alloc.h>
