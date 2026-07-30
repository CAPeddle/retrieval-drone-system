#pragma once

// Shared synthetic modulation sequences for the TRK-009 laser-detector cluster
// (U1). Header-only, seeded (cv::RNG), deterministic. Produces 640x480 CV_8UC1
// frame sequences carrying real FrameMetadata (sequence numbers + synthetic
// capture timestamps at a nominal 60 fps) so downstream units can drive the
// window-integrity path (contiguity, capture-gap, wrap) without hardware.
//
// Design anchors (plan 2026-07-21-001):
//   - The true laser is a square-wave-modulated Gaussian dot at f_mod (15 Hz
//     default) over a dark, noisy background. Detection is per-pixel spectral
//     power at the modulation bin over a two-period (8-frame) window (KTD-1).
//   - The adversarial sequences (step, translating edge, off-bin interferer)
//     are the ones that a naive single-period detector would false-fire on; the
//     benchmark and the U3+ detector suite assert zero detections on them.
//   - The on-bin interferer (an ambient source aliasing exactly onto f_mod) IS
//     detected — expected behaviour, asserted as such (R3), to be recorded as a
//     residual exposure in the planned ADR-005 amendment (U9, lands with
//     Phase B).
//   - Opaque occlusion for the moving edge follows the composite-occlusion
//     learning: the edge overwrites the pixels behind it, it does not add to
//     them.
//
// Everything here is generation-time only (never on the hot path), so clarity
// beats the §7.1 no-allocation discipline; consumers own the hot-path buffers.

#include "frame_metadata.hpp"

#include <opencv2/core.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace modulation_fixture {

// ─── Frame geometry and timebase ─────────────────────────────────────────────
constexpr int kRows = 480;
constexpr int kCols = 640;
constexpr double kFps = 60.0;
// 1/60 s in ns, rounded — the nominal gap-free inter-frame spacing.
constexpr std::int64_t kFramePeriodNs = 16'666'667;
// Arbitrary non-zero bases so tests never rely on seq/time starting at zero.
constexpr std::uint64_t kBaseSequence = 1000;
constexpr std::int64_t kBaseTimestampNs = 5'000'000'000;

// A generated frame plus the metadata a real capture would carry.
struct Frame {
    cv::Mat image;                  // CV_8UC1, kRows x kCols
    tracking::FrameMetadata meta;
};

// Modulated-dot description. `amplitude` is the peak intensity ADDED on ON
// frames (before background + saturation); `radius` is the Gaussian sigma.
struct DotSpec {
    double cx = kCols / 2.0;
    double cy = kRows / 2.0;
    double radius = 4.0;
    double amplitude = 60.0;
    double phase_frac = 0.0;   // fractional cycle offset in [0,1)
    double freq_hz = 15.0;
};

inline cv::Point2d dot_centre(const DotSpec& spec) { return {spec.cx, spec.cy}; }

// Square wave at `freq_hz`, 50/50 duty (ADR-005), sampled at kFps. ON when the
// fractional phase is in the first half of the cycle. Public so tests can assert
// ON/OFF frame means directly.
inline bool square_on(double freq_hz, double phase_frac, int frame_index) {
    double phase = frame_index * freq_hz / kFps + phase_frac;
    phase -= std::floor(phase);
    return phase < 0.5;
}

