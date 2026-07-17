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
};

}  // namespace tracking
