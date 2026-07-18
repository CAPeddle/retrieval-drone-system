---
title: "OpenCV version-portability seam: one aruco detector source for 4.6 and 4.10"
captured: 2026-07-18
applies_to: [tracking-core]
tags: [opencv, aruco, cmake, version-portability, preprocessor-seam]
problem_type: pattern
source: internal
category: cpp
module: tracking-core/src/core/tracking
severity: medium
applies_when:
  - "One C++ source must build against two installed OpenCV versions with divergent APIs (dev box 4.6 Debian-packaged vs Pi 5 4.10)"
  - "An OpenCV module migrated between contrib and mainline across the version boundary (aruco -> objdetect in 4.7)"
  - "A version #if guard is being added around a third-party call and post-processing logic risks being duplicated per branch"
  - "A link error appears only for symbols from a contrib module despite headers resolving"
symptoms:
  - "undefined reference to cv::aruco::getPredefinedDictionary at link time on the 4.6 dev box — headers found, contrib aruco library not requested in find_package"
  - "cv::aruco::detectMarkers free function and DetectorParameters::create() absent in >=4.7 where ArucoDetector class and by-value Dictionary replace them"
root_cause: wrong_api
resolution_type: code_fix
---

# OpenCV version-portability seam: one aruco detector source for 4.6 and 4.10

## Summary

One C++ source must build and behave identically on OpenCV 4.6 (dev box,
contrib `aruco` free-function API) and 4.10 (Pi 5, `objdetect`
`ArucoDetector` class API); the break landed at 4.7. The seam has three
coordinated pieces — a `#if CV_VERSION` guard covering includes AND member
types AND the detect call; a version-branched CMake `find_package` component
(`aruco` < 4.7, `objdetect` ≥ 4.7); and a single shared post-processing helper
so the branches diverge only in the detect call. Miss the CMake piece and you
get a link error on one box only; miss the shared helper and the branches
silently drift because each machine compiles only its own copy. The on-target
Pi run is the only existence proof of the ≥ 4.7 branch.

# OpenCV 4.6/4.10 ArUco API seam — three coordinated pieces or it breaks

## Context

The tracking core builds on two OpenCV versions: the dev box ships 4.6, where
ArUco lives in the **contrib `aruco` module** and exposes only the
free-function API (`cv::aruco::detectMarkers`, `Ptr<Dictionary>`,
`DetectorParameters::create()`); the Pi 5 ships 4.10, where ArUco was merged
into **`objdetect`** and exposes the class API (`cv::aruco::ArucoDetector`).
The breaking change landed in **OpenCV 4.7** — that release is both where the
class API became canonical and where the module moved out of contrib, which is
why every guard in this codebase tests `CV_VERSION_MAJOR == 4 &&
CV_VERSION_MINOR < 7`.

TRK-011's `CalibrationMarkerDetector` (the ADR-004 Phase 2 calibration-health
marker detector) had to run identically on both. Getting there surfaced a
pattern with three coordinated pieces — miss any one and the build breaks or
the branches silently drift apart. This was learned the hard way: an early cut
omitted the CMake piece and produced a real link error on the dev box, and the
first working version duplicated the refinement loop in both branches until
review flagged it as semantic-drift risk.

## Guidance

The seam is one pattern with three pieces. All three must move together.

### Piece 1 — one preprocessor guard covering includes, member types, AND the detect call

The same `#if CV_VERSION_MAJOR == 4 && CV_VERSION_MINOR < 7` condition appears
three times in the detector, and all three sites must agree. Guarding the
include but not the members (or vice versa) fails to compile on one box or the
other.

**Includes** — `opencv2/core/version.hpp` must come first so the guard macros
exist, then the branch picks contrib `aruco.hpp` vs
`objdetect/aruco_detector.hpp`
(`tracking-core/src/core/include/calibration_marker_detector.hpp`):

```cpp
#include <opencv2/core.hpp>
#include <opencv2/core/version.hpp>

#if CV_VERSION_MAJOR == 4 && CV_VERSION_MINOR < 7
#include <opencv2/aruco.hpp>
#else
#include <opencv2/objdetect/aruco_detector.hpp>
#endif
```

