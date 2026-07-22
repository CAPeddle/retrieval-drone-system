// TRK-009a / U2 — PSD strategy benchmark.
//
// Measures the two candidate correlation strategies from KTD-4 over the shared
// synthetic modulation fixture (U1) and reports timing + a correctness table.
// The KTD-4 rule is applied by the *executor* to the on-Pi Release numbers; this
// binary computes the rule mechanically as a convenience but does not bake in a
// winner (the winner depends on measured hardware timing).
//
//   Option A — full-frame 8-point DFT at the modulation bin, every pixel.
//   Option B — brightness-prefiltered: a per-window max-projection gates which
//              pixels get the (expensive) 8-tap DFT.
//
// KTD-4 rule: B is admissible only if it detects every case A detects on the
// synthetic suite — including the dim-at-floor dot and the bright-clutter scene.
// Among admissible strategies choose the lower p99 on the Pi; a tie or any
// ambiguity → A (no extra threshold, simpler). Dev-host timings are NOT evidence
// (production-binary-oracle learning): only on-target Release numbers decide.
//
// Standalone binary: links tracking_core_lib, reaches the header-only fixture via
// an include path to tests/cpp_unit, and is never registered with ctest.

#include "modulation_fixture.hpp"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace {

using modulation_fixture::DotSpec;
using modulation_fixture::Frame;

// ─── Provisional thresholds (R14) ────────────────────────────────────────────
// PROVISIONAL — pending the operator's modulated-laser recording session. These
// exist only to make the A/B correctness comparison meaningful; they are NOT the
// shipped detector thresholds (those are pinned in U3 config, KTD-2/KTD-8).
constexpr double kWindow = 8;                 // two modulation periods (KTD-1)
constexpr double kPowerMin = 2500.0;          // raw |X2|^2 floor
constexpr double kPurityMin = 0.5;            // > worst-case step purity (1/3), KTD-2
constexpr double kBrightnessPrefilter = 30.0; // Option B candidate gate (see log note)
constexpr int kMinClusterArea = 4;
constexpr int kMaxClusterArea = 6000;
constexpr double kTruthTolPx = 8.0;

// DFT at bin 2 of 8: Re = sum cos(pi*n/2) v[n], Im = sum -sin(pi*n/2) v[n].
constexpr std::array<double, 8> kCos = {1, 0, -1, 0, 1, 0, -1, 0};
constexpr std::array<double, 8> kNegSin = {0, -1, 0, 1, 0, -1, 0, 1};

struct Scratch {
    cv::Mat power;      // CV_32F
    cv::Mat mask;       // CV_8U
    cv::Mat max_proj;   // CV_8U (Option B)
    cv::Mat candidate;  // CV_8U (Option B)
    cv::Mat labels, stats, centroids;
    cv::Mat kernel;
    explicit Scratch(int rows, int cols) {
        power.create(rows, cols, CV_32F);
        mask.create(rows, cols, CV_8U);
        max_proj.create(rows, cols, CV_8U);
        candidate.create(rows, cols, CV_8U);
        kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3));
    }
};

struct DetectResult {
    int clusters = 0;
    bool detected_at_truth = false;
};

// Consumes each timed detect() result so the optimiser cannot elide the call.
volatile int g_sink = 0;

// One detect cycle over an 8-frame window. `prefilter` selects Option B.
DetectResult detect(const std::array<const cv::Mat*, 8>& win, bool prefilter,
                    cv::Point2d truth, Scratch& s) {
    const int rows = win[0]->rows;
    const int cols = win[0]->cols;

    if (prefilter) {
        win[0]->copyTo(s.max_proj);
        for (int n = 1; n < 8; ++n) {
            cv::max(s.max_proj, *win[n], s.max_proj);
        }
        cv::compare(s.max_proj, kBrightnessPrefilter, s.candidate, cv::CMP_GT);
    }

    s.power.setTo(0);
    s.mask.setTo(0);
    for (int y = 0; y < rows; ++y) {
        std::array<const unsigned char*, 8> pr;
        for (int n = 0; n < 8; ++n) {
            pr[n] = win[n]->ptr<unsigned char>(y);
        }
        const unsigned char* cand = prefilter ? s.candidate.ptr<unsigned char>(y) : nullptr;
        float* powrow = s.power.ptr<float>(y);
        unsigned char* mrow = s.mask.ptr<unsigned char>(y);
        for (int x = 0; x < cols; ++x) {
            if (cand != nullptr && cand[x] == 0) {
                continue;
            }
            double re = 0.0;
            double im = 0.0;
            double sum = 0.0;
            double sumsq = 0.0;
            for (int n = 0; n < 8; ++n) {
                const double v = pr[n][x];
                re += kCos[n] * v;
                im += kNegSin[n] * v;
                sum += v;
                sumsq += v * v;
            }
            const double bin_power = re * re + im * im;
            const double ac = sumsq - sum * sum / kWindow;
            // Two-sided purity (KTD-2): pure on-frequency signal -> 1.0,
            // worst-case step -> 1/3.
            const double purity = ac > 1e-6 ? 2.0 * bin_power / (kWindow * ac) : 0.0;
            if (bin_power >= kPowerMin && purity >= kPurityMin) {
                powrow[x] = static_cast<float>(bin_power);
                mrow[x] = 255;
            }
        }
    }

    cv::morphologyEx(s.mask, s.mask, cv::MORPH_CLOSE, s.kernel);
    const int n = cv::connectedComponentsWithStats(s.mask, s.labels, s.stats,
                                                   s.centroids, 8, CV_32S);

    DetectResult result;
    for (int label = 1; label < n; ++label) {
        const int area = s.stats.at<int>(label, cv::CC_STAT_AREA);
        if (area < kMinClusterArea || area > kMaxClusterArea) {
            continue;
        }
        ++result.clusters;
        const double cx = s.centroids.at<double>(label, 0);
        const double cy = s.centroids.at<double>(label, 1);
        if (std::hypot(cx - truth.x, cy - truth.y) <= kTruthTolPx) {
            result.detected_at_truth = true;
        }
    }
    return result;
}

