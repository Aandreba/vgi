#include <chrono>
#include <iostream>
#include <thread>
#include <vgi/buffer/transfer.hpp>
#include <vgi/buffer/uniform.hpp>
#include <vgi/buffer/vertex.hpp>
#include <vgi/cmdbuf.hpp>
#include <vgi/device.hpp>
#include <vgi/frame.hpp>
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

static int run() {
    vgi::window win{vgi::device::all().front(), u8"Hello world!", 900, 600};
    vgi::log("Detected devices ({}):", vgi::device::all().size());
    for (const vgi::device& device: vgi::device::all()) {
        vgi::log("{}", device.name());
    }

    // Create vertex buffer
    vgi::vertex_buffer_guard vertices{win, 3};
    {
        vgi::transfer_buffer_guard transfer{
                win, std::initializer_list<vgi::vertex>{
                             {.origin = {0.5f, 0.5f, 0.0f}, .color = {1.0f, 0.0f, 0.0f, 1.0f}},
                             {.origin = {-0.5f, 0.5f, 0.0f}, .color = {0.0f, 1.0f, 0.0f, 1.0f}},
                             {.origin = {0.0f, -0.5f, 0.0f}, .color = {0.0f, 0.0f, 1.0f, 1.0f}}}};

        vgi::command_buffer cmdbuf{win};
        cmdbuf->copyBuffer(transfer, vertices, vk::BufferCopy{.size = transfer->size()});
        std::move(cmdbuf).submit_and_wait();
    }

    // Create uniform buffer
    vgi::uniform_buffer_guard<uniform> uniforms{win};
    uniforms.write(win, uniform{.projection = glm::perspective(glm::radians(60.0f), 900.0f / 600.0f,
                                                               0.01f, 1000.0f)});

    vgi::graphics_pipeline_guard pipeline{
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
    vgi::descriptor_pool_guard desc_pool{win, pipeline};
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

    while (true) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_EVENT_QUIT:
                    return 0;
                case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
                    if (event.window.windowID == win) return 0;
                    break;
                }
                default:
                    break;
            }
        }

        // Render loop
        vgi::frame frame{win};
        vgi::log("{} FPS", 1.0f / frame.delta);
        frame.beginRendering(0.0f, 0.0f, 0.2f);

        frame->setViewport(0, vk::Viewport{
                                      .width = static_cast<float>(win.draw_size().width),
                                      .height = static_cast<float>(win.draw_size().height),
                                      .maxDepth = 1.0f,
                              });
        frame->setScissor(0, vk::Rect2D{.extent = win.draw_size()});

        // Draw triangle
        frame.bindDescriptorSet(pipeline, desc_pool);
        frame->bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
        frame->bindVertexBuffers(0, {vertices}, {0});
        frame->draw(3, 1, 0, 0);
        frame->endRendering();
    }
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
