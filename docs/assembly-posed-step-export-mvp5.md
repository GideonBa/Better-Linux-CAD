# Posed Assembly STEP Export MVP-5

Status: implemented the first derived end-to-end assembly geometry deliverable. A project assembly can now recompute referenced part geometry, pose component occurrences with persisted rigid transforms, compose one OCCT compound, and write one STEP file.

## Export boundary

`AssemblyStepExporter` is a geometry-layer service. It consumes a `const Project&` and never changes assembly or part model intent.

The build path is:

```text
Project
  -> validate assembly/member/component structure
  -> collect referenced part documents in lexicographic id order
  -> recompute each referenced part once into one ShapeCache per part document
  -> collect visible, non-suppressed component occurrences in lexicographic id order
  -> copy and pose each occurrence's final part shape
  -> add posed shapes to one OCCT compound
  -> GeometryShape
  -> existing StepExporter
  -> STEP file
```

Repeated component occurrences reuse the same recomputed part cache. They receive independent transformed shape copies before compound composition.

## Transform contract

Assembly export deliberately does not define another placement convention. OCCT shape placement follows the exact persisted `RigidTransform` semantics already owned by `AssemblyTransformEvaluator`:

```text
rotation units: degrees
rotation type: active right-handed fixed-axis
application order: X, then Y, then Z
translation units: millimeters
final order: rotate X -> rotate Y -> rotate Z -> translate
```

The implementation applies four explicit OCCT transforms in that order. This avoids relying on implicit matrix-composition order and keeps exported geometry aligned with solved target evaluation.

## Visibility and suppression

Only components satisfying both conditions enter the compound:

```text
visibility == Visible
suppression_state == Active
```

Hidden and suppressed occurrences are skipped deterministically. They remain persistent project intent and are not mutated by export.

## Failure policy

The exporter fails closed before writing usable output when:

- project assembly structure does not resolve member or component part documents;
- a referenced part recompute fails;
- a referenced part recomputes without a final shape;
- an OCCT rigid transform fails or produces an empty shape;
- no visible active component remains for the posed compound;
- the existing STEP writer rejects transfer or file output.

Errors from project validation, recompute, and the existing STEP writer are preserved. Assembly-specific geometry/export failures use `geometry.assembly_step_exporter`.

## Derived-data rule

The following are derived and unpersisted:

- per-export `ShapeCache` instances;
- recomputed part final shapes;
- transformed component shape copies;
- the posed OCCT compound;
- `AssemblyStepExportSummary`.

No assembly geometry cache, STEP entity identity, or transformed BRep is written into `Project`, `AssemblyDocument`, or `ComponentInstance`.

## Headless end-to-end example

`blcad_export_posed_assembly` executes the full current assembly path:

```text
read_project_json_file
  -> AssemblyConstraintGraph::build
  -> select one connected multi-component group
  -> AssemblyRigidBodySolver::solve
  -> AssemblySolveResultApplier::apply
  -> AssemblyStepExporter::write_step
```

Checked-in input:

```text
examples/posed_assembly.blcad.project.json
```

Example command:

```bash
./build/dev-geometry/blcad_export_posed_assembly \
  examples/posed_assembly.blcad.project.json \
  build/posed_assembly.step
```

The example contains two occurrences of one rectangular plate. One occurrence is grounded; the second starts above it. A planar Mate is solved and explicitly applied before both posed occurrences are exported as one STEP compound.

## Proven behavior

`tests/geometry/assembly_step_exporter_tests.cpp` covers:

- two differently posed instances of one recomputed part;
- one `ShapeCache` per referenced part document and reuse across occurrences;
- compound solid count and total volume for repeated instances;
- exported STEP bounding-box correctness for Z rotation followed by translation;
- hidden and suppressed occurrence exclusion;
- deterministic repeated STEP `DATA` sections;
- failure on an unresolvable project member;
- failure when a referenced part recomputes without a final shape.

## Still not implemented

- STEP assembly product structure with named occurrences and hierarchy; the seed exports one geometric compound;
- component geometry instancing instead of transformed shape copies;
- joints, limits, and motion;
- rigid or flexible subassemblies;
- collision/interference analysis.