struct Timing {
    double mean_us = 0.0;
    double p99_us = 0.0;
};

Timing summarise(std::vector<double>& samples) {
    Timing t;
    if (samples.empty()) {
        return t;
    }
    double total = 0.0;
    for (double v : samples) {
        total += v;
    }
    t.mean_us = total / static_cast<double>(samples.size());
    std::sort(samples.begin(), samples.end());
    const std::size_t idx =
        static_cast<std::size_t>(0.99 * static_cast<double>(samples.size() - 1));
    t.p99_us = samples[idx];
    return t;
}

struct Scenario {
    std::string name;
    std::vector<Frame> frames;
    bool positive = false;  // a true modulated laser IS present
    cv::Point2d truth{0, 0};
};

std::vector<Scenario> build_scenarios() {
    const cv::Point2d dot_centre{200.0, 240.0};
    DotSpec dim;
    dim.cx = dot_centre.x;
    dim.cy = dot_centre.y;
    dim.radius = 4.0;
    dim.amplitude = 24.0;  // dim-at-floor: PSD-detectable but faint

    DotSpec mid = dim;
    mid.amplitude = 50.0;
    DotSpec bright = dim;
    bright.amplitude = 100.0;
    DotSpec empty = dim;
    empty.amplitude = 0.0;  // background only

    const int nf = 16;
    std::vector<Scenario> s;
    s.push_back({"true_dot_dim", modulation_fixture::modulated_dot(1, dim, nf), true, dot_centre});
    s.push_back({"true_dot_mid", modulation_fixture::modulated_dot(2, mid, nf), true, dot_centre});
    s.push_back({"true_dot_bright", modulation_fixture::modulated_dot(3, bright, nf), true, dot_centre});
    s.push_back({"bright_clutter",
                 modulation_fixture::bright_clutter_scene(4, dim, nf), true, dot_centre});
    s.push_back({"step_offset8",
                 modulation_fixture::static_step(5, /*offset=*/8, nf), false, {}});
    s.push_back({"moving_edge",
                 modulation_fixture::translating_edge(6, nf), false, {}});
    s.push_back({"empty_noise", modulation_fixture::modulated_dot(7, empty, nf), false, {}});
    return s;
}

// Evaluate one strategy across every scenario: correctness on the steady-state
// (last full) window, timing across all windows x repeats.
struct StrategyReport {
    std::vector<DetectResult> correctness;  // per scenario
    std::vector<Timing> timing;             // per scenario
    Timing overall;
};

StrategyReport run_strategy(const std::vector<Scenario>& scenarios, bool prefilter,
                            int repeats, Scratch& s) {
    StrategyReport rep;
    std::vector<double> all_samples;
    for (const Scenario& sc : scenarios) {
        const int num = static_cast<int>(sc.frames.size());
        const int last_start = num - 8;

        // Correctness on the steady-state window.
        std::array<const cv::Mat*, 8> win;
        for (int n = 0; n < 8; ++n) {
            win[n] = &sc.frames[last_start + n].image;
        }
        rep.correctness.push_back(detect(win, prefilter, sc.truth, s));

        // Timing across every window, repeated.
        std::vector<double> samples;
        samples.reserve(static_cast<std::size_t>((last_start + 1) * repeats));
        for (int r = 0; r < repeats; ++r) {
            for (int start = 0; start <= last_start; ++start) {
                for (int n = 0; n < 8; ++n) {
                    win[n] = &sc.frames[start + n].image;
                }
                const auto t0 = std::chrono::steady_clock::now();
                const DetectResult sink = detect(win, prefilter, sc.truth, s);
                const auto t1 = std::chrono::steady_clock::now();
                g_sink += sink.clusters;
                samples.push_back(
                    std::chrono::duration<double, std::micro>(t1 - t0).count());
            }
        }
        rep.timing.push_back(summarise(samples));
        all_samples.insert(all_samples.end(), samples.begin(), samples.end());
    }
    rep.overall = summarise(all_samples);
    return rep;
}

