ME:

---------------------------------------------------------------------------------------------------------

As part of the Drone ball retrieval system we are working on I've drafted an initial plan for the camera processing system including some questions for further consideration. Review it for edge cases that have been missed. 

I would define the software as more than “visualisation” from the start:
Tracking Visualisation & Coordinate Export System
Its job is to ingest one or more camera streams, detect/track configured objects, estimate their position in a shared coordinate space where possible, visualise the state, and export structured tracking data to other modules.

Proposed system boundary
In scope
The module should own:

Camera input ingestion
One camera initially.
Scalable to multiple cameras.
Support live camera input and recorded/replay input for testing.
Object detection input
Laser point.
Ball.
Drone.
Future object types via plugin/interface.
Tracking state
Object identity.
Object type.
2D image coordinates per camera.
Optional fused/world coordinates.
Confidence.
Timestamp.
Tracking status: detected, predicted, lost, reacquired.
Coordinate transformation
Per-camera image coordinates.
Shared world coordinate frame when calibration data exists.
Graceful degradation when only one uncalibrated camera is available.
Visualisation
Live camera view overlays.
Object trails.
Multi-camera view.
Optional world/top-down view.
Debug overlays for calibration, confidence, detection masks, etc.
Export
World coordinates of tracked objects.
Per-camera detections.
Tracking metadata.
Stable interface for downstream control software.
Out of scope for now
I would keep these outside the first boundary:

Motor/servo/laser control
The visualisation/tracking module should export state.
A separate control module should consume state and decide actuation.
Path planning
For drones or balls, this module tracks and visualises; it does not decide navigation.
Hard real-time guarantees
We can design for low latency, but not hard real-time unless the hardware/software stack requires it.
Final object-specific ML model training
Detection algorithms can be plugged in, but the architecture should not depend on one detector.
Core architectural principle
The core should be headless and testable.
The GUI should not contain tracking logic. Instead:

Camera Sources
     ↓
Frame Ingestion
     ↓
Object Detectors
     ↓
Per-Camera Tracking
     ↓
Multi-Camera Fusion / Coordinate Mapping
     ↓
Tracking State Store
     ↓
Visualisation UI + Export Interfaces
The visualisation layer observes the state. It should not own the state.
First-pass component model
1. CameraSource
Responsible for producing timestamped frames.
Possible implementations:

UsbCameraSource
PiCameraSource
VideoFileReplaySource
ImageSequenceSource
MockCameraSource
NetworkCameraSource
Interface concept:

struct Frame {
    CameraId camera_id;
    Timestamp timestamp;
    Image image;
    FrameMetadata metadata;
};

class ICameraSource {
public:
    virtual std::optional<Frame> next_frame() = 0;
};
Important requirement:

The rest of the system should not care whether frames come from a real camera, a video file, or a test fixture.
This is important for TDD.
2. ObjectDetector
Responsible for producing observations from a frame.
Example object types:

LaserPoint
Ball
Drone
CalibrationMarker
Unknown
Interface concept:

struct ImageObservation {
    ObjectType type;
    ObjectId object_id;          // optional/stable if known
    CameraId camera_id;
    Timestamp timestamp;
    Point2D image_position;
    BoundingBox2D bounding_box;  // optional
    double confidence;
};

class IObjectDetector {
public:
    virtual std::vector<ImageObservation> detect(const Frame& frame) = 0;
};
Different detectors can exist:

LaserSpotDetector
BallColourDetector
DroneShapeDetector
AprilTagDetector
MockDetector
CompositeDetector
3. Tracker
Responsible for smoothing, identity continuity, loss/reacquisition.

struct Track2D {
    ObjectId object_id;
    ObjectType type;
    CameraId camera_id;
    Timestamp timestamp;
    Point2D image_position;
    Vector2D image_velocity;
    double confidence;
    TrackStatus status;
};
This layer can later use:

Simple nearest-neighbour tracking
Kalman filter
Particle filter
Optical flow
Detector + predictor hybrid
But the interface should remain stable.
4. CoordinateMapper
Responsible for converting camera-local coordinates into a shared frame.
There are three levels:

Level 0 — raw image coordinates only
Camera A sees ball at pixel x=320, y=240
Useful for early prototype.

Level 1 — normalised image coordinates
x = -0.2, y = 0.35 relative to image centre
Useful before full calibration.

Level 2 — world coordinates
ball = {x=1.2 m, y=0.4 m, z=0.8 m}
Requires calibration and/or multiple cameras.
Interface concept:

struct WorldObservation {
    ObjectId object_id;
    ObjectType type;
    Timestamp timestamp;
    Point3D position;
    Vector3D velocity;
    double confidence;
    Covariance3D uncertainty;
    std::vector<CameraId> contributing_cameras;
};
Important design choice:

The system should export uncertainty, not just position.
For control systems, knowing “position is roughly here” is often more useful than pretending the coordinate is exact.
5. FusionEngine
Responsible for combining observations from multiple cameras.
Cases:

1 camera:
  - export 2D image coordinates
  - optionally estimate world position if object is constrained to a known plane

2 cameras:
  - triangulate world position if calibrated

3+ cameras:
  - triangulate with redundancy
  - reject outlier cameras
  - improve confidence
Requirement:

Adding another camera should not require changing detector, tracker, exporter, or visualisation core logic.
6. TrackingStateStore
The system needs one central model of current state.

struct TrackingState {
    Timestamp timestamp;
    std::vector<Track2D> image_tracks;
    std::vector<WorldObservation> world_tracks;
    SystemHealth health;
};
Consumers:

GUI
Exporter
Logger
Control module
Test harness
This enables a clean observer/pub-sub model.
7. Visualisation
The UI should be downstream-only.
Views:

Single camera overlay
Multi-camera grid
World/top-down view
Object trail view
Latency/debug panel
Calibration view
Requirement:

Visualisation must never be required for the tracking core to run.
This allows headless operation on a Raspberry Pi, drone companion computer, or test CI runner.
8. ExportInterface
This is where production design matters.
Possible export options:

JSON over WebSocket
MQTT
ZeroMQ
gRPC
REST snapshot endpoint
CSV/log file
ROS2 topic, if robotics ecosystem becomes relevant
For your project, I would initially design the internal contract independent of transport:

class ITrackingDataSink {
public:
    virtual void publish(const TrackingState& state) = 0;
};
Then implementations:

