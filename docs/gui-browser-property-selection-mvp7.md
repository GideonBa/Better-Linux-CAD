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
