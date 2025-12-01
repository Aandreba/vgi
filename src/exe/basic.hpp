#pragma once

#include <vgi/buffer/uniform.hpp>
#include <vgi/buffer/vertex.hpp>
#include <vgi/math/camera.hpp>
#include <vgi/pipeline.hpp>
#include <vgi/pipeline/shader.hpp>
#include <vgi/resource/mesh.hpp>
#include <vgi/vgi.hpp>

struct uniform {
    vgi::std140<glm::mat4> projection;
    vgi::std140<glm::mat4> model = glm::mat4{1};
    vgi::std140<glm::mat4> view = glm::mat4{1};
};

struct basic_scene : public vgi::layer {
    vgi::mesh<uint16_t> mesh;
    vgi::uniform_buffer<uniform> uniforms;
    vgi::graphics_pipeline pipeline;
    vgi::descriptor_pool desc_pool;
    vgi::math::perspective_camera camera;

    void on_attach(vgi::window& win) override;
    void on_update(vgi::window& win, vk::CommandBuffer cmdbuf, uint32_t current_frame,
                   const vgi::timings& ts) override;

    void on_render(vgi::window& win, vk::CommandBuffer cmdbuf, uint32_t current_frame,
                   const vgi::timings& ts) override;
    void on_detach(vgi::window& win) override;
};
