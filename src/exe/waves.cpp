#include "waves.hpp"

#include <vgi/asset/gltf.hpp>
#include <vgi/buffer/storage.hpp>
#include <vgi/fs.hpp>
#include <vgi/texture.hpp>

namespace skeleton {
    // https://www.khronos.org/files/gltf20-reference-guide.pdf
    void scene::on_attach(vgi::window& win) {
        this->asset = {win, std::filesystem::current_path() / VGI_OS("src/exe/assets/Knight.glb")};
        this->pipeline = vgi::graphics_pipeline{
                win, vgi::shader_stage{win, vgi::base_path / u8"shaders" / u8"waves.vert.spv"},
                vgi::shader_stage{win, vgi::base_path / u8"shaders" / u8"waves.frag.spv"},
                vgi::graphics_pipeline_options{
                        .cull_mode = vk::CullModeFlagBits::eNone,
                        .fron_face = vk::FrontFace::eCounterClockwise,
                        .push_constants = {{
                                vk::PushConstantRange{
                                        .offset = 0,
                                        .size = sizeof(glm::mat4),
                                        .stageFlags = vk::ShaderStageFlagBits::eVertex,
                                },
                        }},
                        .bindings = {{
                                vk::DescriptorSetLayoutBinding{
                                        .binding = 0,
                                        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                                        .descriptorCount = 1,
                                        .stageFlags = vk::ShaderStageFlagBits::eFragment,
                                },
                                vk::DescriptorSetLayoutBinding{
                                        .binding = 1,
                                        .descriptorType = vk::DescriptorType::eStorageBuffer,
                                        .descriptorCount = 1,
                                        .stageFlags = vk::ShaderStageFlagBits::eVertex,
                                },
                        }},
                }};

        this->skins.reserve(this->asset.skins.size());
        for (const vgi::gltf::skin& skin: this->asset.skins) {
            // FIXME: This code assumes we have a single texture used by the model, which may not
            // always be true.
            this->skins.emplace_back(win, this->pipeline, skin, this->asset.textures.at(0).texture);
        }
    }

    static void draw_mesh(const vgi::window& win, const vgi::graphics_pipeline& pipeline,
                          vk::CommandBuffer cmdbuf, uint32_t current_frame, vgi::gltf::asset& asset,
                          size_t mesh, std::optional<size_t> skin, glm::mat4 transform,
                          std::span<struct skin> skinning) {
        cmdbuf.pushConstants(pipeline, vk::ShaderStageFlagBits::eVertex, UINT32_C(0),
                             vk::ArrayProxy<const glm::mat4>{transform});

        if (skin.has_value()) {
            const vk::DescriptorSet& set = skinning[*skin].descriptor[current_frame];
            cmdbuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline, 0, set, {});
        }

        for (const vgi::gltf::primitive& gltf_prim: asset.meshes.at(mesh).primitives) {
            gltf_prim.bind_and_draw(cmdbuf);
        }
    }

    static void process_node(const vgi::window& win, const vgi::graphics_pipeline& pipeline,
                             vk::CommandBuffer cmdbuf, uint32_t current_frame,
                             vgi::gltf::asset& asset, vgi::gltf::node& node,
                             vgi::math::transf3d parent_transf, std::span<skin> skinning) {
        glm::vec3 origin = node.local_origin;
        glm::quat rotation = node.local_rotation;
        glm::vec3 scale = node.local_scale;
        // TODO Animation

        // Compute node's full transformation
        vgi::math::transf3d local_transf{origin, rotation, scale};
        vgi::math::transf3d model_transf = parent_transf * local_transf;

        // Update attached joints
        for (vgi::gltf::joint& joint: node.attachments) {
            skinning[joint.skin].buffer.write(win, model_transf * joint.inv_bind, current_frame,
                                              joint.index);
        }

        // If this node has a mesh, submit a draw command
        if (node.mesh) {
            draw_mesh(win, pipeline, cmdbuf, current_frame, asset, *node.mesh, node.skin,
                      model_transf, skinning);
        }

        // Process children
        for (size_t child: node.children) {
            process_node(win, pipeline, cmdbuf, current_frame, asset, asset.nodes.at(child),
                         model_transf, skinning);
        }
    }

    void scene::on_update(vgi::window& win, vk::CommandBuffer cmdbuf, uint32_t current_frame,
                          const vgi::timings& ts) {
        // TODO
    }

    void scene::on_render(vgi::window& win, vk::CommandBuffer cmdbuf, uint32_t current_frame) {
        this->pipeline.bind(cmdbuf);
        for (size_t root: this->asset.scenes[0].roots) {
            process_node(win, pipeline, cmdbuf, current_frame, this->asset, this->asset.nodes[root],
                         {}, this->skins);
        }
    }

    void scene::on_detach(vgi::window& win) {
        win->waitIdle();
        std::move(this->pipeline).destroy(win);
        for (skin& skin: this->skins) std::move(skin).destroy(win);
        std::move(this->asset).destroy(win);
    }

    skin::skin(vgi::window& win, const vgi::graphics_pipeline& pipeline,
               const vgi::gltf::skin& info, const vgi::texture_sampler& tex) :
        descriptor(win, pipeline), buffer(win, info.joints) {
        tex.update_descriptors(win, this->descriptor, 0);
        this->buffer.update_descriptors(win, this->descriptor, 1);
    }

    void skin::destroy(vgi::window& win) && {
        std::move(this->descriptor).destroy(win);
        std::move(this->buffer).destroy(win);
    }
}  // namespace skeleton
