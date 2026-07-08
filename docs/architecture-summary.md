# Architecture Summary

Source: `zielarchitektur-parametrisches-cad-system.tex`.

Extracted sub-architectures included through `\input`:

- `zielarchitektur-hole-wizard.tex`: Hole Wizard
- `zielarchitektur-feature-mirror-und-muster.tex`: feature mirroring and patterns
- `zielarchitektur-rundungen-und-phasen.tex`: fillets and chamfers
- `zielarchitektur-assembly-system.tex`: assembly system with constraints

## Goal

BLCAD is intended to become an independent parametric CAD system for Linux. The model does not only store final BRep geometry, but the underlying design intent: parameters, sketches, features, dependencies, and semantic references.

## Fundamental decision

- Do not fork FreeCAD.
- Use OCCT as the geometry kernel.
- Keep the custom CAD core above OCCT.
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
- `DatumAxis`
- `Feature`, including `FilletFeature` and `ChamferFeature`
- `FeatureReference`
- `DependencyGraph`
- `ShapeCache`
- `ComponentInstance`
- `AssemblyConstraint`
- `Joint`

## Critical architecture topics

- Parameters must be first-class objects.
- Features store rules, not only result geometry.
- Recompute runs through a dependency graph.
- Sketches on faces require stable semantic references.
- OCCT shapes are a cache, not the primary model.
- The OCCT path lives in an optional `blcad_geometry` target: adapters for rectangle extrusion and centered cut, a small ShapeCache, recompute execution for `AdditiveExtrude` and `SubtractiveExtrude`, full document recompute, and STEP export of the final shape.
- The ShapeCache remains in the geometry layer; `PartDocument` remains OCCT-free and is computed into the cache through `execute_document`.
- Parameter values can be changed through `PartDocument::set_parameter_value`; a change marks dependents and drives incremental recompute.
- Assembly parameters must flow into parts in a controlled way.
- Fillets and chamfers are their own parametric features with semantic edge references, not only late BRep corrections.
- The assembly system describes spatial relationships through constraints: a constraint graph and solver determine component positions and remaining degrees of freedom; joints later allow controlled motion.
- Edge and assembly references remain semantic so they can survive model changes.
