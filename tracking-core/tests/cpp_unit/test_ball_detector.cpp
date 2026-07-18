// Unit tests for the TRK-010 ball detector. Synthetic images only, no hardware.
#include "ball_detector.hpp"

#include <gtest/gtest.h>
#include <opencv2/imgproc.hpp>

#include <optional>

namespace {

constexpr int kRows = 480;
constexpr int kCols = 640;

tracking::BallConfig default_config() {
    tracking::BallConfig config;
    config.radius_m = 0.03;
    config.expected_radius_px_min = 10;
    config.expected_radius_px_max = 80;
    config.min_circularity = 0.82;  // above a square's ceiling (pi/4 ~= 0.785)
    config.detection_blur_kernel = 5;
    config.brightness_threshold = 128;
    return config;
}

// A bright filled circle on a dark background — the NoIR ball model.
cv::Mat frame_with_circle(cv::Point centre, int radius, int channels = 1) {
    cv::Mat frame(kRows, kCols, channels == 3 ? CV_8UC3 : CV_8UC1, cv::Scalar::all(0));
    cv::circle(frame, centre, radius, cv::Scalar::all(255), cv::FILLED);
    return frame;
}

TEST(BallDetectorTest, DetectsSyntheticCircle) {
    tracking::BallDetector detector(default_config(), kRows, kCols);
    const cv::Point truth(320, 240);
    const int radius = 30;
    const auto obs = detector.detect(frame_with_circle(truth, radius));
    ASSERT_TRUE(obs.has_value());
    EXPECT_NEAR(obs->centroid_px.x, truth.x, 1.5);
    EXPECT_NEAR(obs->centroid_px.y, truth.y, 1.5);
    EXPECT_NEAR(obs->radius_px, radius, radius * 0.10);
    EXPECT_GT(obs->circularity, 0.9);
}

TEST(BallDetectorTest, UniformFrameReturnsNullopt) {
    tracking::BallDetector detector(default_config(), kRows, kCols);
    EXPECT_FALSE(detector.detect(cv::Mat(kRows, kCols, CV_8UC1, cv::Scalar(0))).has_value());
    EXPECT_FALSE(detector.detect(cv::Mat(kRows, kCols, CV_8UC1, cv::Scalar(255))).has_value());
}

TEST(BallDetectorTest, PrefersCircularOverSquare) {
    tracking::BallDetector detector(default_config(), kRows, kCols);
    cv::Mat frame(kRows, kCols, CV_8UC1, cv::Scalar(0));
    // A square (low circularity) and a circle (high) both within the area band.
    cv::rectangle(frame, cv::Rect(80, 200, 60, 60), cv::Scalar(255), cv::FILLED);
    cv::circle(frame, cv::Point(460, 240), 30, cv::Scalar(255), cv::FILLED);
    const auto obs = detector.detect(frame);
    ASSERT_TRUE(obs.has_value());
    EXPECT_NEAR(obs->centroid_px.x, 460, 3.0);  // the circle won
    EXPECT_GT(obs->circularity, 0.85);
}

TEST(BallDetectorTest, RejectsTooSmallAndTooLarge) {
    tracking::BallDetector detector(default_config(), kRows, kCols);
    // Below expected_radius_px_min (10).
    EXPECT_FALSE(detector.detect(frame_with_circle({320, 240}, 4)).has_value());
    // Above expected_radius_px_max (80).
    EXPECT_FALSE(detector.detect(frame_with_circle({320, 240}, 120)).has_value());
}

TEST(BallDetectorTest, EdgeBallDoesNotCrashOrReturnOutOfRange) {
    tracking::BallDetector detector(default_config(), kRows, kCols);
    // Circle centred on the frame corner: only a quarter is visible.
    const auto obs = detector.detect(frame_with_circle({0, 0}, 40));
    // Either rejected, or accepted with an in-range radius — never a crash and
    // never an observation outside the configured band.
    if (obs.has_value()) {
        EXPECT_GE(obs->radius_px, 10.0);
        EXPECT_LE(obs->radius_px, 80.0);
    }
    SUCCEED();
}

TEST(BallDetectorTest, HandlesBgrAndGrayscaleInput) {
    tracking::BallDetector detector(default_config(), kRows, kCols);
    EXPECT_TRUE(detector.detect(frame_with_circle({320, 240}, 30, 1)).has_value());
    EXPECT_TRUE(detector.detect(frame_with_circle({320, 240}, 30, 3)).has_value());
}

TEST(BallDetectorTest, ConfidenceIsCircularityForNow) {
    tracking::BallDetector detector(default_config(), kRows, kCols);
    const auto obs = detector.detect(frame_with_circle({320, 240}, 30));
    ASSERT_TRUE(obs.has_value());
    EXPECT_DOUBLE_EQ(obs->confidence, obs->circularity);
}

}  // namespace
