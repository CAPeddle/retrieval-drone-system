#pragma once

// TRK-008: frame quality gate — the first processing stage after ingestion.
// Cheap checks (histogram mean + Laplacian variance) that stop under/over-
// exposed or motion-blurred frames from wasting detection budget and
// producing unreliable observations. Not a hot-path bottleneck, but it runs
// per frame, so its working buffers are pre-allocated (§7.1).

#include "config.hpp"

#include <opencv2/core.hpp>

#include <cstdint>

namespace tracking {

enum class FrameQuality {
    GOOD,      // enters the detection pipeline
    DEGRADED,  // marginal (within 20% of a threshold) — continue but flag
    REJECT,    // discard before detection
};

class FrameQualityAssessor {
public:
    // Pre-allocates grayscale and Laplacian working buffers for rows x cols
    // input. Startup-only; assess() then never allocates for frames of the
    // constructed size (grayscale or BGR).
    FrameQualityAssessor(const FrameQualityConfig& config, int rows, int cols);

    // Per-frame gate. Order: exposure (cv::mean, cheap) rejects before the
    // Laplacian runs; DEGRADED when a value clears its threshold by less
    // than 20% of the threshold.
    FrameQuality assess(const cv::Mat& frame);

    // Frames this assessor has rejected since construction (system health
    // reporting, TRK-023 consumes this).
    std::uint64_t rejected_count() const { return rejected_count_; }

    // Exposed for tests calibrating the DEGRADED band.
    static double laplacian_variance(const cv::Mat& gray);

private:
    FrameQualityConfig config_;
    cv::Mat gray_;       // pre-allocated conversion target
    cv::Mat laplacian_;  // pre-allocated CV_64F Laplacian response
    std::uint64_t rejected_count_ = 0;
};

}  // namespace tracking
