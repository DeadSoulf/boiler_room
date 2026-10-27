#!/usr/bin/env python3
"""Update BoilerPanel v0.1/v0.2/v0.3 to Swipe v0.4.0, preserving the working BSP.
Default: read-only. --apply: timestamped backup, then install application sources.
No network access, firmware flashing, C6 update, new fonts or flash erasure.
"""
from __future__ import annotations

import argparse
import ast
from datetime import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import sys

OLD_MAIN_SHA256 = "927a25ad9be9b33d91a6a8d849898d38b8feb8de11003b400c16ed931ce6dc1b"
OLD_V02_SHA256 = "6356eb3c1bdbd2a2ea9dd7efeebd45de89410306695c0c96d56430a22ad84fa1"
OLD_V03 = {'panel_radio.cpp': '693c3b5cd19da17184671a0dd31f60ad1a87fcee2cea10ef725767c62a6cd6e6', 'panel_types.h': 'ae9d11b2c13ca295b93916938dbff06957aaf4532e7ba8ef4d528da4921ecd2e', 'main.cpp': 'f3b868683746c7902129618cc374fad61698d22c4562efc04db7a768948f48ff', 'panel_radio.h': 'e673b8dc7628e82277a4933a0a53f9def74fa7090deda9256e7a4eb83c64a88c'}
FILES = ("main.cpp", "panel_types.h", "panel_radio.h", "panel_radio.cpp", "display_idle.h")


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def normalized_sha(path: Path) -> str:
    content = path.read_text(encoding="utf-8-sig").replace("\r\n", "\n")
    return hashlib.sha256(content.encode("utf-8")).hexdigest()


def required_codepoints(source: str) -> set[int]:
    # Strip comments before enumerating ordinary C/C++ string literals.
    tokens = re.findall(r'"(?:\\.|[^"\\])*"|//[^\n]*|/\*[\s\S]*?\*/', source)
    result: set[int] = set()
    for token in tokens:
        if not token.startswith('"'):
            continue
        text = ast.literal_eval(token)
        result.update(ord(c) for c in text if c not in "\n\r\t")
    return result


def verify_fonts(path: Path, needed: set[int]) -> None:
    content = path.read_text(encoding="utf-8-sig")
    for size in (14, 19, 25, 55):
        match = re.search(rf"unicode_list_{size}\[\]\s*=\s*\{{(.*?)\}};", content, re.S)
        cmap = re.search(rf"cmaps_{size}\[\]\s*=\s*\{{(.*?)\}};", content, re.S)
        if match is None or cmap is None:
            raise ValueError(f"Unsupported existing font layout, size {size}. No files changed.")
        start = re.search(r"\.range_start\s*=\s*(\d+)", cmap.group(1))
        if start is None:
            raise ValueError("Font range_start is missing. No files changed.")
        available = {int(start.group(1)) + int(n) for n in re.findall(r"\d+", match.group(1))}
        missing = sorted(needed - available)
        if missing:
            codes = ", ".join(f"U+{n:04X}" for n in missing)
            raise ValueError(f"Installed font {size} is missing {codes}. No files changed.")


