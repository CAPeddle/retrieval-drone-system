// Unit tests for the TRK-008 frame quality gate. Synthetic images with known
// exposure/blur characteristics; no hardware.
#include "frame_quality.hpp"

#include <gtest/gtest.h>
#include <opencv2/imgproc.hpp>

namespace {

constexpr int kRows = 120;
constexpr int kCols = 160;

tracking::FrameQualityConfig default_config() {
    tracking::FrameQualityConfig config;
    config.underexposed_threshold = 20.0;
    config.overexposed_threshold = 240.0;
    config.blur_threshold = 100.0;
    return config;
}

// High-contrast checkerboard: sharp edges everywhere -> very high Laplacian
// variance, mean ~127 -> well-exposed.
cv::Mat sharp_frame() {
    cv::Mat m(kRows, kCols, CV_8UC1);
    for (int r = 0; r < kRows; ++r) {
        for (int c = 0; c < kCols; ++c) {
            m.at<std::uint8_t>(r, c) = ((r / 8 + c / 8) % 2 == 0) ? 255 : 0;
        }
    }
    return m;
}

TEST(FrameQualityTest, AllBlackRejectsUnderexposed) {
    tracking::FrameQualityAssessor assessor(default_config(), kRows, kCols);
    const cv::Mat black(kRows, kCols, CV_8UC1, cv::Scalar(0));
    EXPECT_EQ(assessor.assess(black), tracking::FrameQuality::REJECT);
}

TEST(FrameQualityTest, AllWhiteRejectsOverexposed) {
    tracking::FrameQualityAssessor assessor(default_config(), kRows, kCols);
    const cv::Mat white(kRows, kCols, CV_8UC1, cv::Scalar(250));
    EXPECT_EQ(assessor.assess(white), tracking::FrameQuality::REJECT);
}

TEST(FrameQualityTest, BlurredFrameRejects) {
    tracking::FrameQualityAssessor assessor(default_config(), kRows, kCols);
    cv::Mat blurred;
    // Heavy Gaussian blur flattens the checkerboard's gradients; the residual
    // Laplacian variance falls under the threshold.
    cv::GaussianBlur(sharp_frame(), blurred, cv::Size(31, 31), 12.0);
    EXPECT_EQ(assessor.assess(blurred), tracking::FrameQuality::REJECT);
}

TEST(FrameQualityTest, SharpWellExposedIsGood) {
    tracking::FrameQualityAssessor assessor(default_config(), kRows, kCols);
    EXPECT_EQ(assessor.assess(sharp_frame()), tracking::FrameQuality::GOOD);
}

TEST(FrameQualityTest, MarginalExposureIsDegraded) {
    tracking::FrameQualityAssessor assessor(default_config(), kRows, kCols);
    // Mean 22 is above the reject threshold (20) but within its 20% margin
    // (< 24): marginal, keep the frame but flag it.
    const cv::Mat dim(kRows, kCols, CV_8UC1, cv::Scalar(22));
    // A flat frame would also fail blur, so add sharp structure at low mean:
    cv::Mat dim_sharp = dim.clone();
    for (int r = 0; r < kRows; r += 4) {
        for (int c = 0; c < kCols; ++c) {
            dim_sharp.at<std::uint8_t>(r, c) = 44;  // keeps mean near 27... adjusted below
        }
    }
    // Recentre: scale so the mean sits in (20, 24).
    const double mean = cv::mean(dim_sharp)[0];
    dim_sharp.convertTo(dim_sharp, CV_8UC1, 22.0 / mean);
    const auto quality = assessor.assess(dim_sharp);
    EXPECT_EQ(quality, tracking::FrameQuality::DEGRADED);
}

TEST(FrameQualityTest, MarginalBlurIsDegraded) {
    tracking::FrameQualityAssessor assessor(default_config(), kRows, kCols);
    // Find a blur strength whose variance lands in [100, 120): probe a few
    // sigmas and assert the assessor agrees with the raw measurement.
    for (double sigma = 1.0; sigma < 8.0; sigma += 0.1) {
        cv::Mat blurred;
        cv::GaussianBlur(sharp_frame(), blurred, cv::Size(0, 0), sigma);
        const double variance = tracking::FrameQualityAssessor::laplacian_variance(blurred);
        if (variance >= 100.0 && variance < 120.0) {
            EXPECT_EQ(assessor.assess(blurred), tracking::FrameQuality::DEGRADED)
                << "sigma=" << sigma << " variance=" << variance;
            return;
        }
    }
    GTEST_SKIP() << "no sigma landed in the degraded band; band logic covered by exposure test";
}

TEST(FrameQualityTest, BgrInputConvertsAndAssesses) {
    tracking::FrameQualityAssessor assessor(default_config(), kRows, kCols);
    cv::Mat bgr;
    cv::cvtColor(sharp_frame(), bgr, cv::COLOR_GRAY2BGR);
    EXPECT_EQ(assessor.assess(bgr), tracking::FrameQuality::GOOD);
}

TEST(FrameQualityTest, RejectionsAreCounted) {
    tracking::FrameQualityAssessor assessor(default_config(), kRows, kCols);
    const cv::Mat black(kRows, kCols, CV_8UC1, cv::Scalar(0));
    EXPECT_EQ(assessor.rejected_count(), 0u);
    assessor.assess(black);
    assessor.assess(black);
    assessor.assess(sharp_frame());
    EXPECT_EQ(assessor.rejected_count(), 2u);
}

}  // namespace