namespace detail {

inline tracking::FrameMetadata make_meta(std::uint64_t seq, std::int64_t t_ns) {
    tracking::FrameMetadata m;
    m.capture_timestamp_ns = t_ns;
    m.sequence_number = seq;
    m.capture_duration_us = 2000;  // representative grab time; not load-bearing here
    m.camera_id = 0;
    return m;
}

// Dark, noisy background into a fresh CV_32F accumulator (worked in float so
// additive dots and saturation compose cleanly before the final CV_8U clamp).
inline void fill_background(cv::Mat& acc, cv::RNG& rng, double base, double sigma) {
    acc.create(kRows, kCols, CV_32F);
    acc.setTo(base);
    cv::Mat noise(kRows, kCols, CV_32F);
    rng.fill(noise, cv::RNG::NORMAL, 0.0, sigma);
    acc += noise;
}

// Add an (optionally anisotropic) Gaussian blob, restricted to a bounding box so
// generation stays cheap. `aniso_x` > 1 stretches the +x lobe to model the
// asymmetric bloom of a saturated dot.
inline void add_gaussian(cv::Mat& acc, double cx, double cy, double sigma,
                         double amplitude, double aniso_x = 1.0) {
    const double reach = 4.0 * sigma * std::max(1.0, aniso_x);
    const int x0 = std::max(0, static_cast<int>(std::floor(cx - reach)));
    const int x1 = std::min(kCols - 1, static_cast<int>(std::ceil(cx + reach)));
    const int y0 = std::max(0, static_cast<int>(std::floor(cy - reach)));
    const int y1 = std::min(kRows - 1, static_cast<int>(std::ceil(cy + reach)));
    for (int y = y0; y <= y1; ++y) {
        float* row = acc.ptr<float>(y);
        const double dy = (y - cy) / sigma;
        for (int x = x0; x <= x1; ++x) {
            const double sx = (x > cx) ? sigma * aniso_x : sigma;
            const double dx = (x - cx) / sx;
            row[x] += static_cast<float>(amplitude * std::exp(-0.5 * (dx * dx + dy * dy)));
        }
    }
}

// Clamp/round CV_32F -> CV_8U (convertTo saturates, giving free clipping for the
// saturated-dot case).
inline cv::Mat to_u8(const cv::Mat& acc) {
    cv::Mat u8;
    acc.convertTo(u8, CV_8U);
    return u8;
}

}  // namespace detail

// ─── Sequence scaffold ───────────────────────────────────────────────────────
// The one loop every generator shares: seeded RNG, per-frame paint callback,
// CV_8U conversion, contiguous metadata at nominal 60 fps. `paint(acc, rng, i)`
// must fill `acc` completely (background included) as CV_32F. New generators
// should build on this rather than hand-rolling the scaffold.
template <typename PaintFrame>
std::vector<Frame> generate_sequence(int num_frames, unsigned seed,
                                     PaintFrame&& paint) {
    cv::RNG rng(seed);
    std::vector<Frame> out;
    out.reserve(num_frames);
    for (int i = 0; i < num_frames; ++i) {
        cv::Mat acc;
        paint(acc, rng, i);
        out.push_back({detail::to_u8(acc),
                       detail::make_meta(kBaseSequence + i,
                                         kBaseTimestampNs + i * kFramePeriodNs)});
    }
    return out;
}

// ─── Generators ──────────────────────────────────────────────────────────────

// Square-wave-modulated Gaussian dot over dark noise — the true laser.
inline std::vector<Frame> modulated_dot(unsigned seed, const DotSpec& spec,
                                        int num_frames, double bg_base = 12.0,
                                        double bg_sigma = 3.0) {
    return generate_sequence(num_frames, seed, [&](cv::Mat& acc, cv::RNG& rng, int i) {
        detail::fill_background(acc, rng, bg_base, bg_sigma);
        if (square_on(spec.freq_hz, spec.phase_frac, i)) {
            detail::add_gaussian(acc, spec.cx, spec.cy, spec.radius, spec.amplitude);
        }
    });
}

// Off-frequency interferer (20 Hz by default): decoheres across two periods and
// must fail the purity gate → zero detections (R3).
inline std::vector<Frame> off_frequency_dot(unsigned seed, const DotSpec& base,
                                            int num_frames, double freq_hz = 20.0) {
    DotSpec spec = base;
    spec.freq_hz = freq_hz;
    return modulated_dot(seed, spec, num_frames);
}

// On-bin interferer: an ambient source at `source_hz` (45 Hz PWM) that ALIASES
// onto the modulation bin when sampled at 60 fps (|45-60| = 15 Hz). This IS
// detected — expected behaviour, asserted as such; a residual exposure to be
// recorded in the planned ADR-005 amendment (U9, Phase B), not a failure.
inline std::vector<Frame> on_bin_interferer(unsigned seed, const DotSpec& base,
                                            int num_frames, double source_hz = 45.0) {
    DotSpec spec = base;
    spec.freq_hz = source_hz;
    return modulated_dot(seed, spec, num_frames);
}

