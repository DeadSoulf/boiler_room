#!/usr/bin/env python3
"""Reproduce the three radio failure cases; exit 0 means defects observed."""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import shlex
import shutil
import subprocess
import sys
import tempfile
import time


PACKAGE = Path(__file__).resolve().parent
SOURCE_FILES = ("src/panel_radio.cpp", "src/panel_radio.h", "src/panel_types.h")
PROBES = (
    ("repro_wifi_transition", []),
    ("repro_wifi_begin_failure", []),
    (
        "repro_ble",
        [
            "-DCONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE=1",
            "-DCONFIG_NIMBLE_ENABLED=1",
            "-DCONFIG_BT_NIMBLE_ROLE_OBSERVER=1",
        ],
    ),
)
SUCCESS_MARKER = "Дефект воспроизведён:"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def execute(command, env, timeout, normalize):
    started = time.monotonic()
    record = {"command": [normalize(str(arg)) for arg in command]}
    try:
        result = subprocess.run(
            command,
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=timeout,
            check=False,
        )
        record.update(
            status="completed",
            returncode=result.returncode,
            stdout=normalize(result.stdout),
            stderr=normalize(result.stderr),
        )
    except subprocess.TimeoutExpired as error:
        def decode(value):
            return value.decode("utf-8", "replace") if isinstance(value, bytes) else value or ""

        record.update(
            status="timeout",
            returncode=None,
            stdout=normalize(decode(error.stdout)),
            stderr=normalize(decode(error.stderr)),
            timeout_seconds=timeout,
        )
    except OSError as error:
        record.update(status="launch_error", returncode=None, stdout="", stderr=normalize(str(error)))
    record["duration_seconds"] = round(time.monotonic() - started, 3)
    return record


