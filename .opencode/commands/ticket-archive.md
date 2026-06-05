---
description: Archive older Done tickets while keeping the newest 10 on the board
agent: build
---

Archive older Done tickets:
!`python tools/board/ticket_archive.py --keep 10`

Then read the command output and summarise which tickets moved to `docs/board-archive.md`. The `--keep 10` value is intentionally visible here so it can be edited if the board should retain a different number of recent Done items. Archival is a move into the archive, not a delete.