JsonFileSink
WebSocketSink
MqttSink
Ros2Sink
InMemoryTestSink
Proposed requirement set v0.1
Functional requirements
IDRequirementTRK-001The system shall accept frames from one or more camera sources.TRK-002The system shall support live camera input and recorded replay input.TRK-003The system shall detect configurable tracked object types.TRK-004The system shall support at least laser point, ball, and drone as object categories.TRK-005The system shall produce per-camera 2D observations for detected objects.TRK-006The system shall maintain object tracks across frames.TRK-007The system shall mark tracks as lost when observations disappear.TRK-008The system shall support reacquisition of lost tracks.TRK-009The system shall support optional conversion from image coordinates to world coordinates.TRK-010The system shall support multi-camera fusion when calibration data is available.TRK-011The system shall export tracking state through a stable interface.TRK-012The system shall visualise camera feeds with tracking overlays.TRK-013The system shall visualise world-coordinate tracks where available.TRK-014The system shall expose confidence/uncertainty with each tracked position.TRK-015The system shall operate without a GUI in headless mode.
Non-functional requirements
IDRequirementNFR-001The tracking core shall be testable without real cameras.NFR-002The system shall support deterministic replay of recorded input.NFR-003The design shall separate input, detection, tracking, fusion, visualisation, and export.NFR-004Object detectors shall be replaceable without modifying the tracking pipeline.NFR-005Export transports shall be replaceable without modifying tracking logic.NFR-006The system shall measure and expose processing latency.NFR-007The system shall tolerate temporary camera loss.NFR-008The system shall tolerate temporary object detection loss.NFR-009The system shall log enough data to reproduce tracking behaviour offline.NFR-010The core model shall use explicit units: pixels, normalised coordinates, metres, seconds.Key interface requirement: coordinate export
I would define the exported tracking message around this kind of structure:

{
  "timestamp": "2026-05-07T14:32:18.123Z",
  "frame_index": 18422,
  "objects": [
    {
      "object_id": "ball-1",
      "type": "ball",
      "status": "tracked",
      "world_position": {
        "available": true,
        "x_m": 1.24,
        "y_m": 0.52,
        "z_m": 0.18
      },
      "world_velocity": {
        "available": true,
        "x_mps": 0.1,
        "y_mps": -0.3,
        "z_mps": 0.0
      },
      "confidence": 0.91,
      "uncertainty": {
        "position_stddev_m": 0.025
      },
      "observations": [
        {
          "camera_id": "cam-1",
          "x_px": 612,
          "y_px": 384,
          "confidence": 0.96
        },
        {
          "camera_id": "cam-2",
          "x_px": 441,
          "y_px": 391,
          "confidence": 0.87
        }
      ]
    }
  ]
}
This gives downstream modules enough information to decide whether the data is usable.
For example, the laser control system may only act when:

object.status == tracked
world_position.available == true
confidence > 0.8
position_stddev_m < 0.03
timestamp age < 100 ms
Important design decision: one module or two?
There are two viable boundaries.

Option A — Combined tracking + visualisation module
tracking_visualisation/
  owns camera input
  owns detection
  owns tracking
  owns visualisation
  owns export
Good for early development. Faster iteration.
Risk: visualisation and tracking can become coupled.

Option B — Tracking core + visualisation app
tracking_core/
  camera input
  detection
  tracking
  fusion
  export model

tracking_viewer/
  subscribes to tracking_core
  renders state
This is cleaner and more production-grade.
My recommendation:

Use Option B, but keep both in the same repository initially.
That gives you speed now and clean separation later.
Suggested repository/module layout
tracking-system/
  CMakeLists.txt
  README.md

  src/
    core/
      camera/
      detection/
      tracking/
      calibration/
      fusion/
      export/
      state/

    app/
      tracking_service/
      tracking_viewer/

  tests/
    unit/
    integration/
    replay/

  data/
    calibration/
    test_sequences/

  tools/
    record_camera_stream.cpp
    replay_sequence.cpp
    calibrate_cameras.cpp
If you use Python first for prototyping, the same structure still applies:

tracking_system/
  core/
    camera/
    detection/
    tracking/
    calibration/
    fusion/
    export/
    state/
  apps/
    tracking_service/
    viewer/
  tests/
TDD strategy
I would not start by testing the GUI. Start with the core contracts.

Unit tests
CameraSource emits timestamped frames
MockDetector produces expected observations
Tracker maintains identity across frames
Tracker marks object lost after N missed frames
CoordinateMapper converts known points correctly
FusionEngine triangulates synthetic camera observations
Exporter serialises valid tracking messages
Integration tests
Replay recorded sequence → expected object trajectory
Two synthetic cameras → expected 3D world point
Camera loss → system remains alive
Detector confidence drops → track confidence degrades
Golden-data tests
Use recorded or synthetic scenarios:

laser_static_center
laser_moves_left_to_right
ball_rolls_across_floor
object_occluded_then_reappears
two_cameras_same_ball
camera_2_temporarily_missing
This is where the architecture becomes robust.
First decisions to make
Please answer these in order; short answers are enough.

1. Primary runtime environment
Should we design first for:
A. Python prototype
Fastest for OpenCV, calibration, experimentation.
B. C++ production core
Better for latency, robustness, embedded deployment.
C. Hybrid
Python first, with interfaces shaped so the core can later move to C++.
My recommendation: C — Hybrid, with strong interface discipline from day one.
2. Primary output consumer
Who consumes the exported coordinates first?
A. Visualisation only
B. Laser control algorithm
C. Drone/robot control system
D. Data logging/research tooling
E. Multiple consumers from the start
My recommendation: B + D first.
3. World coordinate assumptions
Should the first version assume:
A. No world coordinates, only image coordinates
B. 2D world plane, e.g. floor/table/wall surface
C. Full 3D world coordinates from multiple calibrated cameras
D. Support all, but degrade gracefully depending on calibration/input
My recommendation: D, implemented in stages: image coordinates → plane coordinates → 3D.
4. Camera scaling target
What should the first production design explicitly support?
A. 1 camera only, but no hardcoded assumptions
B. 1–2 cameras
C. 1–4 cameras
D. Arbitrary N cameras
My recommendation: design for D, test initially with 1–2.
5. First tracked object
Which object should define the first vertical slice?
A. Laser point
B. Ball
C. Drone
D. Calibration marker
My recommendation: Laser point first because it gives a clean, high-contrast, controllable signal. Then ball. Then drone.
My suggested first baseline is:

Hybrid Python/C++-friendly architecture.
Headless tracking core.
Viewer as separate consumer.
One or more camera sources.
Laser point as first tracked object.
Export TrackingState snapshots.
Start with image coordinates.
Add calibrated world coordinates as a second phase.
Design all interfaces for multi-camera from the beginning.
Answer the five decisions above, and I’ll turn this into a tighter v0.2 requirements document with explicit interfaces, data contracts, and acceptance tests.

