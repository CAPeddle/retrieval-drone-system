#pragma once

// TRK-031: FrameSource backed by a recorded clip. Deterministic,
// hardware-realistic input for pipeline tests when no camera is attached —
// the seed of the TRK-025 replay harness (ADR-007's replay gate direction).
// End of clip reports grab failure exactly like a camera disconnect, unless
// loop mode is on.

#include "frame_source.hpp"

#include <opencv2/videoio.hpp>

#include <chrono>
#include <string>

namespace tracking {

class ReplaySource final : public FrameSource {
public:
    struct Options {
        double fps = 0.0;   // > 0: pace grabs at this rate (sleep_until); 0: unpaced.
        bool loop = false;  // Restart from the first frame at end of clip.
    };

    // Opens the clip; startup-only (may allocate). Check is_open().
    explicit ReplaySource(const std::string& path);
    ReplaySource(const std::string& path, const Options& options);

    bool is_open() const { return capture_.isOpened(); }
    bool grab(cv::Mat& out) override;
    // True once for the first frame after a loop restart (R4/U5), then clears.
    bool consume_wrap() override;

private:
    cv::VideoCapture capture_;
    Options options_;
    std::chrono::steady_clock::time_point next_{};
    bool started_ = false;
    bool wrapped_pending_ = false;  // set on loop rewind, consumed by consume_wrap()
};

}  // namespace tracking
