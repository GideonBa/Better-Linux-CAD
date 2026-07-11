# Assembly Constraint Target Resolution MVP-5

Status: implemented read-only resolution of supported generated-face assembly targets to component-local planar descriptors.

## Goal

This block connects persistent `AssemblyConstraintTarget` intent to the first supported geometric reference family without introducing constraint equations or a rigid-body solver.

The resolver answers four questions for one target:

1. Which component occurrence does the target name?
2. Which project-owned `PartDocument` does that occurrence reference?
3. Which supported semantic generated face does the target name inside that part?
4. What is the deterministic component-local planar frame of that face?

The result is derived geometry data. It is not model intent and is not serialized.

Assembly-space transform evaluation now exists as the next separate layer in `docs/assembly-rigid-transform-evaluation-mvp5.md`.

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

`component_transform` is returned separately as the persisted free-placement intent of the component occurrence. The resolver deliberately does not apply it to the planar descriptor.

## Resolution path

For a target such as:

```text
component.face_plate
feature.base_extrude.top
```

resolution proceeds as follows:

```text
AssemblyConstraintTarget
  -> Project::assembly()
  -> ComponentInstanceId
  -> ComponentInstance
  -> referenced_part_document
  -> Project-owned PartDocument
  -> SemanticFaceReference
  -> WorkplaneResolver::resolve_generated_face(...)
  -> component-local origin/x-axis/y-axis/normal
```

The target resolver is read-only. It receives a `const Project&`, reads the existing occurrence and part model, and returns a new derived descriptor.

## Supported semantic reference family

The first supported assembly target family is the generated planar face family already proven by the MVP-2 workplane path:

```text
feature.<feature-id>.top
feature.<feature-id>.bottom
feature.<feature-id>.right
feature.<feature-id>.left
feature.<feature-id>.front
feature.<feature-id>.back
```

The source feature must resolve inside the component's referenced `PartDocument` and must currently be an `AdditiveExtrude` supported by generated-face workplane resolution.

Examples:

```text
feature.base_extrude.top
feature.base_extrude.bottom
feature.housing_block.front
```

The parser validates the target family at resolution time. Persistent `AssemblyConstraintTarget` records remain intentionally more general and continue to store non-empty semantic tokens without geometry-layer interpretation.

## Reuse of generated-face geometry

The existing `WorkplaneResolver` owns the deterministic frame construction for generated additive-extrude faces. This block exposes that same path through:

```text
WorkplaneResolver::resolve_generated_face(PartDocument, SemanticFaceReference)
```

Derived workplanes and assembly target resolution therefore use one generated-face frame implementation. Face origins, basis axes, normals, bounds calculations, and source-profile restrictions are not duplicated in the assembly resolver.

The assembly target resolver consumes the resolved frame and copies only `origin`, `x_axis`, `y_axis`, and `normal` into its component-local planar descriptor. Workplane bounds are not assembly constraint target model intent.

## Explicit failure behavior

Resolution fails explicitly when:

- the target component instance does not exist in the project assembly
- the component's referenced part does not resolve to a project-owned `PartDocument`
- the semantic token is malformed
- the semantic token belongs to an unsupported reference family
- the generated-face source feature does not exist in the referenced part
- the source feature is not a supported additive extrude
- the existing generated-face geometry path cannot resolve the source sketch or required parameters

Unsupported examples include:

```text
bolt.main_axis
feature.base_extrude.edge.top_front
```

Semantic axes remain unsupported. This is deliberate because a stable axis-reference family is required before full Concentric or Insert solving can be implemented.

## Placement boundary

`RigidTransform` stores translation in millimeters and rotation values in degrees. Target resolution does not interpret those values itself.

For example, a component may store:

```text
translation_mm = (10, 20, 30)
rotation_deg   = (5, 15, 25)
```

If its local top face resolves to origin `(0, 0, 8)` and normal `(0, 0, 1)`, this resolver returns that local frame plus the unchanged `RigidTransform`.

The next layer is now explicit:

```text
ResolvedAssemblyConstraintTarget
  local_plane
  component_transform

  -> AssemblyTransformEvaluator::evaluate_plane(...)

AssemblySpacePlanarDescriptor
```

`AssemblyTransformEvaluator` defines active right-handed fixed-axis X-then-Y-then-Z rotation, equivalent to `Rz * Ry * Rx` for column vectors. That convention is documented in `docs/assembly-rigid-transform-evaluation-mvp5.md`.

Keeping transform math outside target resolution prevents semantic lookup and coordinate-space evaluation from becoming one coupled subsystem.

## Read-only and persistence boundary

Resolution does not:

- mutate component transforms
- mutate grounding, visibility, or suppression state
- add, remove, or change assembly constraint records
- add generated references or derived workplanes to the original part document
- take ownership of a `ShapeCache`
- persist a resolved target descriptor

The descriptor can be regenerated from project model intent and current part parameters. It therefore remains derived data like constraint graph connectivity and evaluated assembly-space frames.

## Tests

`tests/geometry/assembly_constraint_target_resolver_tests.cpp` covers:

- top, bottom, right, left, front, and back generated-face targets
- component and referenced-part identity in the resolved result
- separate preservation of component `RigidTransform`
- missing component targets
- missing project-owned part documents
- missing source features
- malformed semantic tokens
- unsupported semantic axis targets
- unsupported generated-edge targets
- deterministic repeated resolution
- unchanged component transform, constraint records, semantic tokens, and part derived-workplane count

The resolver is registered in the optional geometry target and its tests are registered in `blcad_geometry_tests`.

The separate transform-evaluation suite covers the local-to-assembly coordinate convention; this resolver suite intentionally continues to assert local geometry plus separate placement intent.

## Deferred work

This block itself does not implement:

- Mate or Distance constraint equations
- Concentric equations or semantic axis references
- rigid-body solving
- solved component transform updates
- remaining-DOF computation
- underconstrained, fully constrained, or overconstrained state analysis
- enforced grounding or suppression participation rules

Component-local to assembly-space point/vector/frame evaluation is no longer deferred globally; it is implemented as the separate `AssemblyTransformEvaluator` block.

## Next technical step

The next assembly block is read-only planar Mate/Distance equation/residual construction over assembly-space target descriptors.

It should resolve both supported generated-face targets, evaluate both local planes through `AssemblyTransformEvaluator`, and construct documented deterministic geometric residual data without solving or mutating component transforms. Concentric remains deferred until semantic axis references are stable.
