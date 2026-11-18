#!/usr/bin/env python3
"""Collect finalized, explicitly selected through-198 proof; never execute tests."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import sys

ANSI = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]")
DIGEST = re.compile(r"[0-9a-f]{64}\Z")
CACHE = {}


def digest(path):
    path = path.resolve()
    if path not in CACHE:
        value = hashlib.sha256()
        with path.open("rb") as stream:
            for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                value.update(chunk)
        CACHE[path] = value.hexdigest()
    return CACHE[path]


def file_record(path, root=None):
    path = Path(path)
    if not path.is_file() or path.is_symlink():
        raise ValueError(f"missing regular evidence file: {path}")
    label = str(path.relative_to(root)) if root and path.is_relative_to(root) else str(path)
    return {"path": label, "sha256": digest(path), "bytes": path.stat().st_size}


def load(path):
    return json.loads(Path(path).read_text())


def catch_footer(path):
    text = ANSI.sub("", Path(path).read_text(errors="replace"))
    passed = list(re.finditer(r"All tests passed \(([\d,]+) assertions? in ([\d,]+) test cases?\)", text))
    totals = list(re.finditer(r"^test cases:\s*([\d,]+)([^\n]*)\nassertions:\s*([\d,]+)([^\n]*)", text, re.M))
    if not passed and not totals:
        raise ValueError("no complete Catch result footer; interrupted output is not acceptance")
    integer = lambda value: int(value.replace(",", ""))
    if totals and (not passed or totals[-1].start() > passed[-1].start()):
        value = totals[-1]
        def component(suffix, status):
            found = re.search(r"([\d,]+) " + status + r"\b", suffix)
            return integer(found.group(1)) if found else 0
        result = {"cases": integer(value[1]), "passed": component(value[2], "passed"),
                  "failed": component(value[2], "failed"), "skipped": component(value[2], "skipped"),
                  "assertions": integer(value[3]), "failed_assertions": component(value[4], "failed")}
    else:
        result = {"cases": integer(passed[-1][2]), "passed": integer(passed[-1][2]),
                  "failed": 0, "skipped": 0, "assertions": integer(passed[-1][1]), "failed_assertions": 0}
    if result["passed"] + result["failed"] + result["skipped"] != result["cases"]:
        raise ValueError("Catch case partition is inconsistent")
    return result


def completion(entry, evidence_root):
    if entry.get("process_json"):
        path = Path(entry["process_json"])
        value = load(path)
        if value.get("finished") is not True:
            raise ValueError("process supervisor has not recorded completion")
        if "records" in value:
            records = [row for row in value["records"] if row.get("log") == entry.get("log")]
            if len(records) != 1:
                raise ValueError("log does not resolve to exactly one process record")
            code = records[0].get("exit_code")
        else:
            code = value.get("exit_code")
        if type(code) is not int:
            raise ValueError("completed process record lacks an integer exit code")
        return {"exit_code": code, "source": "completed supervisor record",
                "record": file_record(path, evidence_root)}
    if type(entry.get("exit_code")) is not int or not entry.get("exit_evidence"):
        raise ValueError("explicit completed-process exit evidence is pending")
    return {"exit_code": entry["exit_code"], "source": entry["exit_evidence"]}


def collect(plan, repo):
    root = Path(plan["evidence_root"]).resolve()
    errors, pending = [], []
    result = {"schema": "blcad.through198-final-proof.v2", "evidence_root_at_collection": str(root),
              "policy": "Only explicit finalized runs are eligible; no interrupted run, diagnostic override, unrecorded timing or inferred binary identity establishes acceptance.",
              "notes": plan.get("notes", [])}
    def checked(label, operation):
        try:
            return operation()
        except (OSError, ValueError, KeyError, TypeError, struct.error) as error:
            errors.append(f"{label}: {error}")
            return {"status": "invalid", "error": str(error)}
    result["selection_plan"] = checked("selection plan", lambda: file_record(Path(plan["selection_plan_path"]), root))
    result["collector"] = checked("collector source", lambda: file_record(Path(__file__), root))
    def runtime():
        entry = plan["runtime"]
        path = Path(entry["path"])
        record = file_record(path, root)
        if record["sha256"] != entry["sha256"]:
            raise ValueError("frozen runtime manifest digest changed")
        files = load(path)
        if not isinstance(files, dict) or len(files) != entry["expected_files"]:
            raise ValueError("frozen runtime inventory count mismatch")
        for relative, expected in files.items():
            source = repo / relative
            if not DIGEST.fullmatch(expected) or not source.resolve().is_relative_to(repo):
                raise ValueError(f"invalid manifest entry: {relative}")
            if file_record(source)["sha256"] != expected:
                raise ValueError(f"runtime source changed: {relative}")
        # Same explicit inventory policy as the frozen manifest producer.
        names = set()
        for folder in ("src", "include", "tests", "assets", "cmake", "scripts", "examples"):
            names.update(str(path.relative_to(repo)) for path in (repo / folder).rglob("*")
                         if path.is_file() and "__pycache__" not in path.parts)
        names.update(("CMakeLists.txt", "CMakePresets.json"))
        if names != set(files):
            raise ValueError("runtime inventory changed after freeze")
        return {"status": "verified", **record, "file_count": len(files), "freeze": entry["freeze"]}
    result["runtime"] = checked("runtime", runtime)
    result["builds"] = {}
    for name, entry in plan["builds"].items():
        def build(entry=entry):
            state = completion(entry, root)
            if state["exit_code"] != 0:
                raise ValueError("build process did not exit zero")
            value = load(entry["process_json"])
            if not set(entry.get("expected_targets", ())).issubset(value.get("command", ())):
                raise ValueError("completed build command lacks required targets")
            return {"status": "passed", "completion": state, "log": file_record(Path(entry["log"]), root)}
        result["builds"][name] = checked(name, build)
    result["binaries"] = {}
    for name, entry in plan["binaries"].items():
        def binary(entry=entry):
            path = repo / entry["path"]
            record = file_record(path, repo)
            if entry.get("saved_identity"):
                old = load(entry["saved_identity"])[entry["path"]]
                if record["sha256"] != old["sha256"] or record["bytes"] != old["bytes"]:
                    raise ValueError("binary changed from its selected saved identity")
            return {"status": "verified", **record, "verified_build": entry["verified_build"],
                    "identity_source": file_record(Path(entry["saved_identity"]), root)
                    if entry.get("saved_identity") else {"source": "current bytes; no historical identity asserted"},
                    "execution_context": entry.get("execution_context")}
        result["binaries"][name] = checked(name, binary)
    def retained_profile_transition():
        entry = plan["retained_profile_transition"]
        proof = load(entry["record"])
        for field in ("record", "before_runtime", "before_binaries", "after_binaries"):
            if digest(Path(entry[field])) != entry[field + "_sha256"]:
                raise ValueError(f"selected retained-source evidence changed: {field}")
        before_runtime, after_runtime = load(entry["before_runtime"]), load(entry["after_runtime"])
        before_binaries, after_binaries = load(entry["before_binaries"]), load(entry["after_binaries"])
        if set(before_runtime) != set(after_runtime) or set(before_binaries) != set(after_binaries):
            raise ValueError("retained-source comparison inventories changed membership")
        changed_inputs = sorted(key for key in before_runtime if before_runtime[key] != after_runtime[key])
        changed_artifacts = sorted(key for key in before_binaries if before_binaries[key] != after_binaries[key])
        unchanged_artifacts = sorted(set(after_binaries) - set(changed_artifacts))
        if changed_inputs != sorted(entry["expected_changed_inputs"]) or changed_artifacts != sorted(entry["expected_changed_artifacts"]):
            raise ValueError("transition changed more than the explicitly selected optional profile")
        if (sorted(proof["changed_runtime_inputs"]) != changed_inputs or
                sorted(proof["changed_artifacts"]) != changed_artifacts or
                sorted(proof["unchanged_artifacts"]) != unchanged_artifacts or
                Path(proof["source_manifest_before"]).resolve() != Path(entry["before_runtime"]).resolve() or
                Path(proof["source_manifest_after"]).resolve() != Path(entry["after_runtime"]).resolve()):
            raise ValueError("retained-source attestation differs from independently compared inventories")
        if Path(entry["after_runtime"]).resolve() != Path(plan["runtime"]["path"]).resolve():
            raise ValueError("retained-source transition does not end at selected current runtime")
        actual = {}
        for relative, expected in after_binaries.items():
            current = file_record(repo / relative, repo)
            if current["sha256"] != expected["sha256"] or current["bytes"] != expected["bytes"]:
                raise ValueError(f"current artifact differs from final saved identity: {relative}")
            actual[relative] = current
        origins = []
        for origin in entry.get("retained_artifact_origins", []):
            snapshot = load(origin["snapshot"])
            for relative in origin["artifacts"]:
                if snapshot[relative] != after_binaries[relative]:
                    raise ValueError(f"older retained artifact changed: {relative}")
            origins.append({"name": origin["name"], "snapshot": file_record(Path(origin["snapshot"]), root),
                            "artifacts": origin["artifacts"]})
        return {"status": "verified", "record": file_record(Path(entry["record"]), root),
                "before_runtime": file_record(Path(entry["before_runtime"]), root),
                "after_runtime": file_record(Path(entry["after_runtime"]), root),
                "before_binaries": file_record(Path(entry["before_binaries"]), root),
                "after_binaries": file_record(Path(entry["after_binaries"]), root),
                "changed_runtime_inputs": changed_inputs, "changed_artifacts": changed_artifacts,
                "unchanged_artifacts": unchanged_artifacts, "retained_artifact_origins": origins,
                "identity_only_artifacts": actual,
                "boundary": "Functional runs retain their recorded runtime22/earlier attribution; build28 does not imply a functional rerun on runtime23."}
    result["retained_profile_transition"] = checked("retained optional-profile transition", retained_profile_transition)
    def registry():
        entry = plan["gui_registry"]
        partition = load(entry["partition"])
        groups, records = {}, {}
        for name in ("default", "broad", "housing", "audit"):
            path = Path(entry[name])
            names = path.read_text().splitlines()
            if any(not name for name in names) or len(set(names)) != len(names):
                raise ValueError(f"{name} registry contains empty or duplicate names")
            groups[name] = set(names)
            records[name] = {"count": len(names), **file_record(path, root)}
        if [len(groups[name]) for name in ("default", "broad", "housing", "audit")] != [1028, 1022, 1, 5]:
            raise ValueError("expected exact default1028 = broad1022 + housing1 + audit5 registry")
        if (groups["broad"] & groups["audit"] or groups["broad"] & groups["housing"] or
                groups["housing"] & groups["audit"] or
                groups["broad"] | groups["housing"] | groups["audit"] != groups["default"]):
            raise ValueError("GUI execution partition is overlapping or incomplete")
        if partition.get("counts") != {name: len(values) for name, values in groups.items()}:
            raise ValueError("saved partition count metadata differs from actual registry names")
        for name, record in records.items():
            saved = partition["registry_files"][name]
            if saved["count"] != record["count"] or saved["sha256"] != record["sha256"]:
                raise ValueError(f"saved registry identity differs: {name}")
        if partition["binary"]["sha256"] != result["binaries"]["gui"].get("sha256"):
            raise ValueError("GUI registry was captured from a different executable")
        return {"status": "verified", "partition": file_record(Path(entry["partition"]), root),
                "groups": records, "disjoint": True, "complete_union": True,
                "specifications": partition["specifications"]}
    result["gui_registry"] = checked("GUI registry", registry)
    result["suites"] = {}
    for name, entry in plan["suites"].items():
        if not entry.get("log"):
            pending.append(name)
            result["suites"][name] = {"status": "pending", "purpose": entry.get("purpose")}
            continue
        def suite(entry=entry):
            path = Path(entry["log"])
            if entry.get("interrupted") or "interrupted" in path.name:
                raise ValueError("explicitly interrupted run is ineligible")
            state = completion(entry, root)
            counts = catch_footer(path)
            if state["exit_code"] != 0 or counts["failed"] or counts["failed_assertions"]:
                raise ValueError("suite did not complete with zero failures/zero exit")
            if entry.get("expected_cases") is not None and counts["cases"] != entry["expected_cases"]:
                raise ValueError("selected case count differs from exact planned partition")
            if entry.get("expected_assertions") is not None and counts["assertions"] != entry["expected_assertions"]:
                raise ValueError("assertion count differs from explicitly finalized evidence")
            if entry.get("expected_skipped") is not None and counts["skipped"] != entry["expected_skipped"]:
                raise ValueError("suite skipped a required executed case")
            binary_name = entry["binary"]
            return {"status": "passed", **counts, "log": file_record(path, root), "completion": state,
                    "binary": binary_name, "binary_sha256": result["binaries"][binary_name].get("sha256"),
                    "platform": entry.get("platform"), "scale": entry.get("scale"),
                    "filter": entry.get("filter"), "environment": entry.get("environment", {}),
                    "execution_context": entry.get("execution_context")}
        result["suites"][name] = checked(name, suite)
    default_groups = {name: result["suites"][suite] for name, suite in
                      {"broad": "gui_broad", "housing": "gui_housing", "audit": "gui_audit"}.items()}
    default_complete = (result["gui_registry"].get("status") == "verified" and
                        all(group.get("status") == "passed" for group in default_groups.values()))
    result["gui_default_execution"] = {
        "status": "complete" if default_complete else "incomplete",
        "policy": "Disjoint mixed-platform default registry coverage: offscreen broad, native xcb housing100, offscreen metadata audit. Native8 plus connected2 per scale is focused coverage, not a full native suite.",
        "registered_cases": 1028,
        "partitions": {name: {"suite": suite, "status": result["suites"][suite]["status"]}
                       for name, suite in {"broad": "gui_broad", "housing": "gui_housing", "audit": "gui_audit"}.items()}}
    if default_complete:
        totals = {key: sum(group[key] for group in default_groups.values())
                  for key in ("cases", "passed", "skipped", "assertions")}
        if totals["cases"] != 1028:
            errors.append("GUI execution partitions do not total the exact1028 default registry")
        result["gui_default_execution"]["totals"] = totals
    result["native_captures"] = {}
    for name, entry in plan["native_captures"].items():
        def captures(entry=entry):
            path = Path(entry["directory"])
            images = sorted(path.glob("*.png"))
            required = set(entry.get("required_files", ()))
            if not images or not required.issubset({image.name for image in images}):
                raise ValueError("required native captures are missing")
            records = []
            for image in images:
                with image.open("rb") as stream:
                    header = stream.read(24)
                if header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
                    raise ValueError(f"invalid PNG header: {image.name}")
                width, height = struct.unpack(">II", header[16:24])
                if not width or not height or width > 1920 or height > 1080:
                    raise ValueError(f"capture exceeds recorded1920x1080 desktop: {image.name}")
                records.append({**file_record(image, root), "width_px": width, "height_px": height})
            return {"status": "hashed", "platform": "xcb", "scale": entry["scale"], "files": records,
                    "boundary": "capture hashes complement executed native assertions; screenshots alone do not prove interaction or geometry"}
        result["native_captures"][name] = checked(name, captures)
    entry = plan.get("startup")
    if not entry or not entry.get("record"):
        pending.append("startup")
        result["startup"] = {"status": "pending"}
    else:
        def startup():
            value = load(entry["record"])
            if value.get("status") != "passed" or value.get("exit_code") != 0 or value.get("healthy_before_close") is not True or value.get("close") != "WM_DELETE_WINDOW":
                raise ValueError("startup requires visible healthy window and clean native close")
            return {"status": "passed", "record": file_record(Path(entry["record"]), root),
                    "log": file_record(Path(entry["log"]), root), "capture": file_record(Path(entry["capture"]), root),
                    "binary_sha256": result["binaries"]["app"].get("sha256")}
        result["startup"] = checked("startup", startup)
    result["validation_errors"], result["pending_gates"] = errors, pending
    result["status"] = "passed" if not errors and not pending else "incomplete"
    result["numbered_acceptance_authority"] = "docs/mvp-plan.md; collector reports evidence only"
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--plan", type=Path, required=True)
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("refusing to overwrite an existing proof artifact")
    plan = load(args.plan)
    if args.plan.resolve() != Path(plan["selection_plan_path"]).resolve():
        parser.error("plan argument differs from its explicitly recorded selection-plan path")
    result = collect(plan, args.repo.resolve())
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    print(json.dumps({"status": result["status"], "pending": result["pending_gates"], "errors": result["validation_errors"]}))
    return 0 if result["status"] == "passed" else 1


if __name__ == "__main__":
    sys.exit(main())
