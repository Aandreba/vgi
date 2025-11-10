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
    using skin_buffer = vgi::storage_buffer<glm::mat4>;

    struct uniform {
        vgi::std140<glm::mat4> mvp;
        vgi::std140<uint32_t> has_skin;
    };

    struct primitive {
        vgi::descriptor_pool pool;
        vgi::uniform_buffer<uniform> uniform;
        bool has_skin;

        primitive() = default;
        primitive(const vgi::window& win, const vgi::pipeline& pipeline, const skin_buffer* skin);
        void destroy(const vgi::window& parent) &&;
    };

    struct mesh {
        std::vector<primitive> primitives;

        mesh() = default;
        mesh(const vgi::window& win, const vgi::pipeline& pipeline, size_t count);
        void destroy(const vgi::window& parent) &&;
    };

    struct scene : public vgi::layer {
        vgi::gltf::asset asset;
        vgi::graphics_pipeline pipeline;
        vgi::math::perspective_camera camera;
        std::vector<mesh> meshes;
        std::vector<skin_buffer> skins;

        void on_attach(vgi::window& win) override;
        void on_update(vgi::window& win, vk::CommandBuffer cmdbuf, uint32_t current_frame,
                       const vgi::timings& ts) override;
        void on_render(vgi::window& win, vk::CommandBuffer cmdbuf, uint32_t current_frame) override;
        void on_detach(vgi::window& win) override;
    };
};  // namespace skeleton
