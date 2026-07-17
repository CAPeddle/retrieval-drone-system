#pragma once

// TRK-006: the ingestion thread — entry point of the hot path. Grabs frames
// from a FrameSource, timestamps them (steady_clock), and pushes into the
// FrameRingBuffer. Runs under SCHED_FIFO pinned to a dedicated core on the Pi
// (§7.1); on unprivileged dev machines both degrade gracefully with a WARN.

#include "frame_ring_buffer.hpp"

#include <opencv2/core.hpp>

#include <atomic>
#include <cstdint>
#include <thread>

namespace tracking {

// Seam between the capture loop and the physical camera, so tests drive the
// thread with synthetic frames. One virtual call per frame — not per pixel —
// which stays within the hot-path budget.
class FrameSource {
public:
    virtual ~FrameSource() = default;
    // Fills `out` with the next frame; false on failure/disconnect. The
    // source owns pacing (a real camera blocks at its frame rate).
    virtual bool grab(cv::Mat& out) = 0;
};

class CaptureThread {
public:
    struct Options {
        int cpu_core = 2;          // pipeline.capture_cpu_core
        int thread_priority = 80;  // pipeline.capture_thread_priority (SCHED_FIFO)
        std::uint8_t camera_id = 0;
    };

    // Non-owning references: source and buffer must outlive the thread.
    CaptureThread(FrameSource& source, FrameRingBuffer& buffer, const Options& options);
    ~CaptureThread();  // stops and joins if still running

    CaptureThread(const CaptureThread&) = delete;
    CaptureThread& operator=(const CaptureThread&) = delete;

    // Launches the capture thread (applies SCHED_FIFO + affinity best-effort).
    // Sequence numbers continue across stop()/start() cycles.
    void start();
    // Signals the atomic stop flag and joins. Safe to call when not running.
    void stop();

    std::uint64_t frames_captured() const {
        return frames_captured_.load(std::memory_order_relaxed);
    }
    std::uint64_t frames_dropped() const {
        return frames_dropped_.load(std::memory_order_relaxed);
    }
    std::uint64_t grab_failures() const {
        return grab_failures_.load(std::memory_order_relaxed);
    }

private:
    void run();

    FrameSource& source_;
    FrameRingBuffer& buffer_;
    Options options_;
    std::thread thread_;
    std::atomic<bool> stop_{false};
    std::atomic<std::uint64_t> frames_captured_{0};
    std::atomic<std::uint64_t> frames_dropped_{0};
    std::atomic<std::uint64_t> grab_failures_{0};
    std::uint64_t next_sequence_ = 0;  // touched only by the capture thread / start-stop gaps
    cv::Mat scratch_;                  // grab target, reused every frame (§7.1: no per-frame alloc)
};

}  // namespace tracking
