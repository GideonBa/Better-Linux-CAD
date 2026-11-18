#!/usr/bin/env python3
"""Validate and summarize existing P01 evidence; never run or invent measurements."""
import argparse
import datetime
import hashlib
import json
import math
from pathlib import Path
import re
import statistics
import sys

GROUPS = [("states", n) for n in (2, 10, 50)] + [("assembly", n) for n in (10, 100, 1000)]
METRICS = {"states": {"effective_evaluation_batch_ms", "geometry_batch_ms"},
           "assembly": {"resolver_ms", "physical_properties_ms"}}
SHA256 = re.compile(r"[0-9a-f]{64}\Z")


def sha256(path):
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def finite(value):
    try:
        return type(value) in (int, float) and math.isfinite(value) and value >= 0
    except OverflowError:
        return False


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON field: {key}")
        result[key] = value
    return result


def read_json(path, errors):
    try:
        if path.is_symlink():
            raise ValueError("symbolic-link evidence is unsupported")
        value = json.loads(path.read_text(), object_pairs_hook=unique_object,
                           parse_constant=lambda value: (_ for _ in ()).throw(
                               ValueError(f"nonfinite JSON literal: {value}")))
        if not isinstance(value, dict):
            raise ValueError("expected JSON object")
        return value
    except (OSError, ValueError, RecursionError) as error:
        errors.append(f"{path.name}: {error}")
        return {}


def local_file(root, name, errors):
    if not isinstance(name, str) or not name or Path(name).is_absolute():
        errors.append(f"invalid relative evidence filename: {name!r}")
        return None
    path = root / name
    if not path.resolve().is_relative_to(root.resolve()) or path.is_symlink():
        errors.append(f"evidence filename escapes directory or is a symlink: {name}")
        return None
    return path


def check_hash(path, expected, errors, label):
    if not isinstance(expected, str) or not SHA256.fullmatch(expected):
        errors.append(f"{label}: invalid SHA-256")
        return False
    try:
        if path is None or not path.is_file() or path.is_symlink():
            raise ValueError("missing regular evidence file")
        if sha256(path) != expected:
            raise ValueError("SHA-256 mismatch")
        return True
    except (OSError, ValueError) as error:
        errors.append(f"{label}: {error}")
        return False


def distribution(values, unit):
    ordered = sorted(values)
    result = {"count": len(ordered), "p50": None, "p95": None, "max": None, "unit": unit}
    if ordered:
        result.update(p50=statistics.median(ordered),
                      p95=ordered[math.ceil(.95 * len(ordered)) - 1], max=ordered[-1])
    return result


