#include "waves.hpp"

#include <vgi/fs.hpp>

void waves_scene::on_attach(vgi::window& win) {
    this->mesh = vgi::mesh<uint16_t>::load_plane_and_wait(win, 1024, 1024);
    this->uniforms = vgi::uniform_buffer<waves_uniform>{win, vgi::window::MAX_FRAMES_IN_FLIGHT};
    this->pipeline = vgi::graphics_pipeline{
            win, vgi::shader_stage{win, vgi::base_path / u8"shaders" / u8"basic.vert.spv"},
            vgi::shader_stage{win, vgi::base_path / u8"shaders" / u8"basic.frag.spv"},
            vgi::graphics_pipeline_options{
                    .cull_mode = vk::CullModeFlagBits::eNone,
                    .fron_face = vk::FrontFace::eCounterClockwise,
                    .bindings = {{vk::DescriptorSetLayoutBinding{
                            .binding = 0,
                            .descriptorType = vk::DescriptorType::eUniformBuffer,
                            .descriptorCount = 1,
                            .stageFlags = vk::ShaderStageFlagBits::eVertex,
                    }}},
            }};

    this->desc_pool = vgi::descriptor_pool{win, this->pipeline};
    for (uint32_t i = 0; i < this->desc_pool.size(); ++i) {
        const vk::DescriptorBufferInfo buf_info{
                .buffer = uniforms,
                .offset = sizeof(waves_uniform) * i,
                .range = sizeof(waves_uniform),
        };

        win->updateDescriptorSets(
                vk::WriteDescriptorSet{
                        .dstSet = this->desc_pool[i],
                        .dstBinding = 0,
                        .descriptorCount = 1,
                        .descriptorType = vk::DescriptorType::eUniformBuffer,
                        .pBufferInfo = &buf_info,
                },
                {});
    }
}

void waves_scene::on_update(vgi::window& win, vk::CommandBuffer cmdbuf, uint32_t current_frame,
                            const vgi::timings& ts) {
    this->camera.origin = glm::vec3{0.0f, 1.0f, 2.0f};
    this->camera.direction = glm::normalize(-this->camera.origin);
}

void waves_scene::on_render(vgi::window& win, vk::CommandBuffer cmdbuf, uint32_t current_frame) {
    cmdbuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, this->pipeline, 0,
                              this->desc_pool[current_frame], {});
    cmdbuf.bindPipeline(vk::PipelineBindPoint::eGraphics, this->pipeline);
    this->mesh.bind_and_draw(cmdbuf);
}

void waves_scene::on_detach(vgi::window& win) {
    win->waitIdle();
    std::move(this->mesh).destroy(win);
}
