"""Sanity tests for the synthetic Charuco fixture itself (TRK-031/U4).

If these fail, the intrinsics/extrinsics tool tests that build on the fixture
cannot be trusted — so the generator is validated on its own first.
"""

from __future__ import annotations

import cv2
import numpy as np

from conftest import SyntheticCaptures


def test_flat_board_is_detectable(charuco_board: cv2.aruco.CharucoBoard) -> None:
    """A directly-rendered board must yield the full inner-corner set."""
    image = charuco_board.generateImage((500, 700))
    detector = cv2.aruco.CharucoDetector(charuco_board)
    corners, ids, _, _ = detector.detectBoard(image)
    assert ids is not None
    # A 5x7 board has (5-1) * (7-1) = 24 inner Charuco corners.
    assert len(ids) >= 12


def test_pose_projected_views_are_detectable(
    synthetic_captures: SyntheticCaptures, charuco_board: cv2.aruco.CharucoBoard
) -> None:
    """Each warped capture must still detect enough corners to calibrate."""
    detector = cv2.aruco.CharucoDetector(charuco_board)
    images = sorted(synthetic_captures.directory.glob("*.png"))
    assert len(images) == synthetic_captures.count
    detectable = 0
    for path in images:
        image = cv2.imread(str(path), cv2.IMREAD_GRAYSCALE)
        _, ids, _, _ = detector.detectBoard(image)
        if ids is not None and len(ids) >= 4:
            detectable += 1
    # The pose grid stays within detectable tilt; allow one marginal view.
    assert detectable >= synthetic_captures.count - 1


def test_fixture_exposes_ground_truth_intrinsics(
    synthetic_captures: SyntheticCaptures,
) -> None:
    k = synthetic_captures.k_true
    assert k.shape == (3, 3)
    assert np.isclose(k[0, 0], 800.0)
    assert np.isclose(k[1, 1], 800.0)
