#include "waves.hpp"

#include <vgi/asset/gltf.hpp>
#include <vgi/fs.hpp>
#include <vgi/texture.hpp>

// https://www.khronos.org/files/gltf20-reference-guide.pdf
void waves_scene::on_attach(vgi::window& win) {
    this->asset = {win, std::filesystem::current_path() / VGI_OS("src/exe/assets/Knight.glb")};
    this->pipeline = vgi::graphics_pipeline{
            win, vgi::shader_stage{win, vgi::base_path / u8"shaders" / u8"waves.vert.spv"},
            vgi::shader_stage{win, vgi::base_path / u8"shaders" / u8"waves.frag.spv"},
            vgi::graphics_pipeline_options{
                    .cull_mode = vk::CullModeFlagBits::eNone,
                    .fron_face = vk::FrontFace::eCounterClockwise,
                    .bindings = {{
                            vk::DescriptorSetLayoutBinding{
                                    .binding = 0,
                                    .descriptorType = vk::DescriptorType::eUniformBuffer,
                                    .descriptorCount = 1,
                                    .stageFlags = vk::ShaderStageFlagBits::eVertex,
                            },
                            vk::DescriptorSetLayoutBinding{
                                    .binding = 1,
                                    .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                                    .descriptorCount = 1,
                                    .stageFlags = vk::ShaderStageFlagBits::eFragment,
                            },
                            vk::DescriptorSetLayoutBinding{
                                    .binding = 2,
                                    .descriptorType = vk::DescriptorType::eUniformBuffer,
                                    .descriptorCount = 1,
                                    .stageFlags = vk::ShaderStageFlagBits::eVertex,
                            },
                    }},
            }};
}

static void process_node(vgi::asset::gltf::asset& asset, vgi::asset::gltf::node& node,
                         vgi::math::transf3d parent_transf = {}) {
    glm::vec3 origin = node.local_origin;
    glm::quat rotation = node.local_rotation;
    glm::vec3 scale = node.local_scale;
    // TODO Animation

    vgi::math::transf3d local_transf{origin, rotation, scale};
    vgi::math::transf3d model_transf = local_transf * parent_transf;
}

void waves_scene::on_update(vgi::window& win, vk::CommandBuffer cmdbuf, uint32_t current_frame,
                            const vgi::timings& ts) {
    for (size_t rid: this->asset.scenes[0].roots) {
        auto& root = this->asset.nodes[rid];
        // TODO
    }
}

void waves_scene::on_render(vgi::window& win, vk::CommandBuffer cmdbuf, uint32_t current_frame) {
    // TODO
}

void waves_scene::on_detach(vgi::window& win) {
    win->waitIdle();
    std::move(this->pipeline).destroy(win);
    std::move(this->asset).destroy(win);
}
