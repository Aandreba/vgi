#include <chrono>
#include <iostream>
#include <thread>
#include <vgi/buffer/transfer.hpp>
#include <vgi/buffer/uniform.hpp>
#include <vgi/buffer/vertex.hpp>
#include <vgi/cmdbuf.hpp>
#include <vgi/device.hpp>
#include <vgi/fs.hpp>
#include <vgi/log.hpp>
#include <vgi/pipeline.hpp>
#include <vgi/vgi.hpp>
#include <vgi/window.hpp>

using namespace std::literals;

struct uniform {
    vgi::std140<glm::mat4> projection;
    vgi::std140<glm::mat4> view = glm::mat4{1};
    vgi::std140<glm::mat4> model = glm::mat4{1};
};

struct triangle_scene : public vgi::scene {
    vgi::vertex_buffer vertices;
    vgi::uniform_buffer<uniform> uniforms;
    vgi::graphics_pipeline pipeline;
    vgi::descriptor_pool desc_pool;

    void on_attach(vgi::window& win) override {
        // Create vertex buffer
        this->vertices = vgi::vertex_buffer{win, 3};
        {
            vgi::transfer_buffer_guard transfer{
                    win,
                    std::initializer_list<vgi::vertex>{
                            {.origin = {0.5f, 0.5f, 0.0f}, .color = {1.0f, 0.0f, 0.0f, 1.0f}},
                            {.origin = {-0.5f, 0.5f, 0.0f}, .color = {0.0f, 1.0f, 0.0f, 1.0f}},
                            {.origin = {0.0f, -0.5f, 0.0f}, .color = {0.0f, 0.0f, 1.0f, 1.0f}}}};

            vgi::command_buffer cmdbuf{win};
            cmdbuf->copyBuffer(transfer, vertices, vk::BufferCopy{.size = transfer->size()});
            std::move(cmdbuf).submit_and_wait();
        }

        // Create uniform buffer
        this->uniforms = vgi::uniform_buffer<uniform>{win};
        uniforms.write(win, uniform{.projection = glm::perspective(
                                            glm::radians(60.0f), 900.0f / 600.0f, 0.01f, 1000.0f)});

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

        // WARNING: Currently sharing the same uniform buffer for all frames.
        this->desc_pool = vgi::descriptor_pool{win, this->pipeline};
        for (vk::DescriptorSet desc_set: desc_pool) {
            const vk::DescriptorBufferInfo buf_info{
                    .buffer = uniforms,
                    .offset = 0,
                    .range = sizeof(uniform),
            };

            win->updateDescriptorSets(
                    vk::WriteDescriptorSet{
                            .dstSet = desc_set,
                            .dstBinding = 0,
                            .descriptorCount = 1,
                            .descriptorType = vk::DescriptorType::eUniformBuffer,
                            .pBufferInfo = &buf_info,
                    },
                    {});
        }
    }

    void on_update(vk::CommandBuffer cmdbuf, const vgi::timings& ts) override {
        // TODO
    }

    void on_render(vk::CommandBuffer cmdbuf) override {
        // TODO
    }

    void on_detach(vgi::window& win) override {
        std::move(this->vertices).destroy(win);
        std::move(this->uniforms).destroy(win);
        std::move(this->pipeline).destroy(win);
        std::move(this->desc_pool).destroy(win);
    }
};

static int run() {
    vgi::log("Detected devices ({}):", vgi::device::all().size());
    for (const vgi::device& device: vgi::device::all()) {
        vgi::log("{}", device.name());
    }

    vgi::add_layer<vgi::window>(vgi::device::all().front(), u8"Hello world!", 900, 600);
    vgi::run();
    return 0;
}

static void show_error_box(const char* msg) noexcept {
    try {
        if (SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, reinterpret_cast<const char*>(u8"Error"),
                                     msg, nullptr)) {
            std::cout << msg << std::endl;
        } else {
            std::cout << SDL_GetError() << std::endl;
        }
    } catch (...) {
    }
}

int main() {
    try {
        vgi::init(u8"Entorn VGI");
    } catch (const std::exception& e) {
        show_error_box(e.what());
        return 1;
    } catch (...) {
        show_error_box("Unknown exception");
        return 1;
    }

    try {
        int ret = run();
        vgi::quit();
        return ret;
    } catch (const std::exception& e) {
        show_error_box(e.what());
        vgi::quit();
        return 1;
    } catch (...) {
        show_error_box("Unknown exception");
        vgi::quit();
        return 1;
    }
}
