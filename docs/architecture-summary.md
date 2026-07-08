# Architecture Summary

Source: condensed from the current repository architecture documents.

## Goal

BLCAD is intended to become an independent parametric CAD system for Linux. The model does not only store final BRep geometry, but the underlying design intent: parameters, sketches, features, dependencies, semantic references, and later assembly relationships.

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
- `DatumAxis`
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

The first MVP-2 seed introduces semantic generated-face references and resolves them in the geometry layer:

- `SemanticFaceReference` can reference `feature.base_extrude.top`.
- `DerivedWorkplane` can expose that semantic face as a sketch workplane.
- `Sketch` can reference either a standard datum plane or a derived workplane.
- The dependency graph connects source feature, derived workplane, sketch, and dependent cut feature.
- JSON serialization supports `derived_workplanes`.
- `WorkplaneResolver` resolves `datum.xy` and `feature.base_extrude.top` into concrete workplane frames.
- `ResolvedWorkplane` carries rectangular bounds for the derived top face.
- Subtractive recompute maps circle-profile centers through the resolved workplane before executing the cut.
- The geometry layer rejects circle profiles that exceed the resolved top-face bounds.
- `examples/top_face_cut.blcad.json` demonstrates an off-center cut on the derived top-face workplane.

This is still intentionally not a full topological-naming system. No raw OCCT face IDs are stored in `PartDocument`.

## Critical architecture topics

- Parameters must be first-class objects.
- Features store rules, not only result geometry.
- Recompute runs through a dependency graph.
- Sketches on generated faces require stable semantic references.
- OCCT shapes are a cache, not the primary model.
- The OCCT path lives in an optional `blcad_geometry` target: adapters for rectangle extrusion and circular cut, a small ShapeCache, a WorkplaneResolver, recompute execution for `AdditiveExtrude` and `SubtractiveExtrude`, full document recompute, bounds validation for the current top-face case, and STEP export of the final shape.
- The ShapeCache remains in the geometry layer; `PartDocument` remains OCCT-free and is computed into the cache through `execute_document`.
- JSON serialization stores model intent only; it does not serialize OCCT shapes or ShapeCache contents.
- Parameter values can be changed through `PartDocument::set_parameter_value`; a change marks dependents and drives incremental recompute.
- Derived workplanes are resolved geometrically without turning raw OCCT faces into core model references.
- The next step should verify incremental recompute through derived-workplane dependency paths before broader generated-face support.
- Assembly parameters must later flow into parts in a controlled way.
- Fillets and chamfers are their own parametric features with semantic edge references, not only late BRep corrections.
- The assembly system will describe spatial relationships through constraints: a constraint graph and solver determine component positions and remaining degrees of freedom; joints later allow controlled motion.
- Edge and assembly references should remain semantic so they can survive model changes.
