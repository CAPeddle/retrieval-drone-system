from src.viewer.utils import decode_frame


def test_decode_frame_returns_none_for_empty_payload():
    assert decode_frame(b"") is None
