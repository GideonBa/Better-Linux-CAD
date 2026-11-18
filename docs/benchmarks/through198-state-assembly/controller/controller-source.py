#!/usr/bin/env python3
"""Exclusive opt-in P01 controller. Run only after all builds and tests have ended."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import time


def utc():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def publish(path, data):
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n")
    temporary.replace(path)


def alive(group):
    try:
        os.killpg(group, 0)
        return True
    except ProcessLookupError:
        return False


def stop_group(child):
    """Terminate the owned session, including runner children, and reap its leader."""
    for sig, deadline in ((signal.SIGTERM, 5), (signal.SIGKILL, 5)):
        if alive(child.pid):
            try:
                os.killpg(child.pid, sig)
            except ProcessLookupError:
                pass
        end = time.monotonic() + deadline
        while alive(child.pid) and time.monotonic() < end:
            child.poll()  # Reap an exited leader before checking its group again.
            time.sleep(.05)
        if not alive(child.pid):
            break
    child.wait()
    return not alive(child.pid)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--phase", choices=("smoke", "series"), required=True)
    parser.add_argument("--timeout", type=float, help="per native process; defaults120 smoke/600 series")
    args = parser.parse_args()
    repo, executable, destination = args.repo.resolve(), args.executable.resolve(), args.output.resolve()
    per_process = args.timeout if args.timeout is not None else (120 if args.phase == "smoke" else 600)
    if not executable.is_file() or executable.stat().st_size == 0:
        parser.error("built nonempty profile executable is required")
    if not 0 < per_process <= 600:
        parser.error("per-process deadline must be in (0,600]")
    if destination.exists() and any(destination.iterdir()):
        parser.error("select a fresh or empty controller output directory")
    destination.mkdir(parents=True, exist_ok=True)
    jobs = [("smoke-states-2", "states", 2), ("smoke-assembly-10", "assembly", 10)] if args.phase == "smoke" else [("series", "all", None)]
    controller_source = Path(__file__).read_bytes()
    (destination / "controller-source.py").write_bytes(controller_source)
    report = {"schema": "blcad.p01-exclusive-controller.v1", "phase": args.phase,
              "controller_source_file": "controller-source.py",
              "controller_source_sha256": hashlib.sha256(controller_source).hexdigest(),
              "status": "running", "started_utc": utc(), "per_process_timeout_seconds": per_process,
              "policy": "one process group at a time; root confirms no concurrent builds/tests/apps; smoke is never pooled with publication",
              "jobs": []}
    report_path = destination / "exclusive-controller.json"
    publish(report_path, report)
    for name, mode, size in jobs:
        command = [sys.executable, str(repo / "scripts/run_variant_assembly_profile.py"),
                   "--repo", str(repo), "--executable", str(executable), "--output", str(destination / name),
                   "--mode", mode, "--processes", "1" if args.phase == "smoke" else "3",
                   "--timeout", str(per_process)]
        if size is not None:
            command += ["--size", str(size)]
        # Final series:18*600s plus20min for provenance/publication overhead.
        overall = per_process + 60 if args.phase == "smoke" else 18 * per_process + 1200
        record = {"name": name, "command": command, "started_utc": utc(),
                  "outer_deadline_seconds": overall, "stdout_stderr": name + ".controller.log"}
        print(json.dumps(record), flush=True)
        started = time.monotonic()
        child = None
        try:
            with (destination / record["stdout_stderr"]).open("w") as output:
                child = subprocess.Popen(command, stdout=output, stderr=subprocess.STDOUT,
                                         cwd=repo, start_new_session=True)
                try:
                    code = child.wait(timeout=overall)
                    status = "passed" if code == 0 else "failed"
                except subprocess.TimeoutExpired:
                    absent = stop_group(child)
                    code, status = 124, "outer-timeout"
                    record["owned_group_absence_confirmed"] = absent
                if alive(child.pid):
                    record["owned_group_absence_confirmed"] = stop_group(child)
                    status = "unclean-exit"
                else:
                    record["owned_group_absence_confirmed"] = True
        except BaseException as error:
            if child is not None:
                record["owned_group_absence_confirmed"] = stop_group(child)
            record.update(status="interrupted", error=str(error), elapsed_seconds=time.monotonic() - started)
            report["jobs"].append(record)
            report.update(status="interrupted", finished_utc=utc())
            publish(report_path, report)
            raise
        record.update(status=status, returncode=code, elapsed_seconds=time.monotonic() - started,
                      finished_utc=utc())
        report["jobs"].append(record)
        publish(report_path, report)
        print(f"{name}: {status}; owned process group absent={record['owned_group_absence_confirmed']}", flush=True)
        if status != "passed":
            # A final series runner still attempts all six groups internally.
            # A failed smoke blocks the publication run until its cause is reviewed.
            report.update(status="failed", finished_utc=utc())
            publish(report_path, report)
            return 1
    report.update(status="passed", finished_utc=utc())
    publish(report_path, report)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
