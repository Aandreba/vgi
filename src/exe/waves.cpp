#include "waves.hpp"

#include <vgi/asset/gltf.hpp>
#include <vgi/fs.hpp>
#include <vgi/texture.hpp>

void waves_scene::on_attach(vgi::window& win) {
    auto asset = vgi::asset::gltf::import(VGI_OS("D:\\Projects\\vgi\\src\\exe\\assets"));
    vgi::surface surf{VGI_OS("c:/Users/aandr/Pictures/Screenshot 2025-10-17 180616.png")};
    this->image = vgi::texture_sampler{win, vgi::texture::upload_and_wait(win, surf)};

    this->mesh = vgi::mesh<uint32_t>::load_plane_and_wait(win, 1024, 1024);
    this->uniforms = vgi::uniform_buffer<waves_uniform>{win, vgi::window::MAX_FRAMES_IN_FLIGHT};
    this->pipeline = vgi::graphics_pipeline{
            win, vgi::shader_stage{win, vgi::base_path / u8"shaders" / u8"waves.vert.spv"},
            vgi::shader_stage{win, vgi::base_path / u8"shaders" / u8"waves.frag.spv"},
            vgi::graphics_pipeline_options{
                    .cull_mode = vk::CullModeFlagBits::eNone,
                    .fron_face = vk::FrontFace::eCounterClockwise,
                    .bindings = {{vk::DescriptorSetLayoutBinding{
                                          .binding = 0,
                                          .descriptorType = vk::DescriptorType::eUniformBuffer,
                                          .descriptorCount = 1,
                                          .stageFlags = vk::ShaderStageFlagBits::eVertex,
                                  },
                                  vk::DescriptorSetLayoutBinding{
                                          .binding = 1,
                                          .descriptorType =
                                                  vk::DescriptorType::eCombinedImageSampler,
                                          .descriptorCount = 1,
                                          .stageFlags = vk::ShaderStageFlagBits::eFragment,
                                  }}},
            }};

    this->desc_pool = vgi::descriptor_pool{win, this->pipeline};
    for (uint32_t i = 0; i < this->desc_pool.size(); ++i) {
        const vk::DescriptorBufferInfo buf_info{
                .buffer = uniforms,
                .offset = sizeof(waves_uniform) * i,
                .range = sizeof(waves_uniform),
        };
        const vk::DescriptorImageInfo img_info = this->image.descriptor_info(i);

        win->updateDescriptorSets(
                {vk::WriteDescriptorSet{
                         .dstSet = this->desc_pool[i],
                         .dstBinding = 0,
                         .descriptorCount = 1,
                         .descriptorType = vk::DescriptorType::eUniformBuffer,
                         .pBufferInfo = &buf_info,
                 },
                 vk::WriteDescriptorSet{
                         .dstSet = this->desc_pool[i],
                         .dstBinding = 1,
                         .descriptorCount = 1,
                         .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                         .pImageInfo = &img_info,
                 }},
                {});
    }
}

void waves_scene::on_update(vgi::window& win, vk::CommandBuffer cmdbuf, uint32_t current_frame,
                            const vgi::timings& ts) {
    this->camera.origin = glm::vec3{0.0f, 1.0f, 2.0f};
    this->camera.direction = glm::normalize(-this->camera.origin);
    glm::mat4 model =
            glm::rotate(glm::mat4{1.0f}, 0.0f * glm::radians(90.0f), glm::vec3{1.0f, 0.0f, 0.0f});

    this->uniforms.write(
            win,
            waves_uniform{
                    .mvp = this->camera.perspective(win.draw_size()) * this->camera.view() * model,
            },
            current_frame);
}

void waves_scene::on_render(vgi::window& win, vk::CommandBuffer cmdbuf, uint32_t current_frame) {
    cmdbuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, this->pipeline, 0,
                              this->desc_pool[current_frame], {});
    cmdbuf.bindPipeline(vk::PipelineBindPoint::eGraphics, this->pipeline);
    this->mesh.bind_and_draw(cmdbuf);
}

void waves_scene::on_detach(vgi::window& win) {
    win->waitIdle();
    std::move(this->image).destroy(win);
    std::move(this->uniforms).destroy(win);
    std::move(this->pipeline).destroy(win);
    std::move(this->desc_pool).destroy(win);
    std::move(this->mesh).destroy(win);
}