---------------------------------------------------------------------------------------------------------

This is a remarkably solid and well-architected draft. Structuring the system with a headless core and a decoupled visualization/export layer (Option B) is absolutely the right call for a robotics pipeline, as it prevents UI threads from bottlenecking the tracking loop. Your interface concepts and TDD strategy are also spot-on.

Looking over the proposed boundaries and requirements, here are the edge cases and potential architectural blind spots that should be accounted for before finalizing the v0.2 requirements.

### **1. Environmental & Physical Edge Cases**
*   **Dynamic Scene Intrusions:** The testing and operating environment won't be perfectly sterile. You need to account for highly unpredictable actors entering the frame. If a 7-year-old girl reaches in to grab the ball before the drone gets there, or a 5-year-old boy suddenly darts across the camera's field of view, the system will face sudden, massive occlusions and unexpected motion vectors. The tracker (`TRK-007`/`TRK-008`) needs logic to gracefully handle an object instantly disappearing behind a moving obstacle rather than simply drifting out of frame.
*   **Z-Axis Occlusion (The "Hover" Problem):** When the drone is directly above the ball to retrieve it, the top-down or angled cameras may lose sight of the ball entirely. The `FusionEngine` needs to handle cases where the drone's bounding box completely envelops the ball's last known position.
*   **Lighting and Glare:** A glossy ball or drone chassis can reflect light, creating a "ghost" laser point or overexposing the camera sensor. The `ObjectDetector` interfaces might need an explicit way to flag "low-quality/glare-heavy" frames.

### **2. Ingestion & Hardware Edge Cases**
*   **USB Bus Saturation:** If you scale to 2+ cameras (`TRK-010`), pulling multiple uncompressed 1080p/60fps streams over a single USB controller will drop frames. The `FrameMetadata` should explicitly include a sequence number or hardware timestamp to detect dropped frames before they reach the tracker.
*   **Variable Frame Rates & Jitter:** `CameraSource` implementations rarely output frames at a perfectly locked interval. If Camera A delivers frames at 30fps and Camera B dips to 24fps due to low light exposure adjustments, the `FusionEngine` needs a robust interpolation strategy to match observations occurring at slightly different `Timestamp` values.
*   **Physical Camera Bump:** A slight bump to a tripod ruins Level 2 (world coordinate) calibration. The system should ideally have a continuous sanity check (e.g., observing a static calibration marker in the background) to detect if a camera has moved, degrading the confidence of its world mapping.

### **3. Tracking & Fusion Edge Cases**
*   **The Identity Crisis (Multi-Object Confusion):** If two identical balls are in the frame, or if a laser pointer briefly crosses paths with a red ball, the `Tracker` needs a strategy for resolving identity swaps. 
*   **Ghost Tracks:** False positives from the detector (e.g., a round shadow interpreted as a ball) could initiate a track. `TRK-006` should specify a "provisional" track state—requiring an object to be detected for *N* consecutive frames before it is officially exported as a valid `TrackingState`.

### **4. Export & Data Handling Edge Cases**
*   **Network Backpressure:** If you export via JSON/WebSocket (`TRK-011`) and the downstream consumer (or Wi-Fi network) lags, the export buffer could fill up. The `ExportInterface` must define whether it drops stale state packets or blocks the tracking loop (hint: always drop stale telemetry, never block the core).
*   **Clock Synchronization:** If cameras are on different physical nodes (e.g., NetworkCameraSource), system clocks will drift. The `TrackingState` timestamp must define whether it uses a single master clock or attempts to synchronize distributed timestamps (like NTP or PTP).

---

To turn this into a finalized v0.2 requirements document with explicit data contracts, which paths are you leaning towards for the five foundational decisions (Runtime, Output Consumer, World Coordinates, Camera Scaling, and First Tracked Object)?


ME:
---------------------------------------------------------------------------------------------------------
the edge cases are not secondary; they change the architecture. The v0.2 design should treat the system as a probabilistic, degraded-mode tracking system, not a simple “detect object → draw overlay → export coordinates” pipeline.
Your answers set the baseline as:

Runtime: Hybrid architecture
Primary consumer: System controller
Coordinate model: Degrade gracefully across image, plane, and 3D world coordinates
Camera scaling: Arbitrary N cameras
First tracked object: Laser point
That means the export contract must be safe enough for a controller to consume. The controller must know not only where the object is, but also how trustworthy the estimate is.
v0.2 architectural adjustment
I would revise the pipeline to this:

Camera Sources
  ↓
Frame Synchronisation / Timing Layer
  ↓
Frame Quality Assessment
  ↓
Object Detection
  ↓
Track Lifecycle Manager
  ↓
Occlusion & Identity Resolver
  ↓
Coordinate Mapping
  ↓
Multi-Camera Fusion
  ↓
World State Estimator
  ↓
Tracking State Store
  ↓
Non-blocking Export + Visualisation
The new pieces are important:

Frame Synchronisation / Timing Layer
Frame Quality Assessment
Track Lifecycle Manager
Occlusion & Identity Resolver
World State Estimator
Non-blocking Export
Calibration Health Monitor
These are not implementation details. They should become part of the requirements.
Updated core concepts
1. Frame metadata must be richer
The original FrameMetadata was too weak. It should explicitly support dropped frame detection, clock handling, and quality assessment.

struct FrameMetadata {
    CameraId camera_id;

    uint64_t sequence_number;
    Timestamp capture_timestamp;
    Timestamp receive_timestamp;

    std::optional<Timestamp> hardware_timestamp;

    uint32_t width_px;
    uint32_t height_px;
    double nominal_fps;
    std::optional<double> exposure_ms;
    std::optional<double> gain_db;

    FrameQuality quality;
    std::vector<FrameQualityFlag> quality_flags;
};
Example quality flags:

DroppedFrameSuspected
LateFrame
OverExposed
UnderExposed
MotionBlur
GlareDetected
PartialFrame
CameraClockUncertain
CalibrationMarkerMissing
CalibrationMarkerShifted
This directly addresses:

USB bus saturation
Variable frame rates
Jitter
Glare
Clock uncertainty
Physical camera bump
2. Tracks need lifecycle states
A track should not become “real” immediately after one detection.
Proposed states:

Provisional
Confirmed
Predicted
Occluded
Lost
Retired
Rejected
Meaning:
StateMeaningProvisionalPossible object, not yet trustedConfirmedStable enough to export as realPredictedTemporarily not detected, but expected position is still usableOccludedNot visible because another object or obstacle likely blocks itLostNo longer reliableRetiredTrack ended normallyRejectedFalse positive / ghost track
This gives you a cleaner answer to the child-running-through-the-scene problem. The ball should not instantly become lost just because a moving occluder crosses the frame. It should move from:

