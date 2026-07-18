#!/usr/bin/env python3
"""Unit tests for the CAM-002 MJPEG frame parser. Runs anywhere: no hardware,
no network — `python3 -m unittest test_mjpeg_parser` from this directory."""

import unittest

from mjpeg_web_relay import MJPEGFrameParser, SOI, EOI


def jpeg(payload: bytes) -> bytes:
    """Build a JPEG-shaped frame: SOI + marker-free payload + EOI."""
    assert SOI not in payload and EOI not in payload
    return SOI + b"\xff\xe0" + payload + EOI


class TestMJPEGFrameParser(unittest.TestCase):
    def test_single_frame_single_chunk(self):
        frame = jpeg(b"A" * 100)
        self.assertEqual(MJPEGFrameParser().feed(frame), [frame])

    def test_multiple_frames_single_chunk(self):
        f1, f2 = jpeg(b"A" * 50), jpeg(b"B" * 80)
        self.assertEqual(MJPEGFrameParser().feed(f1 + f2), [f1, f2])

    def test_frame_split_across_arbitrary_chunk_boundaries(self):
        f1, f2 = jpeg(b"C" * 300), jpeg(b"D" * 200)
        stream = f1 + f2
        for chunk_size in (1, 2, 3, 7, 64):
            parser = MJPEGFrameParser()
            got = []
            for i in range(0, len(stream), chunk_size):
                got.extend(parser.feed(stream[i : i + chunk_size]))
            self.assertEqual(got, [f1, f2], f"chunk_size={chunk_size}")

    def test_split_soi_marker_across_chunks(self):
        frame = jpeg(b"E" * 40)
        parser = MJPEGFrameParser()
        # First byte of SOI (0xFF) arrives at the end of one chunk...
        self.assertEqual(parser.feed(frame[:1]), [])
        # ...rest of the frame in the next.
        self.assertEqual(parser.feed(frame[1:]), [frame])

    def test_garbage_before_soi_is_discarded(self):
        frame = jpeg(b"F" * 30)
        got = MJPEGFrameParser().feed(b"\x00\x01\x02 not jpeg " + frame)
        self.assertEqual(got, [frame])

    def test_incomplete_frame_held_until_eoi_arrives(self):
        frame = jpeg(b"G" * 500)
        parser = MJPEGFrameParser()
        self.assertEqual(parser.feed(frame[:-1]), [])  # EOI not yet complete
        self.assertEqual(parser.feed(frame[-1:]), [frame])

    def test_resync_after_oversized_garbage(self):
        parser = MJPEGFrameParser()
        # A bogus SOI followed by > MAX_BUFFER of markerless data forces a resync.
        from mjpeg_web_relay import MAX_BUFFER

        parser.feed(SOI + b"\x00" * (MAX_BUFFER + 10))
        frame = jpeg(b"H" * 20)
        self.assertEqual(parser.feed(frame), [frame])


if __name__ == "__main__":
    unittest.main()
