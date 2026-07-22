// U1 self-verification for the shared modulation fixture. These are the
// "known-answer" checks that a synthetic generator needs: they prove the ON/OFF
// contrast, the ground-truth centre, determinism, and the metadata timebase are
// what the detector suite (U3+) and the benchmark (U2) will rely on.

#include "modulation_fixture.hpp"

#include <gtest/gtest.h>

#include <opencv2/core.hpp>

namespace {

using modulation_fixture::DotSpec;
using modulation_fixture::Frame;

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
    EXPECT_GT(max_present, 12.0 + 0.5 * spec.amplitude);

    // ...and after departure every frame is background only.
    for (int i = present; i < 16; ++i) {
        EXPECT_LT(modulation_fixture::roi_mean(frames[i].image, centre, 3), 30.0)
            << "frame " << i << " should be background after the dot departs";
    }
}

}  // namespace
