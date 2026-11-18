# Headless state and nested Assembly baseline

This opt-in P01 harness measures synchronous Core/Geometry calls for the
States and nested flexible Assembly work in Blocks 189–197. It does not establish
native UI latency, a portable performance budget, or numbered acceptance.
`docs/mvp-plan.md` remains the implementation-status authority; Block 203 owns
later cross-platform thresholds.

`tests/geometry/variant_assembly_profile.cpp` generates scenes through the existing
public document APIs and the small `part_history_fixture.hpp` native Part fixture.
The executable is excluded from the default build and from CTest. Build it
explicitly in an existing configured native geometry build:

```sh
cmake --build build --target blcad_variant_assembly_profile -j8
python3 scripts/run_variant_assembly_profile.py \
  --repo . --executable build/blcad_variant_assembly_profile \
  --output /tmp/blcad-state-assembly-baseline --timeout 600
```

Summarize a completed real series without modifying its raw files:

```sh
python3 scripts/summarize_variant_assembly_profile.py \
  /tmp/blcad-state-assembly-baseline --source-root . \
  --output-dir /tmp/blcad-state-assembly-summary
```

The summarizer verifies source/artifact hashes, three cold and thirty warm samples per metric,
RSS provenance and incomplete/timeout groups before publishing derived statistics. An absent
sample array remains unknown; it is never interpreted as an observed zero samples.

Use the actual configured build-directory path. The runner requires a new or
empty output directory and runs every process sequentially. `--mode states` or
`--mode assembly` and `--size` isolate a published size. Its default three
separate processes per size each record one first sample and ten warm samples.
`--processes 1` is available for correctness smoke checks and marks the series
incomplete for publication. A per-process deadline includes scene setup and
correctness checks. Timeout is an explicit exit-124 failure artifact, including
the costly 1000-leaf Assembly case; sizes are never silently skipped.

States use 2/10/50 named sparse siblings of one native rectangular Part. Width
overrides drive the inherited height formula `width / 2`, with depth 8 mm.
Consecutive siblings have equal geometry and distinct state identity and pinned
material density (4000 or 6000 kg/m³). The independent volume oracle is
`4 * width²`. Each state owns a persistent ShapeCache. The first geometry batch
starts with empty caches; warm batches reuse those same separate caches.
Effective-state evaluation and Geometry execution are separate batch timings.
Outside the clock, every result must match the analytic volume, reject the
equal-valued sibling and Primary freshness contexts, and leave authored Primary
unchanged. Initial physical inspections also check each pinned density and
physical-snapshot identity outside the timed sections. A retained Primary Geometry cache is created
outside timing after the first state batch, preserving first kernel use; its
1600 mm³ result and distinct Primary freshness must survive every warm batch.
Core state-catalog validation occurs during setup, so the first evaluation
batch is not claimed to be the first ever internal Core materialization.

Assemblies contain 10/100/1000 leaf occurrences. A root repeats one wrapper;
the wrapper repeats one two-component module. Every module occurrence has an
independent flexible pose for its moving leaf. All leaves reference one native
2 × 2 × 1 mm Part with density 4000 kg/m³. Root rotations/translations, the
wrapper placement and varying local extensions make lost hierarchy levels or
shared-definition pose leakage observable. Resolver and physical inspection
are separate timings. The first physical inspection has no previous snapshot;
warm inspections receive the preceding immutable snapshot. Outside the clock,
the oracle checks each composed position with independent Z-rotation math,
total volume `4 * leaf_count`, mass `4 * leaf_count * 4e-6` kg, the independently
summed centroid, snapshot freshness, and unchanged Project/module/wrapper
intent. This baseline does not measure occurrence-local solve/motion, flat or
structured STEP export, or native presentation; raw records name these gaps.

Cold means the first measured call or batch in a new process after public-API
fixture validation, with no preceding Geometry execution. It does not mean an
OS/filesystem cache flush. For a state batch only its first state is the first
kernel use in that process. Every raw record retains all 11 timings, executed
feature/reuse counts where available, and warm sample count, min/median/nearest-rank p95/max values.
For ten warm samples the nearest-rank p95 is the tenth sorted value. Setup,
source serialization, cache/snapshot copying and the independent checks are
excluded from the corresponding timings; full process elapsed time remains
available separately.

Each process also records a positive `peak_rss_kib` from Linux
`getrusage(RUSAGE_SELF).ru_maxrss`. It is observed after scene generation, all timed
batches and independent checks, before source/result artifact serialization and
publication. This is process-lifetime peak resident memory in KiB, including
provenance reads, setup, caches/snapshots and checks, without baseline subtraction;
it is not a delta or an isolated per-call allocation measurement. Missing,
nonpositive or invalid RSS makes the process/runner record fail instead of
publishing a passed sample with absent memory evidence.

The runner writes a runtime manifest containing SHA-256 digests of the actual
working-tree sources (including untracked runtime files), Git identity/diff,
selected CMake configuration, the executable and its dynamic libraries. The
executable verifies its actual `/proc/self/exe` and manifest digests. Each passed
process writes the complete generated source intent as a JSON sidecar and
retains its digest, compiler/build/OCCT/configured-Qt metadata, OS/kernel/CPU,
memory and thread count. The runner verifies immutable sources and binary
before/after the series and preserves stdout/stderr, failures and source
sidecars. Large source sidecars and the source manifest use compact JSON so
evidence remains practical within the repository's commit-size limit. It creates
measurements only by executing the actual benchmark;
no checked-in measurement is implied by this source contract.

The [published through-198 baseline](benchmarks/through198-state-assembly/derived/p01-summary.md)
summarizes the completed six-size [series](benchmarks/through198-state-assembly/raw/series.json)
and its unchanged raw records. In that Debug series, warm
state calls execute every feature; retained cache objects do not establish BRep cache hits or
native state-activation latency. The recorded RSS includes executable/provenance hashing.
