# Prismatic Joint Family — MVP-5

Status: implemented as Block 44 of `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

## Frozen intent contract

`Prismatic` is a persistent, solver-independent joint family at both local `AssemblyDocument`
and Project-level occurrence-qualified scope. It owns exactly one coordinate slot:

- role: `translation`
- kind: `linear`
- quantity/unit: signed `LinearDisplacementMm` / `mm`
- both lower and upper limits are required

Canonical JSON uses `"type": "prismatic"` and the Block-42 `coordinates[]` representation.
The historical scalar `limits` and `coordinate` fields remain Revolute-only and are neither
written nor accepted for Prismatic.

## Target and equation contract

Both endpoints must expose an oriented `Frame`; Axis-only targets fail closed because they do
not define the reference X direction needed to lock relative orientation. Target A defines the
positive translation direction through its positive Z axis.

The local and root-space builders share one residual definition and this canonical row order:

1. `direction_alignment` — `zA - zB` (3 dimensionless rows)
2. `transverse_offset_mm` — remove the Z projection from `oB - oA` (3 length rows)
3. `orientation_alignment_sine` (1 dimensionless row)
4. `orientation_alignment_cosine` (1 dimensionless, locally redundant anti-ambiguity row)
5. `translation_error_mm` — `dot(oB - oA, zA) - requested_translation` (1 length row)

This fixes all relative rotation and transverse motion while leaving exactly the requested
signed axial displacement.

## Motion, freshness, and application

Prismatic edges participate in the existing local and cross-hierarchy motion graphs. The
Block-43 role-addressed vector drive selects `translation`; all other active joints hold their
authored family coordinate. Residual evaluation, finite-difference Jacobians, transform
authorities, convergence, freshness snapshots, and copy-then-commit application reuse the
existing shared paths. Application changes exactly the explicitly driven selected coordinate
and the proposed transforms atomically.

The scalar `Quantity` motion overload and scalar degree result fields remain compatibility
adapters for Revolute. Prismatic uses `AssemblyJointDrive::requested_coordinates` and complete
coordinate-slot snapshots as the authority.

## Acceptance coverage

- `[core][assembly-prismatic-joint]`
- `[core][assembly-cross-hierarchy-prismatic-joint]`
- `[geometry][assembly-prismatic-joint]`
- `[geometry][assembly-cross-hierarchy-prismatic-motion]`

## Block 45 handoff

Blocks 45–47 Cylindrical, Planar, and passive Spherical are implemented in their canonical
documents. Blocks 48–84 Part Construction through Path-following Extrude and Extruded Cut are
implemented; Block 84 ProfileSectionReference and Loft Core intent plus JSON is implemented; Block 85 Two-section Loft Geometry on arbitrary planes is next.
