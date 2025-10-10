#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <variant>
#include <vgi/vulkan.hpp>
#include <vgi/window.hpp>

namespace vgi {
    /// @brief Compiled shader code, for use in pipeline stage creation
    struct shader_module {
        /// @brief Creates a new shader module from raw SPIR-V code
        /// @param parent Window that will create the shader module
        /// @param code SPIR-V code
        shader_module(const window& parent, std::span<const uint32_t> code);

        /// @brief Creates a new shader module from a file
        /// @param parent Window that will create the shader module
        /// @param path Path to the file with the shader's source
        shader_module(const window& parent, const std::filesystem::path::value_type* path);

        /// @brief Creates a new shader module from a file
        /// @param parent Window that will create the shader module
        /// @param path Path to the file with the shader's source
        shader_module(const window& parent, const std::filesystem::path& path) :
            shader_module(parent, path.c_str()) {}

        shader_module(const shader_module&) = delete;
        shader_module& operator=(const shader_module&) = delete;

        /// @brief Move constructor
        /// @param other Object to be moved
        shader_module(shader_module&& other) noexcept :
            parent(other.parent), handle(std::move(other.handle)) {}

        /// @brief Move assignment
        /// @param other Object to be moved
        shader_module& operator=(shader_module&& other) noexcept {
            if (this == &other) [[unlikely]]
                return *this;
            std::destroy_at(this);
            std::construct_at(this, std::move(other));
            return *this;
        }

        /// @brief Casts to the underlying `vk::ShaderModule`
        constexpr operator vk::ShaderModule() const noexcept { return this->handle; };
        /// @brief Casts to the underlying `VkShaderModule`
        inline operator VkShaderModule() const noexcept { return this->handle; };

        ~shader_module() noexcept;

    private:
        const window& parent;
        vk::ShaderModule handle;
    };

    /// @brief Helper struct that associates a shader module with one of it's entrypoints
    struct shader_stage {
        constexpr static inline const char8_t* const DEFAULT_ENTRYPOINT = u8"main";

        /// @brief Create a shader stage from a pointer to a shader module
        shader_stage(const shader_module* shader,
                     const char8_t* entrypoint = DEFAULT_ENTRYPOINT) noexcept :
            shader(shader), entrypoint(entrypoint) {
            VGI_ASSERT(shader != nullptr);
        }

        /// @brief Create a shader stage from a shader_module
        shader_stage(shader_module&& shader,
                     const char8_t* entrypoint = DEFAULT_ENTRYPOINT) noexcept :
            shader(std::move(shader)), entrypoint(entrypoint) {}

        inline vk::PipelineShaderStageCreateInfo stage_info(
                vk::ShaderStageFlagBits stage) const noexcept {
            const shader_module* shader = this->operator->();
            VGI_ASSERT(shader != nullptr);
            return vk::PipelineShaderStageCreateInfo{
                    .stage = stage,
                    .module = *shader,
                    .pName = reinterpret_cast<const char*>(this->entrypoint),
            };
        }

        /// @brief Access the underlying `vgi::shader_module`
        inline const shader_module* operator->() const noexcept {
            if (const shader_module* module = std::get_if<shader_module>(&this->shader)) {
                return module;
            } else {
                VGI_ASSERT(std::holds_alternative<const shader_module*>(this->shader));
                return std::get<const shader_module*>(this->shader);
            }
        }

    private:
        std::variant<shader_module, const shader_module*> shader;
        const char8_t* entrypoint;
    };
}  // namespace vgi
