"""Run both defect reproductions in separate processes; print and optionally save JSON."""
from pathlib import Path
import argparse
import json
import subprocess
import sys

sys.dont_write_bytecode = True
from probe_support import emit, entrypoint, provenance, resolve_repo


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", metavar="PATH",
                        help="Repository root; default: ancestor checkout containing this script.")
    parser.add_argument("--output", metavar="DIR", type=Path,
                        help="Optional output directory; create a new results.json there.")
    args = parser.parse_args()
    try:
        repo = resolve_repo(args.repo)
    except ValueError as error:
        parser.error(str(error))
    directory = Path(__file__).resolve().parent
    results = []
    for filename in ("config_validation_probe.py", "persistent_write_failure_probe.py"):
        completed = subprocess.run(
            [sys.executable, "-B", str(directory / filename), "--repo", str(repo)],
            capture_output=True, text=True, check=False,
        )
        try:
            evidence = json.loads(completed.stdout)
        except json.JSONDecodeError:
            # Do not copy a traceback containing machine-specific paths into evidence.
            evidence = {"status": "invalid_probe_output"}
        results.append({"probe": filename, "exit_code": completed.returncode, "evidence": evidence})
    reproduced = all(item["exit_code"] == 0 and item["evidence"].get("defect_reproduced") is True
                     for item in results)
    environment_error = any(item["exit_code"] not in (0, 1)
                            or item["evidence"].get("status") == "invalid_probe_output"
                            for item in results)
    result = {**provenance(repo), "all_expected_defects_reproduced": reproduced, "probes": results}
    if args.output is not None:
        output = args.output.expanduser()
        output.mkdir(parents=True, exist_ok=True)
        # Exclusive creation preserves earlier evidence and never follows a file symlink.
        with (output / "results.json").open("x", encoding="utf-8") as saved:
            saved.write(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
    emit(result)
    return 0 if reproduced else (2 if environment_error else 1)


if __name__ == "__main__":
    entrypoint(main)