void print_table(const char* label, const std::vector<Scenario>& scenarios,
                 const StrategyReport& rep) {
    std::printf("\n== Strategy %s ==\n", label);
    std::printf("%-16s %-9s %8s %8s %10s %10s\n", "scenario", "expect",
                "clusters", "at_dot", "mean_us", "p99_us");
    for (std::size_t i = 0; i < scenarios.size(); ++i) {
        std::printf("%-16s %-9s %8d %8s %10.1f %10.1f\n", scenarios[i].name.c_str(),
                    scenarios[i].positive ? "detect" : "reject",
                    rep.correctness[i].clusters,
                    rep.correctness[i].detected_at_truth ? "yes" : "no",
                    rep.timing[i].mean_us, rep.timing[i].p99_us);
    }
    std::printf("%-16s %-9s %8s %8s %10.1f %10.1f\n", "OVERALL", "", "", "",
                rep.overall.mean_us, rep.overall.p99_us);
}

// A scenario is "detected" by a strategy when it produces a truth-located
// cluster (positive) — the KTD-4 admissibility set is exactly the positives A
// detects. Negatives must reject; a strategy that clusters on a negative is
// flagged (a correctness failure independent of the A-vs-B choice).
bool detected(const DetectResult& r, bool positive) {
    return positive ? r.detected_at_truth : (r.clusters == 0);
}

}  // namespace

int main() {
    std::printf("PSD strategy benchmark (TRK-009a / U2)\n");
    std::printf("window=8 frames (two periods), power_min=%.0f, purity_min=%.2f, "
                "prefilter=%.0f (all PROVISIONAL)\n",
                kPowerMin, kPurityMin, kBrightnessPrefilter);

    const std::vector<Scenario> scenarios = build_scenarios();
    Scratch scratch(modulation_fixture::kRows, modulation_fixture::kCols);
    const int repeats = 100;

    const StrategyReport a = run_strategy(scenarios, /*prefilter=*/false, repeats, scratch);
    const StrategyReport b = run_strategy(scenarios, /*prefilter=*/true, repeats, scratch);

    print_table("A (full-frame DFT)", scenarios, a);
    print_table("B (brightness-prefiltered)", scenarios, b);

    // ─── Mechanical KTD-4 evaluation ─────────────────────────────────────────
    std::printf("\n== KTD-4 evaluation ==\n");
    bool a_correct = true;
    bool b_admissible = true;
    for (std::size_t i = 0; i < scenarios.size(); ++i) {
        const bool pos = scenarios[i].positive;
        const bool a_det = detected(a.correctness[i], pos);
        const bool b_det = detected(b.correctness[i], pos);
        if (!a_det) {
            a_correct = false;
            std::printf("  A MISSES expected outcome on %s\n", scenarios[i].name.c_str());
        }
        // B is inadmissible if it fails to match A on any case A gets right.
        if (a_det && !b_det) {
            b_admissible = false;
            std::printf("  B diverges from A on %s (A=%s, B=%s)\n",
                        scenarios[i].name.c_str(), a_det ? "ok" : "fail",
                        b_det ? "ok" : "fail");
        }
    }

    std::printf("  A meets every expected outcome: %s\n", a_correct ? "yes" : "NO");
    std::printf("  B admissible (matches A everywhere A is correct): %s\n",
                b_admissible ? "yes" : "no");
    std::printf("  A p99=%.1f us, B p99=%.1f us\n", a.overall.p99_us, b.overall.p99_us);

    const char* decision;
    if (!a_correct) {
        decision = "INVESTIGATE — Option A missed an expected detection; thresholds "
                   "or fixture need review before a strategy is chosen";
    } else if (b_admissible && b.overall.p99_us < a.overall.p99_us) {
        decision = "Option B (admissible and lower p99)";
    } else {
        decision = "Option A (B inadmissible or not faster; tie/ambiguity -> A)";
    }
    std::printf("  DECISION (apply to on-Pi Release numbers only): %s\n", decision);
    return 0;
}