**Member types** — the old API hands out `cv::Ptr<>` handles; the new API uses
value types plus a detector object. The public surface stays identical; only
the private members differ:

```cpp
#if CV_VERSION_MAJOR == 4 && CV_VERSION_MINOR < 7
    cv::Ptr<cv::aruco::Dictionary> dictionary_;
    cv::Ptr<cv::aruco::DetectorParameters> params_;
#else
    cv::aruco::Dictionary dictionary_;
    cv::aruco::ArucoDetector detector_;
#endif
```

**Constructor + detect call** — each branch constructs its own flavour and
calls its own entry point
(`tracking-core/src/core/tracking/calibration_marker_detector.cpp`):

```cpp
#if CV_VERSION_MAJOR == 4 && CV_VERSION_MINOR < 7

CalibrationMarkerDetector::CalibrationMarkerDetector(const CalibrationConfig& config)
    : dictionary_(cv::aruco::getPredefinedDictionary(
          aruco_dictionary_id(config.aruco_dictionary))),
      params_(cv::aruco::DetectorParameters::create()) {}
// ... detect():
    cv::aruco::detectMarkers(*gray, dictionary_, corners_, ids_, params_);
    return build_observations(*gray);

#else

CalibrationMarkerDetector::CalibrationMarkerDetector(const CalibrationConfig& config)
    : dictionary_(cv::aruco::getPredefinedDictionary(
          aruco_dictionary_id(config.aruco_dictionary))),
      detector_(dictionary_, cv::aruco::DetectorParameters()) {}
// ... detect():
    detector_.detectMarkers(*gray, corners_, ids_);
    return build_observations(*gray);

#endif
```

### Piece 2 — CMake must request the right component per version

Headers resolving is not linking. On 4.6 the ArUco symbols live in the contrib
`libopencv_aruco` library; on ≥4.7 they live in `libopencv_objdetect`. The
`find_package` must branch on `OpenCV_VERSION`
(`tracking-core/CMakeLists.txt`):

```cmake
# ArUco (TRK-011) lives in the contrib `aruco` module through OpenCV 4.6 and
# was merged into `objdetect` in 4.7+. Request the right one per version so the
# same source links on the 4.6 dev box and the 4.10 Pi 5 (KTD-2).
find_package(OpenCV REQUIRED COMPONENTS core imgproc videoio calib3d)
if(OpenCV_VERSION VERSION_LESS 4.7)
    find_package(OpenCV REQUIRED COMPONENTS aruco)
else()
    find_package(OpenCV REQUIRED COMPONENTS objdetect)
endif()
```

Omitting this produced the actual failure on the dev box: compilation
succeeded (the aruco headers were on the include path from the umbrella
install) but the link failed with

```
undefined reference to `cv::aruco::getPredefinedDictionary(...)'
```

because the contrib library was never added to the link line. See the worked
example below.

### Piece 3 — everything version-independent lives in ONE shared helper

Corner refinement (`cv::cornerSubPix`), centroid computation, and the mean
corner-refinement residual do not depend on which ArUco API filled `ids_` and
`corners_`. They live in a single private `build_observations()` that both
branches call, so the `#if` branches differ **only** in the detect call:

```cpp
// Refines the detected corners in corners_/ids_ (filled by the version-
// specific detect call) into observations. Shared by both API branches so
// the refinement/centroid/residual logic lives in one place.
std::vector<MarkerObservation> build_observations(const cv::Mat& gray) const;
```

The contract is subtle but load-bearing: the version-specific branch's job is
to populate the pre-allocated `ids_`/`corners_` members; the shared helper
consumes them. That narrow interface is what keeps the branches from drifting.

### The tests need the same seam

Any test utility touching the ArUco API needs its own guard — marker
generation also changed at 4.7 (`Dictionary::drawMarker` member function →
`cv::aruco::generateImageMarker` free function). From
`tracking-core/tests/cpp_unit/test_calibration_marker_detector.cpp`:

```cpp
cv::Mat generate_marker(const std::string& dict_name, int id, int side) {
    const int dict_id = tracking::aruco_dictionary_id(dict_name);
    cv::Mat marker;
#if CV_VERSION_MAJOR == 4 && CV_VERSION_MINOR < 7
    cv::Ptr<cv::aruco::Dictionary> dict = cv::aruco::getPredefinedDictionary(dict_id);
    dict->drawMarker(id, side, marker, 1);
#else
    cv::aruco::Dictionary dict = cv::aruco::getPredefinedDictionary(dict_id);
    cv::aruco::generateImageMarker(dict, id, side, marker, 1);
#endif
    return marker;
}
```

### Verification asymmetry — remote testing is load-bearing

Each environment compiles **only its own branch**. The preprocessor discards
the other branch entirely, so a green build-and-test run on the 4.6 dev box
proves nothing about the ≥4.7 branch — not that it compiles, not that it
links, not that it behaves. The ≥4.7 branch is proven **only** by the
on-target Pi 5 run via `tools/pi5-remote-test.sh`. A "both build configs green
locally" result covers both Debug/Release configs of *one* branch and says
nothing about the other. Treat the Pi run as a ship gate for any change inside
the seam, in either direction — a Pi-only workflow would leave the <4.7 branch
equally unproven.

## Why This Matters

- **The three pieces fail differently, which makes partial seams deceptive.**
  Missing the include guard or member guard is a loud compile error. Missing
  the CMake component is a link error that only appears on the 4.6 box — the
  ≥4.7 box links fine because `objdetect` is a core module commonly pulled in
  transitively. Missing the shared helper is the worst: it compiles and passes
  everywhere, then the branches drift the first time someone edits one
  refinement loop and not the other, producing behaviour that differs by
  deployment target. In this codebase that would mean the calibration-health
  residual (`corner_residual_px`, feeding ADR-004 Phase 2 monitoring) silently
  meaning different things on the dev box and the Pi.
- **The verification asymmetry defeats normal intuition about test coverage.**
  Unit tests exercising the detector exhaustively on the dev box still leave
  half the production code — the half that actually runs on the Pi 5 —
  untouched by the compiler. On-target testing is not a nicety here; it is the
  only existence proof for the deployed branch.
- **The pattern generalises.** Any dependency whose API changed between the
  dev-box and Pi versions will need the same three pieces: a version guard
  covering every API-touching site, a build-system branch selecting the right
  component/library, and ruthless extraction of version-independent logic into
  shared code.

## When to Apply

- Adding any new `cv::aruco` usage anywhere in the tree (production or tests)
  — it must carry the `< 4.7` guard and, if it introduces a new translation
  unit, confirm the CMake component branch already covers it.
- Editing either branch of an existing seam — check whether the edit is
  version-independent; if so it belongs in the shared helper, not in one
  branch.
- Diagnosing an `undefined reference to cv::aruco::...` link error — check the
  version-branched `find_package` before suspecting the code.
- Introducing a second version-seamed dependency — replicate all three pieces,
  don't just guard the includes.
- Before claiming a seam change is done — the Pi 5 on-target run
  (`tools/pi5-remote-test.sh`) is required, because local greens only prove
  one branch.

## Examples

### Worked example — the link error from omitting Piece 2

Sequence on the 4.6 dev box, with the source seam (Piece 1) in place but the
plain `find_package(OpenCV REQUIRED COMPONENTS core imgproc videoio calib3d)`
unbranched:

1. Configure succeeds — CMake finds OpenCV 4.6.
2. Compile succeeds — `opencv2/aruco.hpp` is on the include path because the
   distro's `libopencv-dev` installs all headers regardless of which
   components were requested.
3. Link fails:
   `undefined reference to cv::aruco::getPredefinedDictionary`.
4. Cause: `getPredefinedDictionary` (and `detectMarkers`) live in the contrib
   `libopencv_aruco.so`, which is only added to `${OpenCV_LIBS}` when the
   `aruco` component is requested. On ≥4.7 the same symbols live in
   `libopencv_objdetect.so`, which is why the omission never bites on the Pi.