def inspect_process(root, record, manifest_hash, executable_hash, provenance_valid):
    errors = []
    mode, size = record.get("mode"), record.get("size")
    artifact = local_file(root, record.get("artifact"), errors)
    trusted = check_hash(artifact, record.get("artifact_sha256"), errors, "artifact")
    if record.get("artifact") != f"{mode}-{size}-process-{record.get('process_index')}.json":
        errors.append("artifact filename does not match its process identity")
        trusted = False
    raw = read_json(artifact, errors) if artifact is not None else {}
    workload = raw.get("workload", {})
    rows = workload.get("samples", []) if isinstance(workload, dict) else []
    count_known = (raw.get("schema") == "blcad.state-assembly-profile.v1" and
                   isinstance(workload, dict) and "samples" in workload and isinstance(rows, list))
    if not isinstance(rows, list):
        errors.append("workload.samples must be an array")
        rows = []
    if raw.get("schema") != "blcad.state-assembly-profile.v1":
        errors.append("unsupported artifact schema")
        trusted = False
    for key, expected in (("mode", mode), ("size", size),
                          ("runtime_manifest_sha256", manifest_hash),
                          ("executable_sha256", executable_hash)):
        if raw.get(key) != expected:
            errors.append(f"artifact {key} does not match series/runtime manifest")
            trusted = False
    source_name = raw.get("source_intent_file")
    source_hash = raw.get("source_intent_sha256")
    if rows or raw.get("status") == "passed" or source_name is not None:
        source = local_file(root, source_name, errors)
        if source_name != str(record.get("artifact")) + ".source.json":
            errors.append("source intent filename does not match its raw process artifact")
            trusted = False
        source_ok = check_hash(source, source_hash, errors, "source intent sidecar")
        if source_ok:
            before = len(errors)
            read_json(source, errors)
            source_ok = len(errors) == before
        trusted = trusted and source_ok
    trusted = trusted and provenance_valid
    metrics = set(METRICS.get(mode, ()))
    for row in rows:
        if isinstance(row, dict):
            metrics.update(key for key in row if key.endswith("_ms"))
    samples, seen = [], set()
    for ordinal, row in enumerate(rows):
        problem = []
        if not isinstance(row, dict):
            errors.append(f"sample {ordinal}: expected object")
            continue
        iteration = row.get("iteration")
        if type(iteration) is not int or iteration not in range(11) or iteration in seen:
            problem.append("iteration must be unique and in [0,10]")
        else:
            seen.add(iteration)
        phase = "first-cold" if iteration == 0 else "warm"
        if row.get("phase") != phase:
            problem.append("phase does not match iteration")
        for key in sorted(metrics):
            if not finite(row.get(key)):
                problem.append(f"{key} must be finite and nonnegative")
        if problem:
            errors.append(f"sample {ordinal}: " + "; ".join(problem))
        elif trusted:
            samples.append(row)
    if rows and [row.get("iteration") if isinstance(row, dict) else None for row in rows] != list(range(len(rows))):
        errors.append("sample iterations must preserve their actual ordered prefix")
    runner_status, artifact_status = record.get("status"), raw.get("status")
    rss, point, rss_scope = raw.get("peak_rss_kib"), raw.get("peak_rss_measurement_point"), raw.get("peak_rss_scope")
    rss_valid = (trusted and type(rss) is int and finite(rss) and rss > 0 and
                 isinstance(point, str) and bool(point.strip()) and
                 isinstance(rss_scope, str) and bool(rss_scope.strip()))
    if runner_status == "passed":
        if artifact_status != "passed" or record.get("returncode") != 0:
            errors.append("passed runner record requires passed artifact and zero returncode")
        if type(raw.get("process_id")) is not int or raw["process_id"] <= 0:
            errors.append("passed process requires its positive native process id")
        if len(rows) != 11 or len(samples) != 11 or raw.get("warm_repeats") != 10:
            errors.append("passed process requires one cold and ten verified warm samples")
        if not rss_valid:
            errors.append("passed process requires positive integer RSS and its measurement point/scope")
    if runner_status not in ("passed", "failed", "timeout"):
        errors.append("unsupported runner process status")
    if not finite(record.get("elapsed_seconds")):
        errors.append("series elapsed_seconds must be finite and nonnegative")
    status = ("timeout" if "timeout" in (runner_status, artifact_status) else
              "failed" if "failed" in (runner_status, artifact_status) else
              "invalid" if errors else "passed")
    return {"process_index": record.get("process_index"), "status": status,
            "runner_status": runner_status, "artifact_status": artifact_status,
            "returncode": record.get("returncode"), "error": raw.get("error"),
            "timeout_seconds": raw.get("timeout_seconds"), "artifact": record.get("artifact"),
            "artifact_sha256": record.get("artifact_sha256"),
            "source_intent_file": source_name, "source_intent_sha256": source_hash,
            "obtained_samples": len(rows) if count_known else None, "verified_samples": len(samples),
            "cold_samples": sum(row["phase"] == "first-cold" for row in samples),
            "warm_samples": sum(row["phase"] == "warm" for row in samples),
            "validation_errors": errors}, samples, ((rss, point, rss_scope) if rss_valid else None), metrics


