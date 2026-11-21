"""Licence-preservation guard for the DXVM TestPack fork.

This fork strips PanPlay's game-launcher surface (see FORK.md and the plan's Task 3).
The Winlator-derived built-in X server under `com/winlator/` is **LGPL-2.1** and must
survive every stripping pass with its notices intact. This test is the guard: it runs
before any deletion and stays green afterwards.

Run: cd apps/dxvm-testpack && python3 -m pytest tests/component/test_licences.py -q
"""
from __future__ import annotations

import pathlib
import re

import pytest

APP = pathlib.Path(__file__).resolve().parents[2]

# (path, minimum bytes) — the notices the fork must never lose.
NOTICE_FILES = [
    ("third_party/winlator/LICENSE", 1000),
    ("NOTICE.md", 500),
    ("LICENSE", 500),
    ("app/src/main/assets/components-NOTICE.txt", 500),
    ("third_party/winlator/README.md", 100),
]

# LGPL source trees: (directory, glob, minimum file count before stripping).
LGPL_TREES = [
    ("app/src/main/java/com/winlator", "*.java", 120),
    ("app/src/main/cpp/winlator", "*.c", 3),
]

# Phrases whose absence means a notice was rewritten rather than kept.
REQUIRED_PHRASES = [
    ("third_party/winlator/LICENSE", "GNU LESSER GENERAL PUBLIC LICENSE"),
    ("third_party/winlator/LICENSE", "Version 2.1"),
    ("NOTICE.md", "LGPL-2.1"),
    ("NOTICE.md", "app/src/main/java/com/winlator/"),
    ("NOTICE.md", "app/src/main/cpp/winlator/"),
]


@pytest.mark.parametrize("rel,min_bytes", NOTICE_FILES, ids=[p for p, _ in NOTICE_FILES])
def test_notice_file_present_and_non_empty(rel: str, min_bytes: int) -> None:
    path = APP / rel
    assert path.is_file(), f"licence/notice file missing: {rel}"
    size = path.stat().st_size
    assert size >= min_bytes, f"{rel} shrank to {size} bytes (expected >= {min_bytes})"


@pytest.mark.parametrize("rel,needle", REQUIRED_PHRASES, ids=[f"{p}:{n}" for p, n in REQUIRED_PHRASES])
def test_notice_keeps_required_wording(rel: str, needle: str) -> None:
    text = (APP / rel).read_text(errors="replace")
    assert needle in text, f"{rel} no longer mentions {needle!r}"


@pytest.mark.parametrize("rel,glob,minimum", LGPL_TREES, ids=[p for p, _, _ in LGPL_TREES])
def test_lgpl_source_tree_survives(rel: str, glob: str, minimum: int) -> None:
    files = sorted((APP / rel).rglob(glob))
    assert len(files) >= minimum, (
        f"{rel} has {len(files)} {glob} files, expected at least {minimum}: "
        "the LGPL X server must not be stripped")


def test_lgpl_sources_are_marked() -> None:
    """NOTICE.md promises modifications are marked `panvk-launcher` in the sources."""
    marker = re.compile(r"panvk-launcher", re.IGNORECASE)
    marked = 0
    for path in (APP / "app/src/main/java/com/winlator").rglob("*.java"):
        if marker.search(path.read_text(errors="replace")):
            marked += 1
    for path in (APP / "app/src/main/cpp/winlator").rglob("*.c"):
        if marker.search(path.read_text(errors="replace")):
            marked += 1
    assert marked > 0, "no Winlator-derived source still carries its panvk-launcher provenance marker"


def test_notice_attributes_panprobe() -> None:
    """PanProbe's MIT tests are ported into app/src/main/cpp/vktests; MIT needs attribution."""
    text = (APP / "NOTICE.md").read_text(errors="replace")
    assert "PanProbe" in text, "NOTICE.md must attribute the ported PanProbe test sources (MIT)"
