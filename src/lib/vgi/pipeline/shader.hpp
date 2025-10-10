#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <variant>
#include <vgi/vulkan.hpp>
#include <vgi/window.hpp>

namespace vgi {
    struct shader_module {
        shader_module(const window& parent, std::span<const uint32_t> code);
        shader_module(const window& parent, const std::filesystem::path::value_type* path);
        shader_module(const window& parent, const std::filesystem::path& path) :
            shader_module(parent, path.c_str()) {}

        shader_module(const shader_module&) = delete;
        shader_module& operator=(const shader_module&) = delete;

        shader_module(shader_module&& other) noexcept :
            parent(other.parent), handle(std::move(other.handle)) {}

        shader_module& operator=(shader_module&& other) noexcept {
            if (this == &other) [[unlikely]]
                return *this;
            std::destroy_at(this);
            std::construct_at(this, std::move(other));
            return *this;
        }

        inline vk::PipelineShaderStageCreateInfo stage_info(
                vk::ShaderStageFlagBits stage,
                const char8_t* entrypoint = u8"main") const noexcept {
            return vk::PipelineShaderStageCreateInfo{
                    .stage = stage,
                    .module = this->handle,
                    .pName = reinterpret_cast<const char*>(entrypoint),
            };
        }

        constexpr operator vk::ShaderModule() const noexcept { return this->handle; };
        inline operator VkShaderModule() const noexcept { return this->handle; };

        ~shader_module() noexcept;

    private:
        const window& parent;
        vk::ShaderModule handle;
    };
}  // namespace vgi
