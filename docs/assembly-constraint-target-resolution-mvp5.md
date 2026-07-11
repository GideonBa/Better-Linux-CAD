# Assembly Constraint Target Resolution MVP-5

Status: implemented read-only resolution of supported generated-face assembly targets to component-local planar descriptors.

## Goal

This block connects persistent `AssemblyConstraintTarget` intent to the first supported geometric reference family without applying component placement or solving assembly relationships.

The resolver answers:

1. Which component occurrence does the target name?
2. Which project-owned `PartDocument` does that occurrence reference?
3. Which supported semantic generated face does the token name inside that part?
4. What is the deterministic component-local planar frame of that face?

The result is regenerable derived geometry data and is not serialized.

## API

The geometry-layer API lives in `include/blcad/geometry/assembly_constraint_target_resolver.hpp`.

```text
ComponentLocalPlanarDescriptor
  origin
  x_axis
  y_axis
  normal

ResolvedAssemblyConstraintTarget
  component_instance
  referenced_part_document
  source_feature
  face
  local_plane
  component_transform

AssemblyConstraintTargetResolver
  resolve(Project, AssemblyConstraintTarget)
```

`ComponentLocalPlanarDescriptor` is expressed in the referenced part/component-local coordinate system.

`component_transform` is returned separately as persisted placement intent. The resolver does not apply it.

## Resolution path

For:

```text
component.face_plate
feature.base_extrude.top
```

resolution proceeds as:

```text
AssemblyConstraintTarget
  -> Project::assembly()
  -> ComponentInstanceId
  -> ComponentInstance
  -> referenced_part_document
  -> project-owned PartDocument
  -> SemanticFaceReference
  -> WorkplaneResolver::resolve_generated_face
  -> component-local origin/x-axis/y-axis/normal
```

The resolver receives a `const Project&`, reads existing occurrence and part intent, and returns a new descriptor.

## Supported semantic reference family

The first supported assembly target family is:

```text
feature.<feature-id>.top
feature.<feature-id>.bottom
feature.<feature-id>.right
feature.<feature-id>.left
feature.<feature-id>.front
feature.<feature-id>.back
```

The source feature must resolve inside the referenced `PartDocument` and currently be a supported `AdditiveExtrude`.

Examples:

```text
feature.base_extrude.top
feature.base_extrude.bottom
feature.housing_block.front
```

The geometry-layer parser validates the target family at resolution time. Persistent target records remain intentionally more general and store non-empty semantic tokens without record-layer geometry interpretation.

## Reuse of generated-face geometry

`WorkplaneResolver` owns deterministic generated-face frame construction. The shared API is:

```text
WorkplaneResolver::resolve_generated_face(PartDocument, SemanticFaceReference)
```

Derived workplanes and assembly targets therefore share one implementation for face origins, basis axes, normals, bounds inputs, and source-profile restrictions.

The assembly resolver copies `origin`, `x_axis`, `y_axis`, and `normal` into its local planar descriptor. Workplane bounds are not assembly constraint target intent.

## Explicit failure behavior

Resolution fails when:

- the target component does not exist in the project assembly
- the component's referenced part does not resolve to a project-owned `PartDocument`
- the semantic token is malformed
- the token belongs to an unsupported reference family
- the generated-face source feature does not exist
- the source feature is not a supported additive extrude
- the shared generated-face geometry path cannot resolve the source sketch or required parameters

Unsupported examples include:

```text
bolt.main_axis
feature.base_extrude.edge.top_front
```

Semantic axes remain unsupported because a stable axis-reference family is required before Concentric or Insert equation construction.

## Placement boundary

`RigidTransform` stores translation in millimeters and rotation in degrees.

This resolver deliberately does not interpret those values. Placement evaluation belongs to `AssemblyTransformEvaluator`, documented in `docs/assembly-rigid-transform-evaluation-mvp5.md`.

For example, a component may store:

```text
translation_mm = (10, 20, 30)
rotation_deg   = (0, 0, 90)
```

If its local top face resolves to:

```text
origin = (0, 0, 8)
normal = (0, 0, 1)
```

this resolver returns that local frame plus the unchanged transform.

The separate transform layer can then produce assembly-space geometry under the canonical X-then-Y-then-Z convention.

## Downstream use

Two downstream geometry layers now consume resolver output.

### Transform evaluation

```text
ResolvedAssemblyConstraintTarget.local_plane
+ ResolvedAssemblyConstraintTarget.component_transform
  -> AssemblyTransformEvaluator
  -> AssemblySpacePlanarDescriptor
```

### Planar constraint residual construction

```text
AssemblyConstraint
  -> AssemblyConstraintEquationBuilder
  -> resolve target A/B through AssemblyConstraintTargetResolver
  -> evaluate target A/B through AssemblyTransformEvaluator
  -> Mate or Distance residual descriptor
```

The resolver itself remains independent from constraint type and residual semantics.

## Read-only and persistence boundary

Resolution does not:

- mutate component transforms
- mutate grounding, visibility, or suppression state
- change assembly constraints
- add generated references or derived workplanes to the original part
- take ownership of a `ShapeCache`
- persist a resolved target descriptor

The descriptor is regenerated from project model intent and current part parameters.

## Tests

`tests/geometry/assembly_constraint_target_resolver_tests.cpp` covers:

- all six generated-face targets
- component and referenced-part identity
- separate component transform preservation
- missing component targets
- missing project-owned parts
- missing source features
- malformed semantic tokens
- unsupported semantic axis targets
- unsupported generated-edge targets
- deterministic repeated resolution
- unchanged component transforms, constraint records, semantic tokens, and part derived-workplane count

Targeted test command:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-target]"
```

The transform and planar equation suites separately verify downstream coordinate mapping and residual semantics.

## Deliberate limitations

This resolver itself does not implement:

- component-local-to-assembly transform math
- constraint equation semantics
- rigid-body solving
- solved transform updates
- remaining-DOF analysis
- enforced grounding or suppression participation

Transform evaluation and planar Mate/Distance residual construction are implemented as separate downstream layers.

## Current downstream boundary

The repository-wide next assembly step is a first rigid-body solver seed over deterministic graph groups and the implemented planar residual descriptors.

Semantic axis references and Concentric residual construction remain deferred.
