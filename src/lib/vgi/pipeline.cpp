#include "pipeline.hpp"

#include "buffer/vertex.hpp"
#include "math.hpp"
#include "vgi.hpp"

namespace vgi {
    pipeline::pipeline(const window& parent,
                       std::span<const vk::DescriptorSetLayoutBinding> bindings) {
        std::optional<uint32_t> binding_count = math::check_cast<uint32_t>(bindings.size());
        if (!binding_count) throw std::runtime_error{"too many bindings"};

        std::vector<vk::DescriptorPoolSize> pool_sizes;
        pool_sizes.reserve(bindings.size());
        for (const vk::DescriptorSetLayoutBinding& binding: bindings) {
            std::optional<uint32_t> count =
                    math::check_mul(binding.descriptorCount, window::MAX_FRAMES_IN_FLIGHT);
            if (!count) throw std::runtime_error{"too many bindings"};

            pool_sizes.push_back({
                    .type = binding.descriptorType,
                    .descriptorCount = count.value(),
            });
        }

        this->pool = parent->createDescriptorPool(vk::DescriptorPoolCreateInfo{
                .maxSets = window::MAX_FRAMES_IN_FLIGHT,
                .poolSizeCount = binding_count.value(),
                .pPoolSizes = pool_sizes.data(),
        });

        this->set_layout = parent->createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo{
                .bindingCount = binding_count.value(),
                .pBindings = bindings.data(),
        });

        this->pipeline_layout = parent->createPipelineLayout(vk::PipelineLayoutCreateInfo{
                .setLayoutCount = 1,
                .pSetLayouts = &this->set_layout,
        });

        const vk::DescriptorSetAllocateInfo alloc_info{
                .descriptorPool = this->pool,
                .descriptorSetCount = 1,
                .pSetLayouts = &this->set_layout,
        };

        for (uint32_t i = 0; i < window::MAX_FRAMES_IN_FLIGHT; ++i) {
            vkn::allocateDescriptorSets(parent, alloc_info, &this->sets[i]);
        }
    }

    void pipeline::destroy(const window& parent) && {
        if (this->handle) parent->destroyPipeline(this->handle);
        if (this->pipeline_layout) parent->destroyPipelineLayout(this->pipeline_layout);
        if (this->set_layout) parent->destroyDescriptorSetLayout(this->set_layout);
        if (this->pool) parent->destroyDescriptorPool(this->pool);
    }

    graphics_pipeline::graphics_pipeline(const window& parent, const shader_stage& vertex,
                                         const shader_stage& fragment,
                                         const graphics_pipeline_options& options) :
        pipeline(parent, options.bindings) {
        const vk::PipelineShaderStageCreateInfo stages[] = {
                vertex.stage_info(vk::ShaderStageFlagBits::eVertex),
                fragment.stage_info(vk::ShaderStageFlagBits::eFragment)};

        const auto vertex_binding = vertex::input_binding(options.vertex_binding);
        const auto vertex_attributes = vertex::input_attributes(options.vertex_binding);
        const vk::PipelineVertexInputStateCreateInfo vertex_input_state{
                .vertexBindingDescriptionCount = 1,
                .pVertexBindingDescriptions = &vertex_binding,
                .vertexAttributeDescriptionCount = std::size(vertex_attributes),
                .pVertexAttributeDescriptions = vertex_attributes.data(),
        };

        // Input assembly state describes how primitives are assembled
        const vk::PipelineInputAssemblyStateCreateInfo input_assembly_state{
                .topology = options.topology,
        };

        // Viewport state sets the number of viewports and scissor used in this pipeline
        // Note: This is actually overridden by the dynamic states (see below)
        const vk::PipelineViewportStateCreateInfo viewport_state{
                .viewportCount = 1,
                .scissorCount = 1,
        };

        // Rasterization state
        const vk::PipelineRasterizationStateCreateInfo rasterization_state{
                .polygonMode = options.polygon_mode,
                .cullMode = options.cull_mode,
                .frontFace = options.fron_face,
                .lineWidth = 1.0f,
        };

        // We still don't make use of multi sampling (for anti-aliasing), the state must still be
        // set and passed to the pipeline
        const vk::PipelineMultisampleStateCreateInfo multisample_state{
                .rasterizationSamples = vk::SampleCountFlagBits::e1,
        };

        // Depth and stencil state containing depth and stencil compare and test operations
        // We only use depth tests and want depth tests and writes to be enabled.
        const vk::PipelineDepthStencilStateCreateInfo depth_stencil_state{
                .depthTestEnable = options.depth_compare_op != vk::CompareOp::eNever,
                .depthWriteEnable = options.depth_compare_op != vk::CompareOp::eNever,
                .depthCompareOp = options.depth_compare_op,
        };

        // Color blend state describes how blend factors are calculated (if used)
        // We need one blend attachment state per color attachment (even if blending is not used)
        const vk::PipelineColorBlendAttachmentState blend_attachment_states[] = {{
                .blendEnable = static_cast<bool>(options.color_blend_mask),
                .colorWriteMask = options.color_blend_mask,
        }};
        const vk::PipelineColorBlendStateCreateInfo color_blend_state{
                .attachmentCount = std::size(blend_attachment_states),
                .pAttachments = blend_attachment_states,
        };

        // Enable dynamic states
        // Most states are baked into the pipeline, but there is somee state that can be dynamically
        // changed within the command buffer to mak e things easuer To be able to change these we
        // need do specify which dynamic states will be changed using this pipeline. Their actual
        // states are set later on in the command buffer
        constexpr const vk::DynamicState dynamic_states[] = {vk::DynamicState::eViewport,
                                                             vk::DynamicState::eScissor};
        const vk::PipelineDynamicStateCreateInfo dynamic_state_info{
                .dynamicStateCount = std::size(dynamic_states),
                .pDynamicStates = dynamic_states,
        };

        // Attachment information for dynamic rendering
        const vk::Format swapchain_format = parent.format();
        vk::PipelineRenderingCreateInfo pipeline_rendering{
                .colorAttachmentCount = 1,
                .pColorAttachmentFormats = &swapchain_format,
                .depthAttachmentFormat = vk::Format::eUndefined,
                .stencilAttachmentFormat = vk::Format::eUndefined,
        };

        const vk::GraphicsPipelineCreateInfo create_info{
                .pNext = &pipeline_rendering,
                .stageCount = std::size(stages),
                .pStages = stages,
                .pVertexInputState = &vertex_input_state,
                .pInputAssemblyState = &input_assembly_state,
                .pTessellationState = nullptr,
                .pViewportState = &viewport_state,
                .pRasterizationState = &rasterization_state,
                .pMultisampleState = &multisample_state,
                .pDepthStencilState = &depth_stencil_state,
                .pColorBlendState = &color_blend_state,
                .pDynamicState = &dynamic_state_info,
                .layout = this->layout(),
        };

        this->handle = parent->createGraphicsPipeline(nullptr, create_info).value;
    }
}  // namespace vgi
