"""Reproduce acceptance of three incompatible INI overrides; never build/flash."""
from pathlib import Path
import contextlib
import io
import sys
import tempfile

sys.dont_write_bytecode = True
from probe_support import baseline, emit, entrypoint, exception_details, load_updater, parse_repo, provenance


def main():
    repo = parse_repo(__doc__)
    updater = load_updater(repo)
    cases = {
        "baseline": lambda text: text,
        "chip_variant_override": lambda text: text + "\nboard_build.chip_variant = esp32p4\n",
        "mcu_override": lambda text: text + "\nboard_build.mcu = esp32s3\n",
        "lvgl_major_override": lambda text: text.replace("lvgl@~8.3.0", "lvgl@9.0.0"),
    }
    results = []
    for name, mutate in cases.items():
        with tempfile.TemporaryDirectory(prefix="boiler-updater-config-") as temporary:
            root = Path(temporary) / "bsp"
            src = baseline(repo, root)
            ini = src.parent / "platformio.ini"
            original = ini.read_text(encoding="utf-8")
            changed = mutate(original)
            if name != "baseline" and changed == original:
                raise ValueError("Fixture no longer contains the input required by this probe.")
            ini.write_text(changed, encoding="utf-8")
            result = {"case": name, "configuration_changed": changed != original}
            try:
                with contextlib.redirect_stdout(io.StringIO()):
                    _, plan = updater.preflight(root, repo)
                result.update({"accepted": True, "recognized_version": plan["version"],
                               "planned_source_changes": len(plan["edits"])})
            except Exception as error:
                result.update({"accepted": False, **exception_details(error)})
            result["configuration_preserved"] = ini.read_text(encoding="utf-8") == changed
            results.append(result)
    control_ok = (results[0].get("accepted") is True
                  and results[0].get("recognized_version") == "v031"
                  and results[0].get("planned_source_changes") == 6
                  and results[0]["configuration_preserved"])
    reproduced = control_ok and all(
        case.get("accepted") is True and case["configuration_changed"]
        and case["configuration_preserved"] for case in results[1:]
    )
    emit({**provenance(repo), "mode": "preflight_only_no_build_no_flash",
          "baseline_control_passed": control_ok, "defect_reproduced": reproduced,
          "results": results})
    return 0 if reproduced else (1 if control_ok else 2)


if __name__ == "__main__":
    entrypoint(main)
