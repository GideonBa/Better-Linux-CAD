# Architecture Summary

Source: condensed from the current repository architecture documents.

## Goal

BLCAD is intended to become an independent parametric CAD system for Linux. The model does not only store final BRep geometry, but the underlying design intent: parameters, sketches, features, dependencies, semantic references, construction geometry, and later assembly relationships. The explicit long-term goal is also recorded in `docs/project-goal.md`.

## Fundamental decision

- Do not fork FreeCAD.
- Use OCCT as the geometry kernel.
- Keep the custom CAD core above OCCT.
- Treat OCCT shapes as computed cache data, not as the primary CAD model.
- Dune 3D may serve as a technical reference for sketching and UI ideas, but not as the long-term foundation.

## Layers

- User interface
- Command system
- Parametric core
- Sketch and constraint layer
- Construction geometry and datum-relation layer
- Assembly layer
- Engineering modules
- OCCT geometry kernel
- File and exchange formats

## Core objects

- `Document`
- `PartDocument`
- `AssemblyDocument`
- `Parameter`
- `Expression`
- `Sketch`
- `DatumPlane`
- `DerivedWorkplane`
- `SemanticFaceReference`
- `ConstructionPoint`
- `ConstructionLine` / `DatumAxis`
- `ConstructionPlane`
- `ConstructionRelation`
- `Feature`, including `FilletFeature` and `ChamferFeature`
- `FeatureReference`
- `DependencyGraph`
- `ShapeCache`
- `ComponentInstance`
- `AssemblyConstraint`
- `Joint`

## MVP-1 implemented vertical slice

The MVP-1 code implements the first narrow vertical slice:

- `PartDocument` stores model intent for one part.
- Parameters are typed, named, and validated.
- Datum planes and simple sketches exist as data models.
- Rectangle and circle profiles are supported.
- `AdditiveExtrude` and `SubtractiveExtrude` exist as feature-intent data models.
- `DependencyGraph`, `InvalidationState`, and `RecomputePlan` derive recompute order from model references.
- The optional `blcad_geometry` target converts the model into OCCT geometry.
- `ShapeCache` stores computed feature shapes and the final shape.
- `StepExporter` writes the final shape to STEP.
- `PartDocument::set_parameter_value` enables numeric incremental recompute.
- JSON and `.blcad.json` helpers persist and restore model intent.
- `blcad_export_step` provides a headless file-to-STEP workflow.

## MVP-2 seed

The MVP-2 seed introduces semantic generated-face references and resolves them in the geometry layer:

- `SemanticFaceReference` can reference `feature.base_extrude.top`, `feature.base_extrude.bottom`, `feature.base_extrude.right`, `feature.base_extrude.left`, `feature.base_extrude.front`, and `feature.base_extrude.back`.
- `DerivedWorkplane` can expose those semantic faces as sketch workplanes.
- `Sketch` can reference either a standard datum plane or a derived workplane.
- The dependency graph connects source feature, derived workplane, sketch, and dependent cut feature.
- JSON serialization supports `derived_workplanes` with `top`, `bottom`, `right`, `left`, `front`, and `back` faces.
- `WorkplaneResolver` resolves `datum.xy`, `feature.base_extrude.top`, `feature.base_extrude.bottom`, `feature.base_extrude.right`, `feature.base_extrude.left`, `feature.base_extrude.front`, and `feature.base_extrude.back` into concrete workplane frames.
- `ResolvedWorkplane` carries rectangular bounds for the derived top, bottom, right, left, front, and back faces.
- Subtractive recompute maps circle-profile centers through the resolved workplane before executing the cut.
- The circular cut adapter supports axis-directed through-all cuts for the current principal-axis face cases.
- The geometry layer rejects circle profiles that exceed resolved workplane bounds.
- Incremental recompute follows the derived-workplane dependency path after source-dimension changes.
- `ShapeCache` removes stale dirty feature shapes before incremental recompute, so failed recompute does not leave old cut geometry behind.
- `examples/top_face_cut.blcad.json` demonstrates an off-center cut on the derived top-face workplane.
- `examples/bottom_face_cut.blcad.json` demonstrates an off-center cut on the derived bottom-face workplane.
- `examples/right_face_cut.blcad.json` demonstrates a side cut on the derived right-face workplane.
- `examples/left_face_cut.blcad.json` demonstrates a side cut on the derived left-face workplane.
- `examples/front_face_cut.blcad.json` demonstrates a side cut on the derived front-face workplane.
- `examples/back_face_cut.blcad.json` demonstrates a side cut on the derived back-face workplane.

This is still intentionally not a full topological-naming system. No raw OCCT face IDs are stored in `PartDocument`.

## Future construction geometry

Construction geometry is a planned datum-system layer, not yet implemented. It should introduce user-created construction points, construction lines/axes, and construction planes that are part of model intent.

The intended capability is:

- construction points, lines, and planes can be placed freely in 3D
- sketches can use construction planes as workplanes
- construction geometry can depend on parameters and other reference geometry
- construction geometry can be defined by relations such as parallelism, orthogonality, angle, offset, point-on-line, point-on-plane, line-through-two-points, and plane-through-three-points
- later construction geometry can reference generated semantic faces, edges, vertices, or analytic surfaces without storing raw OCCT topology in the core
- dependency graph edges connect referenced objects to construction geometry and construction geometry to sketches/features

The detailed roadmap is in `docs/construction-geometry-mvp.md`.

## Critical architecture topics

- Parameters must be first-class objects.
- Features store rules, not only result geometry.
- Recompute runs through a dependency graph.
- Sketches on generated faces require stable semantic references.
- User-created construction geometry should be model intent, not temporary UI state.
- Construction planes answer where sketches live; sketch profiles answer what shape sketches describe.
- OCCT shapes are a cache, not the primary model.
- The OCCT path lives in an optional `blcad_geometry` target: adapters for rectangle extrusion and circular cut, a small ShapeCache, a WorkplaneResolver, recompute execution for `AdditiveExtrude` and `SubtractiveExtrude`, full document recompute, incremental recompute, bounds validation for the current top/bottom/right/left/front/back face cases, and STEP export of the final shape.
- The ShapeCache remains in the geometry layer; `PartDocument` remains OCCT-free and is computed into the cache through `execute_document` and `execute_plan`.
- JSON serialization stores model intent only; it does not serialize OCCT shapes or ShapeCache contents.
- Parameter values can be changed through `PartDocument::set_parameter_value`; a change marks dependents and drives incremental recompute.
- Derived workplanes are resolved geometrically without turning raw OCCT faces into core model references.
- General closed sketch profiles are documented as the next larger sketch-modeling block and remain separate from topological naming.
- Construction geometry and relation-driven datum placement are documented as a separate future block and remain separate from the full sketch constraint solver.
- Assembly parameters must later flow into parts in a controlled way.
- Fillets and chamfers are their own parametric features with semantic edge references, not only late BRep corrections.
- The assembly system will describe spatial relationships through constraints: a constraint graph and solver determine component positions and remaining degrees of freedom; joints later allow controlled motion.
- Edge and assembly references should remain semantic so they can survive model changes.
