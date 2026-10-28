"""Shared read-only repository lookup and isolated fixture construction."""
from pathlib import Path
import argparse
import hashlib
import importlib.util
import json
import shutil
import sys

sys.dont_write_bytecode = True
REFERENCE_COMMIT = "b700ef6f837f5fc1dbd0bad91a569159123c3851"


def resolve_repo(argument=None):
    if argument is not None:
        candidates = [Path(argument).expanduser().resolve()]
    else:
        # The installed location is docs/audits/<date>/probes/updater/.
        # Searching ancestors also permits a different audit date or checkout path.
        candidates = list(Path(__file__).resolve().parents)
    for candidate in candidates:
        if ((candidate / "apply_update.py").is_file()
                and (candidate / "tests/fixtures/v031").is_dir()
                and (candidate / "src/boiler_fonts.c").is_file()):
            return candidate
    raise ValueError("Repository with apply_update.py and v031 fixtures not found; use --repo PATH.")


def parse_repo(description):
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument("--repo", metavar="PATH",
                        help="Repository root; default: ancestor checkout containing this script.")
    args = parser.parse_args()
    try:
        return resolve_repo(args.repo)
    except ValueError as error:
        parser.error(str(error))


def load_updater(repo):
    spec = importlib.util.spec_from_file_location("audited_updater", repo / "apply_update.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def provenance(repo):
    return {
        "reference_commit": REFERENCE_COMMIT,
        "updater_sha256": hashlib.sha256((repo / "apply_update.py").read_bytes()).hexdigest(),
    }


def baseline(repo, root):
    """Only root (a TemporaryDirectory child) is written; repo is read-only."""
    fixtures = repo / "tests/fixtures"
    src = root / "examples/LvglUi/src"
    src.mkdir(parents=True)
    (root / "boards").mkdir()
    (root / "src").mkdir()
    for name, destination in (
        ("jc4880p4.json", "boards/jc4880p4.json"),
        ("board_p4.h", "src/board_p4.h"),
        ("screen031.ini", "examples/LvglUi/platformio.ini"),
    ):
        shutil.copy2(fixtures / "bsp" / name, root / destination)
    shutil.copy2(repo / "src/boiler_fonts.c", src / "boiler_fonts.c")
    shutil.copy2(fixtures / "bsp/screen031.c", src / "lvgl_glue.c")
    for path in sorted((fixtures / "v031").iterdir()):
        if path.is_file():
            shutil.copy2(path, src / path.name)
    return src


def exception_details(error):
    # Exception messages may contain absolute temporary/repository paths.
    # Type and errno suffice for this evidence and are portable across locales.
    result = {"error_type": type(error).__name__}
    if isinstance(error, OSError) and error.errno is not None:
        result["errno"] = error.errno
    if isinstance(error, json.JSONDecodeError):
        result.update({"message": error.msg, "line": error.lineno, "column": error.colno})
    return result


def emit(result):
    print(json.dumps(result, ensure_ascii=False, indent=2))


def entrypoint(main):
    try:
        status = main()
    except Exception as error:
        emit({"status": "probe_error", **exception_details(error)})
        status = 2
    raise SystemExit(status)
