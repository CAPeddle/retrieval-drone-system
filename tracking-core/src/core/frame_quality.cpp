#include "frame_quality.hpp"

#include <opencv2/imgproc.hpp>

namespace tracking {

namespace {

constexpr double kDegradedMarginFraction = 0.2;  // "within 20% of thresholds"

}  // namespace

FrameQualityAssessor::FrameQualityAssessor(const FrameQualityConfig& config, int rows,
                                           int cols)
    : config_(config) {
    gray_.create(rows, cols, CV_8UC1);
    laplacian_.create(rows, cols, CV_64F);
}

double FrameQualityAssessor::laplacian_variance(const cv::Mat& gray) {
    cv::Mat laplacian;
    cv::Laplacian(gray, laplacian, CV_64F);
    cv::Scalar mean;
    cv::Scalar stddev;
    cv::meanStdDev(laplacian, mean, stddev);
    return stddev[0] * stddev[0];
}

FrameQuality FrameQualityAssessor::assess(const cv::Mat& frame) {
    const cv::Mat* gray = &frame;
    if (frame.channels() == 3) {
        cv::cvtColor(frame, gray_, cv::COLOR_BGR2GRAY);  // into pre-allocated buffer
        gray = &gray_;
    }

    // Exposure first: cv::mean is far cheaper than the Laplacian, and a hard
    // exposure failure means the blur number is meaningless anyway.
    const double mean = cv::mean(*gray)[0];
    const double under = config_.underexposed_threshold;
    const double over = config_.overexposed_threshold;
    if (mean < under || mean > over) {
        ++rejected_count_;
        return FrameQuality::REJECT;
    }
    const bool exposure_marginal =
        mean < under * (1.0 + kDegradedMarginFraction) ||
        mean > over * (1.0 - kDegradedMarginFraction);

    cv::Laplacian(*gray, laplacian_, CV_64F);  // into pre-allocated buffer
    cv::Scalar lap_mean;
    cv::Scalar lap_stddev;
    cv::meanStdDev(laplacian_, lap_mean, lap_stddev);
    const double variance = lap_stddev[0] * lap_stddev[0];
    if (variance < config_.blur_threshold) {
        ++rejected_count_;
        return FrameQuality::REJECT;
    }
    const bool blur_marginal =
        variance < config_.blur_threshold * (1.0 + kDegradedMarginFraction);

    return (exposure_marginal || blur_marginal) ? FrameQuality::DEGRADED
                                                : FrameQuality::GOOD;
}

}  // namespace tracking
