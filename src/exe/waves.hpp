#pragma once

#include <vgi/asset/gltf.hpp>
#include <vgi/buffer/storage.hpp>
#include <vgi/buffer/uniform.hpp>
#include <vgi/buffer/vertex.hpp>
#include <vgi/math/camera.hpp>
#include <vgi/pipeline.hpp>
#include <vgi/pipeline/shader.hpp>
#include <vgi/resource/mesh.hpp>
#include <vgi/texture.hpp>
#include <vgi/vgi.hpp>

namespace skeleton {
    struct skin {
        vgi::descriptor_pool descriptor;
        vgi::storage_buffer<glm::mat4> buffer;

        skin(vgi::window& win, const vgi::graphics_pipeline& pipeline, const vgi::gltf::skin& info,
             const vgi::texture_sampler& tex);
        void destroy(vgi::window& win) &&;
    };

    struct uniform {
        vgi::std140<glm::mat4> mvp;
        vgi::std140<uint32_t> has_skin;
    };

    struct scene : public vgi::layer {
        vgi::gltf::asset asset;
        vgi::graphics_pipeline pipeline;
        vgi::math::perspective_camera camera;
        std::vector<skin> skins;

        void on_attach(vgi::window& win) override;
        void on_update(vgi::window& win, vk::CommandBuffer cmdbuf, uint32_t current_frame,
                       const vgi::timings& ts) override;
        void on_render(vgi::window& win, vk::CommandBuffer cmdbuf, uint32_t current_frame,
                       const vgi::timings& ts) override;
        void on_detach(vgi::window& win) override;
    };
};  // namespace skeleton
