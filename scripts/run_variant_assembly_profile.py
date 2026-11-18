#!/usr/bin/env python3
"""Run opt-in P01 workloads sequentially, preserving raw provenance and failures."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import time


def digest(path):
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(chunk)
    return value.hexdigest()


def publish(path, value, compact=False):
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, indent=None if compact else 2,
                                    separators=(",", ":") if compact else None,
                                    sort_keys=True) + "\n")
    temporary.replace(path)


def git(repo, *args):
    return subprocess.check_output(["git", "-C", str(repo), *args])


def source_manifest(repo):
    names = git(repo, "ls-files", "-co", "--exclude-standard", "-z").split(b"\0")
    files = []
    for raw in sorted(set(names)):
        if not raw:
            continue
        name = os.fsdecode(raw)
        if name != "CMakeLists.txt" and not name.startswith(
            ("include/", "src/", "tests/", "cmake/", "scripts/")
        ):
            continue
        path = repo / name
        if path.is_file() and not path.is_symlink():
            files.append({"path": name, "sha256": digest(path), "bytes": path.stat().st_size})
    if not files:
        raise RuntimeError("runtime source manifest is empty")
    return files


def verify_sources(repo, files):
    changed = [item["path"] for item in files if not (repo / item["path"]).is_file()
               or digest(repo / item["path"]) != item["sha256"]]
    if changed:
        raise RuntimeError("runtime source changed during profile: " + ", ".join(changed[:10]))
    current_names = [item["path"] for item in source_manifest(repo)]
    if current_names != [item["path"] for item in files]:
        raise RuntimeError("runtime source inventory changed during profile")


def cache_metadata(executable):
    cache = executable.parent / "CMakeCache.txt"
    if not cache.is_file():
        return {"status": "unavailable"}
    selected = {}
    allowed = {"CMAKE_BUILD_TYPE", "CMAKE_CXX_COMPILER", "CMAKE_CXX_FLAGS",
               "CMAKE_CXX_FLAGS_DEBUG", "CMAKE_CXX_FLAGS_RELEASE",
               "CMAKE_CXX_FLAGS_RELWITHDEBINFO", "CMAKE_CXX_FLAGS_MINSIZEREL",
               "BLCAD_BUILD_GEOMETRY", "BLCAD_BUILD_GUI", "BLCAD_BUILD_TESTS",
               "OpenCASCADE_DIR", "Qt6_DIR", "Qt6Core_DIR"}
    for line in cache.read_text().splitlines():
        if line.startswith(("#", "//")) or "=" not in line or ":" not in line:
            continue
        key = line.split(":", 1)[0]
        if key in allowed:
            selected[key] = line.split("=", 1)[1]
    return {"sha256": digest(cache), "selected_configuration": selected}


def linked_libraries(executable):
    result = subprocess.run(["ldd", str(executable)], capture_output=True, text=True, check=False)
    libraries = []
    for line in result.stdout.splitlines():
        words = line.strip().split()
        candidate = words[2] if len(words) >= 3 and words[1] == "=>" else (words[0] if words else "")
        path = Path(candidate)
        if path.is_absolute() and path.is_file():
            libraries.append({"path": str(path), "sha256": digest(path), "bytes": path.stat().st_size})
    return {"returncode": result.returncode, "ldd_stdout": result.stdout,
            "ldd_stderr": result.stderr, "libraries": libraries}


def execute(args):
    repo = args.repo.resolve()
    executable = args.executable.resolve()
    destination = args.output.resolve()
    if not executable.is_file():
        raise RuntimeError("profile executable is unavailable")
    destination.mkdir(parents=True, exist_ok=True)
    if any(destination.iterdir()):
        raise RuntimeError("output directory must be empty; existing evidence is never overwritten")
    executable_sha256 = digest(executable)
    files = source_manifest(repo)
    manifest = {
        "schema": "blcad.profile-runtime-manifest.v1",
        "created_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "git_head": git(repo, "rev-parse", "HEAD").decode().strip(),
        "git_status": git(repo, "status", "--porcelain=v1").decode(),
        "git_diff_sha256": hashlib.sha256(git(repo, "diff", "HEAD", "--binary")).hexdigest(),
        "scope": "all regular working-tree files under include/src/tests/cmake/scripts plus root CMakeLists.txt, including untracked files",
        "files": files,
        "executable": {"path": str(executable), "sha256": executable_sha256,
                       "bytes": executable.stat().st_size},
        "build": cache_metadata(executable),
        "linked_libraries": linked_libraries(executable),
        "runner": {"python": sys.version, "platform": platform.platform(),
                   "cpu_affinity": sorted(os.sched_getaffinity(0)) if hasattr(os, "sched_getaffinity") else None,
                   "process_timeout_seconds": args.timeout, "processes_per_size": args.processes},
    }
    manifest_path = destination / "runtime-manifest.json"
    publish(manifest_path, manifest, compact=True)
    manifest_sha256 = digest(manifest_path)
    workloads = [("states", n) for n in (2, 10, 50)] + [("assembly", n) for n in (10, 100, 1000)]
    if args.mode != "all":
        workloads = [(mode, size) for mode, size in workloads if mode == args.mode]
    if args.size is not None:
        workloads = [(mode, size) for mode, size in workloads if size == args.size]
    if not workloads:
        raise RuntimeError("unsupported workload mode/size")
    summary = {"schema": "blcad.profile-process-series.v1", "status": "running",
               "runtime_manifest_sha256": manifest_sha256,
               "executable_sha256": executable_sha256,
               "policy": "separate sequential processes; one first sample and ten warm repeats in each; no portable budget or UI latency claim",
               "publication_complete": args.processes >= 3,
               "processes": []}
    summary_path = destination / "series.json"
    publish(summary_path, summary)
    failed = False
    for mode, size in workloads:
        for process_index in range(args.processes):
            verify_sources(repo, files)
            if digest(executable) != executable_sha256:
                raise RuntimeError("profile executable changed during series")
            prefix = f"{mode}-{size}-process-{process_index + 1}"
            artifact = destination / f"{prefix}.json"
            command = [str(executable), str(artifact), mode, str(size), str(manifest_path),
                       manifest_sha256, executable_sha256]
            started = time.monotonic()
            try:
                process = subprocess.run(command, capture_output=True, text=True, timeout=args.timeout,
                                         check=False, cwd=repo)
                returncode = process.returncode
                stdout, stderr = process.stdout, process.stderr
                status = "passed" if returncode == 0 else "failed"
            except subprocess.TimeoutExpired as error:
                returncode, status = 124, "timeout"
                stdout = error.stdout.decode(errors="replace") if isinstance(error.stdout, bytes) else error.stdout or ""
                stderr = error.stderr.decode(errors="replace") if isinstance(error.stderr, bytes) else error.stderr or ""
            elapsed = time.monotonic() - started
            (destination / f"{prefix}.stdout.txt").write_text(stdout)
            (destination / f"{prefix}.stderr.txt").write_text(stderr)
            if status == "timeout" or not artifact.exists():
                publish(artifact, {"schema": "blcad.state-assembly-profile.v1", "status": status,
                                   "mode": mode, "size": size, "error": "process deadline exceeded" if status == "timeout" else "process exited before publishing evidence",
                                   "timeout_seconds": args.timeout, "returncode": returncode,
                                   "runtime_manifest_sha256": manifest_sha256,
                                   "executable_sha256": executable_sha256})
            record = json.loads(artifact.read_text())
            if status == "passed" and (record.get("status") != "passed"
                                       or len(record.get("workload", {}).get("samples", [])) != 11):
                status = "failed"
            peak_rss = record.get("peak_rss_kib")
            if status == "passed" and (type(peak_rss) is not int or peak_rss <= 0
                                       or not isinstance(record.get("peak_rss_measurement_point"), str)
                                       or not record["peak_rss_measurement_point"].strip()):
                status = "failed"
            source = destination / record.get("source_intent_file", "__missing_source__")
            if status == "passed" and (not source.is_file()
                                       or digest(source) != record.get("source_intent_sha256")):
                status = "failed"
            failed |= status != "passed"
            summary["processes"].append({"mode": mode, "size": size,
                                          "process_index": process_index + 1, "status": status,
                                          "returncode": returncode, "elapsed_seconds": elapsed,
                                          "artifact": artifact.name, "artifact_sha256": digest(artifact)})
            publish(summary_path, summary)
            print(f"{prefix}: {status} ({elapsed:.2f}s)", flush=True)
    verify_sources(repo, files)
    if digest(executable) != executable_sha256:
        raise RuntimeError("profile executable changed during series")
    summary["status"] = "failed" if failed else "passed"
    summary["finished_utc"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    publish(summary_path, summary)
    return 1 if failed else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--mode", choices=("states", "assembly", "all"), default="all")
    parser.add_argument("--size", type=int)
    parser.add_argument("--processes", type=int, default=3,
                        help="three independent processes are required for a published size")
    parser.add_argument("--timeout", type=float, default=600,
                        help="hard wall-clock deadline per process, including fixture setup/checks")
    args = parser.parse_args()
    if args.processes < 1 or args.timeout <= 0:
        parser.error("processes and timeout must be positive")
    try:
        return execute(args)
    except Exception as error:
        args.output.mkdir(parents=True, exist_ok=True)
        publish(args.output / f"runner-failure-{os.getpid()}.json",
                {"status": "failed", "error": str(error)})
        print(str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
