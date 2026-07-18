#!/usr/bin/env python3
"""Round-trip tests for _lib front-matter parsing/rendering (META-014).

Run directly, no installs: python3 tools/board/test_lib_frontmatter.py
Guards the regression where ticket_move.py silently deleted optional
front-matter fields (depends_on, unknown keys) on every rewrite.
"""
from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from _lib import Frontmatter, parse_frontmatter, render_frontmatter  # noqa: E402

STORY_WITH_DEPENDS = """---
id: TRK-999
status: backlog
subsystem: tracking-core
tier: small
created: 2026-05-31
updated: 2026-05-31
depends_on:
  - "TRK-002"
  - "TRK-003"
spec: null
plan: null
blockers: []
---

## Context

Body text.
"""


class RoundTrip(unittest.TestCase):
    def test_multiline_depends_on_survives(self):
        fm, body = parse_frontmatter(STORY_WITH_DEPENDS)
        self.assertEqual(fm.depends_on, ["TRK-002", "TRK-003"])
        rendered = render_frontmatter(fm) + body
        self.assertEqual(rendered, STORY_WITH_DEPENDS)

    def test_unknown_key_survives(self):
        text = STORY_WITH_DEPENDS.replace("blockers: []\n", "blockers: []\nreviewed_by: gemini\n")
        fm, body = parse_frontmatter(text)
        self.assertEqual(fm.extras, {"reviewed_by": "gemini"})
        self.assertIn("reviewed_by: gemini\n", render_frontmatter(fm))

    def test_absent_depends_on_stays_absent(self):
        text = STORY_WITH_DEPENDS.replace('depends_on:\n  - "TRK-002"\n  - "TRK-003"\n', "")
        fm, _ = parse_frontmatter(text)
        self.assertEqual(fm.depends_on, [])
        self.assertNotIn("depends_on", render_frontmatter(fm))

    def test_empty_scalar_reads_null(self):
        text = STORY_WITH_DEPENDS.replace("spec: null", "spec:")
        fm, _ = parse_frontmatter(text)
        self.assertIsNone(fm.spec)
        self.assertIn("spec: null\n", render_frontmatter(fm))

    def test_inline_list_quotes_stripped(self):
        text = STORY_WITH_DEPENDS.replace(
            'depends_on:\n  - "TRK-002"\n  - "TRK-003"\n', 'depends_on: ["TRK-002", "TRK-003"]\n'
        )
        fm, _ = parse_frontmatter(text)
        self.assertEqual(fm.depends_on, ["TRK-002", "TRK-003"])

    def test_status_mutation_preserves_everything_else(self):
        fm, body = parse_frontmatter(STORY_WITH_DEPENDS)
        fm.status = "in-progress"
        fm.updated = "2026-07-16"
        rendered = render_frontmatter(fm) + body
        fm2, _ = parse_frontmatter(rendered)
        self.assertEqual(fm2.depends_on, ["TRK-002", "TRK-003"])
        self.assertEqual(fm2.status, "in-progress")
        self.assertEqual(fm2.blockers, [])

    def test_constructor_defaults(self):
        fm = Frontmatter(id="X-001", status="backlog")
        self.assertEqual(fm.depends_on, [])
        self.assertEqual(fm.extras, {})


if __name__ == "__main__":
    unittest.main()
