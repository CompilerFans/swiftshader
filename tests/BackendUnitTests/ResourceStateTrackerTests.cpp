#include "Backend/ResourceStateTracker.hpp"

#include <gtest/gtest.h>

TEST(ResourceStateTracker, TransitionUpdatesLogicalLayout)
{
    backend::ResourceStateTracker tracker;
    tracker.transitionImage(1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    EXPECT_EQ(tracker.layoutForImage(1), VK_IMAGE_LAYOUT_GENERAL);
}
