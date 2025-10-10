#include "shader.hpp"

#include <vgi/fs.hpp>

namespace vgi {
    shader_module::shader_module(const window& parent, std::span<const uint32_t> code) :
        parent(parent), handle(parent->createShaderModule(vk::ShaderModuleCreateInfo{
                                .codeSize = code.size_bytes(),
                                .pCode = code.data(),
                        })) {}

    shader_module::shader_module(const window& parent,
                                 const std::filesystem::path::value_type* path) :
        shader_module(parent, read_file<uint32_t>(path)) {}

    shader_module::~shader_module() noexcept {
        if (this->handle) this->parent->destroyShaderModule(this->handle);
    }
}  // namespace vgi
