#!/usr/bin/env python3
"""
PlatformIO pre-build hook (wired via extra_scripts) that stages a character
pack into data/characters/<name>/ so `pio run -t uploadfs` always ships a GIF
character. Mirrors tools/flash_character.py but runs automatically as part of
the filesystem-image build for board envs that bundle a default character.

Pick the source with the CHARACTER env var or the `custom_character` option in
platformio.ini; defaults to characters/bufo.
"""
import json
import os
import shutil
from pathlib import Path

Import("env")  # noqa: F821 - provided by PlatformIO's SCons environment

PROJECT = Path(env.subst("$PROJECT_DIR"))  # noqa: F821
DATA = PROJECT / "data" / "characters"
CAP = 1_800_000

name = env.GetProjectOption("custom_character", "") or os.environ.get(  # noqa: F821
    "CHARACTER", "bufo"
)
src = PROJECT / "characters" / name


def stage() -> None:
    manifest = src / "manifest.json"
    if not manifest.exists():
        print(f"[prep_fs] no manifest.json in {src} — skipping FS staging")
        return

    char_name = json.loads(manifest.read_text())["name"]
    total = sum(f.stat().st_size for f in src.iterdir() if f.is_file())
    if total > CAP:
        raise SystemExit(f"[prep_fs] {total:,} bytes — over the {CAP:,} LittleFS cap")

    # uploadfs flashes everything under data/; the firmware reads one character
    # at a time, so wipe stale siblings first.
    if DATA.exists():
        shutil.rmtree(DATA)
    dst = DATA / char_name
    shutil.copytree(src, dst)
    print(f"[prep_fs] staged {char_name}: {total:,} bytes -> {dst}")


stage()
