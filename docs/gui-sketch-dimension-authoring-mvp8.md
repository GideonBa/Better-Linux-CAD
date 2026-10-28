# GUI Sketch Dimension Authoring MVP-8

Status: implemented in Block 115.

The replacement shell's standalone Part parameter catalog, literal/formula creation, local batch
editing, direct consumers and complete Preview/Apply contract are documented in
`docs/parameter-model.md`. This complements the annotation/dimension workflow below; it does not
change dimension ownership or the expression grammar.

The replacement shell uses `ShellSketchTools` and `GuiSketchDimensionController`. Dimension values
are edited in a `QLineEdit` at the canvas annotation; Enter applies and Escape/focus loss cancels.
Invalid edits leave the editor open and publish a diagnostic.

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
label_offset_mm = [x, y] (optional, defaults to zero)
```

A target is exactly one of:

```text
point  -> SketchPointId
entity -> SketchTopologyEntity id
```

The optional label offset is authored presentation measured in the Sketch plane in millimetres.
It is not a target or a solver variable. Screen positions, OCCT topology, sampled curves, formatted
strings, derived glyph anchors, and solver variables are never persistent dimension identity.
Mode changes, parameter rebinding and topology remapping preserve the offset.

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
| Radius | arc entity or full `CircleProfile` | Length |
| Diameter | arc entity or full `CircleProfile` | Length |
| Angle | line entity, line entity | Angle |
| Arc length | arc entity | Length |

Horizontal/vertical and angular target order is canonicalized by the GUI authoring binder before intent
creation. Headless callers retain explicit ordered-target semantics and must provide the intended positive
orientation.

Radius and diameter also accept a persistent full `CircleProfile`. A full circle already owns a
Length diameter parameter: a driving diameter annotation binds that parameter directly, and a radius
annotation exposes half its value. Radius literal edits write twice the entered radius to the same
parameter. Reference annotations only measure it. No sampled circumference points, extra size parameter,
or duplicate solver equation are introduced. Both annotations follow subsequent parameter edits.
A circle dimension rejects rebinding to an unrelated parameter. A direct circle size accepts literal
edits; direct-to-expression conversion through this dimension editor is rejected atomically. Existing expression-backed
circle parameters accept dimension formula edits with the radius/diameter conversion applied in Core.
Arc length still requires a persistent three-point arc.

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

The shell creates an ordinary typed `dN` parameter at the current measured value when no existing
parameter is supplied. `GuiSketchDimensionController::add` provisions the parameter, previews the
combined solve, and publishes the solved Sketch and dimension catalog in one transaction. Rejection
leaves no parameter behind; one Undo removes both the parameter and dimension.

Literal input accepts a positive finite number with an optional family-matching `mm` or `deg`
suffix. A direct dimension becomes expression-driven through `=formula`; the shell allocates a
collision-free `d_expr_N` parameter and rebinds the dimension inside the same accepted transaction.
An existing expression-backed dimension accepts its replacement formula with or without `=`.
Unknown references, invalid formulas and incompatible units never publish partial edits.

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

The ribbon exposes all nine families, a reference-creation toggle and literal/formula editing.
Parameter rebinding and driving/reference mode switching are available through the controller API;
the ribbon does not yet expose separate actions for them. Enablement derives from stable topology
capabilities and the current selection.

Dragging a dimension badge previews a plane-space delta without mutating the document. Release
includes the final pointer sample and commits exactly one `Move sketch dimension label` transaction.
An unchanged placement creates no history entry. Escape, loss of mouse capture, switching tools or
leaving the Sketch discards the preview.

The viewport shares the numeric font, measured text rectangle and padding between annotation
painting and picking, in logical device-independent pixels. The complete visible badge (including
its border) is selectable at every zoom and display scale. A visible dimension badge takes
precedence over underlying points and curves. Overlapping badges prefer the nearest projected
anchor, with deterministic distance/identity tie-breaking; repeated clicks cycle the complete hit
stack. An unchanged badge click preserves that cycle and creates no document/history transaction.
Selected badges use the application highlight/text colors and paint above overlapping labels;
hovered badges receive an outline. Existing dimension lines and their midpoint-to-badge extensions
are selectable within the curve pick tolerance, below real geometry in the hit order. The
family-specific dimension line/arc presentation follow-up remains tracked by issue #106.

A left double-click resolves the visible badge at the event position independently of prior
selection and opens its driving editor. The matching release is consumed so it cannot cycle to a
different overlapping object. Creation, Rapid Dimension and modal modification/operation tools
retain their own gestures. Enter uses the existing atomic dimension transaction, invalid input
leaves a correctable editor with feedback, and Escape changes neither the document nor history.
Reference dimensions explain their read-only measurement semantics in the status bar.

Clicking a badge and pressing Delete or Backspace removes that dimension. Ctrl-click adds/removes
selection; the delete action is enabled for multiple selected dimensions/constraints as well as
geometry. Annotation-only deletion preserves Sketch geometry, unrelated constraints/dimensions and
all backing parameters, including externally referenced formula/feature inputs. Intentional
geometry deletion retains the existing cascading removal policy. Deletion, editing and placement
use the existing Undo/Redo and Save/Open contracts; no persistence fields were added for picking.

## Rapid Dimension

The Sketch ribbon exposes **Rapid Dimension** (shortcut **D**). Hover shows a disposable measured
preview. Pick a curve, or two endpoints/points (including the fixed Sketch origin), move to the desired
label position, and click empty space to publish one dimension. The tool remains active for the next
measurement. Escape clears the current picks; Escape again exits. Switching tools or leaving the Sketch
also discards the draft. Preview and label placement do not write history.

Inference uses persistent topology roles:

- line: length, with horizontal/vertical projections when placing above/beside a diagonal line;
- point pair: horizontal, vertical or aligned distance according to label position;
- two nonparallel lines: angle; two horizontal/vertical parallel lines: perpendicular axis distance;
- arc: radius; full circle: diameter;
- pressing D again (or the Rapid Dimension button) switches radius and diameter.

The existing Reference toggle chooses reference creation. The placed label location belongs to the
same atomic transaction as the dimension and its parameter. Hover does not run a second solver or
invent persistent geometry. Unsupported targets produce no candidate. Invalid or conflicting creation
keeps the draft and leaves the document unchanged. Double-click a placed driving annotation to edit
its value; Enter accepts valid values and Escape cancels. Radius and diameter badges retain their
R/Ø prefix in both hover previews and accepted annotations. Existing explicit family commands remain
available for deterministic selection-first authoring.

The preview is a rendering-only overlay: it does not take part in hit testing and cannot block the
underlying curve/point selection. Family, measurement and units come from the shared Core authority.

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
optional label_offset_mm
```