Confirmed → Occluded → Predicted → Confirmed
or, if not reacquired:

Confirmed → Occluded → Predicted → Lost → Retired
3. Export must distinguish measured, estimated, and stale data
The system controller must not treat all coordinates equally.
Exported object state should include:

{
  "object_id": "ball-1",
  "type": "ball",
  "track_state": "occluded",
  "position_source": "predicted",
  "coordinate_space": "world_3d",
  "timestamp": "2026-05-07T10:42:12.120Z",
  "age_ms": 64,
  "confidence": 0.62,
  "position": {
    "x_m": 1.42,
    "y_m": 0.33,
    "z_m": 0.04
  },
  "uncertainty": {
    "x_stddev_m": 0.04,
    "y_stddev_m": 0.05,
    "z_stddev_m": 0.12
  },
  "visibility": {
    "visible_camera_count": 1,
    "expected_visible_camera_count": 3,
    "occluded_by": ["drone-1"]
  },
  "safety": {
    "safe_for_control": false,
    "reason": "object_occluded"
  }
}
The key addition is:

safe_for_control
The tracker should not make control decisions, but it should provide enough information for the controller to reject unsafe states.
Updated v0.2 requirements
Environmental and physical robustness
IDRequirementENV-001The system shall tolerate temporary object occlusion without immediately retiring the track.ENV-002The system shall classify tracks as occluded when an object disappears behind another tracked object or detected foreground obstacle.ENV-003The system shall support predicted positions during short occlusions.ENV-004The system shall increase position uncertainty while an object is occluded or predicted.ENV-005The system shall flag frames affected by glare, overexposure, underexposure, or motion blur where detectable.ENV-006The system shall allow detectors to report degraded detection quality independently from object confidence.ENV-007The system shall support sudden large foreground intrusions without corrupting confirmed object identities.Camera ingestion and timing
IDRequirementCAM-001Every frame shall include a monotonically increasing per-camera sequence number.CAM-002Every frame shall include capture and receive timestamps.CAM-003The system shall detect suspected dropped frames.CAM-004The system shall tolerate variable frame intervals per camera.CAM-005The fusion layer shall not assume synchronized camera frames.CAM-006The system shall expose per-camera latency and frame jitter metrics.CAM-007The system shall support cameras with different resolutions, frame rates, and fields of view.CAM-008The system shall support camera health states: active, degraded, delayed, disconnected, recalibration-required.Tracking and identity
IDRequirementTRK-016Tracks shall begin in a provisional state.TRK-017A provisional track shall require configurable evidence before becoming confirmed.TRK-018Confirmed tracks shall remain alive for a configurable grace period after detections disappear.TRK-019The tracker shall model occluded tracks separately from lost tracks.TRK-020The tracker shall support identity preservation across temporary occlusions.TRK-021The tracker shall expose an identity confidence score.TRK-022The tracker shall reject likely ghost tracks.TRK-023The tracker shall detect possible identity swaps and mark the affected tracks as ambiguous.TRK-024The tracker shall support multiple objects of the same type.TRK-025The tracker shall support detector-specific object signatures where available, such as colour, size, marker ID, shape, or motion profile.Fusion and world estimation
IDRequirementFUS-001The fusion layer shall support asynchronous observations from multiple cameras.FUS-002The fusion layer shall fuse observations using timestamp-aware interpolation or prediction.FUS-003The fusion layer shall expose which cameras contributed to each world estimate.FUS-004The fusion layer shall reject outlier camera observations where possible.FUS-005The fusion layer shall degrade gracefully when cameras disappear or become unreliable.FUS-006The fusion layer shall increase uncertainty when fewer cameras contribute to the estimate.FUS-007The fusion layer shall support world estimates derived from one camera only when additional constraints exist, such as a known floor plane.FUS-008The fusion layer shall distinguish measured 3D, estimated 3D, plane-constrained 2D, and image-only coordinates.Calibration health
IDRequirementCAL-001The system shall support camera intrinsic calibration.CAL-002The system shall support camera-to-world extrinsic calibration.CAL-003The system shall define a shared world coordinate frame.CAL-004The system shall persist calibration data with versioning and timestamps.CAL-005The system shall validate calibration at startup.CAL-006The system shall continuously monitor calibration health where static markers are available.CAL-007The system shall mark a camera as recalibration-required if observed reference markers shift beyond a threshold.CAL-008The system shall degrade world-coordinate confidence when calibration health is poor.CAL-009The system shall continue exporting image coordinates even when world calibration is invalid.Export and backpressure
IDRequirementEXP-001Export shall never block the tracking loop.EXP-002Export sinks shall use bounded queues.EXP-003When an export queue is full, stale telemetry shall be dropped before fresh telemetry.EXP-004The system shall expose export drop counters.EXP-005The exported state shall include data age.EXP-006The exported state shall include coordinate availability and coordinate quality.EXP-007The exported state shall include whether the state is considered safe for control.EXP-008The exported state shall support multiple consumers.EXP-009The export contract shall be independent of the transport mechanism.Calibration discussion
Calibration should be treated as a first-class subsystem, not a one-off setup tool.
There are really four calibration problems:

1. Intrinsic calibration
2. Extrinsic calibration
3. World-frame definition
4. Runtime calibration health monitoring
1. Intrinsic calibration
This answers:

How does each individual camera distort the image?
It estimates:

Focal length
Principal point
Lens distortion
Image sensor geometry
This is per camera.
Output:

{
  "camera_id": "cam-1",
  "intrinsics": {
    "fx": 812.4,
    "fy": 810.9,
    "cx": 640.0,
    "cy": 360.0,
    "distortion": [-0.21, 0.08, 0.0, 0.0, -0.01]
  }
}
This can be done offline using a checkerboard, Charuco board, AprilTag grid, or similar.
For production-grade architecture, the core should not care how this was produced. It only consumes a CameraCalibration object.
2. Extrinsic calibration
This answers:

Where is each camera relative to the shared world?
It estimates:

Camera position
Camera orientation
Transform from camera coordinates to world coordinates
Conceptually:

camera_image → camera_ray → world_ray → world_position
For each camera:

struct CameraExtrinsics {
    CameraId camera_id;
    Transform3D camera_to_world;
    double reprojection_error_px;
    CalibrationHealth health;
};
3. World-frame definition
This needs an explicit decision.
For your system, the likely useful world frame is:

Origin: known point in the play/test area
X-axis: left-right across the area
Y-axis: forward-back across the area
Z-axis: vertical height from floor
Unit: metres
For example:

