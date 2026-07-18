"""Shared fixtures for the TRK-012/013 calibration-tool tests (TRK-031 lineage).

The core fixture renders synthetic Charuco captures from ONE consistent pinhole
camera (fixed ground-truth K, zero distortion) at varied poses. This is
load-bearing: cv2.calibrateCamera only recovers focal length when every view is
a projection of the same camera through genuine out-of-plane rotation. Arbitrary
homographies would correspond to no camera and converge to garbage; near-fronto
-parallel-only views leave focal length unobservable.
"""

from __future__ import annotations

import dataclasses
from pathlib import Path

import cv2
import numpy as np
import pytest

# Board geometry mirrors tracking_core.yaml (odd squares per KTD-4).
SQUARES_X = 5
SQUARES_Y = 7
SQUARE_LENGTH_M = 0.025
MARKER_LENGTH_M = 0.020
DICTIONARY = cv2.aruco.DICT_4X4_50

IMAGE_SIZE = (640, 480)  # (width, height) of the synthetic captures
FOCAL_PX = 800.0  # ground-truth focal length


@dataclasses.dataclass(frozen=True)
class SyntheticCaptures:
    directory: Path
    k_true: np.ndarray  # 3x3 ground-truth camera matrix
    image_size: tuple[int, int]  # (width, height)
    count: int


def _charuco_board() -> cv2.aruco.CharucoBoard:
    dictionary = cv2.aruco.getPredefinedDictionary(DICTIONARY)
    return cv2.aruco.CharucoBoard(
        (SQUARES_X, SQUARES_Y), SQUARE_LENGTH_M, MARKER_LENGTH_M, dictionary
    )


def _k_true() -> np.ndarray:
    w, h = IMAGE_SIZE
    return np.array(
        [[FOCAL_PX, 0.0, w / 2.0], [0.0, FOCAL_PX, h / 2.0], [0.0, 0.0, 1.0]],
        dtype=np.float64,
    )


def _pose_grid() -> list[tuple[float, float]]:
    """Twelve (tilt_x_deg, tilt_y_deg) pairs spanning +/-15..35 degrees so the
    focal length is observable (Zhang: planar calibration needs pose diversity)."""
    tilts = [-35.0, -20.0, 20.0, 35.0]
    poses: list[tuple[float, float]] = []
    for tx in tilts:
        poses.append((tx, 15.0))
        poses.append((tx, -25.0))
    poses.append((15.0, 30.0))
    poses.append((-15.0, -30.0))
    poses.append((28.0, 0.0))
    poses.append((0.0, -28.0))
    return poses


def render_captures(directory: Path, count: int = 12) -> SyntheticCaptures:
    """Render `count` Charuco views of one camera to `directory` as PNGs."""
    directory.mkdir(parents=True, exist_ok=True)
    board = _charuco_board()
    k_true = _k_true()
    width, height = IMAGE_SIZE

    # Flat rendered board and its board-plane metric extent.
    flat = board.generateImage((500, 700))
    fh, fw = flat.shape[:2]
    board_w_m = SQUARES_X * SQUARE_LENGTH_M
    board_h_m = SQUARES_Y * SQUARE_LENGTH_M
    # Flat-image corners (px) and their board-plane metric coordinates.
    flat_corners = np.array([[0, 0], [fw, 0], [fw, fh], [0, fh]], dtype=np.float64)
    metric_corners = np.array(
        [[0, 0, 0], [board_w_m, 0, 0], [board_w_m, board_h_m, 0], [0, board_h_m, 0]],
        dtype=np.float64,
    )

    poses = _pose_grid()[:count]
    for i, (tilt_x, tilt_y) in enumerate(poses):
        rvec, _ = cv2.Rodrigues(
            _euler_to_matrix(np.deg2rad(tilt_x), np.deg2rad(tilt_y), 0.0)
        )
        # Place the board centred in front of the camera, ~0.4 m away.
        tvec = np.array([-board_w_m / 2.0, -board_h_m / 2.0, 0.4], dtype=np.float64)
        projected, _ = cv2.projectPoints(metric_corners, rvec, tvec, k_true, None)
        projected = projected.reshape(-1, 2)
        homography = cv2.getPerspectiveTransform(
            flat_corners.astype(np.float32), projected.astype(np.float32)
        )
        view = cv2.warpPerspective(flat, homography, (width, height), borderValue=255)
        cv2.imwrite(str(directory / f"view_{i:02d}.png"), view)

    return SyntheticCaptures(
        directory=directory, k_true=k_true, image_size=IMAGE_SIZE, count=count
    )


def _euler_to_matrix(rx: float, ry: float, rz: float) -> np.ndarray:
    cx, sx = np.cos(rx), np.sin(rx)
    cy, sy = np.cos(ry), np.sin(ry)
    cz, sz = np.cos(rz), np.sin(rz)
    r_x = np.array([[1, 0, 0], [0, cx, -sx], [0, sx, cx]])
    r_y = np.array([[cy, 0, sy], [0, 1, 0], [-sy, 0, cy]])
    r_z = np.array([[cz, -sz, 0], [sz, cz, 0], [0, 0, 1]])
    return r_z @ r_y @ r_x


@pytest.fixture
def synthetic_captures(tmp_path: Path) -> SyntheticCaptures:
    return render_captures(tmp_path / "captures", count=12)


@pytest.fixture
def charuco_board() -> cv2.aruco.CharucoBoard:
    return _charuco_board()