def aggregate(root, source_root=None):
    errors = []
    manifest_path, series_path = root / "runtime-manifest.json", root / "series.json"
    manifest, series = read_json(manifest_path, errors), read_json(series_path, errors)
    if manifest.get("schema") != "blcad.profile-runtime-manifest.v1":
        errors.append("unsupported runtime manifest schema")
    if series.get("schema") != "blcad.profile-process-series.v1":
        errors.append("unsupported series schema")
    manifest_hash = sha256(manifest_path) if manifest_path.is_file() else None
    if series.get("runtime_manifest_sha256") != manifest_hash:
        errors.append("series runtime manifest SHA-256 mismatch")
    executable = manifest.get("executable", {})
    executable_hash = executable.get("sha256") if isinstance(executable, dict) else None
    if not isinstance(executable_hash, str) or not SHA256.fullmatch(executable_hash):
        errors.append("runtime executable SHA-256 is invalid")
    if series.get("executable_sha256") != executable_hash:
        errors.append("series executable SHA-256 mismatch")
    files = manifest.get("files", [])
    if not isinstance(files, list) or not files:
        errors.append("runtime source manifest must contain files")
        files = []
    seen = set()
    for item in files:
        if not isinstance(item, dict):
            errors.append("runtime source manifest entry must be an object")
            continue
        name, value, size = item.get("path"), item.get("sha256"), item.get("bytes")
        if not isinstance(name, str) or not name or name in seen or Path(name).is_absolute() or ".." in Path(name).parts:
            errors.append("runtime manifest source path is invalid or duplicated")
            continue
        seen.add(name)
        if not isinstance(value, str) or not SHA256.fullmatch(value) or type(size) is not int or size < 0:
            errors.append(f"runtime source metadata is invalid: {name}")
        if source_root is not None:
            path = local_file(source_root, name, errors)
            if check_hash(path, value, errors, f"runtime source {name}") and path.stat().st_size != size:
                errors.append(f"runtime source byte count mismatch: {name}")
    provenance_valid = not errors
    records = series.get("processes", [])
    if not isinstance(records, list):
        errors.append("series.processes must be an array")
        records = []
    unknown = [row for row in records if not isinstance(row, dict) or
               type(row.get("size")) is not int or (row.get("mode"), row.get("size")) not in GROUPS]
    if unknown:
        errors.append(f"series contains {len(unknown)} unsupported process records")
    groups = []
    for mode, size in GROUPS:
        group_errors, processes, samples, memories = [], [], [], []
        metrics = set(METRICS[mode])
        selected = [row for row in records if isinstance(row, dict) and
                    row.get("mode") == mode and row.get("size") == size]
        indices = [row.get("process_index") for row in selected]
        if sorted(indices, key=str) != [1, 2, 3] or any(type(index) is not int for index in indices):
            group_errors.append("requires exactly three unique process indices 1,2,3")
        artifact_names = [row.get("artifact") for row in selected]
        if len(set(str(name) for name in artifact_names)) != len(artifact_names):
            group_errors.append("processes must reference distinct raw artifacts")
        consumed_paths, consumed_hashes = set(), set()
        unique_obtained_rows, unknown_sample_processes = 0, 0
        for row in selected:
            item, obtained, memory, found_metrics = inspect_process(
                root, row, manifest_hash, executable_hash, provenance_valid)
            artifact_name, artifact_hash = item["artifact"], item["artifact_sha256"]
            duplicate = str(artifact_name) in consumed_paths or (
                isinstance(artifact_hash, str) and artifact_hash in consumed_hashes)
            if duplicate:
                item["validation_errors"].append("duplicate artifact identity excluded from aggregation")
                item["verified_samples"] = item["cold_samples"] = item["warm_samples"] = 0
                item["status"] = "invalid"
                obtained, memory = [], None
            else:
                consumed_paths.add(str(artifact_name))
                if isinstance(artifact_hash, str):
                    consumed_hashes.add(artifact_hash)
                if item["obtained_samples"] is None:
                    unknown_sample_processes += 1
                else:
                    unique_obtained_rows += item["obtained_samples"]
            processes.append(item)
            samples.extend(obtained)
            metrics.update(found_metrics)
            if memory:
                memories.append(memory)
        timings = {}
        for metric in sorted(metrics):
            timings[metric] = {
                "cold": distribution([row[metric] for row in samples
                                      if metric in row and row["phase"] == "first-cold"], "ms"),
                "warm": distribution([row[metric] for row in samples
                                      if metric in row and row["phase"] == "warm"], "ms")}
            if any(metric not in row for row in samples):
                group_errors.append(f"{metric}: inconsistent metric inventory between processes")
        points, scopes = sorted({row[1] for row in memories}), sorted({row[2] for row in memories})
        if len(points) > 1 or len(scopes) > 1:
            group_errors.append("RSS observations use different measurement points/scopes")
        rss = distribution([row[0] for row in memories], "KiB")
        rss.pop("p95")
        rss["min"] = min((row[0] for row in memories), default=None)
        rss.update(measurement_points=points, measurement_scopes=scopes)
        ready = (provenance_valid and not group_errors and len(processes) == 3 and
                 all(item["status"] == "passed" and not item["validation_errors"] for item in processes) and
                 all(item["cold"]["count"] == 3 and item["warm"]["count"] == 30 for item in timings.values()) and
                 rss["count"] == 3)
        status = ("missing" if not processes else
                  "timeout" if any(item["status"] == "timeout" for item in processes) else
                  "failed" if any(item["status"] == "failed" for item in processes) else
                  "invalid" if any(item["status"] == "invalid" for item in processes) else
                  "passed" if ready else "incomplete")
        groups.append({"mode": mode, "size": size, "status": status, "publication_ready": ready,
                       "processes_obtained": len(processes), "expected_processes": 3,
                       "obtained_samples": unique_obtained_rows if not unknown_sample_processes else None,
                       "known_obtained_samples": unique_obtained_rows,
                       "unknown_sample_processes": unknown_sample_processes,
                       "referenced_sample_rows_known": sum(item["obtained_samples"] or 0 for item in processes),
                       "verified_samples": len(samples), "expected_samples": 33,
                       "metrics": timings, "peak_rss_kib": rss,
                       "validation_errors": group_errors, "processes": processes})
    complete = not errors and series.get("status") == "passed" and all(group["publication_ready"] for group in groups)
    return {"schema": "blcad.p01-aggregate.v1", "status": "passed" if complete else "incomplete",
            "publication_ready": complete, "generated_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
            "runtime_manifest_sha256": manifest_hash,
            "series_sha256": sha256(series_path) if series_path.is_file() else None,
            "executable_sha256": executable_hash, "runtime_manifest_file_count": len(files),
            "runtime_source_verification": "verified against supplied source root" if source_root is not None else
                "manifest digest/entry metadata verified; live source files not rechecked (runner records immutable-run verification)",
            "series_status": series.get("status"), "validation_errors": errors,
            "statistics_policy": {"p50": "median; mean of two central observations for even n",
                "p95": "nearest-rank: sorted[ceil(0.95*n)-1]", "missing": "null; never zero-filled",
                "included": "hash-verified, finite, nonnegative, uniquely phased raw observations; any obtained observations from failed processes remain labelled failed",
                "cold": "iteration 0 in each separate process; no OS cache flush",
                "rss": "one process-lifetime observation per process at its recorded measurement point"},
            "aggregator_sha256": sha256(Path(__file__)), "groups": groups}


