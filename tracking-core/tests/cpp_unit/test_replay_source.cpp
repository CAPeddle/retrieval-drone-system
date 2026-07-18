// TRK-031: hermetic ReplaySource tests. The clip under test is generated
// in-test with cv::VideoWriter — no recordings, hardware, or network needed.
#include "replay_source.hpp"

#include <gtest/gtest.h>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

#include <chrono>
#include <string>

namespace {

constexpr int kRows = 48;
constexpr int kCols = 64;
constexpr int kFrames = 10;

// Writes an MJPG AVI whose frames are solid values 10*i, returns its path.
std::string write_clip(const std::string& name) {
    const std::string path = ::testing::TempDir() + "trk031_" + name + ".avi";
    cv::VideoWriter writer(path, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), 15.0,
                           cv::Size(kCols, kRows), /*isColor=*/true);
    EXPECT_TRUE(writer.isOpened());
    for (int i = 0; i < kFrames; ++i) {
        writer.write(cv::Mat(kRows, kCols, CV_8UC3,
                             cv::Scalar(10.0 * i, 10.0 * i, 10.0 * i)));
    }
    return path;
}

TEST(ReplaySourceTest, PlaysFramesInOrderThenFails) {
    tracking::ReplaySource source(write_clip("order"));
    ASSERT_TRUE(source.is_open());
    cv::Mat frame;
    for (int i = 0; i < kFrames; ++i) {
        ASSERT_TRUE(source.grab(frame)) << "frame " << i;
        EXPECT_EQ(frame.rows, kRows);
        EXPECT_EQ(frame.cols, kCols);
        // MJPG is lossy; solid frames stay within a few grey levels.
        EXPECT_NEAR(cv::mean(frame)[0], 10.0 * i, 4.0) << "frame " << i;
    }
    // End of clip behaves like a camera disconnect.
    EXPECT_FALSE(source.grab(frame));
    EXPECT_FALSE(source.grab(frame));
}

TEST(ReplaySourceTest, LoopModeRestartsAtEnd) {
    tracking::ReplaySource::Options options;
    options.loop = true;
    tracking::ReplaySource source(write_clip("loop"), options);
    ASSERT_TRUE(source.is_open());
    cv::Mat frame;
    for (int i = 0; i < kFrames * 2 + 3; ++i) {
        ASSERT_TRUE(source.grab(frame)) << "frame " << i;
    }
    // 3 frames into the third pass: value == 10*2 = 20.
    EXPECT_NEAR(cv::mean(frame)[0], 20.0, 4.0);
}

TEST(ReplaySourceTest, PacingHoldsConfiguredRate) {
    tracking::ReplaySource::Options options;
    options.fps = 100.0;  // pace 10 frames at 100 fps ~= 90 ms span
    tracking::ReplaySource source(write_clip("paced"), options);
    ASSERT_TRUE(source.is_open());
    cv::Mat frame;
    const auto start = std::chrono::steady_clock::now();
    while (source.grab(frame)) {
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    // Generous bounds (dev machine): unpaced would finish in ~1 ms; a paced
    // run must take at least the nominal span and not wildly more.
    EXPECT_GE(elapsed.count(), 70);
    EXPECT_LT(elapsed.count(), 2000);
}

TEST(ReplaySourceTest, MissingFileReportsClosed) {
    tracking::ReplaySource source("/nonexistent/trk031.avi");
    EXPECT_FALSE(source.is_open());
    cv::Mat frame;
    EXPECT_FALSE(source.grab(frame));
}

}  // namespace
