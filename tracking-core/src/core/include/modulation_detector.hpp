#pragma once

// TRK-009b/c (ADR-005): the modulation-correlation laser detector. Per pixel it
// computes spectral power at the modulation bin over a two-period rolling window
// (KTD-1) and admits only pixels whose power AND two-sided spectral purity clear
// the configured floors (KTD-2) — brightness alone cannot distinguish the laser
// from sunbeams, reflections, or a second pointer (ADR-005). Clustering yields at
// most one LaserObservation per frame; more than one surviving cluster is
// fail-closed (KTD-6). Runs per frame after the quality gate; working buffers are
// pre-allocated (§7.1) and detect() does not allocate in our own code paths.

#include "config.hpp"
#include "frame_metadata.hpp"

#include <opencv2/core.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace tracking {

// One laser candidate for the observation seam (plan U6). psd_power / purity /
// peak_brightness are carried for system_health and future disambiguation; the
// tracker consumes only centroid + the radius the seam derives from cluster_area.
struct LaserObservation {
    cv::Point2f centroid_px{0.0F, 0.0F};   // PSD-power-weighted, sub-pixel
    double psd_power = 0.0;                 // peak modulation-bin power over the cluster
    double purity = 0.0;                    // two-sided purity at the peak pixel (0..1)
    double peak_brightness = 0.0;           // max ON-frame intensity at the peak pixel
    double cluster_area_px = 0.0;           // surviving-component area; the U6 seam maps
                                            // radius_px = sqrt(area / pi) (plan R7)
    std::int64_t capture_timestamp_ns = 0;  // newest window frame's capture timestamp (R7)
};

class ModulationDetector {
public:
    // Pre-allocates all working buffers for rows x cols input; startup-only. The
    // window length derives from the capture rate and modulation frequency
    // (2 * target_fps / modulation_frequency_hz, validated integral >= 8 at load).
    ModulationDetector(const LaserConfig& config, int target_fps, int rows, int cols);

    // Adds a quality-admitted frame (grayscale or BGR) to the rolling window.
    // Enforces sequence-number contiguity and a 1.5x-nominal-period capture-gap
    // ceiling (KTD-3); any violation flushes the window, so detection resumes
    // only after a full refill. No heap allocation in our code paths.
    void push_frame(const cv::Mat& frame, const FrameMetadata& metadata);

    // Discards the window and resets the fill counter. Called by the main loop
    // when a frame carries the replay-wrap marker (R4/U5), and internally on a
    // contiguity/gap violation.
    void flush();

    // The single laser observation for the current window, or nullopt when the
    // window is not yet grace-complete, no cluster survives, or more than one
    // cluster survives (fail-closed, KTD-6). OpenCV's morphology and
    // connected-components allocate their own scratch (as documented on
    // BallDetector); this class's own buffers are pre-allocated.
    std::optional<LaserObservation> detect();

    // True once detection_start contiguous admitted frames have been pushed since
    // the last flush: max(grace_period_cycles * frames_per_cycle, window) (KTD-7).
    // RUNNING latching is the caller's concern (SnapshotBuilder, U6).
    bool grace_complete() const { return fill_ >= detection_start_; }

    int window_length() const { return window_; }
    int detection_start() const { return detection_start_; }
    // Frames dropped as >1-cluster ambiguity (surfaced in system_health, KTD-6).
    std::uint64_t ambiguous_detections() const { return ambiguous_; }
    // Window flushes from contiguity/gap/wrap violations (diagnostics).
    std::uint64_t flush_count() const { return flushes_; }

private:
    std::optional<LaserObservation> correlate_and_extract();

    LaserConfig config_;
    int window_ = 0;
    int frames_per_cycle_ = 0;
    int detection_start_ = 0;
    std::int64_t gap_ceiling_ns_ = 0;
    double purity_ratio_threshold_ = 0.0;  // psd_purity_min * window / 2 (unnormalised ratio)

    // DFT bin-2 coefficients over `window_` samples: Re = sum cos*x, Im = sum -sin*x.
    std::vector<double> coef_cos_;
    std::vector<double> coef_negsin_;

    // Rolling window: window_ CV_32F grayscale frames in a circular buffer.
    std::vector<cv::Mat> ring_;
    int write_index_ = 0;      // next slot to overwrite == oldest frame when full
    int fill_ = 0;             // contiguous admitted pushes since last flush (capped)
    bool have_last_ = false;   // a previous admitted frame exists (for contiguity)
    std::uint64_t last_sequence_ = 0;
    std::int64_t last_timestamp_ns_ = 0;

    // Pre-allocated correlation scratch (CV_32F unless noted).
    cv::Mat gray_;             // CV_8UC1 BGR->gray target
    cv::Mat re_, im_, power_, sum_, sumsq_, ac_, ratio_, sq_;
    cv::Mat mask_, mask_power_, mask_purity_;  // CV_8U
    cv::Mat morph_kernel_;
    cv::Mat labels_, stats_, centroids_;       // connectedComponentsWithStats outputs

    std::uint64_t ambiguous_ = 0;
    std::uint64_t flushes_ = 0;
};

}  // namespace tracking
