# Part Sweep Intent MVP-6

Status: implemented in Block 80.

Block 80 introduces persistent, dependency-tracked intent for `SweepFeature`,
`SweepCutFeature`, and `SweepSurfaceFeature`. Blocks 81–82 execute planar and spatial subsets; the
Core and JSON authority described here remains independent of transient OCCT products.

## Profile and path authority

Every Sweep owns one reusable Block-79 `PathCurveId` as its trajectory. Solid Sweep and
SweepCut consume a closed `ProfileRegionReference` with the `sweep_profile` role. SweepSurface
instead consumes a distinct, open `PathCurveId` as its profile. The document validates that all
referenced sketches, regions, PathCurves, parameters, and Bodies already exist.

The profile and trajectory PathCurves of a surface Sweep must be different. An open profile is
not accepted for a solid operation, and a closed region is not accepted for a surface operation.

## Orientation and twist

By default the Sweep inherits the trajectory's orientation rule. A feature can override that
rule with `profile_normal`, `minimum_twist`, or `fixed_up_vector`. A fixed-up override requires a
finite, non-zero vector and other overrides must not carry one.

An optional twist reference must resolve to an existing Angle parameter. Block 82 applies that
rotation along the path and adds an optional distinct open `guide_path` dependency.

## Body result context

Solid Sweep supports the existing `new_body`, `add`, and `intersect` result modes. SweepCut
requires `cut`, and SweepSurface requires `new_body`. Solid results and targets must reference
solid Bodies; SweepSurface must produce a surface Body. Target and produced-body dependencies use
the same body-history rules as the other Part feature families.

## Persistence

Part JSON always emits a `sweep_features` array and accepts its absence as an older file with no
Sweep features. Each strict record stores:

```text
id
name
type = sweep | sweep_cut | sweep_surface
profile = closed_region(sketch, profile) | open_path(path_curve)
path
orientation_override = null | profile_normal | minimum_twist | fixed_up_vector
fixed_up_vector_override = null | {x, y, z}
twist_parameter = null | ParameterId
guide_path = null | PathCurveId
operation_mode
target_body / produced_body where required by the operation mode
```

Unknown fields, malformed profile variants, incompatible operation modes, missing dependencies,
wrong parameter units, invalid vectors, and Body-kind mismatches fail closed. Resolved frames,
sampled sections, and OCCT topology are never persisted.

## Focused proof

```text
./build/dev-geometry/blcad_core_tests "[core][sweep-feature]"
```

The focused suite covers factory invariants for all three feature types, dependency invalidation
and removal protection, Body-kind enforcement, strict roundtrip JSON, older-file compatibility,
and malformed-record rejection.

Blocks 48–87 are implemented. Block 83 Path-following Extrude and Extruded Cut is implemented; Block 84 ProfileSectionReference and Loft Core intent plus JSON is implemented; Block 85 Two-section Loft Geometry on arbitrary planes is implemented; Block 86 Multi-section Loft is implemented; Block 87 Guided and continuity-controlled Loft is implemented; Block 88 Surface feature Core intent and JSON is next.