(0, 0, 0) = front-left corner of the test area on the floor
Z = 0 means floor plane
Z > 0 means above floor
This matters because the controller needs stable semantics.
A drone controller consuming this:

{ "x_m": 1.2, "y_m": 0.4, "z_m": 0.8 }
must know exactly what those axes mean.
4. Runtime calibration health monitoring
This addresses the tripod bump problem.
The system should support optional static reference markers in the scene.
Example:

AprilTag marker fixed to wall
AprilTag marker fixed to floor corner
Coloured reference dot fixed outside play area
Known calibration object visible in background
Every few frames, the system checks:

Where should this marker appear?
Where does it actually appear?
How large is the reprojection error?
If error exceeds a threshold:

Camera calibration health: degraded
World coordinates from that camera: lower confidence
If severe: recalibration-required
This gives you graceful degradation:

Camera still usable for image overlay
Camera not trusted for world fusion
Camera excluded from triangulation
Calibration process options
Option A — Manual calibration board
Workflow:

1. Place checkerboard/Charuco board in the scene.
2. Capture frames from each camera.
3. Estimate intrinsics and extrinsics.
4. Save calibration profile.
5. Validate reprojection error.
Pros:

Accurate
Well-understood
Good for production baseline
Cons:

Requires setup step
Less automatic
Annoying if cameras move often
Option B — Static scene markers
Workflow:

1. Place known markers permanently in the environment.
2. Cameras detect those markers at startup.
3. System estimates or validates camera pose.
4. System continuously monitors marker position.
Pros:

Good for long-running system
Detects camera bumps
Can support startup self-check
Cons:

Requires visible markers
Markers may be occluded
Environment needs preparation
Option C — Dynamic calibration object
This is close to your earlier idea: introduce a known, recognisable object and compare how it appears across cameras.
Workflow:

1. Place or move a known object through the scene.
2. Each camera detects it.
3. The system matches observations across cameras.
4. Relative camera positions are estimated or refined.
Pros:

More automatic
Good for rough alignment
Can support iterative improvement
Cons:

Harder to make robust
Can be ambiguous
Needs reliable object recognition
May not define absolute scale unless object size or motion path is known
This is useful, but I would not make it the only calibration method.
Option D — Hybrid calibration
Recommended.
Use:

Manual board or known marker layout for initial calibration
Static markers for ongoing health monitoring
Dynamic objects for refinement and sanity checks
This gives the best architecture:

Initial accuracy
Runtime robustness
Graceful degradation
Path toward automatic scaling
Recommended calibration lifecycle
Phase 1 — Image-only prototype
No world calibration required.
Export:

camera_id
x_px
y_px
confidence
timestamp
Use this for laser point detection and visual feedback.
Phase 2 — Single-camera plane calibration
Assume the object lies on a known plane, such as:

floor
table
wall
projection surface
For the laser point, this is especially useful if the laser is projected onto a known wall/floor plane.
A single camera can map:

image pixel → point on known plane
This gives useful 2D world coordinates without full 3D triangulation.
Example:

laser point on wall:
x_m, z_m on wall plane

ball on floor:
x_m, y_m on floor plane, z_m = radius
This should probably be the first real “world coordinate” milestone.
Phase 3 — Multi-camera 3D calibration
Use 2+ cameras to triangulate:

ball centre
drone position
laser spot if visible from multiple views
This supports true 3D.
Phase 4 — Runtime calibration validation
Add static markers and health monitoring.
This protects against:

tripod bump
camera sag
mount vibration
camera accidentally moved
Important calibration design principle
Do not make calibration a global boolean.
Avoid:

calibrated = true
Use per-camera and per-transform health:

cam-1 intrinsics: valid
cam-1 extrinsics: degraded
cam-2 intrinsics: valid
cam-2 extrinsics: valid
world frame: valid
floor plane: valid
Then the fusion engine can reason properly.
Example:

cam-1 can still provide 2D observations
cam-1 should not contribute to 3D world fusion
cam-2 and cam-3 can still triangulate
Revised interface model
ICalibrationProvider
class ICalibrationProvider {
public:
    virtual CameraCalibration get_camera_calibration(CameraId camera_id) const = 0;
    virtual WorldFrameDefinition get_world_frame() const = 0;
    virtual CalibrationHealth get_health(CameraId camera_id) const = 0;
};
ICalibrationMonitor
class ICalibrationMonitor {
public:
    virtual CalibrationHealthReport evaluate(
        const CameraId& camera_id,
        const Frame& frame
    ) = 0;
};
IFrameSynchronizer
class IFrameSynchronizer {
public:
    virtual SynchronisedFrameSet add_frame(Frame frame) = 0;
};
Important: this should not force exact frame matches.
A SynchronisedFrameSet can contain:

latest frame from cam-1
latest frame from cam-2
estimated time alignment error
missing cameras
late cameras
ITrackExporter
class ITrackExporter {
public:
    virtual ExportResult publish_latest(const TrackingState& state) = 0;
};
Where publish_latest() is non-blocking.
Possible result:

struct ExportResult {
    bool accepted;
    uint64_t dropped_messages;
    ExportBackpressureState backpressure_state;
};
Controller-facing safety contract
Since your primary output consumer is the system controller, I would explicitly design the export around controller safety.
A downstream controller should be able to apply rules like:

Do not act if object state is Provisional
Do not act if object state is Lost
Do not act if coordinate age > threshold
Do not act if uncertainty > threshold
Do not act if calibration health is degraded
Do not act if identity is ambiguous
Do not act if occluded by drone/child/unknown foreground object
Therefore the exported object state should contain:

track_state
position_source
coordinate_space
age_ms
confidence
identity_confidence
uncertainty
calibration_health
visibility_state
safe_for_control
unsafe_reasons
I would not let the controller infer all of this from raw tracking fields. The tracking system should provide an explicit safety summary.
Suggested first vertical slice
Given your answers, the best first vertical slice is:

Single camera
Laser point detector
Frame metadata with sequence numbers and timestamps
Track lifecycle: provisional → confirmed → lost
Image-coordinate export
Non-blocking export sink
Simple viewer overlay
Replay-based tests
Then the second slice:

Single camera
Laser point projected onto known plane
Plane calibration
Export 2D world coordinates on the plane
Calibration health status
Then:

Two cameras
Asynchronous frame handling
Triangulated world coordinate estimate
Fusion uncertainty
Camera dropout handling
This avoids designing everything abstractly without proving the core contracts.
Proposed immediate v0.2 additions
I would add these to the v0.2 baseline immediately:

The system is not allowed to block tracking on export.

