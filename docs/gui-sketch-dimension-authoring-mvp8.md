# GUI Sketch Dimension Authoring MVP-8

Status: implemented in Block 115.

This contract exposes typed driving and reference dimensions over stable Block-108 topology targets,
reuses the Block-109 planar solver and Block-114 constraint catalog, binds driving values to the existing
Part parameter/expression authority, publishes semantic in-canvas annotations, and commits parameter,
solved Sketch geometry, and dimension intent as one document-history operation.

## Authority boundary

```text
stable Sketch selection
  -> SketchDimensionIntent candidate
  -> existing SketchConstraintCatalog + SketchDimensionCatalog
  -> typed parameter/expression lookup
  -> disposable combined Block-109 solve
  -> measured-value validation
  -> complete solved Sketch + semantic dimension glyph
  -> one GuiDocumentSession Part/Sketch-intent transaction
```

Core owns dimension identity, family, ordered topology targets, driving/reference mode, parameter binding,
measurement semantics, solver composition, sidecar persistence, and acceptance policy. Geometry owns
plane-space annotation anchors and deterministic tokens/value text. GUI owns compatible-command exposure,
existing-parameter selection, transient preview, freshness checks, and atomic session publication. Qt does
not own units, formulas, solver equations, parameter evaluation, or persistent target identity.

## Persistent dimension intent

`SketchDimensionIntent` contains:

```text
SketchDimensionId
SketchDimensionKind
SketchDimensionMode = driving | reference
ordered stable targets[]
optional ParameterId
```

A target is exactly one of:

```text
point  -> SketchPointId
entity -> SketchTopologyEntity id
```

Screen positions, OCCT topology, sampled curves, mouse handles, formatted strings, glyph coordinates,
and current solver variables are never persistent dimension identity.

Driving dimensions require exactly one compatible `ParameterId`. Reference dimensions reject parameter
bindings and derive their displayed value from the current topology.

## Implemented families

| Family | Ordered target signature | Parameter type when driving |
|---|---|---|
| Horizontal distance | point, point | Length |
| Vertical distance | point, point | Length |
| Aligned distance | point, point | Length |
| Point-to-point distance | point, point | Length |
| Length | line entity | Length |
| Radius | arc entity | Length |
| Diameter | arc entity | Length |
| Angle | line entity, line entity | Angle |
| Arc length | arc entity | Length |

Horizontal/vertical and angular target order is canonicalized by the GUI authoring binder before intent
creation. Headless callers retain explicit ordered-target semantics and must provide the intended positive
orientation.

Block 115 dimensions over full `CircleProfile` records remain outside the shared historical topology
bridge. Radius, diameter, and arc-length authoring therefore target persistent three-point arc entities in
this block. Extending shared topology to full-circle profile entities is a later compatibility extension,
not an implicit screen-space special case.

## Driving and reference behavior

Driving mode means:

```text
Parameter value
  -> typed solver value
  -> solved Sketch topology
  -> final measured-value validation
```

Reference mode means:

```text
current solved topology
  -> deterministic measurement
  -> formatted annotation only
```

A reference dimension never contributes a solver residual and never modifies geometry. Switching a
driving dimension to reference removes its binding. Switching a reference dimension to driving requires
an explicit compatible existing parameter.

Length-family measurements use millimeters. Angle measurements use degrees. Values must be finite;
driving values must be positive. Zero-length reference measurements fail closed because the current typed
`Quantity` length contract represents positive physical lengths.

## Parameter and expression binding

The existing `PartDocument` parameter system remains authoritative:

- direct Length/Angle parameters support literal dimension edits;
- expression-backed parameters support formula edits;
- rebinding selects another existing parameter of the required type;
- converting a direct binding to an expression requires an explicit new `ParameterId` and formula;
- dependency invalidation and expression evaluation use the existing Part parameter graph.

The Qt creation command binds a driving dimension to an existing compatible parameter. It does not create
an anonymous widget-owned value. Headless clients may create parameters first and then author the
corresponding dimension intent.

Literal edits are rejected for expression-backed parameters. Formula edits are rejected when a direct
parameter is not accompanied by an explicit replacement expression parameter id.

## Solver composition

Every dimension solve composes:

```text
historical embedded Sketch constraints
+ accepted Block-114 SketchConstraintCatalog records
+ all accepted Block-115 driving dimensions
```

Mappings to existing Block-109 families are:

| Dimension | Solver representation |
|---|---|
| Horizontal distance | HorizontalDistance |
| Vertical distance | VerticalDistance |
| Aligned / point-to-point | AlignedDistance |
| Line length | AlignedDistance over the line endpoints |
| Radius | Radial |
| Diameter | Diameter |
| Angle | Angular |
| Arc length | calibrated equivalent Radial constraint |

