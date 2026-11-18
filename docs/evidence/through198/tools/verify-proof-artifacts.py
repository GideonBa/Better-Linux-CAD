#!/usr/bin/env python3
"""Verify packaged through-198 evidence without its original /tmp paths."""
import argparse
import gzip
import hashlib
import json
from pathlib import Path


def measured(stream):
    value, count = hashlib.sha256(), 0
    for block in iter(lambda: stream.read(1024 * 1024), b""):
        value.update(block)
        count += len(block)
    return value.hexdigest(), count


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package", type=Path, help="directory containing artifact-index.json")
    args = parser.parse_args()
    root = args.package.resolve()
    index = json.loads((root / "artifact-index.json").read_text())
    if index.get("schema") != "blcad.through198-artifact-index.v1":
        raise ValueError("unsupported artifact-index schema")
    for entry in index["artifacts"]:
        path = root / entry["path"]
        if path.is_symlink() or not path.resolve().is_relative_to(root) or not path.is_file():
            raise ValueError(f"missing regular artifact: {entry['path']}")
        with path.open("rb") as stream:
            actual = measured(stream)
        if actual != (entry["sha256"], entry["bytes"]):
            raise ValueError(f"stored artifact differs: {entry['path']}")
        if entry["encoding"] == "gzip":
            with gzip.open(path, "rb") as stream:
                actual = measured(stream)
            if actual != (entry["original_sha256"], entry["original_bytes"]):
                raise ValueError(f"decompressed bytes differ: {entry['path']}")
    print(f"verified {len(index['artifacts'])} artifacts and all gzip originals")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