All exported coordinates must include coordinate-space, age, confidence, and uncertainty.

The tracking system must distinguish provisional, confirmed, predicted, occluded, lost, and rejected tracks.

The camera layer must expose sequence numbers, capture timestamps, receive timestamps, and frame quality flags.

The fusion layer must support asynchronous observations.

Calibration must be represented as versioned data with explicit health, not as a boolean.

World coordinate export must degrade to plane coordinates or image coordinates when full 3D is unavailable.

The controller-facing export must include a safe_for_control flag and unsafe reasons.
Decisions needed next
I suggest we resolve these before writing the v0.2 interface document.

1. Calibration object strategy
Which baseline do you want?
A. Manual board first
B. Static markers first
C. Dynamic known object first
D. Hybrid from the start
My recommendation: D, but implement in this order:

manual board → static marker validation → dynamic refinement
2. First world plane
For the laser-point vertical slice, where is the laser expected to appear first?
A. Floor
B. Wall
C. Table / test surface
D. Moving object surface, such as ball/drone
My recommendation: B or C first. Avoid moving object surfaces until the coordinate pipeline is proven.

3. Controller transport
For the first controller integration, what should the export transport be?
A. In-process API
B. JSON over WebSocket
C. MQTT
D. ZeroMQ
E. ROS2 topic
My recommendation: B for visibility/debugging, or D if low-latency local control is the priority.

4. First implementation language
For the first vertical slice:
A. Python only
B. C++ only
C. Python prototype with C++-shaped interfaces
D. C++ core with Python tooling
My recommendation: C, though this will run on a Pi5 requiring close to real time position updates
---------------------------------------------------------------------------------------------------------

These v0.2 architectural adjustments are exactly what separate a brittle computer vision script from a robust, production-grade robotics pipeline. The addition of the `safe_for_control` flag and the explicit track lifecycle (Provisional → Confirmed → Occluded → Predicted → Lost) are particularly strong. They map perfectly to the realities of a physical environment where occlusions and dropped frames are guarantees, not exceptions.

Here is my take on your four outstanding decisions, especially keeping in mind that this will ultimately run on a Raspberry Pi 5 with tight latency constraints.

### 1. Calibration Object Strategy
**Recommendation: Option D (Hybrid), implemented in stages.**
Start with the manual checkerboard/Charuco board to get your baseline intrinsics locked in. Next, implement static markers (Option B) for runtime health monitoring. This is non-negotiable for a robust setup. When you eventually move from table-top testing to the floor, having those static markers visible in the background will be the only thing keeping your world-frame sane when your 7-year-old daughter or 5-year-old son suddenly darts through the camera view to grab the ball. The system needs to immediately recognize that the camera didn't move, but massive occlusion just occurred. Dynamic refinement (Option C) can be added much later once the core pipeline is bulletproof.

### 2. First World Plane
**Recommendation: Option C (Table / test surface).**
For the laser-point vertical slice, projecting down onto a table is the most controllable environment. It bounds your Z-axis to exactly `0` relative to the surface, limits the field of view, and keeps the initial testing area physically contained. A wall (Option B) is also highly controllable, but a table translates slightly better to the physics of a ball rolling on a floor later on. Save the floor (Option A) for Phase 2, once your 2D plane extraction is mathematically verified.

### 3. Controller Transport
**Recommendation: Option D (ZeroMQ) for the controller, Option B (JSON/WebSocket) for the viewer.**
Given you are targeting a Raspberry Pi 5 and need close to real-time position updates, JSON over WebSocket is too heavy and non-deterministic for the control loop. String serialization, parsing overhead, and network stack jitter will eat into your latency budget. 
*   **ZeroMQ** (specifically using a PUB/SUB or PUSH/PULL pattern with binary payloads like FlatBuffers or MessagePack) gives you the low-latency, brokerless, bounded-queue transport the control module needs.
*   **WebSocket/JSON** is perfect for the separate `tracking_viewer` app, where a few milliseconds of latency or dropped telemetry frames won't crash a drone.

### 4. First Implementation Language
**Recommendation: Option D (C++ Core with Python Tooling) OR an extremely disciplined Option C.**
This is the trickiest balance. Python is fantastic for rapidly prototyping the OpenCV logic, but on a Pi 5, the Global Interpreter Lock (GIL) and Python's memory management will bottleneck a multi-camera, high-framerate pipeline if you aren't careful. 

If you go with Option C (Python prototype), you must design the Python code almost like C code: strictly pre-allocated memory buffers, heavy reliance on vectorization (NumPy/OpenCV C-extensions), and absolutely no dynamically resizing arrays in the hot tracking loop. 

However, given the real-time control requirement, **Option D** is mathematically safer. Writing the headless core (`CameraSource`, `Tracker`, `FusionEngine`, `ZeroMQ Exporter`) in modern C++ ensures deterministic latency. You can then write your `tracking_viewer`, calibration scripts, and data loggers in Python, binding them to the C++ core via ZeroMQ or `pybind11`. 

---

With these decisions locked in, the v0.2 interface document is ready to be drafted. To ensure the C++ or Python-C interface is perfectly tailored to your hardware, what specific camera models and target frame rates (e.g., two 1080p USB webcams at 60fps, or Pi Camera Modules at 120fps) are you planning to ingest on the Pi 5?

ME:
---------------------------------------------------------------------------------------------------------
I have a Pi5 and a Pi3B and a 5MP Camera IR-Cut Adjustable Focus Night Vision NoIR Camera
---------------------------------------------------------------------------------------------------------

This hardware combination clarifies a lot and gives us a massive advantage for the first vertical slice. 

Here is what your hardware means for the architecture:

**1. The Camera (5MP IR-Cut NoIR on CSI)**
*   **The Interface:** Because this is a native Pi camera (usually an OV5647 or similar sensor using the CSI ribbon cable), it completely bypasses the USB bus. This eliminates the USB saturation edge case for your primary camera. The Pi 5 has two CSI ports, so you can easily scale to two cameras natively before touching USB.
*   **The NoIR Advantage:** An IR-sensitive camera is the absolute best-case scenario for tracking a laser point. If you use an IR laser pointer, or a standard red laser and dim the room slightly, the laser will be the brightest cluster of pixels on the sensor. Thresholding this in OpenCV will cost virtually zero CPU cycles.
*   **The Trade-off (Resolution vs. Latency):** A 5MP sensor can only push about 15 frames per second at full resolution, which is useless for a control loop. We need to lock the `CameraSource` initialization to a binned resolution—ideally **640x480 at 90fps** or **1280x720 at 60fps**. 

