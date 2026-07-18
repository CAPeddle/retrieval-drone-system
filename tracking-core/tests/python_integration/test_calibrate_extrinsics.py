"""TRK-013 integration tests: floor-homography calibration end to end."""

from __future__ import annotations

import json
import sys
from pathlib import Path

import cv2
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import calibrate_extrinsics as ce  # noqa: E402

IMAGE_SIZE = (640, 480)
MARKER_SIDE = 60
# Floor->image homography (metres -> pixels): 250 px/m, offset, slight perspective.
H_TRUE = np.array(
    [[250.0, 0.0, 60.0], [0.0, 250.0, 40.0], [2e-4, 1e-4, 1.0]], dtype=np.float64
)


def _project(floor_xy: np.ndarray) -> np.ndarray:
    p = H_TRUE @ np.array([floor_xy[0], floor_xy[1], 1.0])
    return p[:2] / p[2]


def build_scene(
    tmp_path: Path, floor_by_id: dict[int, tuple[float, float]]
) -> tuple[Path, Path]:
    """Render markers at the image positions their floor coords map to; write a
    layout JSON. Returns (image_path, layout_path)."""
    dictionary = cv2.aruco.getPredefinedDictionary(cv2.aruco.DICT_4X4_50)
    image = np.full((IMAGE_SIZE[1], IMAGE_SIZE[0]), 255, dtype=np.uint8)
    for marker_id, floor in floor_by_id.items():
        centre = _project(np.array(floor, dtype=np.float64))
        marker = cv2.aruco.generateImageMarker(dictionary, marker_id, MARKER_SIDE)
        x = int(round(centre[0] - MARKER_SIDE / 2))
        y = int(round(centre[1] - MARKER_SIDE / 2))
        image[y : y + MARKER_SIDE, x : x + MARKER_SIDE] = marker
    image_path = tmp_path / "scene.png"
    cv2.imwrite(str(image_path), image)
    layout_path = tmp_path / "layout.json"
    layout_path.write_text(
        json.dumps({str(k): list(v) for k, v in floor_by_id.items()})
    )
    return image_path, layout_path


def _run(
    image: Path, layout: Path, output: Path, extra: list[str] | None = None
) -> int:
    args = ["--image", str(image), "--layout", str(layout), "--output", str(output)]
    return ce.main(args + (extra or []))


def test_recovers_homography(tmp_path: Path) -> None:
    floor = {
        0: (0.2, 0.2),
        1: (1.6, 0.2),
        2: (1.6, 1.2),
        3: (0.2, 1.2),
        4: (0.9, 0.7),
        5: (0.5, 1.0),
    }
    image, layout = build_scene(tmp_path, floor)
    output = tmp_path / "extrinsics.json"
    assert _run(image, layout, output) == 0
    data = json.loads(output.read_text())
    for key in (
        "extrinsics_version",
        "camera_id",
        "homography_3x3",
        "floor_anchor_points",
        "reprojection_error_m",
        "undistorted_coordinates",
        "calibration_timestamp",
    ):
        assert key in data, f"missing key {key}"
    # No --intrinsics -> the fit is on raw pixels, so the flag must be False.
    assert data["undistorted_coordinates"] is False
    assert data["reprojection_error_m"] < 0.005
    assert len(data["floor_anchor_points"]) == 6
    # The recovered homography maps a known image point to its floor coord.
    h = np.array(data["homography_3x3"])
    recovered = cv2.perspectiveTransform(
        _project(np.array([0.9, 0.7])).reshape(1, 1, 2), h
    ).reshape(2)
    assert np.linalg.norm(recovered - np.array([0.9, 0.7])) < 0.01


def _write_intrinsics(path: Path, distortion: list[float]) -> None:
    """A minimal intrinsics JSON matching calibrate_intrinsics' schema."""
    path.write_text(
        json.dumps(
            {
                "camera_matrix": [
                    [250.0, 0.0, 320.0],
                    [0.0, 250.0, 240.0],
                    [0.0, 0.0, 1.0],
                ],
                "dist_coeffs": distortion,
            }
        )
    )


def test_intrinsics_path_sets_undistorted_flag(tmp_path: Path) -> None:
    # With --intrinsics (zero distortion here), the undistort branch runs and
    # the flag is True; recovery still succeeds through the pass-through.
    floor = {
        0: (0.2, 0.2),
        1: (1.6, 0.2),
        2: (1.6, 1.2),
        3: (0.2, 1.2),
        4: (0.9, 0.7),
        5: (0.5, 1.0),
    }
    image, layout = build_scene(tmp_path, floor)
    intrinsics = tmp_path / "intrinsics.json"
    _write_intrinsics(intrinsics, [0.0, 0.0, 0.0, 0.0, 0.0])
    output = tmp_path / "extrinsics.json"
    assert _run(image, layout, output, ["--intrinsics", str(intrinsics)]) == 0
    data = json.loads(output.read_text())
    assert data["undistorted_coordinates"] is True
    assert data["reprojection_error_m"] < 0.005


def test_rejects_too_few_anchors(tmp_path: Path) -> None:
    floor = {0: (0.2, 0.2), 1: (1.6, 0.2), 2: (0.9, 1.2)}  # only 3
    image, layout = build_scene(tmp_path, floor)
    output = tmp_path / "extrinsics.json"
    assert _run(image, layout, output) == 1
    assert not output.exists()


def test_rejects_exactly_collinear(tmp_path: Path) -> None:
    floor = {0: (0.2, 0.5), 1: (0.7, 0.5), 2: (1.2, 0.5), 3: (1.6, 0.5), 4: (0.9, 0.5)}
    image, layout = build_scene(tmp_path, floor)
    output = tmp_path / "extrinsics.json"
    assert _run(image, layout, output) == 1
    assert not output.exists()


def test_rejects_nearly_collinear(tmp_path: Path) -> None:
    # Four points on a line, the fifth only 2 mm off it — must still reject.
    floor = {
        0: (0.2, 0.5),
        1: (0.7, 0.5),
        2: (1.2, 0.5),
        3: (1.6, 0.5),
        4: (0.9, 0.502),
    }
    image, layout = build_scene(tmp_path, floor)
    output = tmp_path / "extrinsics.json"
    assert _run(image, layout, output) == 1
    assert not output.exists()


def test_rejects_perturbed_anchor(tmp_path: Path) -> None:
    # Build a good scene, then corrupt one layout entry so its floor coord no
    # longer matches its image position — reprojection must exceed the gate.
    floor = {
        0: (0.2, 0.2),
        1: (1.6, 0.2),
        2: (1.6, 1.2),
        3: (0.2, 1.2),
        4: (0.9, 0.7),
        5: (0.5, 1.0),
    }
    image, _ = build_scene(tmp_path, floor)
    bad = dict(floor)
    bad[4] = (0.9 + 0.3, 0.7)  # 30 cm lie about the floor position
    layout_path = tmp_path / "bad_layout.json"
    layout_path.write_text(json.dumps({str(k): list(v) for k, v in bad.items()}))
    output = tmp_path / "extrinsics.json"
    assert _run(image, layout_path, output) == 1
    assert not output.exists()


def test_missing_intrinsics_is_clean_error(tmp_path: Path) -> None:
    floor = {0: (0.2, 0.2), 1: (1.6, 0.2), 2: (1.6, 1.2), 3: (0.2, 1.2), 4: (0.9, 0.7)}
    image, layout = build_scene(tmp_path, floor)
    output = tmp_path / "extrinsics.json"
    rc = _run(image, layout, output, ["--intrinsics", str(tmp_path / "nope.json")])
    assert rc == 1
    assert not output.exists()
