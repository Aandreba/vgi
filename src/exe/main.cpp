#include <chrono>
#include <iostream>
#include <thread>
#include <vgi/buffer/index.hpp>
#include <vgi/buffer/transfer.hpp>
#include <vgi/buffer/uniform.hpp>
#include <vgi/buffer/vertex.hpp>
#include <vgi/cmdbuf.hpp>
#include <vgi/device.hpp>
#include <vgi/fs.hpp>
#include <vgi/log.hpp>
#include <vgi/main.hpp>
#include <vgi/math/camera.hpp>
#include <vgi/math/transf3d.hpp>
#include <vgi/pipeline.hpp>
#include <vgi/resource/mesh.hpp>
#include <vgi/vgi.hpp>
#include <vgi/window.hpp>

using namespace std::literals;

struct uniform {
    vgi::std140<glm::mat4> projection;
    vgi::std140<glm::mat4> view = glm::mat4{1};
    vgi::std140<glm::mat4> model = glm::mat4{1};
};

struct triangle_scene : public vgi::scene {
    vgi::res<vgi::mesh<uint16_t>> mesh;
    vgi::uniform_buffer<uniform> uniforms;
    vgi::graphics_pipeline pipeline;
    vgi::descriptor_pool desc_pool;
    vgi::math::camera camera;

    void on_attach(vgi::window& win) override {
        // Create vertex buffer
        this->mesh = win.create_resource<vgi::mesh<uint16_t>>(3, 3);
        {
            auto mesh_ref = this->mesh.lock();
            vk::DeviceSize indices_offset = 3 * sizeof(vgi::vertex);

            vgi::transfer_buffer_guard transfer{win,
                                                3 * sizeof(vgi::vertex) + 3 * sizeof(uint16_t)};

            transfer.template write_at<uint16_t>({0, 1, 2}, indices_offset);
            transfer.template write_at<vgi::vertex>(
                    {{.origin = {0.5f, 0.5f, 0.0f}, .color = {1.0f, 0.0f, 0.0f, 1.0f}},
                     {.origin = {-0.5f, 0.5f, 0.0f}, .color = {0.0f, 1.0f, 0.0f, 1.0f}},
                     {.origin = {0.0f, -0.5f, 0.0f}, .color = {0.0f, 0.0f, 1.0f, 1.0f}}},
                    0);

            vgi::command_buffer cmdbuf{win};
            cmdbuf->copyBuffer(transfer, mesh_ref->vertices,
                               vk::BufferCopy{
                                       .srcOffset = 0,
                                       .size = transfer->size(),
                               });
            cmdbuf->copyBuffer(transfer, mesh_ref->indices,
                               vk::BufferCopy{
                                       .srcOffset = indices_offset,
                                       .size = transfer->size(),
                               });
            std::move(cmdbuf).submit_and_wait();
        }

        this->uniforms = vgi::uniform_buffer<uniform>{win, vgi::window::MAX_FRAMES_IN_FLIGHT};
        this->pipeline = vgi::graphics_pipeline{
                win, vgi::shader_stage{win, vgi::base_path / u8"shaders" / u8"triangle.vert.spv"},
                vgi::shader_stage{win, vgi::base_path / u8"shaders" / u8"triangle.frag.spv"},
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
        this->camera.translate(glm::vec3{0.0f, 0.0f, 2.5f});
    }

    void on_update(vgi::window& win, vk::CommandBuffer cmdbuf, uint32_t current_frame,
                   const vgi::timings& ts) override {
        this->camera.rotate(ts.delta, glm::vec3(1.0f, 0.0f, 0.0f));

        glm::mat4 model = glm::rotate(glm::mat4(1.0f), 0.0f * ts.start * glm::radians(90.0f),
                                      glm::vec3(0.0f, 0.0f, 1.0f));

        this->uniforms.write(win,
                             uniform{
                                     .projection = this->camera.perspective(900.0f / 600.0f),
                                     .view = this->camera.view(),
                                     .model = model,
                             },
                             current_frame);
    }

    void on_render(vgi::window& win, vk::CommandBuffer cmdbuf, uint32_t current_frame) override {
        cmdbuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, this->pipeline, 0,
                                  this->desc_pool[current_frame], {});
        cmdbuf.bindPipeline(vk::PipelineBindPoint::eGraphics, this->pipeline);
        this->vertices.bind(cmdbuf);
        cmdbuf.draw(3, 1, 0, 0);
    }

    void on_detach(vgi::window& win) override {
        std::move(this->uniforms).destroy(win);
        std::move(this->pipeline).destroy(win);
        std::move(this->desc_pool).destroy(win);
    }
};

int main() {
    vgi::init(u8"Entorn VGI");

    vgi::log("Arguments ({}):", vgi::argc());
    for (auto arg: vgi::argv()) {
        vgi::log(VGI_OS("- {}"), arg);
    }

    vgi::log("Detected devices ({}):", vgi::device::all().size());
    for (const vgi::device& device: vgi::device::all()) {
        vgi::log("- {}", device.name());
    }

    vgi::emplace_layer<vgi::window>(vgi::device::all().front(), u8"Hello world!", 900, 600)
            .add_scene<triangle_scene>();

    vgi::run();
    return 0;
}
