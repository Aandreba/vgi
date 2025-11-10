#include "waves.hpp"

#include <vgi/asset/gltf.hpp>
#include <vgi/buffer/storage.hpp>
#include <vgi/fs.hpp>
#include <vgi/texture.hpp>

namespace skeleton {
    primitive::primitive(const vgi::window& win, const vgi::pipeline& pipeline,
                         const skin_buffer* skin) :
        pool(win, pipeline), uniform(win), has_skin(skin != nullptr) {
        if (skin) skin->update_descriptors(win, this->pool, 2);
    }

    mesh::mesh(const vgi::window& win, const vgi::pipeline& pipeline, size_t count) {
        this->primitives.reserve(count);
        for (size_t i = 0; i < count; ++i) this->primitives.emplace_back(win, pipeline);
    }

    // https://www.khronos.org/files/gltf20-reference-guide.pdf
    void scene::on_attach(vgi::window& win) {
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
                                        .descriptorType = vk::DescriptorType::eStorageBuffer,
                                        .descriptorCount = 1,
                                        .stageFlags = vk::ShaderStageFlagBits::eVertex,
                                },
                        }},
                }};

        // TODO Create descriptors
    }

    static void draw_mesh(const vgi::window& win, uint32_t current_frame, vgi::gltf::asset& asset,
                          std::span<const mesh> meshes, size_t mesh, std::optional<size_t> skin,
                          vgi::math::transf3d transform) {
        std::span<const vgi::gltf::primitive> gltf_prim = asset.meshes.at(mesh).primitives;
    }

    static void process_node(const vgi::window& win, uint32_t current_frame,
                             vgi::gltf::asset& asset, std::span<const mesh> meshes,
                             vgi::gltf::node& node, vgi::math::transf3d parent_transf,
                             std::span<vgi::storage_buffer<glm::mat4>> skinning) {
        glm::vec3 origin = node.local_origin;
        glm::quat rotation = node.local_rotation;
        glm::vec3 scale = node.local_scale;
        // TODO Animation

        // Compute node's full transformation
        vgi::math::transf3d local_transf{origin, rotation, scale};
        vgi::math::transf3d model_transf = parent_transf * local_transf;

        // Update attached joints
        for (vgi::gltf::joint& joint: node.attachments) {
            skinning[joint.skin].write(win, model_transf * joint.inv_bind, current_frame,
                                       joint.index);
        }

        // If this node has a mesh, submit a draw command
        if (node.mesh) {
            draw_mesh(win, current_frame, asset, meshes, *node.mesh, node.skin, model_transf);
        }

        // Process children
        for (size_t child: node.children) {
            process_node(win, current_frame, asset, meshes, asset.nodes.at(child), model_transf,
                         skinning);
        }
    }

    void scene::on_update(vgi::window& win, vk::CommandBuffer cmdbuf, uint32_t current_frame,
                          const vgi::timings& ts) {
        // TODO
    }

    void scene::on_render(vgi::window& win, vk::CommandBuffer cmdbuf, uint32_t current_frame) {
        for (size_t root: this->asset.scenes[0].roots) {
            process_node(win, current_frame, this->asset, this->asset.nodes[root], {}, {});
        }
    }

    void scene::on_detach(vgi::window& win) {
        win->waitIdle();
        for (mesh& mesh: this->meshes) std::move(mesh).destroy(win);
        std::move(this->pipeline).destroy(win);
        std::move(this->asset).destroy(win);
    }

    void mesh::destroy(const vgi::window& win) && {
        for (primitive& prim: this->primitives) std::move(prim).destroy(win);
    }

    void primitive::destroy(const vgi::window& win) && {
        std::move(this->pool).destroy(win);
        std::move(this->uniform).destroy(win);
    }
}  // namespace skeleton