def main():
    parser = argparse.ArgumentParser(
        description=(
            "Переносимые host-проверки радиодефектов. Exit 0 означает: все ожидаемые "
            "дефекты воспроизведены. Это не подтверждение исправности прошивки."
        ),
        epilog=(
            "Зависимости: Python 3.8+, Linux/POSIX, g++ с C++17, ASan и UBSan. "
            "Исходники прошивки не изменяются; оборудование не используется."
        ),
    )
    parser.add_argument(
        "--repo", type=Path, default=PACKAGE.parents[4], metavar="PATH",
        help="корень boiler_room; по умолчанию корень относительно docs/audits/DATE/probes/radio/run.py",
    )
    parser.add_argument(
        "--output", type=Path, metavar="PATH",
        help="каталог для трёх бинарников и results.json; по умолчанию новый временный каталог",
    )
    parser.add_argument(
        "--cxx", default=os.environ.get("CXX", "g++"), metavar="EXECUTABLE",
        help="один исполняемый файл C++ компилятора, по умолчанию CXX или g++",
    )
    args = parser.parse_args()
    repo = args.repo.expanduser().resolve()
    missing = [name for name in SOURCE_FILES if not (repo / name).is_file()]
    if missing:
        parser.error("в --repo отсутствуют исходники: " + ", ".join(missing))
    compiler = shutil.which(args.cxx)
    if compiler is None:
        parser.error("не найден C++ компилятор: " + args.cxx)
    output = (args.output.expanduser().resolve() if args.output else Path(tempfile.mkdtemp(prefix="boiler-radio-probes-")))
    try:
        output.mkdir(parents=True, exist_ok=True)
    except OSError as error:
        parser.error("невозможно создать --output: " + str(error))

    replacements = sorted(
        ((str(PACKAGE), "<probe>"), (str(repo), "<repo>"), (str(output), "<output>")),
        key=lambda pair: len(pair[0]), reverse=True,
    )

    def normalize(value):
        for original, replacement in replacements:
            value = value.replace(original, replacement)
        return value

    env = os.environ.copy()
    env["ASAN_OPTIONS"] = "detect_leaks=0:halt_on_error=1"
    env["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"
    baseline = json.loads((PACKAGE / "baseline.json").read_text(encoding="utf-8"))
    before = {name: digest(repo / name) for name in SOURCE_FILES}
    report = {
        "schema_version": 1,
        "started_at_utc": datetime.now(timezone.utc).isoformat(),
        "meaning_of_exit_0": "All three expected defects were observed; this is not a firmware acceptance test.",
        "paths": {"<probe>": "this probe package", "<repo>": "selected boiler_room checkout", "<output>": "temporary build output"},
        "python_version": platform.python_version(),
        "platform": platform.platform(),
        "compiler": execute([compiler, "--version"], env, 10, normalize),
        "sanitizer_environment": {key: env[key] for key in ("ASAN_OPTIONS", "UBSAN_OPTIONS")},
        "reference_repository": baseline["repository"],
        "reference_commit": baseline["commit"],
        "tested_source_sha256": before,
        "matches_reference_source_hashes": before == baseline["sha256"],
        "probe_file_sha256": {
            str(path.relative_to(PACKAGE)): digest(path)
            for path in sorted(PACKAGE.rglob("*"))
            if path.is_file() and path.suffix in {".cpp", ".h", ".inc", ".py", ".json", ".md", ".txt"}
        },
        "probes": [],
    }
    if not report["matches_reference_source_hashes"]:
        print("Внимание: исходники отличаются от проверенного commit; ожидаемый дефект мог быть исправлен.")
    infrastructure_failure = report["compiler"].get("returncode") != 0
    all_observed = True
    for name, defines in PROBES:
        binary = output / name
        command = [
            compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-g",
            "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
            "-fno-omit-frame-pointer", "-UNDEBUG",
            "-I", str(PACKAGE / "stubs"), "-I", str(repo / "src"),
        ] + defines + [str(PACKAGE / (name + ".cpp")), "-o", str(binary)]
        print("Сборка:", shlex.join([normalize(arg) for arg in command]), flush=True)
        compiled = execute(command, env, 60, normalize)
        result = {"name": name, "compile": compiled}
        if compiled.get("returncode") != 0:
            result["run"] = {"status": "not_run", "returncode": None, "reason": "compilation did not succeed"}
            result["outcome"] = "compile_failed"
            infrastructure_failure = True
            all_observed = False
            print("Сборка не выполнена:", name)
            print(compiled["stderr"], end="")
        else:
            executed = execute([str(binary)], env, 20, normalize)
            result["run"] = executed
            observed = executed.get("returncode") == 0 and SUCCESS_MARKER in executed["stdout"]
            result["outcome"] = "expected_defect_observed" if observed else "expected_defect_not_confirmed"
            all_observed = all_observed and observed
            infrastructure_failure = infrastructure_failure or executed["status"] != "completed"
            print(executed["stdout"], end="")
            if executed["stderr"]:
                print(executed["stderr"], end="", file=sys.stderr)
            if not observed:
                print("Ожидаемый дефект не подтверждён; проверьте статус выполнения:", name)
        report["probes"].append(result)

    after = {name: digest(repo / name) for name in SOURCE_FILES}
    report["source_files_unchanged"] = before == after
    if before != after:
        infrastructure_failure = True
        report["source_sha256_after"] = after
    returncode = 2 if infrastructure_failure else (0 if all_observed else 1)
    report["exit_code"] = returncode
    report["all_expected_defects_observed"] = all_observed
    report["finished_at_utc"] = datetime.now(timezone.utc).isoformat()
    report_path = output / "results.json"
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("Результаты:", report_path)
    if returncode == 0:
        print("Все три ожидаемых сценария: дефект воспроизведён. Exit 0 не означает исправную прошивку.")
    else:
        print("Проверка завершилась с кодом", returncode, "— отдельные статусы compile/run сохранены в results.json.")
    return returncode


if __name__ == "__main__":
    sys.exit(main())
