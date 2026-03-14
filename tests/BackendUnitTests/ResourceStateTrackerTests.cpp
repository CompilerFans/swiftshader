#include "Backend/ResourceStateTracker.hpp"

#include <gtest/gtest.h>

namespace {

template<typename Handle>
Handle makeTestHandle(uint64_t id)
{
    if constexpr(std::is_pointer_v<Handle>)
    {
        return reinterpret_cast<Handle>(static_cast<uintptr_t>(id));
    }
    else if constexpr(std::is_convertible_v<Handle, void *>)
    {
        Handle handle = {};
        handle = id;
        return handle;
    }
    else
    {
        return static_cast<Handle>(id);
    }
}

}  // namespace

TEST(ResourceStateTracker, TransitionUpdatesLogicalLayout)
{
    backend::ResourceStateTracker tracker;
    tracker.transitionImage(1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    EXPECT_EQ(tracker.layoutForImage(1), VK_IMAGE_LAYOUT_GENERAL);
}

TEST(ResourceStateTracker, TracksImageBarriersFromDependencyInfo)
{
    backend::ResourceStateTracker tracker;
    backend::resetResourceStateTrackerCapture();

    constexpr uint64_t kImageId = 0x1234;
    VkImageMemoryBarrier2 imageBarrier = {};
    imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.image = makeTestHandle<VkImage>(kImageId);
    imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageBarrier.subresourceRange.baseMipLevel = 0;
    imageBarrier.subresourceRange.levelCount = 1;
    imageBarrier.subresourceRange.baseArrayLayer = 0;
    imageBarrier.subresourceRange.layerCount = 1;

    VkDependencyInfo dependencyInfo = {};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers = &imageBarrier;

    tracker.trackDependencyInfo(dependencyInfo);

    EXPECT_EQ(tracker.layoutForImage(kImageId), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    const auto &capture = backend::lastResourceStateTrackerCapture();
    EXPECT_EQ(capture.imageTransitionCount, 1u);
    EXPECT_EQ(capture.lastImageId, kImageId);
    EXPECT_EQ(capture.lastOldLayout, VK_IMAGE_LAYOUT_UNDEFINED);
    EXPECT_EQ(capture.lastNewLayout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
}