// Fractional-phase dot: worst-case edge-in-exposure sampling. The exposure
// straddles ON/OFF transitions, so the per-period amplitude schedule is
// [a, a/2, 0, a/2] rather than a clean [a, a, 0, 0] — ~3 dB bin-power loss. Must
// still detect at the provisional thresholds (R3 boundary characterisation).
// Models the canonical 4-frame period (15 Hz @ 60 fps) only; spec.freq_hz is
// intentionally unused — the half-amplitude schedule has no meaning at other
// integer frame-per-period ratios.
inline std::vector<Frame> fractional_phase_dot(unsigned seed, const DotSpec& spec,
                                               int num_frames, double bg_base = 12.0,
                                               double bg_sigma = 3.0) {
    static const double kSchedule[4] = {1.0, 0.5, 0.0, 0.5};
    return generate_sequence(num_frames, seed, [&](cv::Mat& acc, cv::RNG& rng, int i) {
        detail::fill_background(acc, rng, bg_base, bg_sigma);
        const double scale = kSchedule[i % 4];
        if (scale > 0.0) {
            detail::add_gaussian(acc, spec.cx, spec.cy, spec.radius,
                                 spec.amplitude * scale);
        }
    });
}

// Static luminance step (light switch) at `offset`: the whole frame jumps from
// `level_before` to `level_after`. A single such step is indistinguishable from
// one modulation period in a 4-point DFT (KTD-1) — the structural false-SAFE the
// two-period window closes. Zero detections expected at every offset.
inline std::vector<Frame> static_step(unsigned seed, int offset, int num_frames,
                                      double level_before = 12.0,
                                      double level_after = 90.0, double bg_sigma = 3.0) {
    return generate_sequence(num_frames, seed, [&](cv::Mat& acc, cv::RNG& rng, int i) {
        detail::fill_background(acc, rng, i < offset ? level_before : level_after,
                                bg_sigma);
    });
}

// Translating opaque bright edge (moving-ball surrogate). A bright half-plane
// whose vertical edge sweeps right, OVERWRITING the pixels behind it (opaque,
// per the composite-occlusion learning). Each pixel sees a single step as the
// edge crosses it — transient, never sustained modulation power → zero
// detections (R3).
inline std::vector<Frame> translating_edge(unsigned seed, int num_frames,
                                           double edge_x0 = 80.0, double speed_px = 40.0,
                                           double bright = 200.0, double bg_base = 12.0,
                                           double bg_sigma = 3.0) {
    return generate_sequence(num_frames, seed, [&](cv::Mat& acc, cv::RNG& rng, int i) {
        detail::fill_background(acc, rng, bg_base, bg_sigma);
        const int edge_x =
            std::clamp(static_cast<int>(edge_x0 + i * speed_px), 0, kCols);
        if (edge_x > 0) {
            acc.colRange(0, edge_x).setTo(bright);  // opaque overwrite
        }
    });
}

// Bright-clutter scene: a DIM modulated dot beside a larger, brighter SATURATED
// static blob. The "brightest pixel is not the laser" pitfall (ADR-005): the
// blob is unmodulated (zero power at f_mod) so the PSD detector must reject it
// and find the dim dot — the case a brightness prefilter is most likely to fail
// (KTD-4).
inline std::vector<Frame> bright_clutter_scene(unsigned seed, const DotSpec& dim_dot,
                                               int num_frames, double blob_cx = 470.0,
                                               double blob_cy = 300.0,
                                               double blob_sigma = 22.0,
                                               double blob_amplitude = 600.0) {
    return generate_sequence(num_frames, seed, [&](cv::Mat& acc, cv::RNG& rng, int i) {
        detail::fill_background(acc, rng, 12.0, 3.0);
        // Static saturated blob every frame (unmodulated → no bin power).
        detail::add_gaussian(acc, blob_cx, blob_cy, blob_sigma, blob_amplitude);
        if (square_on(dim_dot.freq_hz, dim_dot.phase_frac, i)) {
            detail::add_gaussian(acc, dim_dot.cx, dim_dot.cy, dim_dot.radius,
                                 dim_dot.amplitude);
        }
    });
}

