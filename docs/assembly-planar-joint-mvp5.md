# Planar Joint Family — MVP-5

Status: implemented as Block 46 of `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

## Frozen intent and JSON contract

`Planar` is available for local and Project-level occurrence-qualified joints. Its bounded,
family-ordered coordinate signature is:

1. `translation_u` — Linear displacement in `mm`
2. `translation_v` — Linear displacement in `mm`
3. `rotation_normal` — Angular displacement in `deg`, within the principal range

All three slots require lower and upper limits. Canonical JSON uses `"type": "planar"` and the
Block-42 `coordinates[]` record. Revolute legacy scalar fields are not accepted or emitted.

## Target and equation contract

Both endpoints must expose an oriented `Frame`. Target A defines positive U through X, positive V
through Y, and positive normal rotation around Z. Axis-only targets fail closed.

Local and root-space builders share this canonical eight-row residual order:

1. normal alignment `zA - zB` (3 dimensionless rows)
2. normal separation `dot(oB - oA, zA)` (1 length row)
3. U translation error (1 length row)
4. V translation error (1 length row)
5. signed normal-rotation sine (1 dimensionless row)
6. signed normal-rotation cosine (1 locally redundant anti-ambiguity row)

At a regular state these rows constrain six rigid-transform variables to the requested three
in-plane coordinates without adding transform variables or a second optimizer.

## Drives, holding, freshness, and application

Role-addressed requests may contain one, two, or all three roles in any order. Validation restores
the canonical family order and fills omitted selected roles from authored values. Every role of
other active joints is held likewise. Complete slot snapshots and explicit requested roles remain
the freshness authority, and local/cross-hierarchy application changes transforms plus exactly the
requested selected roles atomically.

## Acceptance coverage

- `[core][assembly-planar-joint]`
- `[core][assembly-cross-hierarchy-planar-joint]`
- `[geometry][assembly-planar-joint]`
- `[geometry][assembly-cross-hierarchy-planar-motion]`

## Block 47 handoff — completed

Block 47 Ball/Spherical is implemented in `docs/assembly-spherical-joint-mvp5.md` as passive
Point/Point center coincidence with explicit selected-drive rejection. Blocks 48–91 Part
Construction through persistent ordered Loft intent are implemented; Block 85 Two-section Loft
Geometry on arbitrary planes is next.
