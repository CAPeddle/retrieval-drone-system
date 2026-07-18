"""TRK-012 integration tests: file-driven intrinsics calibration end to end."""

from __future__ import annotations

import json
from pathlib import Path


import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import calibrate_intrinsics as ci  # noqa: E402

from conftest import SyntheticCaptures  # noqa: E402


def _run(args: list[str]) -> int:
    return ci.main(args)


def test_produces_valid_intrinsics(
    synthetic_captures: SyntheticCaptures, tmp_path: Path
) -> None:
    output = tmp_path / "intrinsics.json"
    rc = _run(
        [
            "--images",
            str(synthetic_captures.directory),
            "--output",
            str(output),
            "--camera-id",
            "test-cam",
        ]
    )
    assert rc == 0
    assert output.exists()
    data = json.loads(output.read_text())
    # Schema completeness.
    for key in (
        "intrinsics_version",
        "camera_id",
        "camera_matrix",
        "dist_coeffs",
        "image_size",
        "reprojection_error_px",
        "calibration_timestamp",
    ):
        assert key in data, f"missing key {key}"
    assert data["intrinsics_version"] == 1
    assert data["camera_id"] == "test-cam"
    assert data["image_size"] == list(synthetic_captures.image_size)
    assert data["reprojection_error_px"] < 1.0
    # Recovered focal length within 15% of the synthetic ground truth (800 px).
    fx = data["camera_matrix"][0][0]
    fy = data["camera_matrix"][1][1]
    truth = synthetic_captures.k_true[0][0]
    assert abs(fx - truth) / truth < 0.15, f"fx {fx} vs truth {truth}"
    assert abs(fy - truth) / truth < 0.15, f"fy {fy} vs truth {truth}"


def test_rejects_impossible_threshold(
    synthetic_captures: SyntheticCaptures, tmp_path: Path
) -> None:
    output = tmp_path / "intrinsics.json"
    rc = _run(
        [
            "--images",
            str(synthetic_captures.directory),
            "--output",
            str(output),
            "--max-reprojection-error",
            "0.0001",
        ]
    )
    assert rc == 1
    assert not output.exists()  # nothing written on rejection


def test_rejects_too_few_views(tmp_path: Path, charuco_board) -> None:
    # A directory with a single board view is below the min-views gate.
    import cv2

    captures = tmp_path / "few"
    captures.mkdir()
    cv2.imwrite(str(captures / "only.png"), charuco_board.generateImage((500, 700)))
    output = tmp_path / "intrinsics.json"
    rc = _run(["--images", str(captures), "--output", str(output)])
    assert rc == 1
    assert not output.exists()


def test_unwritable_output_is_clean_error(
    synthetic_captures: SyntheticCaptures,
) -> None:
    rc = _run(
        [
            "--images",
            str(synthetic_captures.directory),
            "--output",
            "/proc/nonexistent/intrinsics.json",
        ]
    )
    assert rc == 1  # clean non-zero, not a traceback
