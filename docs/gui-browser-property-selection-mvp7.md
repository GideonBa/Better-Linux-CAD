# GUI Browser, Property Editor, and Selection Synchronization MVP-7

Status: implemented in Block 98.

## Boundary

`GuiDocumentBrowser` is a deterministic, transient projection of the active `PartDocument` or
`Project`. It never owns persistent CAD intent. Fixed groups expose parameters, datums/workplanes,
construction geometry, 2D/3D sketches, paths, every implemented Part feature family, bodies,
assemblies, components, subassemblies, constraints, joints, and reference/analysis entries. Project
trees include the root assembly, child assembly documents, owned Part documents, and cross-hierarchy
relationships.

Every selectable row carries a stable BLCAD semantic id and optional `GuiSelectionKind`. Tree
selection updates the GUI session and Block-97 viewport; viewport picking selects the matching tree
row and refreshes the property table. AIS/TopoDS owners and Qt item identity remain transient.

## Property contract

The property table describes identifiers, text, quantities, expressions, enumerations, booleans,
semantic references, ordered inputs, and status fields. Generated identifiers, derived values, and
properties without a current Core mutation API are visibly read-only. In particular, rename and
active-body controls are not offered as writable intent because the current Core contracts define
neither a general rename operation nor active-body state.

Current writable operations are deliberately narrow:

- direct Part-parameter quantities and existing Part expression formulas;
- Body visibility;
- component visibility, suppression, and grounding;
- subassembly visibility and suppression;
- equivalent edits for Project-owned Part and assembly documents.

Preview validates against a candidate or the exact Core value grammar without changing the active
document. Apply commits one `GuiDocumentSession` transaction and therefore participates in
recompute, diagnostics, dirty state, undo, and redo. Cancel discards the transient task. Invalid
values are reported beside the table. Collections are displayed in authoritative Core order and no
unsupported reorder control is synthesized.

Part nodes also show direct dependencies, direct consumers, and invalidation status. Structured
session diagnostics are projected into a read-only analysis group as well as the diagnostic dock.

## Verification

```bash
./build/dev-gui/blcad_gui_tests "[gui][model-browser]"
./build/dev-gui/blcad_gui_tests "[gui][property-editor]"
./build/dev-gui/blcad_gui_tests "[gui][selection-sync]"
```

Coverage includes deterministic groups, typed/read-only metadata, Preview versus Apply, undoable
quantity/body edits, Project occurrence state changes, and semantic tree selection.

## Replacement-shell feature activation — Block 136

A double-click on a current Part Feature node opens `ShellFeatureEditTools` with persistent inputs
preloaded. The editor reuses the Core update and headless edit lifecycle for all thirteen supported
families, and recomputes downstream Geometry before Apply. Sketch activation retains its existing
route; read-only nodes and Project definition context do not become editable by this route.
Transform-stack records retain their explicit non-goal. Contract and proof:
`docs/gui-feature-edit-mvp9.md`, `[block136-feature-edit]`.

## Presentation visibility and sketch references (October 2026)

Browser checkboxes and the Hide/Show/Show all hidden presentations context menu control presentation visibility for
datums, construction geometry, whole sketches, individual sketch entities/profiles/annotations,
paths, feature results and bodies. Feature rows operate on their result body, including the
current body after downstream operations. Sketches expose selectable Origin, X axis and Y axis
reference rows. The same references can be picked in the sketch viewport; they are immutable
constraint targets rather than authored sketch geometry.

`GuiDocumentSession` owns the hidden semantic identities. Hiding a parent excludes its complete
semantic subtree from rendering, hit tests and drag handles. Hidden body owners also exclude
transient edge/face/vertex targets, including construction sources, Extrude face limits and Surface
selection. The cached current producer mask applies to body-owned targets as well. Visibility is session presentation state: it does not
suppress features, change model JSON, mark a document dirty, or add modeling history entries;
opening/replacing a document resets it. Presentation revisions refresh rendering; authoring freshness
excludes these transient masks, so Hide/Show does not cancel an active construction, dimension,
parameter, hole or Sketch3D workflow. Real document changes retain their existing stale-input guards.
Occurrence checkboxes use existing persistent Core
visibility transactions so nested component/subassembly definitions affect the complete hierarchy.
Showing a Body that is hidden by its persistent Core flag explicitly enables that flag, including
Undo/Redo. Checkboxes reflect both Core and transient visibility. Show all hidden presentations
clears transient masks and preserves independently stored Core visibility flags.

`[shell-reference-visibility]` exercises actual point/axis hits, origin coincidence, a spline
junction drag, history, browser checkbox toggles and Save/Open in one workflow; it runs with the
Qt offscreen adapter and the native Qt/OCCT viewer. `[presentation-visibility]` checks semantic
subtree boundaries and body-owned selection filtering.
