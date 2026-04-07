#include "VulkanApplication.hpp"

#include <iostream>

#include "globals.hpp"

void VulkanApplication::mainLoop()
{
    int fCounter = 0;

    // Loop until told to close
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        drawFrame();

        double currentTime = glfwGetTime();
        lastFrameTime = (currentTime - lastTime) * 1000.0;
        lastTime = currentTime;

        if (fCounter > 500)
        {
            std::cout << "FPS: " << 1000 / lastFrameTime << "\n";
            fCounter = 0;
        }
        else
        {
            fCounter++;
        }
    }

    device.waitIdle();
}


void VulkanApplication::drawFrame()
{            
    auto [result, imageIndex] = swapChain.acquireNextImage(UINT64_MAX, nullptr, *inFlightFences[frameIndex]);
    vk::Result fenceResult = device.waitForFences(*inFlightFences[frameIndex], vk::True, UINT64_MAX);

    if (fenceResult != vk::Result::eSuccess)
    {
        throw std::runtime_error("Failed to wait for fence!");
    }
    
    if (result == vk::Result::eErrorOutOfDateKHR)
    {
        recreateSwapChain();
        return;
    }
    if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
    {
        assert(result == vk::Result::eTimeout || result == vk::Result::eNotReady);
        throw std::runtime_error("Failed to acquire swap chain image!");
    }

    // Only reset fence if above passes
    device.resetFences(*inFlightFences[frameIndex]);

    // Update timeline value for this frame
    uint64_t computeWaitValue = timelineValue;
    uint64_t computeSignalValue = ++timelineValue;
    uint64_t graphicsWaitValue = computeSignalValue;
    uint64_t graphicsSignalValue = ++timelineValue;

    updateUniformBuffer(frameIndex);

    {// Needs to be in braces for some reason?
        recordComputeCommandBuffer();

        //Submit compute work
        vk::TimelineSemaphoreSubmitInfo computeTimelineInfo{.waitSemaphoreValueCount = 1,
                                                            .pWaitSemaphoreValues = &computeWaitValue,
                                                            .signalSemaphoreValueCount = 1,
                                                            .pSignalSemaphoreValues = &computeSignalValue};

        vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eComputeShader};

        vk::SubmitInfo computeSubmitInfo{.pNext = &computeTimelineInfo,
                                            .waitSemaphoreCount = 1,
                                            .pWaitSemaphores = &*semaphore,
                                            .pWaitDstStageMask = waitStages,
                                            .commandBufferCount = 1,
                                            .pCommandBuffers = &*computeCommandBuffers[frameIndex],
                                            .signalSemaphoreCount = 1,
                                            .pSignalSemaphores = &*semaphore};

        queue.submit(computeSubmitInfo, nullptr);
    }

    {
        // Record graphics command bufffer
        recordCommandBuffer(imageIndex);

        // Submit graphics work (after waiting on compute)
        vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eVertexInput;

        vk::TimelineSemaphoreSubmitInfo graphicsTimelineInfo{.waitSemaphoreValueCount = 1,
                                                                .pWaitSemaphoreValues = &graphicsWaitValue,
                                                                .signalSemaphoreValueCount = 1,
                                                                .pSignalSemaphoreValues = &graphicsSignalValue};

        vk::SubmitInfo graphicsSubmitInfo{.pNext = &graphicsTimelineInfo,
                                            .waitSemaphoreCount = 1,
                                            .pWaitSemaphores = &*semaphore,
                                            .pWaitDstStageMask = &waitStage,
                                            .commandBufferCount = 1,
                                            .pCommandBuffers = &*commandBuffers[frameIndex],
                                            .signalSemaphoreCount = 1,
                                            .pSignalSemaphores = &*semaphore};

        queue.submit(graphicsSubmitInfo, nullptr);

        // Present image (after waiting for graphics)
        vk::SemaphoreWaitInfo waitInfo{.semaphoreCount = 1,
                                        .pSemaphores = &*semaphore,
                                        .pValues = &graphicsSignalValue};

        vk::Result result = device.waitSemaphores(waitInfo, UINT64_MAX);
        if (result != vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to wait for semaphore!");
        }

        vk::PresentInfoKHR presentInfo{.waitSemaphoreCount = 0,
                                        .pWaitSemaphores = nullptr,
                                        .swapchainCount = 1,
                                        .pSwapchains = &*swapChain,
                                        .pImageIndices = &imageIndex};

        result = queue.presentKHR(presentInfo);

        // Due to VULKAN_HPP_HANDLE_ERROR_OUT_DATE_AS_SUCCESS being defined, we can check to see if the screen gets resized here
        if ((result == vk::Result::eSuboptimalKHR) || (result == vk::Result::eErrorOutOfDateKHR) || frameBufferResized)
        {
            frameBufferResized = false;
            recreateSwapChain();
        }
        else
        {
            assert(result == vk::Result::eSuccess);
        }
    }

    frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
}