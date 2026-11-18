#!/usr/bin/env python3
"""Archive only explicitly finalized through-198 evidence, preserving original bytes."""
import argparse
import gzip
import hashlib
import json
import posixpath
from pathlib import Path
import re
import shutil
import struct

SAFE = re.compile(r"[A-Za-z0-9_.-]+\Z")


def measured(stream):
    value, count = hashlib.sha256(), 0
    for block in iter(lambda: stream.read(1024 * 1024), b""):
        value.update(block)
        count += len(block)
    return value.hexdigest(), count


def identity(path):
    with path.open("rb") as stream:
        return measured(stream)


def regular(path):
    if not path.is_file() or path.is_symlink():
        raise ValueError(f"missing regular nonsymlink evidence: {path}")
    return path


def safe(value):
    if not isinstance(value, str) or not SAFE.fullmatch(value):
        raise ValueError(f"unsafe artifact label: {value!r}")
    return value


def compact(path, value):
    path.write_text(json.dumps(value, sort_keys=True, separators=(",", ":"), allow_nan=False) + "\n")


def additions(path):
    data = path.read_bytes()
    if b"\0" in data[:8000]:
        return 0, True
    return data.count(b"\n") + int(bool(data) and not data.endswith(b"\n")), False


def collector_result(path, plan, repo):
    scope = {"__name__": "blcad_packaging_collector", "__file__": str(path)}
    # No tests execute and no repository bytecode files are written.
    exec(compile(regular(path).read_text(), str(path), "exec"), scope)
    return scope["collect"](plan, repo)


