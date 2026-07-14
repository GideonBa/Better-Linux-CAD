# Cylindrical Joint Family — MVP-5

Status: implemented as Block 45 of `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

## Frozen intent and JSON contract

`Cylindrical` is available for local `AssemblyDocument` and Project-level occurrence-qualified
joints. Its family-ordered coordinate signature is:

1. `translation` — `linear`, signed `LinearDisplacementMm`, bounded in `mm`
2. `rotation` — `angular`, bounded `AngleDeg` in the principal `[-180, 180]` range

Both slots require lower and upper limits. Canonical JSON uses `"type": "cylindrical"` and the
Block-42 `coordinates[]` record. Revolute legacy scalar fields are neither written nor accepted.

## Target and residual contract

Both targets must expose an oriented `Frame`; target A defines positive Z translation and positive
right-handed twist. Axis-only targets remain incompatible because they provide no deterministic
reference X direction.

Local and root-space builders share this canonical residual row order:

1. Z-direction alignment (3 dimensionless rows)
2. transverse axis offset (3 length rows)
3. signed axial translation error (1 length row)
4. signed periodic twist sine (1 dimensionless row)
5. signed periodic twist cosine (1 locally redundant anti-ambiguity row)

The nine-row system constrains all six rigid transform variables to the two requested coordinate
values. It uses the existing finite-difference Jacobian and numeric optimizer.

## Vector drives, holding, freshness, and application

The internal numeric joint drive now carries the complete family-ordered coordinate vector.
Requests may address one or both roles in any order; validation restores `translation, rotation`
order and fills omitted roles from authored values. Every coordinate of non-selected active joints
is likewise held at its authored value.

Complete coordinate-slot snapshots and explicit requested roles remain the freshness authority.
Local and cross-hierarchy appliers update proposed transforms and exactly the explicitly requested
selected roles on a Project copy before committing atomically. Scalar degree APIs remain
Revolute-only adapters.

## Acceptance coverage

- `[core][assembly-cylindrical-joint]`
- `[core][assembly-cross-hierarchy-cylindrical-joint]`
- `[geometry][assembly-cylindrical-joint]`
- `[geometry][assembly-cross-hierarchy-cylindrical-motion]`

## Block 46 handoff

Block 46 Planar and Block 47 passive Spherical are implemented in their canonical documents.
Blocks 48–86 Part Construction through 3D Sketch JSON and semantic references are implemented;
Block 78 3D Sketch Geometry conversion is implemented; Block 79 connected PathCurve Core intent, JSON, and validation is implemented; Block 80 Sweep feature Core intent and JSON is implemented; Block 81 Basic Sweep Geometry is implemented; Block 82 3D path, twist, and guide-controlled Sweep is implemented; Block 83 Path-following Extrude and Extruded Cut is implemented; Block 84 ProfileSectionReference and Loft Core intent plus JSON is implemented; Block 85 Two-section Loft Geometry on arbitrary planes is implemented; Block 86 Multi-section Loft is implemented; Block 87 Guided and continuity-controlled Loft is next.
