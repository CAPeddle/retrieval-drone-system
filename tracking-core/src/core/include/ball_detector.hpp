#pragma once

// TRK-010: per-frame ball detector. Under the NoIR camera the foam ball is a
// bright blob distinguishable by size and circularity; this detector isolates
// it with a fixed brightness threshold and contour analysis (no temporal
// correlation — that is the laser's job). Runs per frame after the quality
// gate, so working buffers are pre-allocated (§7.1) and detect() does not
// allocate in our own code paths.

#include "config.hpp"

#include <opencv2/core.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace tracking {

struct BallObservation {
    cv::Point2f centroid_px{0.0f, 0.0f};  // sub-pixel, from image moments
    double radius_px = 0.0;               // sqrt(area / pi) from the contour
    double confidence = 0.0;              // provisional: equals circularity (TRK-015 may redefine)
    double circularity = 0.0;             // 4*pi*area / perimeter^2
};

class BallDetector {
public:
    // Pre-allocates working buffers for rows x cols input; startup-only.
    BallDetector(const BallConfig& config, int rows, int cols);

    // Returns the best ball candidate passing all gates, or nullopt. Accepts
    // grayscale or BGR frames of the constructed size. No heap allocation in
    // our code paths; cv::findContours allocates its own output vectors
    // internally (OpenCV 4.x) — the reused members are reserve()'d to minimise
    // that, and the strict no-alloc claim covers only what this class controls.
    std::optional<BallObservation> detect(const cv::Mat& frame);

private:
    BallConfig config_;
    cv::Mat gray_;    // pre-allocated grayscale conversion target
    cv::Mat blurred_; // pre-allocated blur target
    cv::Mat binary_;  // pre-allocated threshold/morph target
    cv::Mat morph_kernel_;
    std::vector<std::vector<cv::Point>> contours_;  // reserve()'d, reused
    std::vector<cv::Point> hull_;                   // reused convex-hull scratch
};

}  // namespace tracking
