#include <gtest/gtest.h>

#include <opencv2/core.hpp>

#include "include/tracking_pipeline.hpp"

TEST(DetectorTest, ReturnsEmptyForEmptyFrame) {
    tracking::Detector detector;
    const cv::Rect box = detector.detect(cv::Mat());
    EXPECT_EQ(box.area(), 0);
}

TEST(TrackerTest, PersistsLastNonEmptyDetection) {
    tracking::Tracker tracker;
    const cv::Rect first = tracker.update(cv::Rect(10, 10, 20, 20));
    const cv::Rect second = tracker.update(cv::Rect());

    EXPECT_EQ(first, second);
}
