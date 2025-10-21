#include "basic.hpp"

#include <vgi/fs.hpp>

void basic_scene::on_attach(vgi::window& win) {
    // Create vertex buffer
    // this->mesh = vgi::mesh<uint16_t>::load_cube_and_wait(win);
    this->mesh = vgi::mesh<uint16_t>::load_sphere_and_wait(win, 16, 16);

    this->uniforms = vgi::uniform_buffer<uniform>{win, vgi::window::MAX_FRAMES_IN_FLIGHT};
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
                .offset = sizeof(uniform) * i,
                .range = sizeof(uniform),
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

    this->camera = vgi::math::camera{};
}

void basic_scene::on_update(vgi::window& win, vk::CommandBuffer cmdbuf, uint32_t current_frame,
                            const vgi::timings& ts) {
    this->camera.origin = glm::vec3{0.0f, 0.0f, 2.0f};
    this->camera.direction = glm::normalize(-this->camera.origin);
    this->camera.rotate(ts.start, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 model = glm::rotate(glm::mat4(1.0f), ts.start * glm::radians(90.0f),
                                  glm::vec3(1.0f, 0.0f, 0.0f));

    this->uniforms.write(win,
                         uniform{
                                 .projection = this->camera.perspective(900.0f / 600.0f),
                                 .model = model,
                                 .view = this->camera.view(),
                         },
                         current_frame);
}

void basic_scene::on_render(vgi::window& win, vk::CommandBuffer cmdbuf, uint32_t current_frame) {
    cmdbuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, this->pipeline, 0,
                              this->desc_pool[current_frame], {});
    cmdbuf.bindPipeline(vk::PipelineBindPoint::eGraphics, this->pipeline);
    this->mesh.bind_and_draw(cmdbuf);
}

void basic_scene::on_detach(vgi::window& win) {
    std::move(this->uniforms).destroy(win);
    std::move(this->pipeline).destroy(win);
    std::move(this->desc_pool).destroy(win);
    std::move(this->mesh).destroy(win);
}