// Saturated (clipped) modulated dot with asymmetric bloom: high amplitude clips
// at 255 and the +x lobe is stretched. Exercises the centroid-bias bound (U4).
inline std::vector<Frame> saturated_dot(unsigned seed, const DotSpec& base,
                                        int num_frames, double aniso_x = 2.0) {
    DotSpec spec = base;
    if (spec.amplitude < 300.0) {
        spec.amplitude = 400.0;  // ensure it clips
    }
    return generate_sequence(num_frames, seed, [&](cv::Mat& acc, cv::RNG& rng, int i) {
        detail::fill_background(acc, rng, 12.0, 3.0);
        if (square_on(spec.freq_hz, spec.phase_frac, i)) {
            detail::add_gaussian(acc, spec.cx, spec.cy, spec.radius, spec.amplitude,
                                 aniso_x);
        }
    });
}

// Settled-then-departing dot: modulated for `present_frames`, then gone. Drives
// the sweep-away / stale-tail case (R13): residual window power sustains
// detections for a few frames after departure, then ceases.
inline std::vector<Frame> departing_dot(unsigned seed, const DotSpec& spec,
                                        int present_frames, int num_frames,
                                        double bg_base = 12.0, double bg_sigma = 3.0) {
    return generate_sequence(num_frames, seed, [&](cv::Mat& acc, cv::RNG& rng, int i) {
        detail::fill_background(acc, rng, bg_base, bg_sigma);
        if (i < present_frames && square_on(spec.freq_hz, spec.phase_frac, i)) {
            detail::add_gaussian(acc, spec.cx, spec.cy, spec.radius, spec.amplitude);
        }
    });
}

// ─── Gap-injection hooks (R4 window-integrity tests) ─────────────────────────

// Simulate `missing` dropped frames from `at_index` onward: the sequence number
// jumps (breaking contiguity) and timestamps shift by the same span, as a real
// capture would report after a dropped-frame burst.
// Precondition: 0 < at_index < frames.size(). at_index == 0 shifts the WHOLE
// sequence uniformly — pairwise contiguity is preserved and no gap exists.
inline void inject_sequence_gap(std::vector<Frame>& frames, std::size_t at_index,
                                std::uint64_t missing) {
    for (std::size_t i = at_index; i < frames.size(); ++i) {
        frames[i].meta.sequence_number += missing;
        frames[i].meta.capture_timestamp_ns +=
            static_cast<std::int64_t>(missing) * kFramePeriodNs;
    }
}

// Simulate a camera stall of `extra_ns` at `at_index`: sequence numbers stay
// contiguous but the inter-frame capture gap exceeds the nominal period, which
// the detector's 1.5x-period gap ceiling must catch.
// Precondition: 0 < at_index < frames.size() (see inject_sequence_gap).
inline void inject_capture_stall(std::vector<Frame>& frames, std::size_t at_index,
                                 std::int64_t extra_ns) {
    for (std::size_t i = at_index; i < frames.size(); ++i) {
        frames[i].meta.capture_timestamp_ns += extra_ns;
    }
}

// ─── Self-check helpers ──────────────────────────────────────────────────────

// Mean intensity over a square ROI centred on `c` (radius `r` px), clamped to
// the frame — used by the fixture's own self-verification tests.
inline double roi_mean(const cv::Mat& image, cv::Point2d c, int r) {
    const int x0 = std::max(0, static_cast<int>(std::floor(c.x)) - r);
    const int x1 = std::min(kCols - 1, static_cast<int>(std::floor(c.x)) + r);
    const int y0 = std::max(0, static_cast<int>(std::floor(c.y)) - r);
    const int y1 = std::min(kRows - 1, static_cast<int>(std::floor(c.y)) + r);
    return cv::mean(image(cv::Range(y0, y1 + 1), cv::Range(x0, x1 + 1)))[0];
}

}  // namespace modulation_fixture
