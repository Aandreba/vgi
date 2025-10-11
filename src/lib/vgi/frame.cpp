#include "frame.hpp"

#include "log.hpp"

namespace vgi {
    frame::frame(window& parent) : parent(parent) {
        // Use a fence to wait until the command buffer has finished execution before using it again
        while (true) {
            switch (parent->waitForFences(parent.in_flight[parent.current_frame], vk::True,
                                          UINT64_MAX)) {
                case vk::Result::eSuccess: {
                    parent->resetFences(parent.in_flight[parent.current_frame]);
                    goto acquire_image;
                }
                case vk::Result::eTimeout:
                    [[unlikely]] continue;
                default:
                    VGI_UNREACHABLE;
                    break;
            }
        }

    acquire_image:
        // Get the next swap chain image from the implementation
        // Note that the implementation is free to return the images in any order, so we must use
        // the acquire function and can't just cycle through the images/imageIndex on our own
        while (true) {
            vk::ResultValue<uint32_t> result{{}, {}};
            try {
                result = parent->acquireNextImageKHR(parent.swapchain, UINT64_MAX,
                                                     parent.image_available[parent.current_frame]);
            } catch (const vk::OutOfDateKHRError&) {
                log_err("Window resizing not yet implemented");
                throw;
            } catch (...) {
                throw;
            }

            switch (result.result) {
                case vk::Result::eSuccess: {
                    this->current_image = result.value;
                    goto timings;
                }
                case vk::Result::eNotReady:
                case vk::Result::eTimeout: {
                    continue;
                }
                case vk::Result::eSuboptimalKHR: {
                    log_warn("Window resizing is not yet implemented");
                    break;
                }
                default:
                    VGI_UNREACHABLE;
                    break;
            }
        }

    timings:
        parent.cmdbufs[parent.current_frame].reset();
        parent.cmdbufs[parent.current_frame].begin(vk::CommandBufferBeginInfo{});

        this->time_point = std::chrono::steady_clock::now();
        if (!parent.first_frame) {
            VGI_ASSERT(!parent.last_frame);
            parent.first_frame = parent.last_frame = this->time_point;
        }

        this->start_time = this->time_point - parent.first_frame.value();
        this->start =
                std::chrono::duration_cast<std::chrono::duration<float>>(this->start_time).count();

        this->delta_time = this->time_point - parent.last_frame.value();
        this->delta =
                std::chrono::duration_cast<std::chrono::duration<float>>(this->delta_time).count();

        parent.last_frame = this->time_point;
    }

    void frame::beginRendering(float r, float g, float b, float a) const {
        vk::RenderingAttachmentInfo color_attachment{
                .imageView = *this,
                .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
                .loadOp = vk::AttachmentLoadOp::eClear,
                .storeOp = vk::AttachmentStoreOp::eStore,
                .clearValue = {.color = {.float32 = {{r, g, b, a}}}},
        };

        (*this)->beginRendering(vk::RenderingInfo{
                .renderArea = {.extent = this->parent.swapchain_info.imageExtent},
                .layerCount = 1,
                .colorAttachmentCount = 1,
                .pColorAttachments = &color_attachment,
                .pDepthAttachment = nullptr,
                .pStencilAttachment = nullptr,
        });
    }

    void frame::bindDescriptorSet(const pipeline& pipeline, const descriptor_pool& pool) {
        (*this)->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline, 0,
                                    pool[this->parent.current_frame], {});
    }

    frame::~frame() noexcept(false) {
        const vk::CommandBuffer& cmdbuf = **this;
        cmdbuf.end();

        parent.queue.submit(
                vk::SubmitInfo{
                        .waitSemaphoreCount = 1,
                        .pWaitSemaphores = &parent.image_available[parent.current_frame],
                        .commandBufferCount = 1,
                        .pCommandBuffers = &cmdbuf,
                        .signalSemaphoreCount = 1,
                        .pSignalSemaphores = &parent.render_complete[parent.current_frame],
                },
                parent.in_flight[parent.current_frame]);

        try {
            switch (parent.queue.presentKHR(vk::PresentInfoKHR{
                    .waitSemaphoreCount = 1,
                    .pWaitSemaphores = &parent.render_complete[parent.current_frame],
                    .swapchainCount = 1,
                    .pSwapchains = &parent.swapchain,
                    .pImageIndices = &this->current_image,
            })) {
                case vk::Result::eSuccess:
                    break;
                case vk::Result::eSuboptimalKHR: {
                    log_warn("Window resizing is not yet implemented");
                    break;
                }
                default:
                    VGI_UNREACHABLE;
                    break;
            }
        } catch (const vk::OutOfDateKHRError&) {
            log_err("Window resizing not yet implemented");
            throw;
        } catch (...) {
            throw;
        }

        parent.current_frame = math::check_add<uint32_t>(parent.current_frame, 1).value_or(0) %
                               window::MAX_FRAMES_IN_FLIGHT;
    }
}  // namespace vgi
