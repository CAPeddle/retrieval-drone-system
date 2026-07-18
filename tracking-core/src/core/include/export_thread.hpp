#pragma once

// Plan KTD-1 / U13: the export thread. Consumes TrackingSnapshots from the
// SPSC ring and publishes them, decoupling the hot path from serialisation
// and the socket. Lifecycle clones the CaptureThread pattern (TRK-006):
// atomic stop flag, start()/stop() join semantics, destructor safety net.
// Not a real-time thread — it deliberately runs unpinned at default priority
// so it can never priority-invert the capture thread.

#include <atomic>
#include <cstdint>
#include <thread>

#include "spsc_ring.hpp"
#include "tracking_snapshot.hpp"
#include "zmq_publisher.hpp"

namespace tracking {

class ExportThread {
public:
    ExportThread(SpscRing<TrackingSnapshot>& ring, ZmqPublisher& publisher);
    ~ExportThread();
    ExportThread(const ExportThread&) = delete;
    ExportThread& operator=(const ExportThread&) = delete;

    void start();
    // Signals stop, drains any outstanding snapshot (publishing it — plan
    // R23), and joins. Safe to call twice.
    void stop();

    std::uint64_t snapshots_published() const noexcept {
        return published_.load(std::memory_order_relaxed);
    }

private:
    void run();
    double read_cpu_temp_c();

    SpscRing<TrackingSnapshot>* ring_;
    ZmqPublisher* publisher_;
    std::thread thread_;
    std::atomic<bool> stop_{false};
    std::atomic<std::uint64_t> published_{0};
    double cpu_temp_c_ = -1.0;
    std::int64_t last_temp_read_ns_ = 0;
};

}  // namespace tracking