def preflight(root: Path, package: Path) -> tuple[Path, dict[str, str]]:
    example = root / "examples" / "LvglUi"
    src = example / "src"
    main = src / "main.cpp"
    glue = src / "lvgl_glue.c"
    fonts = src / "boiler_fonts.c"
    ini = example / "platformio.ini"
    board = root / "boards" / "jc4880p4.json"
    for path in (main, glue, fonts, ini, board):
        if not path.is_file():
            raise ValueError(f"Required existing project file not found: {path}")
    for name in FILES:
        if not (package / "src" / name).is_file():
            raise ValueError(f"Missing update payload: {name}")

    payload_main = package / "src" / "main.cpp"
    current_sha = normalized_sha(main)
    if current_sha not in (OLD_MAIN_SHA256, OLD_V02_SHA256, OLD_V03["main.cpp"], normalized_sha(payload_main)):
        raise ValueError("main.cpp differs from the supplied v0.1/v0.2/v0.3 or this update. "
                         "Stopped to preserve your edits. No files changed.")
    # Accept only complete known v0.3 or v0.4 sets, not mixed versions.
    if current_sha == OLD_V03["main.cpp"]:
        for name in FILES[1:]:
            target=src/name
            if name in OLD_V03:
                if not target.is_file() or normalized_sha(target)!=OLD_V03[name]:
                    raise ValueError(f"Edited/incomplete v0.3 file: {name}. No files changed.")
            elif target.exists():
                raise ValueError(f"Unrelated {name} already exists. No files changed.")
    else:
        for name in FILES[1:]:
            target=src/name
            if target.exists() and normalized_sha(target)!=normalized_sha(package/"src"/name):
                raise ValueError(f"An unrelated or edited {name} exists. No files changed.")
            if current_sha==normalized_sha(payload_main) and not target.is_file():
                raise ValueError(f"Incomplete current release: {name}. No files changed.")

    variant = json.loads(board.read_text(encoding="utf-8-sig")).get("build", {}).get("chip_variant")
    if variant != "esp32p4_es":
        raise ValueError(f"Expected esp32p4_es for the confirmed chip v1.3; found {variant!r}. "
                         "The updater will not alter the board configuration.")
    glue_text = glue.read_text(encoding="utf-8-sig")
    checks = (r"#define\s+LV_HOR\s+PANEL_NATIVE_W\b", r"#define\s+LV_VER\s+PANEL_NATIVE_H\b")
    if not all(re.search(pattern, glue_text) for pattern in checks) or "board_p4_flush_region" not in glue_text:
        raise ValueError("The existing portrait adapter was not recognized. No files changed.")
    needed: set[int] = set()
    for name in FILES:
        needed.update(required_codepoints((package / "src" / name).read_text(encoding="utf-8")))
    verify_fonts(fonts, needed)
    protected = {str(path): sha(path) for path in (glue, fonts, ini, board)}
    return src, protected


def atomic_write(path: Path, content: bytes) -> None:
    temporary = path.with_name(path.name + ".standalone-update.tmp")
    created = False
    try:
        with temporary.open("xb") as out:
            created = True
            out.write(content)
            out.flush()
            os.fsync(out.fileno())
        os.replace(temporary, path)
    finally:
        if created and temporary.exists():
            temporary.unlink()


def apply(src: Path, root: Path, package: Path, protected: dict[str, str]) -> None:
    new_files = {name: (package / "src" / name).read_bytes() for name in FILES}
    if all((src / name).exists() and (src / name).read_bytes() == data for name, data in new_files.items()):
        print("ALREADY CURRENT: Swipe v0.4.0. No changes made.")
        return
    backup = root / "boiler_backups" / datetime.now().strftime("before_swipe_%Y%m%d_%H%M%S_%f")
    backup.mkdir(parents=True, exist_ok=False)
    previous: dict[str, bytes | None] = {}
    for name in FILES:
        target = src / name
        previous[name] = target.read_bytes() if target.exists() else None
        if target.exists():
            shutil.copy2(target, backup / name)
    manifest = {"version": "Swipe v0.4.0", "created": datetime.now().isoformat(),
                "target": str(src), "new_files": [k for k, v in previous.items() if v is None],
                "unchanged_files_sha256": protected}
    (backup / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    try:
        for name, content in new_files.items():
            atomic_write(src / name, content)
        for filename, expected in protected.items():
            if sha(Path(filename)) != expected:
                raise RuntimeError(f"Protected file changed concurrently: {filename}")
    except Exception:
        for name, content in previous.items():
            if content is None:
                (src / name).unlink(missing_ok=True)
            else:
                atomic_write(src / name, content)
        raise
    print("SUCCESS: Swipe v0.4.0 source installed.")
    print("BACKUP:", backup)
    print("UNCHANGED: display driver, touch adapter, fonts, platformio.ini, board config.")
    print("NOT FLASHED. Compile first, then upload using your existing PlatformIO project.")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("bsp", type=Path, help="Existing guition-jc4880p4-bsp directory")
    parser.add_argument("--apply", action="store_true", help="Back up and install the UI source update")
    args = parser.parse_args()
    root = args.bsp.expanduser().resolve()
    package = Path(__file__).resolve().parent
    try:
        src, protected = preflight(root, package)
        print("CHECK OK: installed v0.1/v0.2/v0.3/Swipe source, portrait BSP, esp32p4_es, text glyphs.")
        if args.apply:
            apply(src, root, package, protected)
        else:
            print("READ-ONLY CHECK. Add --apply to install the update.")
    except (OSError, ValueError, RuntimeError, SyntaxError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
