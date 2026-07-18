#pragma once

// TRK-015: observation gating and association. Replaces the pre-TRK-014 stub.
// Association is strictly type-keyed (plan R2): an observation only ever feeds
// a track of the same ObjectType — cross-type capture at the laser-on-ball
// moment would collapse the ADR-007 clause-8 distance to zero, a false-SAFE
// path. Candidate (track, observation) pairs are taken globally nearest-first
// within the pixel gate, so when two same-type observations contend for one
// track the nearer one wins.

#include <cstdint>
#include <vector>

#include "config.hpp"
#include "track.hpp"

namespace tracking {

class Tracker {
public:
    Tracker(const TrackConfig& track_config, const GatingConfig& gating_config);

    // Processes one cycle: associates `observations` to live tracks, spawns
    // Provisional tracks for unmatched observations (subject to per-type
    // slots), ticks every track (observed or not), and removes Retired ones.
    // `now_ns` is the cycle's capture-clock time. Returns the active set by
    // const reference — no per-cycle allocation once warm.
    const std::vector<Track>& update(const std::vector<Observation>& observations,
                                     std::int64_t now_ns);

    const std::vector<Track>& tracks() const noexcept { return tracks_; }

    // Per-type live-track limits (plan R2): single laser and ball in v0.3; a
    // Lost track no longer counts against its slot (plan R3), so a replacement
    // can spawn immediately after fast motion breaks a track.
    static int type_limit(ObjectType type) noexcept;

    // Bounds for the pre-allocated working sets. Observations beyond the cap
    // in one cycle are ignored (v0.3 produces at most one per detector).
    static constexpr std::size_t kMaxTracks = 16;
    static constexpr std::size_t kMaxObservations = 16;

private:
    TrackConfig track_config_;
    GatingConfig gating_config_;
    TrackIdGenerator id_generator_;
    std::vector<Track> tracks_;             // reserve(kMaxTracks)
    std::vector<std::uint8_t> observed_;    // per-track flag for this cycle
    std::vector<std::uint8_t> assigned_;    // per-observation flag for this cycle
};

}  // namespace tracking
