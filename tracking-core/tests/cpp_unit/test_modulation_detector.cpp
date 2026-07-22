// TRK-009b/c (U3/U4/U5): ModulationDetector unit + rejection suite. Drives the
// detector with the shared synthetic modulation fixture (U1). The rejection
// scenarios (step at every offset, translating edge, off-bin interferer) are
// first-class gates: the two-period window plus the purity floor must produce
// ZERO detections on any transient a naive single-period detector would fire on
// (KTD-1). All thresholds here are the provisional config defaults (R14).

#include "modulation_detector.hpp"
#include "modulation_fixture.hpp"

#include <gtest/gtest.h>
#include <opencv2/imgproc.hpp>

#include <optional>
#include <vector>

namespace {

using modulation_fixture::DotSpec;
using modulation_fixture::Frame;
using tracking::LaserObservation;
using tracking::ModulationDetector;

constexpr int kFps = 60;
constexpr int kRows = modulation_fixture::kRows;
constexpr int kCols = modulation_fixture::kCols;

tracking::LaserConfig MakeConfig() {
    tracking::LaserConfig c;
    c.modulation_frequency_hz = 15.0;
    c.modulation_duty_cycle = 0.5;
    c.psd_power_min = 2500.0;
    c.psd_purity_min = 0.5;
    c.min_cluster_size_px = 4;
    c.max_cluster_size_px = 6000;
    c.grace_period_cycles = 2;
    return c;
}

// Pushes every frame and records detect() after each — the per-frame sliding
// window the live loop drives.
std::vector<std::optional<LaserObservation>> RunDetector(ModulationDetector& d,
                                                 const std::vector<Frame>& frames) {
    std::vector<std::optional<LaserObservation>> out;
    out.reserve(frames.size());
    for (const Frame& f : frames) {
        d.push_frame(f.image, f.meta);
        out.push_back(d.detect());
    }
    return out;
}

int CountDetections(const std::vector<std::optional<LaserObservation>>& r) {
    int n = 0;
    for (const auto& o : r) {
        if (o.has_value()) {
            ++n;
        }
    }
    return n;
}

// A synchronized second disc painted onto ON frames — a second modulated cluster
// for the fail-closed test. Painted opaque so it forms its own component.
void PaintSecondDot(std::vector<Frame>& frames, cv::Point centre, int radius,
                    int value, double freq) {
    for (std::size_t i = 0; i < frames.size(); ++i) {
        if (modulation_fixture::square_on(freq, 0.0, static_cast<int>(i))) {
            cv::circle(frames[i].image, centre, radius, cv::Scalar(value), -1);
        }
    }
}

// ─── U3: detection and rejection ─────────────────────────────────────────────

TEST(ModulationDetector, DetectsTrueModulatedDot) {
    DotSpec spec;
    spec.cx = 200.0;
    spec.cy = 240.0;
    spec.amplitude = 80.0;
    ModulationDetector d(MakeConfig(), kFps, kRows, kCols);
    const auto r = RunDetector(d, modulation_fixture::modulated_dot(1, spec, 16));

    ASSERT_GT(CountDetections(r), 0);
    // Steady-state window (frame 15) must place the dot at ground truth.
    ASSERT_TRUE(r[15].has_value());
    EXPECT_NEAR(r[15]->centroid_px.x, spec.cx, 1.0);
    EXPECT_NEAR(r[15]->centroid_px.y, spec.cy, 1.0);
}

TEST(ModulationDetector, NoOutputBeforeGraceComplete) {
    DotSpec spec;
    spec.cx = 200.0;
    spec.cy = 240.0;
    spec.amplitude = 80.0;
    ModulationDetector d(MakeConfig(), kFps, kRows, kCols);
    const auto r = RunDetector(d, modulation_fixture::modulated_dot(1, spec, 16));
    // detection_start == 8: no observation is possible before the 8th push.
    for (int i = 0; i < 7; ++i) {
        EXPECT_FALSE(r[i].has_value()) << "frame " << i << " fired before grace complete";
    }
    EXPECT_EQ(d.window_length(), 8);
    EXPECT_EQ(d.detection_start(), 8);
}

TEST(ModulationDetector, RejectsStaticScene) {
    // static_step with an offset beyond the clip is a constant field — no
    // modulation, no detection.
    ModulationDetector d(MakeConfig(), kFps, kRows, kCols);
    const auto r = RunDetector(d, modulation_fixture::static_step(2, /*offset=*/100, 16));
    EXPECT_EQ(CountDetections(r), 0);
}

TEST(ModulationDetector, RejectsThirtyHzAlternation) {
    // 30 Hz at 60 fps alternates every frame -> bin 4, zero power at the
    // modulation bin (bin 2).
    DotSpec spec;
    spec.cx = 200.0;
    spec.cy = 240.0;
    spec.amplitude = 80.0;
    spec.freq_hz = 30.0;
    ModulationDetector d(MakeConfig(), kFps, kRows, kCols);
    const auto r = RunDetector(d, modulation_fixture::modulated_dot(3, spec, 16));
    EXPECT_EQ(CountDetections(r), 0);
}

TEST(ModulationDetector, RejectsStepAtEveryWindowOffset) {
    // The KTD-1 gate: a single luminance step, wherever it lands in the window,
    // must never be admitted. Sliding the window across the step at each start
    // offset places the transition at every in-window position.
    for (int offset = 8; offset <= 16; ++offset) {
        ModulationDetector d(MakeConfig(), kFps, kRows, kCols);
        const auto r = RunDetector(d, modulation_fixture::static_step(4, offset, 24));
        EXPECT_EQ(CountDetections(r), 0) << "step at offset " << offset << " fired";
    }
}

TEST(ModulationDetector, RejectsTranslatingEdge) {
    ModulationDetector d(MakeConfig(), kFps, kRows, kCols);
    const auto r = RunDetector(d, modulation_fixture::translating_edge(5, 16));
    EXPECT_EQ(CountDetections(r), 0);
}

TEST(ModulationDetector, RejectsOffFrequencyInterferer) {
    DotSpec spec;
    spec.cx = 200.0;
    spec.cy = 240.0;
    spec.amplitude = 80.0;
    ModulationDetector d(MakeConfig(), kFps, kRows, kCols);
    // 20 Hz decoheres across two 15 Hz periods -> fails the purity gate.
    const auto r = RunDetector(d, modulation_fixture::off_frequency_dot(6, spec, 16, 20.0));
    EXPECT_EQ(CountDetections(r), 0);
}

TEST(ModulationDetector, DimDotAtFloorBoundary) {
    DotSpec below;
    below.cx = 200.0;
    below.cy = 240.0;
    below.amplitude = 14.0;  // centre power ~8*14^2 = 1568 < 2500 floor
    ModulationDetector d_below(MakeConfig(), kFps, kRows, kCols);
    EXPECT_EQ(CountDetections(RunDetector(d_below, modulation_fixture::modulated_dot(7, below, 16))), 0);

    DotSpec above = below;
    above.amplitude = 30.0;  // centre power ~8*30^2 = 7200 > floor
    ModulationDetector d_above(MakeConfig(), kFps, kRows, kCols);
    EXPECT_GT(CountDetections(RunDetector(d_above, modulation_fixture::modulated_dot(7, above, 16))), 0);
}

TEST(ModulationDetector, DetectsFractionalPhaseDot) {
    // Worst-case edge-in-exposure schedule [a,a/2,0,a/2] — ~3 dB down but still a
    // clean on-bin tone, so it must detect at the provisional thresholds (R3).
    DotSpec spec;
    spec.cx = 200.0;
    spec.cy = 240.0;
    spec.amplitude = 100.0;
    ModulationDetector d(MakeConfig(), kFps, kRows, kCols);
    const auto r = RunDetector(d, modulation_fixture::fractional_phase_dot(8, spec, 16));
    EXPECT_GT(CountDetections(r), 0);
}

TEST(ModulationDetector, DetectsOnBinInterfererAsExpected) {
    // 45 Hz aliases onto the 15 Hz bin: DETECTED is the correct behaviour (R3),
    // recorded as a residual exposure in the ADR-005 amendment (U9).
    DotSpec spec;
    spec.cx = 200.0;
    spec.cy = 240.0;
    spec.amplitude = 80.0;
    ModulationDetector d(MakeConfig(), kFps, kRows, kCols);
    const auto r = RunDetector(d, modulation_fixture::on_bin_interferer(9, spec, 16, 45.0));
    EXPECT_GT(CountDetections(r), 0);
}

// ─── U3: window-integrity discipline (KTD-3) ─────────────────────────────────

TEST(ModulationDetector, SequenceGapFlushesAndRefills) {
    DotSpec spec;
    spec.cx = 200.0;
    spec.cy = 240.0;
    spec.amplitude = 80.0;
    auto frames = modulation_fixture::modulated_dot(10, spec, 16);
    modulation_fixture::inject_sequence_gap(frames, /*at_index=*/8, /*missing=*/3);

    ModulationDetector d(MakeConfig(), kFps, kRows, kCols);
    const auto r = RunDetector(d, frames);
    EXPECT_GE(d.flush_count(), 1u);
    // The push at frame 8 breaks contiguity -> flush; the next full window is
    // frames 8..15, so detection cannot resume until frame 15.
    for (int i = 8; i <= 14; ++i) {
        EXPECT_FALSE(r[i].has_value()) << "frame " << i << " fired during refill";
    }
    EXPECT_TRUE(r[15].has_value());
}

TEST(ModulationDetector, CaptureGapOverCeilingFlushes) {
    DotSpec spec;
    spec.cx = 200.0;
    spec.cy = 240.0;
    spec.amplitude = 80.0;
    auto frames = modulation_fixture::modulated_dot(11, spec, 16);
    // A 2x-period stall at frame 8 exceeds the 1.5x gap ceiling.
    modulation_fixture::inject_capture_stall(frames, /*at_index=*/8,
                                             2 * modulation_fixture::kFramePeriodNs);
    ModulationDetector d(MakeConfig(), kFps, kRows, kCols);
    const auto r = RunDetector(d, frames);
    EXPECT_GE(d.flush_count(), 1u);
    for (int i = 8; i <= 14; ++i) {
        EXPECT_FALSE(r[i].has_value()) << "frame " << i << " fired during refill";
    }
    EXPECT_TRUE(r[15].has_value());
}

TEST(ModulationDetector, ExplicitFlushEmitsNothingUntilFullRefill) {
    // This is the wrap path (U5): the main loop calls flush() when a frame
    // carries FrameMetadata.wrapped, and detection resumes only after a full
    // refill of contiguous admitted frames.
    DotSpec spec;
    spec.cx = 200.0;
    spec.cy = 240.0;
    spec.amplitude = 80.0;
    const auto frames = modulation_fixture::modulated_dot(12, spec, 24);

    ModulationDetector d(MakeConfig(), kFps, kRows, kCols);
    std::vector<std::optional<LaserObservation>> r;
    for (int i = 0; i < 24; ++i) {
        // The U6 seam order: push, then flush if the frame carries the wrap
        // marker, then detect. Frame 12 is the "wrapped" frame here.
        d.push_frame(frames[i].image, frames[i].meta);
        if (i == 12) {
            d.flush();
        }
        r.push_back(d.detect());
    }
    EXPECT_TRUE(r[11].has_value());  // detecting normally before the wrap
    for (int i = 12; i <= 19; ++i) {
        EXPECT_FALSE(r[i].has_value()) << "frame " << i << " fired during post-flush refill";
    }
    EXPECT_TRUE(r[20].has_value());  // 8 contiguous frames after flush -> resumes
}

TEST(ModulationDetector, SweepAwayTailCeasesWithinFourFrames) {
    // Settled dot for 12 frames then instant departure. Residual window power may
    // sustain detections for a few frames; they must cease within 4 (R13).
    DotSpec spec;
    spec.cx = 200.0;
    spec.cy = 240.0;
    spec.amplitude = 80.0;
    const int present = 12;
    ModulationDetector d(MakeConfig(), kFps, kRows, kCols);
    const auto r = RunDetector(d, modulation_fixture::departing_dot(13, spec, present, 24));

    EXPECT_GT(CountDetections(r), 0);  // it did detect while present
    for (int i = present + 4; i < 24; ++i) {
        EXPECT_FALSE(r[i].has_value())
            << "frame " << i << " still firing > 4 frames after departure";
    }
}

// ─── U4: clustering, centroid, fail-closed disambiguation ────────────────────

TEST(ModulationDetector, TwoModulatedClustersFailClosed) {
    DotSpec spec;
    spec.cx = 200.0;
    spec.cy = 240.0;
    spec.amplitude = 80.0;
    auto frames = modulation_fixture::modulated_dot(14, spec, 16);
    // A second synchronized modulated disc far from the first.
    PaintSecondDot(frames, cv::Point(450, 240), /*radius=*/5, /*value=*/110, 15.0);

    ModulationDetector d(MakeConfig(), kFps, kRows, kCols);
    const auto r = RunDetector(d, frames);
    EXPECT_TRUE(r[15].has_value() == false)
        << "two clusters must fail closed (no observation)";
    EXPECT_GT(d.ambiguous_detections(), 0u);
}

TEST(ModulationDetector, ClusterBelowMinAreaRejected) {
    // A normal dot's cluster (~tens of px) is below a 500 px minimum -> rejected.
    tracking::LaserConfig cfg = MakeConfig();
    cfg.min_cluster_size_px = 500;
    DotSpec spec;
    spec.cx = 200.0;
    spec.cy = 240.0;
    spec.amplitude = 80.0;
    ModulationDetector d(cfg, kFps, kRows, kCols);
    EXPECT_EQ(CountDetections(RunDetector(d, modulation_fixture::modulated_dot(15, spec, 16))), 0);
}

TEST(ModulationDetector, ClusterAboveMaxAreaRejected) {
    tracking::LaserConfig cfg = MakeConfig();
    cfg.max_cluster_size_px = 10;  // any real dot exceeds this
    DotSpec spec;
    spec.cx = 200.0;
    spec.cy = 240.0;
    spec.amplitude = 80.0;
    ModulationDetector d(cfg, kFps, kRows, kCols);
    EXPECT_EQ(CountDetections(RunDetector(d, modulation_fixture::modulated_dot(16, spec, 16))), 0);
}

TEST(ModulationDetector, SaturatedBloomCentroidBiasBounded) {
    // The saturated, +x-bloomed dot biases the power-weighted centroid toward +x;
    // the clipped core dominates the power, so the bias stays small. Bound pinned
    // from the synthetic (the plan's <1 px estimate was pre-implementation).
    DotSpec spec;
    spec.cx = 200.0;
    spec.cy = 240.0;
    ModulationDetector d(MakeConfig(), kFps, kRows, kCols);
    const auto r = RunDetector(d, modulation_fixture::saturated_dot(17, spec, 16, /*aniso_x=*/2.0));
    ASSERT_TRUE(r[15].has_value());
    EXPECT_LT(std::abs(r[15]->centroid_px.x - spec.cx), 3.0);
    EXPECT_LT(std::abs(r[15]->centroid_px.y - spec.cy), 1.5);
}

TEST(ModulationDetector, PeakBrightnessSampledFromOnFrames) {
    // At frame 15 (phase 0) the newest frame is OFF (dot absent), so peak
    // brightness must come from an earlier ON frame, not the dark newest frame.
    DotSpec spec;
    spec.cx = 200.0;
    spec.cy = 240.0;
    spec.amplitude = 80.0;
    ASSERT_FALSE(modulation_fixture::square_on(15.0, 0.0, 15));  // newest is OFF
    ModulationDetector d(MakeConfig(), kFps, kRows, kCols);
    const auto r = RunDetector(d, modulation_fixture::modulated_dot(18, spec, 16));
    ASSERT_TRUE(r[15].has_value());
    // ON-frame brightness ~ background(12) + amplitude(80); a dark-frame read
    // would be ~12.
    EXPECT_GT(r[15]->peak_brightness, 50.0);
    EXPECT_GT(r[15]->cluster_area_px, 0.0);
}

}  // namespace