def markdown(summary):
    def number(value):
        return "—" if value is None else f"{value:.6g}"
    def timing(value):
        return f"{number(value['p50'])} / {number(value['p95'])} / {number(value['max'])} ({value['count']})"
    lines = ["# P01 state / nested Assembly baseline", "",
             f"Overall: **{summary['status']}**; publication ready: **{str(summary['publication_ready']).lower()}**.", "",
             "Timing cells: p50 / nearest-rank p95 / observed max in ms (actual count). Cold expects 3; warm expects 30. Missing observations are shown as —.", "",
             "| Mode | Size | Status | Processes /3 | Samples verified / obtained | Metric | Cold | Warm | Peak RSS KiB min / p50 / max (process count) |",
             "| --- | ---: | --- | ---: | ---: | --- | --- | --- | --- |"]
    for group in summary["groups"]:
        for index, (metric, values) in enumerate(group["metrics"].items()):
            memory = group["peak_rss_kib"]
            rss = " / ".join(number(memory[key]) for key in ("min", "p50", "max")) + f" ({memory['count']})" if index == 0 else ""
            obtained = str(group["obtained_samples"]) if group["obtained_samples"] is not None else f"≥{group['known_obtained_samples']} (unknown in {group['unknown_sample_processes']} processes)"
            lines.append(f"| {group['mode']} | {group['size']} | {group['status']} | {group['processes_obtained']} | {group['verified_samples']} / {obtained} | `{metric}` | {timing(values['cold'])} | {timing(values['warm'])} | {rss} |")
    lines += ["", "Cold is the first measured batch in each new process, without an OS cache flush. RSS is process-lifetime peak resident memory, not a per-call allocation or baseline-subtracted delta."]
    for group in summary["groups"]:
        memory = group["peak_rss_kib"]
        if memory["measurement_points"]:
            lines += ["", f"{group['mode']} {group['size']} RSS measurement point: " + "; ".join(memory["measurement_points"]) + "."]
        failures = list(group["validation_errors"])
        for item in group["processes"]:
            notes = item["validation_errors"] + ([str(item["error"])] if item["error"] else [])
            if notes:
                failures.append(f"process {item['process_index']} ({item['status']}): " + "; ".join(notes))
        if failures:
            lines += ["", f"{group['mode']} {group['size']} validation: " + " · ".join(failures)]
    if summary["validation_errors"]:
        lines += ["", "Global validation: " + " · ".join(summary["validation_errors"])]
    lines += ["", f"Runtime manifest SHA-256: `{summary['runtime_manifest_sha256']}`.",
              f"Series SHA-256: `{summary['series_sha256']}`.", "",
              "This synchronous headless baseline does not establish native UI latency, unmeasured solve/export performance, a portable budget, or numbered acceptance.", ""]
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("evidence_directory", type=Path)
    parser.add_argument("--output-dir", type=Path, help="defaults to evidence directory; raw files are never rewritten")
    parser.add_argument("--source-root", type=Path, help="additionally verify manifest source hashes/sizes against this unchanged tree")
    args = parser.parse_args()
    root = args.evidence_directory.resolve()
    destination = (args.output_dir or root).resolve()
    outputs = [destination / "p01-summary.json", destination / "p01-summary.md"]
    if not root.is_dir():
        parser.error("evidence directory is unavailable")
    if any(path.exists() for path in outputs):
        parser.error("derived output already exists; select a new output directory")
    summary = aggregate(root, args.source_root.resolve() if args.source_root else None)
    destination.mkdir(parents=True, exist_ok=True)
    outputs[0].write_text(json.dumps(summary, sort_keys=True, separators=(",", ":"), allow_nan=False) + "\n")
    outputs[1].write_text(markdown(summary))
    print(f"{summary['status']}: " + ", ".join(f"{row['mode']}/{row['size']}={row['status']} samples {row['verified_samples']}/{row['obtained_samples'] if row['obtained_samples'] is not None else 'unknown'}" for row in summary['groups']))
    return 0 if summary["publication_ready"] else 1


if __name__ == "__main__":
    sys.exit(main())
