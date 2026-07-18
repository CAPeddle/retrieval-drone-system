// TRK-015: observation gating and association implementation. Semantics per
// the v0.3 slice plan (R2 type-keyed gates, R3 slot release at Lost) and
// TRK-015's acceptance criteria.

#include "tracker.hpp"

#include <algorithm>
#include <array>
#include <cstddef>

namespace tracking {

namespace {

// A candidate association within the gate. Squared distance is enough for
// ordering and avoids the sqrt on the hot path.
struct Candidate {
    float distance_sq = 0.0F;
    std::uint16_t track_index = 0;
    std::uint16_t observation_index = 0;
};

float distance_sq(const cv::Point2f& a, const cv::Point2f& b) noexcept {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

}  // namespace

Tracker::Tracker(const TrackConfig& track_config, const GatingConfig& gating_config)
    : track_config_(track_config), gating_config_(gating_config) {
    tracks_.reserve(kMaxTracks);
    observed_.reserve(kMaxTracks);
    assigned_.reserve(kMaxObservations);
}

int Tracker::type_limit(ObjectType type) noexcept {
    switch (type) {
        case ObjectType::Laser:
            return 1;
        case ObjectType::Ball:
            return 1;
        case ObjectType::CalibrationMarker:
            return 8;  // multi-marker; not tracked at runtime in v0.3 but the
                       // gating logic stays general (TRK-015 acceptance)
    }
    return 0;
}

const std::vector<Track>& Tracker::update(const std::vector<Observation>& observations,
                                          std::int64_t now_ns) {
    const std::size_t obs_count = std::min(observations.size(), kMaxObservations);
    observed_.assign(tracks_.size(), 0);
    assigned_.assign(obs_count, 0);

    // Collect every in-gate same-type candidate pair, then take them globally
    // nearest-first so "nearest wins" holds for both contention directions
    // (two observations for one track, two tracks for one observation).
    std::array<Candidate, kMaxTracks * kMaxObservations> candidates{};
    std::size_t candidate_count = 0;
    const float gate_sq = static_cast<float>(gating_config_.max_distance_px *
                                             gating_config_.max_distance_px);
    for (std::size_t t = 0; t < tracks_.size(); ++t) {
        if (!tracks_[t].accepts_observations()) {
            continue;  // Lost/Retired take no associations (plan R3)
        }
        for (std::size_t o = 0; o < obs_count; ++o) {
            if (observations[o].type != tracks_[t].type()) {
                continue;  // type-keyed, structurally (plan R2)
            }
            const float d_sq = distance_sq(observations[o].centroid_px,
                                           tracks_[t].position_px());
            if (d_sq < gate_sq) {
                candidates[candidate_count++] = {d_sq, static_cast<std::uint16_t>(t),
                                                 static_cast<std::uint16_t>(o)};
            }
        }
    }
    std::sort(candidates.begin(), candidates.begin() + candidate_count,
              [](const Candidate& a, const Candidate& b) {
                  return a.distance_sq < b.distance_sq;
              });
    for (std::size_t c = 0; c < candidate_count; ++c) {
        const Candidate& cand = candidates[c];
        if (observed_[cand.track_index] != 0 || assigned_[cand.observation_index] != 0) {
            continue;
        }
        tracks_[cand.track_index].observe(observations[cand.observation_index]);
        observed_[cand.track_index] = 1;
        assigned_[cand.observation_index] = 1;
    }

    // Unmatched observations spawn Provisional tracks when a per-type slot is
    // free. Slot occupancy counts only tracks still accepting observations.
    for (std::size_t o = 0; o < obs_count; ++o) {
        if (assigned_[o] != 0 || tracks_.size() >= kMaxTracks) {
            continue;
        }
        const ObjectType type = observations[o].type;
        int live_of_type = 0;
        for (const Track& track : tracks_) {
            if (track.type() == type && track.accepts_observations()) {
                ++live_of_type;
            }
        }
        if (live_of_type >= type_limit(type)) {
            continue;
        }
        tracks_.emplace_back(id_generator_.allocate(), observations[o], track_config_);
        observed_.push_back(1);  // the birth observation counts for this cycle
    }

    for (std::size_t t = 0; t < tracks_.size(); ++t) {
        tracks_[t].tick(observed_[t] != 0, now_ns);
    }

    tracks_.erase(std::remove_if(tracks_.begin(), tracks_.end(),
                                 [](const Track& track) {
                                     return track.state() == TrackState::Retired;
                                 }),
                  tracks_.end());
    return tracks_;
}

}  // namespace tracking
