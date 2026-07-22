#pragma once

// TRK-006: seam between the capture loop and the physical camera, so tests
// (and future replay sources, TRK-025) drive the pipeline with synthetic
// frames without pulling in threading internals. One virtual call per frame —
// not per pixel — which stays within the hot-path budget.

#include <opencv2/core.hpp>

namespace tracking {

class FrameSource {
public:
    virtual ~FrameSource() = default;
    // Fills `out` with the next frame; false on failure/disconnect. The
    // source owns pacing (a real camera blocks at its frame rate).
    virtual bool grab(cv::Mat& out) = 0;

    // TRK-009 U5 (R4): returns true exactly once for the frame that begins a new
    // pass after a discontinuity (e.g. a replay clip looping back to its start),
    // then clears. The capture thread calls this immediately after a successful
    // grab and stamps the result into that frame's FrameMetadata.wrapped, so the
    // marker is frame-aligned across the ring buffer. Live cameras never wrap;
    // the default is false.
    virtual bool consume_wrap() { return false; }
};

}  // namespace tracking