**2. The Compute Nodes (Pi 5 vs Pi 3B)**
*   **The Core (Pi 5):** The Pi 5 has a phenomenal CPU but no dedicated deep-learning GPU. This reinforces **Decision 4: C++ Core with Python Tooling**. Writing the tracking core in modern C++ ensures you can process 60-90fps deterministically while leaving enough CPU overhead for the drone control stack.
*   **The Networked Sensor (Pi 3B):** Keep the Pi 3B in your back pocket for Phase 3. Once the single-camera pipeline is proven, you can attach a USB webcam to the Pi 3B, write a lightweight C++ script that grabs frames and publishes them over ZeroMQ via Wi-Fi/Ethernet, and have your Pi 5 `FusionEngine` ingest them.

---

### Drone Ball Retrieval System: v0.2 Interface Contracts

Based on our decisions (Hybrid C++/Python, Tabletop Plane Calibration, ZeroMQ Transport, and Degraded-Mode Safety), here are the explicit C++ data contracts for the headless tracking core.

#### 1. Frame Ingestion & Metadata
The core must know exactly when a frame was captured, not just when it was processed.

```cpp
enum class FrameQualityFlag {
    None = 0,
    DroppedFrameSuspected = 1,
    MotionBlurDetected = 2,
    SensorSaturated = 4  // Crucial for laser/glare tracking
};

struct FrameMetadata {
    std::string camera_id;
    uint64_t sequence_number;
    uint64_t capture_timestamp_ms;  // Hardware/V4L2 timestamp
    uint64_t receive_timestamp_ms;  // System clock upon ingestion
    
    uint16_t width_px;
    uint16_t height_px;
    uint8_t current_fps;
    
    uint32_t quality_flags; // Bitmask of FrameQualityFlag
};

struct Frame {
    FrameMetadata metadata;
    cv::Mat image; // OpenCV matrix
};
```

#### 2. The Track Lifecycle
The tracking state machine must strictly separate a "blip" from a confirmed object, and gracefully handle occlusion.

```cpp
enum class TrackState {
    Provisional, // 1-2 frames of detection; do not export yet
    Confirmed,   // Solid track; safe to export
    Predicted,   // Lost visibility, but extrapolating via velocity
    Occluded,    // Expected to be blocked by a known foreground object
    Lost,        // Missing too long; unsafe
    Rejected     // Deemed a false positive (e.g., a shiny reflection)
};

enum class ObjectType {
    LaserPoint,
    Ball,
    Drone,
    CalibrationMarker
};
```

#### 3. Coordinate Calibration & Mapping
The system gracefully degrades from 3D to 2D plane coordinates to raw pixels depending on available calibration data.

```cpp
enum class CoordinateSpace {
    ImagePixels,     // Raw x,y
    Plane2D_World,   // x,y on the tabletop/floor in meters (Z assumed 0)
    Full3D_World     // Triangulated x,y,z in meters
};

struct PositionEstimate {
    CoordinateSpace space;
    double x;
    double y;
    double z; // 0.0 if Plane2D
    
    double velocity_x;
    double velocity_y;
    double velocity_z;

    double uncertainty_radius_m; // 1-sigma confidence radius
};
```

#### 4. The Controller-Facing Export Contract (ZeroMQ)
This is the final JSON/MessagePack payload published to the ZeroMQ `PUB` socket. Notice that it abstracts the complex computer vision logic into a simple `safe_for_control` boolean for the downstream drone flight controller.

```json
{
  "timestamp_ms": 1715103857123,
  "system_latency_ms": 12,
  "objects": [
    {
      "object_id": "laser_target_1",
      "type": "LaserPoint",
      "track_state": "Confirmed",
      "age_since_last_observation_ms": 8,
      
      "position": {
        "coordinate_space": "Plane2D_World",
        "x_m": 0.45,
        "y_m": 0.22,
        "z_m": 0.0,
        "uncertainty_m": 0.015
      },
      
      "safety": {
        "safe_for_control": true,
        "unsafe_reasons": []
      }
    },
    {
      "object_id": "ball_1",
      "type": "Ball",
      "track_state": "Predicted",
      "age_since_last_observation_ms": 145,
      
      "position": {
        "coordinate_space": "Plane2D_World",
        "x_m": 0.60,
        "y_m": 0.35,
        "z_m": 0.05,
        "uncertainty_m": 0.12
      },
      
      "safety": {
        "safe_for_control": false,
        "unsafe_reasons": ["AgeExceedsThreshold", "HighUncertainty"]
      }
    }
  ]
}
```

### Next Steps: The First Vertical Slice

With these contracts defined, we are ready to build. I recommend attacking the pipeline in this specific order to unblock yourself as fast as possible:

1.  **The Camera Ingestion Loop (C++):** Write a standalone C++ script using `cv::VideoCapture` (interfacing with `v4l2` for the Pi Camera). Its only job is to pull 640x480 frames at 60fps, attach `FrameMetadata`, and log if a frame drops. 
2.  **The ZeroMQ Publisher (C++):** Add a ZeroMQ `PUB` socket to that script. Publish dummy JSON payloads at 60Hz.
3.  **The Python Viewer:** Write a simple Python script using `zmq` and `cv2.imshow` that subscribes to the feed and renders the dummy tracking state onto a black screen to prove your transport layer has low latency.

Would you like to start by drafting the CMake setup and the C++ `CameraSource` ingestion loop, or would you prefer to stub out the ZeroMQ transport layer first?

ME:
---------------------------------------------------------------------------------------------------------
I want a complete design document capturing the design and architectural decisions made. This is the core document which we will build on and from referring to in future
---------------------------------------------------------------------------------------------------------

# System Architecture & Design Document: Drone Ball Retrieval System
**Module:** Tracking & Visualisation Core
**Version:** 0.2 (Baseline Architecture)
**Status:** Approved for Implementation

---

## 1. Executive Summary
The Tracking & Visualisation Core is a headless, low-latency software module responsible for ingesting camera streams, detecting configured targets (laser points, balls, drones), estimating their positions, and broadcasting coordinate data. 

The system is designed as a **probabilistic, degraded-mode tracking pipeline**. It explicitly prioritises deterministic execution and downstream safety. It does not control the drone; rather, it provides a highly reliable, mathematically bounded state estimate to a separate system controller, allowing that controller to make safe actuation decisions even in the presence of dynamic occlusions, dropped frames, or hardware latency.

---

## 2. Operating Environment & Hardware Profile
The system is tailored for the specific physical and computational realities of the deployment environment.

