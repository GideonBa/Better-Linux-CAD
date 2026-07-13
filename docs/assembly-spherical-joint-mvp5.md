# Spherical Joint Family — MVP-5

Status: implemented as Block 47 of `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

## Frozen intent and JSON contract

`Spherical` is available for local and Project-level occurrence-qualified joints. It is a passive
joint family with no authored coordinate slots. Canonical JSON uses:

```json
{
  "type": "spherical",
  "coordinates": []
}
```

The spelling `spherical` is the only accepted family spelling. Revolute legacy `limits` and
`coordinate` fields are neither accepted nor emitted, and any non-empty coordinate list fails
validation.

## Target and equation contract

Both endpoints must expose `Point`. `ConstructionPoint`, generated vertices, and generated circular
edge centers are valid Point producers; `.seat` is not implicitly projected to its Frame origin.
Compatibility therefore remains capability-driven without hidden source-family conversions.

Local component-space targets and occurrence-qualified targets are both evaluated in assembly root
space. The residual has exactly three length rows, in X/Y/Z order:

```text
center_offset_mm = point_b - point_a
```

Zero residual means that both selected centers coincide. The relationship introduces no transform
variables and reuses the shared six-variable rigid-body optimizer.

## Motion closure, freshness, and application

Every active Spherical joint participates in local and cross-hierarchy motion connectivity. When
another joint is selected, Spherical contributes its three center-coincidence rows as a passive
relationship. Inactive or suppressed relationships retain the existing graph exclusion rules.

Spherical cannot be selected as a drive. Both public motion solvers reject that request explicitly,
including an empty role-addressed drive. A driven ball orientation remains deferred until a
singularity-aware coordinate representation and ordering contract is frozen.

Motion results retain the existing relationship, hierarchy-boundary, authority-transform, semantic
PartDocument, state, target, and complete coordinate-slot freshness checks. The Spherical slot
snapshot is empty by definition. Application may commit transforms produced by a different selected
joint, but never writes a Spherical coordinate.

## Acceptance coverage

- `[core][assembly-spherical-joint]`
- `[core][assembly-cross-hierarchy-spherical-joint]`
- `[geometry][assembly-spherical-joint]`
- `[geometry][assembly-cross-hierarchy-spherical-joint]`

## Assembly MVP handoff

Block 47 completes the planned Assembly target/relationship/joint sequence. The next technical step
is Block 48 — Body identity and `PartDocument` ownership — in
`docs/part-construction-sequence-mvp6.md`.