Absent offsets in historical version-1 sidecars read as zero. Zero offsets are omitted on write;
malformed arrays and non-finite values are rejected. Layout participates in dirty state, transactional
Save/Open and Undo/Redo through the existing dimension catalog.

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
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[sketch-dimension-workflow]"
./build/dev/blcad_core_tests "[integration][sketch-expression-edit]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[integration][sketch-live-solve]"
```

The proof covers all family signatures, deterministic sidecar ordering/roundtrip, driving/reference
semantics, typed parameters, direct and expression edits, arc-length calibration, semantic glyphs,
selection roles, atomic add/edit/undo/redo, Save/Open, Qt command registration, and later drag enforcement.

## Dimension label input validation — 2026-10-28

Issues #107 and #108 share the confirmed font-metrics rectangle versus 12-DIP anchor-disk mismatch.
The original implementation fails the four new input regression cases, including text edges,
geometry overlap and read-only feedback (28 failed assertions / 448 evaluated). Native integration
also exposed a separate enablement bug: multiple selected dimensions disabled the existing Delete
action even though its transaction already supported them. Both causes are fixed without replacing
the dimension editor, solver, persistence or deletion transaction.

Final validation, with production/test sources frozen throughout the completed runs:

- All five GUI-preset targets built successfully.
- Full offscreen GUI: **577 cases — 557 passed, 20 expected native-only skips; 76,327 assertions**.
- Native Qt/OCCT affected Sketch, dimension, drag, constraint, Rapid Dimension, combined desktop
  and required shell-coverage proof: **49 cases / 3,115 assertions**, no skips.
- Native dimension-input proof at 150% scale: **7 cases / 1,606 assertions**, no skips.
- Separate native application startup reached the intentional six-second timeout (124).
- Inspected `dimension-selected-overlap.png` at both scales, checked the diff and verified all
  **739 source/test/asset/build-definition hashes** remained unchanged during final validation.

The native tests require actual window/keyboard focus before sending keys and flush complete
stationary click sequences together, matching the existing combined shell fixtures. Initial fixture
iterations exposed asynchronous focus/mouse-event interference; these checks keep selection and
keyboard delivery strict. Existing capture helpers require the output directory to be created first.
An initial sandboxed full run was stopped after STEP `QSaveFile` publication failures. The unchanged
export control passed outside that sandbox (**6 cases / 188 assertions**, one native-only skip);
the final full and native runs used those file permissions. Final native/scaled/startup logs contain
no High-severity, BadDrawable or BadWindow errors; the available X11 visual retains the known OCCT
Medium depth/stencil warning.

Exact final logs: `/tmp/blcad-issue107-build-final.log`, `/tmp/blcad-issue107-gui-final.log`,
`/tmp/blcad-issue107-native-final.log`, `/tmp/blcad-issue107-native-150.log` and
`/tmp/blcad-issue107-startup.log`. Capture roots use `/tmp/blcad-issue107-native-final` and
`/tmp/blcad-issue107-native-150`; frozen inputs are recorded in
`/tmp/blcad-issue107-source-freeze.json`. Reproduction commands live in
`docs/development-setup.md`. Numbered status remains through Block 137; Block 138 stays planned.

## Next boundary

Block 116 owns trim, extend, split, Sketch corner fillet, and Sketch corner chamfer. Those commands may
consume Block-114 constraints and Block-115 dimensions during topology rewrite, but they must define
explicit dependency-remap or fail-closed behavior rather than silently retargeting authored intent.
