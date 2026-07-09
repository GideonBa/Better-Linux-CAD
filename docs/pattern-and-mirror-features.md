# Pattern and Mirror Features

Status: target architecture. Not implemented yet.

Mirror, linear pattern, and circular pattern must preserve the design intent of the seed feature, not just copy finished geometry. Pattern and mirror operations are themselves normal features in the feature tree: they have parameters, are managed by the dependency graph, and recompute on change.

## Feature pattern, not geometry copy

A pattern has three parts: a seed (feature/body/component), a transform rule (translate/rotate/mirror), and parameters (count, spacing, angle, axis, plane, direction).

```text
Bad (loses meaning):
  Copy BRep shape of HoleCut_01 five times, apply boolean cuts.

Good (keeps meaning):
  LinearPatternFeature
    seed_feature = HoleFeature_M6_Clearance
    count = 5
    spacing = 20 mm
    direction = DatumAxis_X
```

OCCT may still compute the resulting geometry, but the core stores the semantic pattern operation.

## Three levels

- **Feature pattern** — repeats a modeling operation inside one part (e.g. a hole repeated along an edge).
- **Body pattern** — repeats a whole body inside one part document.
- **Component pattern** — repeats component instances in an assembly (e.g. screws on a bolt circle). See `docs/assembly-system.md`.

## Data models

```text
PatternFeature            # base type
  id, name, pattern_type, target_scope
  seed_references, target_body
  transform_definition, instance_count, pattern_parameters
  dependency_links, generated_instances, generated_shape

LinearPatternFeature
  seed_references, target_body
  direction_1_reference, count_1, spacing_1
  direction_2_reference, count_2, spacing_2   # optional second direction
  skip_instances, operation_mode

CircularPatternFeature
  seed_references, target_body
  axis_reference, count, total_angle, angle_offset, equal_spacing
  skip_instances, operation_mode

MirrorFeature
  seed_references, target_body
  mirror_plane_reference, keep_original, operation_mode
```

Instance angle for a closed 360° pattern is `alpha_i = alpha_0 + i * (alpha_total / n)`; for an open partial pattern with defined start and end it is `alpha_i = alpha_0 + i * (alpha_total / (n - 1))`. The system must know which case applies.

Directions, axes, and mirror planes are semantic references (`DatumAxis_X`, `Flange.main_axis`, `Housing.center_plane`), never raw `Edge_31` / `Face_22`.

## Operation modes

| Mode              | Behavior |
|-------------------|----------|
| Add               | instances add material (ribs, bosses). |
| Cut               | instances remove material (holes, slots). |
| New Body          | each instance creates a new body. |
| Join              | repeated bodies are unioned with the target. |
| Intersect         | keep only overlaps (later). |
| Component Instance | repeat assembly instances instead of part geometry. |

## Skip instances

`skip_instances = [3, 7]` produces all theoretical positions but omits those instances. This must be stored parametrically so it survives later changes.

## Deriving patterns from holes

A screw pattern should be derivable directly from a hole feature created by the hole wizard (`docs/hole-wizard.md`):

```text
HoleFeature HousingBoltHoles (circular_pattern, count = Assembly.bolt_count)
ComponentPattern HousingBolts
  seed_component = Screw_M6x25_01
  source_hole_feature = HousingBoltHoles
  placement = hole_axes
```

Changing `Assembly.bolt_count` updates holes and screw instances together.

## Dependency-graph integration

A pattern depends on its seed feature, its reference axis/direction/plane, and its parameters.

```text
Assembly.bolt_count -> CircularPatternFeature_BoltHoles -> Flange.Shape
RibFeature_Left -> MirrorFeature_RightRib -> Housing.Shape
```

Changing the seed recomputes the pattern; changing the count adds/removes instances; changing axis/direction recomputes all positions.

## Performance

Distinguish preview (lightweight shapes / bounding boxes), the semantic model, and final geometry. Large patterns should use instanced display instead of duplicating every shape immediately. Validate before generating (missing direction, invalid axis/plane, instances outside the target body, cut instances that do not intersect, self-intersection, excessive instance count, dependency cycles).

## MVP scope and sequence

1. Add `LinearPatternFeature` and `CircularPatternFeature` intent models with semantic references and parameters.
2. Add `MirrorFeature` with a semantic mirror-plane reference.
3. Wire into `PartDocument` validation and dependency-graph nodes.
4. Add OCCT execution (transform + boolean) in the geometry layer.
5. Add JSON serialization and roundtrip tests.
6. Add recompute tests where changing count/spacing/angle updates instances.
7. Add validation and preview.
8. Later: skip instances, component patterns, body patterns, path patterns.

## Out of scope for the first version

- path patterns along a curve.
- sketch-entity patterns.
- component and body patterns before the assembly / multi-body models exist.
