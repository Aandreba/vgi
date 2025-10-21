#include "cmdbuf.hpp"

#include "defs.hpp"
#include "math.hpp"
#include "vgi.hpp"

namespace vgi {
    command_buffer::command_buffer(window& parent) : parent(parent) {
        // Find if any of the flying command buffers has finished already
        for (size_t i = 0; i < parent.flying_cmdbufs.size(); ++i) {
            window::flying_command_buffer& flying = parent.flying_cmdbufs[i];
            switch (parent->getFenceStatus(flying.fence)) {
                case vk::Result::eSuccess: {
                    // This cmdbuf has finished execution, reuse it and it's fence
                    parent->resetFences(flying.fence);
                    flying.cmdbuf.reset();
                    this->cmdbuf = flying.cmdbuf;
                    this->fence = flying.fence;
                    std::swap(flying, parent.flying_cmdbufs.back());
                    parent.flying_cmdbufs.pop_back();
                    goto found;
                }
                case vk::Result::eNotReady:
                    continue;
                default:
                    VGI_UNREACHABLE;
                    break;
            }
        }

        // No free command buffer was found, allocate a new one
        this->fence = parent->createFence({});
        vkn::allocateCommandBuffers(
                parent,
                vk::CommandBufferAllocateInfo{.commandPool = parent.cmdpool,
                                              .level = vk::CommandBufferLevel::ePrimary,
                                              .commandBufferCount = 1},
                &this->cmdbuf);

    found:
        this->cmdbuf.begin(vk::CommandBufferBeginInfo{
                .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    }

    void command_buffer::raw_submit(std::span<const vk::Semaphore> signal_semaphores) {
        VGI_ASSERT(this->cmdbuf && this->fence);

        std::optional<uint32_t> signal_count = math::check_cast<uint32_t>(signal_semaphores.size());
        if (!signal_count) throw vgi_error{"too many signal semaphores"};

        this->cmdbuf.end();
        this->parent.queue.submit(
                vk::SubmitInfo{
                        .commandBufferCount = 1,
                        .pCommandBuffers = &this->cmdbuf,
                        .signalSemaphoreCount = signal_count.value(),
                        .pSignalSemaphores = signal_semaphores.data(),
                },
                this->fence);
    }

    void command_buffer::submit(std::span<const vk::Semaphore> signal_semaphores) && {
        this->raw_submit(signal_semaphores);
        this->parent.flying_cmdbufs.emplace_back(std::exchange(this->cmdbuf, nullptr),
                                                 std::exchange(this->fence, nullptr));
    }

    void command_buffer::submit_and_wait() && {
        this->raw_submit();
        try {
            while (true) {
                switch (this->parent->waitForFences(this->fence, vk::True, UINT64_MAX)) {
                    case vk::Result::eSuccess: {
                        // Since we know this command buffer has already finished, put it at the
                        // front of the list so the next time we need a command buffer we can find
                        // an available one quickly
                        this->parent.flying_cmdbufs.emplace_front(
                                std::exchange(this->cmdbuf, nullptr),
                                std::exchange(this->fence, nullptr));
                        return;
                    }
                    case vk::Result::eTimeout:
                        [[unlikely]] continue;
                    default:
                        VGI_UNREACHABLE;
                        break;
                }
            }
        } catch (...) {
            // We don't know what happend to the command buffer, so better put it at the back of the
            // queue
            this->parent.flying_cmdbufs.emplace_back(std::exchange(this->cmdbuf, nullptr),
                                                     std::exchange(this->fence, nullptr));
            throw;
        }
    }

    bool command_buffer::submit_and_wait(const std::chrono::duration<uint64_t, std::nano>& d) && {
        this->raw_submit();
        try {
            switch (this->parent->waitForFences(this->fence, vk::True, d.count())) {
                case vk::Result::eSuccess: {
                    // Since we know this command buffer has already finished, put it at the front
                    // of the list so the next time we need a command buffer we can find an
                    // available one quickly
                    this->parent.flying_cmdbufs.emplace_front(std::exchange(this->cmdbuf, nullptr),
                                                              std::exchange(this->fence, nullptr));
                    return true;
                }
                case vk::Result::eTimeout: {
                    // THe command buffer is still working, so we put it at the back of the list.
                    this->parent.flying_cmdbufs.emplace_back(std::exchange(this->cmdbuf, nullptr),
                                                             std::exchange(this->fence, nullptr));
                    return false;
                }
                default:
                    VGI_UNREACHABLE;
            }
        } catch (...) {
            // We don't know what happend to the command buffer, so better put it at the back of the
            // queue
            this->parent.flying_cmdbufs.emplace_back(std::exchange(this->cmdbuf, nullptr),
                                                     std::exchange(this->fence, nullptr));
            throw;
        }
    }

    command_buffer::~command_buffer() {
        if (this->cmdbuf) this->parent->freeCommandBuffers(this->parent.cmdpool, this->cmdbuf);
        if (this->fence) this->parent->destroyFence(this->fence);
    }
}  // namespace vgi
