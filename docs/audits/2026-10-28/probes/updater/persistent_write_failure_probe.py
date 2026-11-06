"""Reproduce rollback-metadata loss with a process-local Linux RLIMIT_FSIZE."""
from pathlib import Path
import contextlib
import errno
import io
import json
import signal
import sys
import tempfile

sys.dont_write_bytecode = True
from probe_support import baseline, emit, entrypoint, exception_details, load_updater, parse_repo, provenance


def main():
    repo = parse_repo(__doc__)
    if not sys.platform.startswith("linux"):
        raise RuntimeError("This probe requires Linux resource.RLIMIT_FSIZE and SIGXFSZ.")
    import resource

    old_limit = resource.getrlimit(resource.RLIMIT_FSIZE)
    if old_limit[0] != resource.RLIM_INFINITY and old_limit[0] <= 512:
        raise RuntimeError("Existing file-size limit must exceed the injected 512-byte limit.")
    updater = load_updater(repo)
    result = {**provenance(repo),
              "fault": "process-local RLIMIT_FSIZE=512 after two successful source replacements; SIGXFSZ ignored to return EFBIG"}
    with tempfile.TemporaryDirectory(prefix="boiler-updater-write-") as temporary:
        root = Path(temporary) / "bsp"
        src = baseline(repo, root)
        with contextlib.redirect_stdout(io.StringIO()):
            _, plan = updater.preflight(root, repo)
        original_write = updater.atomic_write
        completed_writes = 0
        limit_activated = False

        def introduce_write_failure(path, content):
            nonlocal completed_writes, limit_activated
            original_write(path, content)
            completed_writes += 1
            if completed_writes == 2:
                resource.setrlimit(resource.RLIMIT_FSIZE, (512, old_limit[1]))
                limit_activated = True

        old_signal = signal.signal(signal.SIGXFSZ, signal.SIG_IGN)
        updater.atomic_write = introduce_write_failure
        try:
            try:
                with contextlib.redirect_stdout(io.StringIO()):
                    updater.apply(src, root, repo, plan)
                result["apply"] = {"unexpected_success": True}
            except Exception as error:
                result["apply"] = exception_details(error)
        finally:
            try:
                resource.setrlimit(resource.RLIMIT_FSIZE, old_limit)
            finally:
                signal.signal(signal.SIGXFSZ, old_signal)
                updater.atomic_write = original_write
        result["fault_activated"] = limit_activated
        result["limit_restored"] = resource.getrlimit(resource.RLIMIT_FSIZE) == old_limit
        backups = list((root / "boiler_backups").iterdir())
        if len(backups) != 1:
            raise RuntimeError("Expected exactly one backup from the interrupted transaction.")
        backup = backups[0]
        manifest = backup / "manifest.json"
        result["backup_manifest_bytes"] = manifest.stat().st_size
        result["applied_source_files"] = [name for name, data in plan["edits"].items()
                                          if updater.current_bytes(root, name) == data]
        result["original_source_files"] = [name for name in plan["edits"]
                                           if updater.current_bytes(root, name) == plan["observed"][name]]
        result["backup_data_intact"] = all(
            (backup / name).read_bytes() == plan["observed"][name]
            for name in plan["edits"] if plan["observed"][name] is not None
        )
        try:
            json.loads(manifest.read_text(encoding="utf-8"))
            result["backup_manifest_parseable"] = True
        except json.JSONDecodeError as error:
            result["backup_manifest_parseable"] = False
            result["parse_error"] = exception_details(error)
        try:
            with contextlib.redirect_stdout(io.StringIO()):
                updater.restore(root, backup)
            result["documented_restore_after_fault_removed"] = {"success": True}
        except Exception as error:
            result["documented_restore_after_fault_removed"] = exception_details(error)
        reproduced = (
            limit_activated and result["limit_restored"]
            and result["apply"].get("errno") == errno.EFBIG
            and result["backup_manifest_bytes"] == 512
            and len(result["applied_source_files"]) == 2
            and len(result["original_source_files"]) == len(plan["edits"]) - 2
            and result["backup_data_intact"]
            and result["backup_manifest_parseable"] is False
            and result["documented_restore_after_fault_removed"].get("error_type") == "JSONDecodeError"
        )
        result["defect_reproduced"] = reproduced
    emit(result)
    return 0 if reproduced else 1


if __name__ == "__main__":
    entrypoint(main)