def require_passed(proof):
    if proof.get("schema") != "blcad.through198-final-proof.v2" or proof.get("status") != "passed":
        raise ValueError("only a finalized passed collector result can be acceptance evidence")
    if proof.get("validation_errors") or proof.get("pending_gates"):
        raise ValueError("collector validation errors or pending gates remain")
    if proof.get("retained_profile_transition", {}).get("status") != "verified":
        raise ValueError("v2 requires the verified retained optional-profile transition")
    if proof.get("gui_default_execution", {}).get("status") != "complete":
        raise ValueError("v2 requires complete disjoint mixed-platform default GUI coverage")
    for name, suite in proof["suites"].items():
        if suite.get("status") != "passed" or suite.get("failed") or suite.get("failed_assertions"):
            raise ValueError(f"suite is not finalized acceptance: {name}")
        if suite["completion"]["exit_code"] != 0:
            raise ValueError(f"suite has no completed zero exit: {name}")
        if "interrupted" in Path(suite["log"]["path"]).name:
            raise ValueError(f"interrupted log is ineligible: {name}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--plan", type=Path, required=True)
    parser.add_argument("--proof", type=Path, required=True, help="final collector JSON, unmodified")
    parser.add_argument("--collector", type=Path, default=Path("/tmp/blcad-collect-through198-proof.py"))
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True, help="fresh package directory outside the repository")
    parser.add_argument("--repo-prefix", default="docs/evidence/through198")
    parser.add_argument("--manifest-lines", type=int, default=1414)
    parser.add_argument("--capture-picks", type=Path, help="optional scale→PNG-name arrays; defaults final plan required_files")
    args = parser.parse_args()
    repo, output = args.repo.resolve(), args.output.resolve()
    if output.is_relative_to(repo) or (output.exists() and any(output.iterdir())):
        parser.error("output must be new/empty and outside the repository")
    prefix = Path(args.repo_prefix)
    if prefix.is_absolute() or ".." in prefix.parts:
        parser.error("repository artifact prefix must be relative and cannot escape")
    plan = json.loads(regular(args.plan).read_text())
    proof = json.loads(regular(args.proof).read_text())
    if args.plan.resolve() != Path(plan["selection_plan_path"]).resolve():
        raise ValueError("plan argument differs from the collector's explicitly selected plan")
    require_passed(proof)
    current = collector_result(args.collector.resolve(), plan, repo)
    require_passed(current)
    if current != proof:
        raise ValueError("selected sources/plan/binaries differ from the final collector result")
    evidence_root = Path(plan["evidence_root"]).resolve()
    picks = json.loads(args.capture_picks.read_text()) if args.capture_picks else {
        str(scale): entry.get("required_files", []) for scale, entry in plan["native_captures"].items()}
    if not isinstance(picks, dict) or set(picks) - set(plan["native_captures"]):
        raise ValueError("capture selection contains an unsupported scale")
    output.mkdir(parents=True, exist_ok=True)
    artifacts, aliases = [], {}
    used_targets = set()

    def record(source, destination, category, expected=None, compress=False):
        source = regular(Path(source)).resolve()
        if compress and source.suffix not in (".log", ".txt"):
            raise ValueError("only finalized logs/registry text may be compressed; never code or manifest")
        original_hash, original_bytes = identity(source)
        if expected and (original_hash, original_bytes) != (expected["sha256"], expected["bytes"]):
            raise ValueError(f"source differs from final proof: {source}")
        if destination in used_targets:
            old = next(item for item in artifacts if item["path"] == destination)
            if (old["original_sha256"], old["original_bytes"]) != (original_hash, original_bytes):
                raise ValueError(f"conflicting artifact target: {destination}")
            aliases[str(source)] = destination
            if source.is_relative_to(evidence_root):
                aliases[str(source.relative_to(evidence_root))] = destination
            return old
        target = output / destination
        if not target.resolve().is_relative_to(output):
            raise ValueError("artifact target escapes package")
        target.parent.mkdir(parents=True, exist_ok=True)
        if compress:
            with source.open("rb") as incoming, target.open("wb") as raw:
                with gzip.GzipFile(filename="", mode="wb", fileobj=raw, mtime=0, compresslevel=9) as outgoing:
                    shutil.copyfileobj(incoming, outgoing, 1024 * 1024)
            with gzip.open(target, "rb") as incoming:
                if measured(incoming) != (original_hash, original_bytes):
                    raise ValueError(f"lossless gzip assertion failed: {source}")
        else:
            if source != target.resolve():
                shutil.copyfile(source, target)
            if identity(target) != (original_hash, original_bytes):
                raise ValueError(f"unmodified copy assertion failed: {source}")
        stored_hash, stored_bytes = identity(target)
        lines, binary = additions(target)
        entry = {"path": destination, "category": category, "encoding": "gzip" if compress else "identity",
                 "sha256": stored_hash, "bytes": stored_bytes, "insertions": lines, "git_binary": binary,
                 "original_path": str(source), "original_sha256": original_hash, "original_bytes": original_bytes}
        if target.suffix == ".png":
            with target.open("rb") as stream:
                header = stream.read(24)
            if header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
                raise ValueError("selected capture is not an original PNG")
            width, height = struct.unpack(">II", header[16:24])
            if not 0 < width <= 1920 or not 0 < height <= 1080:
                raise ValueError("capture dimensions differ from the recorded native desktop")
            entry.update(width_px=width, height_px=height)
        artifacts.append(entry)
        used_targets.add(destination)
        aliases[str(source)] = destination
        if source.is_relative_to(evidence_root):
            aliases[str(source.relative_to(evidence_root))] = destination
        return entry

    def process(entry):
        source = entry.get("process_json")
        if source:
            path = Path(source)
            name = "provenance/process-" + identity(regular(path))[0][:16] + ".json"
            record(path, name, "supervision")

    runtime = record(plan["runtime"]["path"], "runtime/runtime-manifest.json", "runtime", proof["runtime"])
    if runtime["insertions"] != args.manifest_lines or runtime["insertions"] > 1500:
        raise ValueError("exact uncompressed runtime manifest line count differs from the reviewed standalone group")
    record(args.plan, "proof/selected-plan.json", "proof", proof["selection_plan"])
    record(args.proof, "proof/original-collector.json", "proof")
    record(args.collector, "tools/collect-through198-proof.py", "tools", proof["collector"])
    record(Path(__file__), "tools/package-through198-proof.py", "tools")
    verifier = Path(__file__).with_name("verify_proof_artifacts.py")
    record(verifier, "tools/verify-proof-artifacts.py", "tools")
    if args.capture_picks:
        record(args.capture_picks, "proof/capture-selection.json", "proof")
    transition = plan["retained_profile_transition"]
    transition_proof = proof["retained_profile_transition"]
    record(transition["record"], "profile-only-retained-proof.json", "retained-profile",
           transition_proof["record"])
    before_runtime = record(transition["before_runtime"], "runtime/runtime22-manifest.json",
                            "runtime-retained", transition_proof["before_runtime"])
    if before_runtime["insertions"] != args.manifest_lines or before_runtime["insertions"] > 1500:
        raise ValueError("retained exact runtime manifest differs from its standalone line group")
    # The after manifest is the same exact current inventory already archived.
    record(transition["after_runtime"], "runtime/runtime-manifest.json", "runtime",
           transition_proof["after_runtime"])
    record(transition["before_binaries"], "provenance/before-profile-binaries.json", "retained-profile",
           transition_proof["before_binaries"])
    record(transition["after_binaries"], "provenance/after-profile-binaries.json", "retained-profile",
           transition_proof["after_binaries"])
    origin_rows = []
    selected_origins = transition.get("retained_artifact_origins", [])
    proved_origins = transition_proof["retained_artifact_origins"]
    if len(selected_origins) != len(proved_origins):
        raise ValueError("retained origin inventory differs from the finalized proof")
    for selected, proved in zip(selected_origins, proved_origins):
        if selected["name"] != proved["name"] or selected["artifacts"] != proved["artifacts"]:
            raise ValueError("retained origin attribution differs from the finalized proof")
        target = "provenance/origin-binaries-" + proved["snapshot"]["sha256"][:16] + ".json"
        record(selected["snapshot"], target, "retained-profile", proved["snapshot"])
        origin_rows.append({"name": selected["name"], "snapshot": target,
                            "original_snapshot_record": proved["snapshot"],
                            "artifacts": selected["artifacts"]})
    origins = {"schema": "blcad.through198-retained-origin-map.v1",
               "source_selection": "proof/selected-plan.json", "source_collector": "proof/original-collector.json",
               "before_runtime": "runtime/runtime22-manifest.json", "after_runtime": "runtime/runtime-manifest.json",
               "before_binaries": "provenance/before-profile-binaries.json",
               "after_binaries": "provenance/after-profile-binaries.json",
               "changed_inputs": transition_proof["changed_runtime_inputs"],
               "changed_artifacts": transition_proof["changed_artifacts"],
               "unchanged_artifacts": transition_proof["unchanged_artifacts"],
               "origins": origin_rows, "boundary": transition_proof["boundary"]}
    compact(output / "provenance/retained-artifact-origins.json", origins)
    record(output / "provenance/retained-artifact-origins.json",
           "provenance/retained-artifact-origins.json", "retained-profile")
    for name, entry in plan["builds"].items():
        record(entry["log"], f"logs/build-{safe(name)}.log.gz", "logs", proof["builds"][name]["log"], True)
        process(entry)
    for name, entry in plan["suites"].items():
        record(entry["log"], f"logs/{safe(name)}.log.gz", "logs", proof["suites"][name]["log"], True)
        process(entry)
    registry = plan["gui_registry"]
    for name in ("default", "broad", "housing", "audit"):
        record(registry[name], f"registry/{name}.txt.gz", "registry", proof["gui_registry"]["groups"][name], True)
    record(registry["partition"], "registry/partition.json", "registry", proof["gui_registry"]["partition"])
    for entry in plan["binaries"].values():
        if entry.get("saved_identity"):
            path = Path(entry["saved_identity"])
            if str(path.resolve()) not in aliases:
                record(path, "provenance/binary-identities-" + identity(regular(path))[0][:16] + ".json", "supervision")
    for scale, names in picks.items():
        if not isinstance(names, list) or len(set(names)) != len(names):
            raise ValueError("capture names must be a unique list per scale")
        by_name = {Path(item["path"]).name: item for item in proof["native_captures"][scale]["files"]}
        for name in names:
            safe(name)
            if not name.endswith(".png") or name not in by_name:
                raise ValueError(f"capture is not in finalized proof: {scale}/{name}")
            source = Path(plan["native_captures"][scale]["directory"]) / name
            record(source, f"native/{safe(scale)}/{name}", "native-" + scale, by_name[name])
    startup, startup_proof = plan["startup"], proof["startup"]
    record(startup["record"], "startup/startup.json", "startup", startup_proof["record"])
    record(startup["log"], "startup/startup.log.gz", "startup", startup_proof["log"], True)
    record(startup["capture"], "startup/startup.png", "startup", startup_proof["capture"])

    def portable(value):
        if isinstance(value, list):
            return [portable(item) for item in value]
        if not isinstance(value, dict):
            return value
        result = {key: portable(item) for key, item in value.items()}
        if isinstance(value.get("path"), str) and "sha256" in value and "bytes" in value:
            destination = aliases.get(value["path"])
            if destination:
                artifact = next(item for item in artifacts if item["path"] == destination)
                result.update(path=destination, sha256=artifact["sha256"], bytes=artifact["bytes"],
                              encoding=artifact["encoding"], original_record={key: value[key] for key in ("path", "sha256", "bytes")})
                if artifact["encoding"] == "gzip":
                    result.update(uncompressed_sha256=artifact["original_sha256"], uncompressed_bytes=artifact["original_bytes"])
            else:
                result["artifact_storage"] = "identity only; executable or unselected capture not included"
        return result

    durable = portable(proof)
    durable.update(schema="blcad.through198-portable-proof.v2", artifact_root="..",
                   original_collector="original-collector.json", artifact_index="../artifact-index.json")
    compact(output / "proof/proof.json", durable)
    path = output / "proof/proof.json"
    record(path, "proof/proof.json", "derived")
    # Human links resolve from README at package root, independent of /tmp.
    mvp_link = posixpath.relpath("docs/mvp-plan.md", str(prefix))
    p01_link = posixpath.relpath("docs/benchmarks/through198-state-assembly/derived/p01-summary.md", str(prefix))
    totals = proof["gui_default_execution"]["totals"]
    lines = ["# Through198 finalized verification evidence", "",
             "[Portable proof](proof/proof.json) · [original collector output](proof/original-collector.json) · "
             "[selected plan](proof/selected-plan.json) · [exact runtime inventory](runtime/runtime-manifest.json)", "",
             "Only explicitly selected completed passing runs are acceptance evidence. Failed, interrupted and diagnostic runs are excluded. "
             f"[The MVP plan]({mvp_link}) owns numbered status; [P01 measurements]({p01_link}) are separate.", "",
             f"The default GUI registry has **{totals['cases']} cases: {totals['passed']} passed, {totals['skipped']} skipped, "
             f"{totals['assertions']} assertions** across disjoint offscreen broad, native housing100 and offscreen audit partitions. "
             f"This is mixed-platform coverage, not {totals['cases']} passing cases or a complete native suite. The focused native 8 plus connected 2 cases per scale "
             "prove the required bounded scenarios; overlapping focused rows below must not be added to the default totals.", "",
             "[Retained profile transition](profile-only-retained-proof.json) · "
             "[runtime22 inventory](runtime/runtime22-manifest.json) · [before binary identities](provenance/before-profile-binaries.json) · "
             "[after binary identities](provenance/after-profile-binaries.json) · [retained origin map](provenance/retained-artifact-origins.json)", "",
             "Functional/native evidence keeps its recorded runtime22 attribution, with the earlier Core execution retained by exact build20 byte identity. "
             "Runtime23/build28 changes only the optional profiler fixture/executable; no functional runtime23 rerun is implied. "
             "The advanced Part portion of X01 and partial-198 X02 retain their complete 204 boundaries; the 199–211 requirements remain planned in the MVP plan.", "",
             "| Suite | Cases | Passed | Skipped | Assertions | Platform / scale | Finalized raw log |",
             "| --- | ---: | ---: | ---: | ---: | --- | --- |"]
    for name, suite in proof["suites"].items():
        lines.append(f"| {name} | {suite['cases']} | {suite['passed']} | {suite['skipped']} | {suite['assertions']} | {suite.get('platform')} / {suite.get('scale') or '—'} | [gzip](logs/{name}.log.gz) |")
    lines += ["", "Both exact current and retained runtime manifests are uncompressed and occupy separate standalone staging groups. Logs and registry lists alone use lossless gzip; decompression is checked against the original SHA-256 and byte count. No source code is compressed. PNGs are copied without image editing or recompression.", "",
              "| GUI registry | Exact count | Original list |", "| --- | ---: | --- |"]
    for name, entry in proof["gui_registry"]["groups"].items():
        lines.append(f"| {name} | {entry['count']} | [gzip](registry/{name}.txt.gz) |")
    lines += ["", "[Registry partition](registry/partition.json) records the disjoint union; suite case counts are not inferred from partial filters.", "",
              "| Native scale | Selected original captures |", "| --- | --- |"]
    for scale, names in picks.items():
        links = " · ".join(f"[{name}](native/{scale}/{name})" for name in names)
        actual_scale = str(proof["native_captures"][scale]["scale"])
        label = scale if actual_scale == scale else f"{actual_scale} ({scale})"
        lines.append(f"| {label} | {links or 'none selected'} |")
    lines += ["", "Capture hashes complement executed native assertions. Images alone do not prove interaction or geometry. Unselected captures retain their original hash records but are not claimed to be included here.", "",
              "Native startup: [completed record](startup/startup.json), [raw log gzip](startup/startup.log.gz), [original capture](startup/startup.png). The record requires a healthy visible window and a clean WM_DELETE_WINDOW close.", "",
              "Build logs and completed process records are linked from the portable proof and retained in logs/ and provenance/. Executable bytes are not included; their exact SHA-256 identities remain in both proof records. Original /tmp paths are provenance labels, not required artifact links.", "",
              "Verify every stored artifact and every decompressed original after /tmp has gone:", "",
              "```sh", "python3 tools/verify-proof-artifacts.py .", "```", "",
              "[Artifact index](artifact-index.json) stores original/compressed hashes and bytes. [Staging plan](staging-groups.json) keeps each group at≤1500 numeric insertions; binaries are explicitly marked. Inspect the actual staged diff and numstat before each commit, including unrelated staged work. This archive contains no timing or performance claim.", ""]
    (output / "README.md").write_text("\n".join(lines))
    record(output / "README.md", "README.md", "derived")
    index = {"schema": "blcad.through198-artifact-index.v1", "repository_prefix": str(prefix),
             "policy": "all copied bytes unchanged; gzip originals verified by decompression; no code compression",
             "unindexed_self_records": ["artifact-index.json", "staging-groups.json"], "artifacts": artifacts}
    compact(output / "artifact-index.json", index)
    groups = []
    categories = ["runtime", "runtime-retained", "proof", "retained-profile", "tools", "supervision", "registry", "logs"]
    categories += sorted({item["category"] for item in artifacts if item["category"].startswith("native-")})
    categories += ["startup", "derived"]
    for category in categories:
        entries = [item for item in artifacts if item["category"] == category]
        bucket, total, ordinal = [], 0, 1
        for entry in entries:
            count = entry["insertions"]
            if count > 1500:
                raise ValueError(f"one uncompressed artifact exceeds1500 additions; no code/raw rewriting allowed: {entry['path']}")
            if bucket and total + count > 1500:
                groups.append({"name": f"{category}-{ordinal}", "insertions": total, "files": bucket})
                bucket, total, ordinal = [], 0, ordinal + 1
            bucket.append(str(prefix / entry["path"]))
            total += count
        if bucket:
            groups.append({"name": f"{category}-{ordinal}", "insertions": total, "files": bucket})
    groups.append({"name": "artifact-index-and-staging", "insertions": 2,
                   "files": [str(prefix / "artifact-index.json"), str(prefix / "staging-groups.json")]})
    compact(output / "staging-groups.json", {"schema": "blcad.through198-staging-groups.v1", "limit": 1500,
            "source_manifest_standalone": True, "retained_source_manifest_standalone": True, "groups": groups,
            "policy": "new-file estimates only; inspect actual git diff --cached --numstat before each commit"})
    print(json.dumps({"status": "packaged", "artifacts": len(artifacts), "groups": [
        {"name": group["name"], "insertions": group["insertions"]} for group in groups]}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