*   **Primary Compute Node:** Raspberry Pi 5.
*   **Sensor Stack:** 5MP IR-Cut Adjustable Focus NoIR Cameras.
    *   *Ingestion Path:* Native CSI ribbon cable (bypasses USB bus saturation).
    *   *Target Capture:* 640x480 at 60–90fps.
    *   *Advantage:* NoIR sensors allow for zero-cost CPU thresholding of IR or standard red laser points in dim lighting.
*   **Networked Compute (Phase 3):** Raspberry Pi 3B (used for remote network-attached sensors).
*   **Environmental Constraints:** The operating area is dynamic. The system must natively tolerate sudden, massive frame occlusions caused by unpredictable actors (e.g., a 7-year-old or 5-year-old entering the testing area or crossing the camera plane).

---

## 3. Core Architectural Decisions

### ADR-001: Hybrid C++ Core with Python Tooling
*   **Decision:** The primary tracking loop (Ingestion, Detection, Fusion, Export) will be written in modern C++. Visualization, logging, and offline calibration tools will be written in Python.
*   **Rationale:** The Raspberry Pi 5 requires deterministic memory management to sustain 60+ fps across multiple cameras. C++ ensures the core runs without garbage collection pauses, leaving CPU headroom for the flight controller. Python bindings (`pybind11` or ZeroMQ) enable rapid UI development without coupling the GUI to the tracking loop.

### ADR-002: ZeroMQ Control Transport
*   **Decision:** The C++ core will publish state via a ZeroMQ `PUB` socket using bounded queues. 
*   **Rationale:** Downstream controllers require strict low-latency telemetry. ZeroMQ provides brokerless, non-blocking delivery. If the network or controller lags, stale telemetry is dropped immediately rather than blocking the tracking pipeline.

### ADR-003: Graceful Coordinate Degradation
*   **Decision:** The system will not enforce a binary "Calibrated/Uncalibrated" state. It will degrade gracefully across three coordinate spaces: `Full3D_World` -> `Plane2D_World` -> `ImagePixels`.
*   **Rationale:** Allows the system to remain useful for control or logging even if a camera is bumped or full 3D triangulation fails.

### ADR-004: Hybrid Calibration Lifecycle
*   **Decision:** Calibration will occur in three phases: Manual initialization (Checkerboard/Charuco), static marker health monitoring (AprilTags in the background), and future dynamic object refinement.
*   **Rationale:** Static markers allow the system to instantly detect if a camera has physically shifted, automatically downgrading the trust of that camera's observations.

---

## 4. Pipeline Architecture
The system follows a strict, one-way data flow model.

**Ingestion & Timing**
`Camera Sources` → `Frame Synchronisation Layer` → `Frame Quality Assessment`
*↓*
**Detection & Tracking**
`Object Detectors` → `Track Lifecycle Manager` → `Occlusion & Identity Resolver`
*↓*
**Mapping & State**
`Coordinate Mapper` → `Multi-Camera Fusion` → `World State Estimator` → `Tracking State Store`
*↓*
**Distribution**
`ZeroMQ Non-Blocking Exporter`

---

## 5. Data Contracts & Interfaces (C++ Baseline)

### 5.1 Frame Ingestion
The system relies on hardware-level timestamps to detect dropped frames before they reach the tracker.

```cpp
enum class FrameQualityFlag {
    None = 0,
    DroppedFrameSuspected = 1,
    MotionBlurDetected = 2,
    SensorSaturated = 4
};

struct FrameMetadata {
    std::string camera_id;
    uint64_t sequence_number;
    uint64_t capture_timestamp_ms;  // Hardware/V4L2 timestamp
    uint64_t receive_timestamp_ms;  // System clock upon ingestion
    uint16_t width_px;
    uint16_t height_px;
    uint8_t current_fps;
    uint32_t quality_flags;         // Bitmask of FrameQualityFlag
};
```

### 5.2 Track Lifecycle Management
Objects do not instantly appear or disappear. To handle environmental occlusions (e.g., family members walking through the frame, drone hovering over the ball), tracks move through a strict state machine.

```cpp
enum class TrackState {
    Provisional, // 1-2 frames of detection; unsafe to export to controller
    Confirmed,   // Solid track; safe to export
    Predicted,   // Lost visibility (e.g., under a drone), extrapolating via velocity
    Occluded,    // Blocked by a known, tracked foreground obstacle
    Lost,        // Missing beyond grace period; unsafe
    Rejected     // Algorithm determined false positive
};
```

### 5.3 Coordinate Export (Controller Payload)
The final output is decoupled from the internal CV logic. It provides a highly digestible payload for the flight controller, featuring an explicit `safe_for_control` flag.

```json
{
  "timestamp_ms": 1715103857123,
  "system_latency_ms": 12,
  "objects": [
    {
      "object_id": "laser_target_1",
      "type": "LaserPoint",
      "track_state": "Confirmed",
      "age_since_last_observation_ms": 8,
      "position": {
        "coordinate_space": "Plane2D_World",
        "x_m": 0.45,
        "y_m": 0.22,
        "z_m": 0.0,
        "uncertainty_m": 0.015
      },
      "safety": {
        "safe_for_control": true,
        "unsafe_reasons": []
      }
    }
  ]
}
```

---

## 6. Controller Safety Contract
The tracking core guarantees that it will never make autonomous control decisions. Instead, it adheres to the following safety commitments:

1.  **Bounded Queues:** The export interface will drop stale tracking frames if the controller queue backs up. The controller will only ever receive the most current reality.
2.  **Explicit Age:** Every object payload includes `age_since_last_observation_ms`. The controller can configure a hard cutoff (e.g., >100ms triggers a drone hover/pause).
3.  **Uncertainty Quantification:** The fusion engine outputs a standard deviation (`uncertainty_m`). A degraded camera calibration or a track moving from `Confirmed` to `Predicted` will instantly spike this value, allowing the controller to reject the position estimate.

---

## 7. Implementation Strategy: The First Vertical Slice
To derisk the architecture rapidly, development will proceed in the following order:

*   **Objective:** Track a laser pointer on a tabletop using a single Pi 5 CSI camera.
*   **Step 1:** Implement the C++ `CameraSource` using `v4l2` to pull 640x480 frames at 60fps, attaching accurate `FrameMetadata`.
*   **Step 2:** Implement a simple OpenCV threshold detector optimized for the NoIR sensor to isolate the highest intensity pixel cluster.
*   **Step 3:** Implement the Tabletop `CoordinateMapper` (Phase 2 Calibration: `Plane2D_World`).
*   **Step 4:** Implement the ZeroMQ `PUB` sink in C++ and a lightweight Python visualization script (`tracking_viewer.py`) that subscribes to the ZeroMQ socket and renders the JSON state.



