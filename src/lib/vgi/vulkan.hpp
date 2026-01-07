#pragma once

#define VGI_VMA_CHECK(expr) ::vk::detail::resultCheck(static_cast<::vk::Result>(expr), __FUNCTION__)

#ifdef __APPLE__
#define VK_ENABLE_BETA_EXTENSIONS
#endif
#include <vulkan/vulkan.hpp>
#define VMA_VULKAN_VERSION 1003000
#if __has_include(<vma/vk_mem_alloc.h>)
#include <vma/vk_mem_alloc.h>
#else
#include <vk_mem_alloc.h>
#endif