5. Fix: the `if(OpenCV_VERSION VERSION_LESS 4.7)` component branch shown
   above.

The trap: "the headers compile, so the dependency is wired up" is false for
component-based packages. Compile visibility and link visibility are granted
by different mechanisms.

### Before/after — the shared-helper refactor (Piece 3)

**Before (review-flagged shape):** each `#if` branch carried its own full
copy of the post-detection loop — per-corner `cornerSubPix` refinement,
centroid accumulation, and residual computation — roughly:

```cpp
#if CV_VERSION_MAJOR == 4 && CV_VERSION_MINOR < 7
std::vector<MarkerObservation> CalibrationMarkerDetector::detect(const cv::Mat& frame) {
    // ... grayscale conversion ...
    cv::aruco::detectMarkers(*gray, dictionary_, corners_, ids_, params_);
    // ~20 lines: refine corners, compute centroid, compute residual  <-- copy 1
}
#else
std::vector<MarkerObservation> CalibrationMarkerDetector::detect(const cv::Mat& frame) {
    // ... grayscale conversion ...
    detector_.detectMarkers(*gray, corners_, ids_);
    // ~20 lines: the same loop, duplicated                            <-- copy 2
}
#endif
```

Review flagged the duplication as semantic-drift risk: because each
environment compiles only one copy, a divergence between the copies would
never be caught by any single machine's build or tests.

**After (current shape):** the loop exists once as
`build_observations(const cv::Mat& gray)`, consuming the member
`ids_`/`corners_` state that the version-specific detect call populates. Each
branch is reduced to grayscale conversion + its one-line detect call +
`return build_observations(*gray);` — the only lines that genuinely differ
between OpenCV versions. (Full current code in
`tracking-core/src/core/tracking/calibration_marker_detector.cpp`, lines
28–53 for the helper, 55–93 for the branches.)

The refactor rule of thumb: inside a version seam, a line that would be
identical in both branches is a bug waiting to happen — hoist it.

## Trade-offs

- The seam doubles the API surface to reason about and halves what any single
  machine can verify — the price of supporting a Debian-packaged dev box
  without requiring a source-built OpenCV. The alternative (pinning one OpenCV
  version everywhere, as the spdlog entry pins v1.15.3) was rejected because
  the dev box's 4.6 comes from the distro and the Pi's 4.10 from its OS image;
  neither is worth rebuilding for a two-branch seam with a narrow interface.
- The `< 4.7` boundary is encoded twice (C++ `CV_VERSION_MINOR < 7`, CMake
  `VERSION_LESS 4.7`). They must agree; the link-error episode is what
  disagreement looks like. A comment at each site points at the other.

## Related

- `tracking-core/src/core/include/calibration_marker_detector.hpp` — header
  seam (includes + members) and the `build_observations` declaration.
- `tracking-core/src/core/tracking/calibration_marker_detector.cpp` — both
  branches around the shared helper.
- `tracking-core/CMakeLists.txt` — version-branched `find_package` block.
- `tracking-core/tests/cpp_unit/test_calibration_marker_detector.cpp` —
  `generate_marker` test-side seam (`drawMarker` vs `generateImageMarker`).
- `tools/pi5-remote-test.sh` — the on-target run that is the only proof of
  the ≥4.7 branch.
- ADR-004 (calibration health monitoring) — the consumer of this detector;
  TRK-011 is the implementing ticket, KTD-2 the version-seam decision record.
- For the pin-the-version (rather than branch-on-version) response to a
  library-version reality, see
  [spdlog async hot-path logging](2026-07-16-spdlog-async-hot-path-logging.md)
  — its v1.14 promise gotcha was solved by pinning v1.15.3.
- On why on-target Pi runs are the load-bearing proof (local tests can pass
  while the on-target branch fails), see
  [composite test targets must model occlusion](2026-07-18-composite-test-targets-model-occlusion.md),
  which uses the same `pi5-remote-test.sh` workflow.