Arc length uses deterministic iterative calibration. Each pass computes the equivalent radius from the
requested arc length and the current three-point-arc sweep, solves from the original source topology, then
measures the resulting actual arc length. At most ten passes are attempted. Publication is allowed only
when the final measured arc length matches the bound parameter within the canonical relative tolerance.

## Acceptance and conflict policy

A preview is commit-capable only when:

```text
solver status = under_constrained | fully_constrained
and every driving dimension measurement matches its parameter
and the solved topology materializes through historical Sketch JSON without identity loss
```

The following remain preview-only:

```text
redundant
conflicting
invalid_reference
non_convergent
measurement mismatch
materialization identity loss
```

Rejected previews retain the exact input dimension catalog. Conflict/redundancy identifiers strip the
internal `dimension/` or `intent/` solver prefixes before GUI publication.

## In-canvas annotation and interaction

`SketchDimensionGlyphLayoutResolver` derives:

```text
semantic_id = sketch/<SketchId>/dimension/<SketchDimensionId>
candidate_id = <SketchDimensionId>
token
formatted value text
plane-space anchor
mode = driving | reference
state = accepted | preview | conflict
ordered target ids
```

Tokens are family-specific (`H`, `V`, `L`, `R`, `Ø`, `∠`, and related distance/arc symbols). Reference
values are parenthesized. All values use deterministic fixed precision and explicit `mm` or `deg` units.

Annotations are published as `GuiSketchHitKind::Dimension`. `GuiSelection` carries both persistent
`semantic_id` and exact interaction `candidate_id`, so endpoint, curve, glyph, and dimension roles remain
distinguishable even when they originate from one persistent Sketch entity. Two endpoint roles of the
same line may therefore participate in one point-to-point dimension selection.

The Sketch menu exposes all nine families, a reference-creation toggle, literal/formula editing,
parameter rebinding, and driving/reference mode switching. Action enablement derives from stable topology
capabilities and the current semantic selection.

## Atomic commit, freshness, undo, and redo

Before adding a dimension, the GUI controller verifies:

- the active Part matches the preview Part;
- Block-114 constraint and Block-115 dimension catalogs match the preview snapshots;
- the target Sketch still exists;
- migrated source topology matches the preview topology;
- the preview is accepted and contains a complete solved Sketch.

A successful add publishes one history entry:

```text
Add sketch dimension
```

Dimension editing uses the corresponding single transaction labels:

```text
Edit sketch dimension
Edit sketch dimension expression
Rebind sketch dimension
Change sketch dimension mode
```

Each transaction publishes the changed parameter state, complete solved Sketch, and complete dimension
catalog together. Global `GuiDocumentSession` undo/redo restores all three exactly.

## Persistence

Canonical document-level sidecar marker:

```text
schema  = blcad.sketch_dimensions.mvp8
version = 1
```

For a Part file `model.blcad.json`, `GuiDocumentSession` uses:

```text
model.blcad.json.sketch-dimensions.json
```

The sidecar stores only:

```text
document id
per-Sketch catalogs
dimension id
family
mode
ordered targets
optional ParameterId
```

It does not store measured values, solved coordinates, residuals, Jacobians, rank, DOF, diagnostics,
formatted text, glyph anchors, or hit-test products. Historical `blcad.part_document.mvp1` remains
unchanged.

Dimension catalogs are part of session dirty state, Save/Open, and every document-history snapshot.
Malformed schemas, duplicate ids/catalogs, wrong Part/Sketch scope, missing topology targets, or invalid
parameter bindings fail before session publication.

## Later drag behavior

Block-110 drag controllers created from `GuiDocumentSession` rebuild their baseline from accepted
Block-114 constraints and Block-115 driving dimensions. Release rechecks both catalogs and the complete
effective system. The final dragged topology must still satisfy every driving dimension measurement.

For arc-length dimensions, a drag that changes sweep without satisfying the bound final arc length is
rejected rather than publishing a stale equivalent-radius approximation.

## Focused proof

```bash
./build/dev/blcad_core_tests "[core][sketch-dimensions]"
./build/dev-geometry/blcad_geometry_tests "[geometry][sketch-dimensions]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][sketch-dimensions]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[integration][sketch-expression-edit]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[integration][sketch-live-solve]"
```

The proof covers all family signatures, deterministic sidecar ordering/roundtrip, driving/reference
semantics, typed parameters, direct and expression edits, arc-length calibration, semantic glyphs,
selection roles, atomic add/edit/undo/redo, Save/Open, Qt command registration, and later drag enforcement.

## Next boundary

Block 116 owns trim, extend, split, Sketch corner fillet, and Sketch corner chamfer. Those commands may
consume Block-114 constraints and Block-115 dimensions during topology rewrite, but they must define
explicit dependency-remap or fail-closed behavior rather than silently retargeting authored intent.
