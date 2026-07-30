// U1 self-verification for the shared modulation fixture. These are the
// "known-answer" checks that a synthetic generator needs: they prove the ON/OFF
// contrast, the ground-truth centre, determinism, the metadata timebase, and
// each adversarial generator's load-bearing property are what the detector
// suite (U3+) and the benchmark (U2) rely on.

#include "modulation_fixture.hpp"

#include <gtest/gtest.h>
#include <opencv2/core.hpp>

#include <array>

namespace {

using modulation_fixture::DotSpec;
using modulation_fixture::Frame;

constexpr double kBg = 12.0;

TEST(ModulationFixture, OnFramesAreBrighterThanOffFramesAtTheDot) {
    DotSpec spec;
    spec.amplitude = 60.0;
    const auto frames = modulation_fixture::modulated_dot(/*seed=*/1, spec, 8);

    // Default phase 0: frames 0,1 ON; 2,3 OFF (15 Hz @ 60 fps → period 4).
    ASSERT_TRUE(modulation_fixture::square_on(spec.freq_hz, spec.phase_frac, 0));
    ASSERT_FALSE(modulation_fixture::square_on(spec.freq_hz, spec.phase_frac, 2));

    const auto centre = modulation_fixture::dot_centre(spec);
    const double on_mean = modulation_fixture::roi_mean(frames[0].image, centre, 3);
    const double off_mean = modulation_fixture::roi_mean(frames[2].image, centre, 3);

    // The ON/OFF contrast should be a large fraction of the configured amplitude
    // (background is dark and low-noise, so most of the amplitude survives).
    EXPECT_GT(on_mean - off_mean, 0.5 * spec.amplitude);
}

TEST(ModulationFixture, IsDeterministicForAFixedSeed) {
    DotSpec spec;
    const auto a = modulation_fixture::modulated_dot(/*seed=*/7, spec, 8);
    const auto b = modulation_fixture::modulated_dot(/*seed=*/7, spec, 8);
    ASSERT_EQ(a.size(), b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        EXPECT_EQ(cv::countNonZero(a[i].image != b[i].image), 0)
            << "frame " << i << " differs between two same-seed generations";
    }
}

TEST(ModulationFixture, DifferentSeedsGiveDifferentNoise) {
    DotSpec spec;
    const auto a = modulation_fixture::modulated_dot(/*seed=*/1, spec, 4);
    const auto b = modulation_fixture::modulated_dot(/*seed=*/2, spec, 4);
    EXPECT_GT(cv::countNonZero(a[0].image != b[0].image), 0);
}

TEST(ModulationFixture, MetadataIsContiguousAtNominalFrameRate) {
    DotSpec spec;
    const auto frames = modulation_fixture::modulated_dot(/*seed=*/3, spec, 8);
    for (std::size_t i = 1; i < frames.size(); ++i) {
        EXPECT_EQ(frames[i].meta.sequence_number,
                  frames[i - 1].meta.sequence_number + 1);
        EXPECT_EQ(frames[i].meta.capture_timestamp_ns -
                      frames[i - 1].meta.capture_timestamp_ns,
                  modulation_fixture::kFramePeriodNs);
    }
}

TEST(ModulationFixture, StaticStepJumpsAtTheConfiguredOffset) {
    const int offset = 4;
    const auto frames =
        modulation_fixture::static_step(/*seed=*/5, offset, /*num_frames=*/8,
                                        /*level_before=*/12.0, /*level_after=*/90.0);
    const cv::Point2d centre{modulation_fixture::kCols / 2.0,
                             modulation_fixture::kRows / 2.0};
    const double before = modulation_fixture::roi_mean(frames[offset - 1].image, centre, 5);
    const double after = modulation_fixture::roi_mean(frames[offset].image, centre, 5);
    EXPECT_GT(after - before, 40.0);
}

TEST(ModulationFixture, InjectSequenceGapBreaksContiguity) {
    DotSpec spec;
    auto frames = modulation_fixture::modulated_dot(/*seed=*/9, spec, 8);
    modulation_fixture::inject_sequence_gap(frames, /*at_index=*/4, /*missing=*/3);
    EXPECT_EQ(frames[4].meta.sequence_number, frames[3].meta.sequence_number + 4)
        << "a 3-frame drop should leave a gap of 4 in the sequence numbers";
}

TEST(ModulationFixture, InjectCaptureStallExceedsThePeriodButKeepsSequence) {
    DotSpec spec;
    auto frames = modulation_fixture::modulated_dot(/*seed=*/9, spec, 8);
    const std::int64_t stall = 2 * modulation_fixture::kFramePeriodNs;
    modulation_fixture::inject_capture_stall(frames, /*at_index=*/4, stall);
    EXPECT_EQ(frames[4].meta.sequence_number, frames[3].meta.sequence_number + 1)
        << "a stall must not change sequence numbers";
    EXPECT_GT(frames[4].meta.capture_timestamp_ns - frames[3].meta.capture_timestamp_ns,
              static_cast<std::int64_t>(1.5 * modulation_fixture::kFramePeriodNs));
}

TEST(ModulationFixture, DepartingDotVanishesAfterPresentFrames) {
    DotSpec spec;
    spec.amplitude = 60.0;
    const int present = 8;
    const auto frames =
        modulation_fixture::departing_dot(/*seed=*/11, spec, present, /*num_frames=*/16);
    const auto centre = modulation_fixture::dot_centre(spec);

    // Within the present span there is at least one clearly-ON frame...
    double max_present = 0.0;
    for (int i = 0; i < present; ++i) {
        max_present = std::max(max_present,
                               modulation_fixture::roi_mean(frames[i].image, centre, 3));
    }
    EXPECT_GT(max_present, kBg + 0.5 * spec.amplitude);

    // ...and after departure every frame is background only.
    for (int i = present; i < 16; ++i) {
        EXPECT_LT(modulation_fixture::roi_mean(frames[i].image, centre, 3), 30.0)
            << "frame " << i << " should be background after the dot departs";
    }
}

// ─── Adversarial-generator known-answer checks ───────────────────────────────
// Each generator below encodes a load-bearing property the Phase B safety
// suites will trust; these tests pin the property, not just "it runs".

// The 20 Hz interferer must follow its OWN square wave (period 3 at 60 fps),
// which visibly differs from the 15 Hz pattern within one 8-frame window.
TEST(ModulationFixture, OffFrequencyDotFollowsItsOwnSquareWave) {
    DotSpec spec;
    spec.amplitude = 60.0;
    const double freq = 20.0;
    const auto frames =
        modulation_fixture::off_frequency_dot(/*seed=*/13, spec, 12, freq);
    const auto centre = modulation_fixture::dot_centre(spec);

    bool differs_from_15hz = false;
    for (int i = 0; i < 12; ++i) {
        const bool on = modulation_fixture::square_on(freq, spec.phase_frac, i);
        const double mean = modulation_fixture::roi_mean(frames[i].image, centre, 3);
        if (on) {
            EXPECT_GT(mean, kBg + 0.4 * spec.amplitude) << "frame " << i;
        } else {
            EXPECT_LT(mean, kBg + 0.2 * spec.amplitude) << "frame " << i;
        }
        if (on != modulation_fixture::square_on(15.0, spec.phase_frac, i)) {
            differs_from_15hz = true;
        }
    }
    EXPECT_TRUE(differs_from_15hz)
        << "20 Hz pattern must decohere from the 15 Hz pattern within the window";
}

// The 45 Hz PWM source aliases exactly onto the modulation bin: its 8-frame
// ON/OFF pattern is a cyclic shift of the true 15 Hz pattern (run-length-2
// structure preserved). This is the residual exposure R3 asserts as expected.
TEST(ModulationFixture, OnBinInterfererAliasesToACyclicShiftOf15Hz) {
    DotSpec spec;
    spec.amplitude = 60.0;
    const double source_hz = 45.0;
    const auto frames =
        modulation_fixture::on_bin_interferer(/*seed=*/17, spec, 8, source_hz);
    const auto centre = modulation_fixture::dot_centre(spec);

    std::array<bool, 8> observed{};
    std::array<bool, 8> p15{};
    for (int i = 0; i < 8; ++i) {
        const double mean = modulation_fixture::roi_mean(frames[i].image, centre, 3);
        observed[i] = mean > kBg + 0.3 * spec.amplitude;
        // Generator truth must match the analytic 45 Hz schedule.
        EXPECT_EQ(observed[i],
                  modulation_fixture::square_on(source_hz, spec.phase_frac, i))
            << "frame " << i;
        p15[i] = modulation_fixture::square_on(15.0, spec.phase_frac, i);
    }
    bool is_shift_of_15hz = false;
    for (int k = 0; k < 8 && !is_shift_of_15hz; ++k) {
        bool match = true;
        for (int i = 0; i < 8; ++i) {
            if (observed[i] != p15[(i + k) % 8]) {
                match = false;
                break;
            }
        }
        is_shift_of_15hz = match;
    }
    EXPECT_TRUE(is_shift_of_15hz)
        << "45 Hz sampled at 60 fps must alias to a phase-shifted 15 Hz pattern";
}

// The fractional-phase dot's documented schedule [a, a/2, 0, a/2] must be what
// the frames actually contain (this generator bypasses square_on entirely).
TEST(ModulationFixture, FractionalPhaseDotFollowsHalfAmplitudeSchedule) {
    DotSpec spec;
    spec.amplitude = 60.0;
    const auto frames = modulation_fixture::fractional_phase_dot(/*seed=*/19, spec, 8);
    const auto centre = modulation_fixture::dot_centre(spec);

    const double m0 = modulation_fixture::roi_mean(frames[0].image, centre, 2);
    const double m1 = modulation_fixture::roi_mean(frames[1].image, centre, 2);
    const double m2 = modulation_fixture::roi_mean(frames[2].image, centre, 2);
    const double m3 = modulation_fixture::roi_mean(frames[3].image, centre, 2);

    EXPECT_LT(m2, kBg + 8.0) << "phase-2 frame must be background only";
    EXPECT_NEAR(m1, m3, 6.0) << "the two half-amplitude frames must match";
    // Full-amplitude excess over background is ~2x the half-amplitude excess.
    EXPECT_NEAR(m0 - kBg, 2.0 * (m1 - kBg), 0.35 * (m0 - kBg));
    // And the schedule repeats with period 4.
    EXPECT_NEAR(modulation_fixture::roi_mean(frames[4].image, centre, 2), m0, 6.0);
}

// The clutter blob must be saturated AND static (unmodulated → zero bin power)
// while the dim dot beside it still modulates — the "brightest pixel is not the
// laser" scene contract.
TEST(ModulationFixture, BrightClutterBlobIsStaticAndSaturatedWhileDotModulates) {
    DotSpec dim;
    dim.cx = 200.0;
    dim.cy = 240.0;
    dim.amplitude = 24.0;
    const cv::Point2d blob{470.0, 300.0};
    const auto frames = modulation_fixture::bright_clutter_scene(/*seed=*/23, dim, 8);

    double blob_min = 255.0;
    double blob_max = 0.0;
    for (const Frame& f : frames) {
        const double m = modulation_fixture::roi_mean(f.image, blob, 3);
        blob_min = std::min(blob_min, m);
        blob_max = std::max(blob_max, m);
    }
    EXPECT_GT(blob_min, 250.0) << "blob centre must be saturated in every frame";
    EXPECT_LT(blob_max - blob_min, 2.0) << "blob must be static across ON/OFF frames";

    const auto dot_centre = modulation_fixture::dot_centre(dim);
    const double dot_on = modulation_fixture::roi_mean(frames[0].image, dot_centre, 3);
    const double dot_off = modulation_fixture::roi_mean(frames[2].image, dot_centre, 3);
    EXPECT_GT(dot_on - dot_off, 0.4 * dim.amplitude)
        << "the dim dot must still modulate beside the blob";
}

// The saturated dot must actually clip at 255 and bloom farther on +x than -x
// (the asymmetry the U4 centroid-bias bound exercises).
TEST(ModulationFixture, SaturatedDotClipsAndBloomsAsymmetrically) {
    DotSpec spec;  // amplitude below 300 is raised to 400 by the generator
    const auto frames = modulation_fixture::saturated_dot(/*seed=*/29, spec, 4,
                                                          /*aniso_x=*/2.0);
    ASSERT_TRUE(modulation_fixture::square_on(spec.freq_hz, spec.phase_frac, 0));

    double max_val = 0.0;
    cv::minMaxLoc(frames[0].image(cv::Range(static_cast<int>(spec.cy) - 4,
                                            static_cast<int>(spec.cy) + 5),
                                  cv::Range(static_cast<int>(spec.cx) - 4,
                                            static_cast<int>(spec.cx) + 5)),
                  nullptr, &max_val);
    EXPECT_EQ(max_val, 255.0) << "ON frame must clip at the sensor ceiling";

    const double offset = 2.5 * spec.radius;
    const double plus_x = modulation_fixture::roi_mean(
        frames[0].image, {spec.cx + offset, spec.cy}, 2);
    const double minus_x = modulation_fixture::roi_mean(
        frames[0].image, {spec.cx - offset, spec.cy}, 2);
    EXPECT_GT(plus_x, minus_x + 30.0)
        << "the +x bloom lobe must be measurably brighter than the -x lobe";
}

// The translating edge must be opaque (overwrites, never adds) and advance at
// the configured speed; a crossed pixel sees exactly one step.
TEST(ModulationFixture, TranslatingEdgeIsOpaqueAndAdvances) {
    const double edge_x0 = 80.0;
    const double speed = 40.0;
    const double bright = 200.0;
    const auto frames = modulation_fixture::translating_edge(/*seed=*/31, 8, edge_x0,
                                                             speed, bright);
    const double y = modulation_fixture::kRows / 2.0;

    // Behind the edge the value is EXACTLY the edge brightness — overwrite, not
    // additive blend (additive would read ~background + bright).
    const double behind = modulation_fixture::roi_mean(frames[0].image, {40.0, y}, 3);
    EXPECT_NEAR(behind, bright, 1.0) << "opaque edge must overwrite background+noise";

    // A pixel ahead of the edge is background until the edge crosses it, then
    // bright in every later frame (single step per pixel).
    const cv::Point2d probe{150.0, y};  // crossed when edge_x >= 150 (frame 2)
    EXPECT_LT(modulation_fixture::roi_mean(frames[0].image, probe, 2), 30.0);
    EXPECT_LT(modulation_fixture::roi_mean(frames[1].image, probe, 2), 30.0);
    for (int i = 2; i < 8; ++i) {
        EXPECT_NEAR(modulation_fixture::roi_mean(frames[i].image, probe, 2), bright, 1.0)
            << "frame " << i << ": crossed pixel must stay at edge brightness";
    }
}

}  // namespace
