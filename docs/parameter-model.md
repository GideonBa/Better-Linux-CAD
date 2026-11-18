# Parameter Model, Expressions, and Top-Down Design

Status: implemented bounded Part parameters, expressions, Assembly bindings and replacement-shell
parameter management. The wider scope/navigation model below remains target architecture.

The [post-204 parameter dependency tree plan](parameter-dependency-tree-plan.md) assigns the
requested E view, rooted Assembly/Part tables and exact formula-use highlighting to **205–211**.
It extends these authorities and the 194–195 reference/Project engine once delivered; current bounded
Part formulas/bindings do not already provide general cross-document expressions. Inspection
keeps definition parameters separate from occurrence paths and does not mutate model visibility.

The wider assigned execution order is consolidated in [Blocks 189–198](inventor-parity-sequence.md):
model states/variants, cross-document adaptive references and contextual Assembly authoring.
Material dimensional types/resources are assigned earlier to 149–151, and curve-local equation
variables to 160. Existing Part expressions and Assembly-to-Part bindings stay authoritative.
The historical implementation sequence below is not an independent queue; no second expression
evaluator or implicit global-parameter scope is introduced by the new plan.

Parameters are first-class objects. Features reference them by identity, and the graphical table
uses the same validated model and dependency graph as recompute and persistence.


## Engineering resource quantities — Block 149A boundary

Shared material/catalog intent uses positive finite `MassDensity` (canonical kg/m³) and
`EngineeringPressure` (canonical Pa), with explicit named conversion factories. Density × mm³
volume will produce mass through the physical-properties authority in 150; density is never stored
as a Length parameter or an unlabelled scalar. Typed standards-table geometry cells reuse the
existing Length/LinearDisplacement/Angle/Count quantities. This resource foundation does not extend
the supported Part/Assembly parameter-expression kinds or create another expression evaluator.
See `engineering-modules.md` for the single resource/provenance authority.

## Parameter object

A parameter is a named value with type, unit, scope, and an optional formula.

```text
Parameter
  id      = "asm.bolt_circle_radius"
  name    = "bolt_circle_radius"
  value   = 50
  unit    = "mm"
  type    = "length"
  scope   = "assembly"
  formula = optional
```

## Scopes

| Scope    | Meaning |
|----------|---------|
| Global   | Project-wide parameters usable across several documents. |
| Assembly | Parameters of an assembly; central for top-down design. |
| Part     | Parameters of a single part, e.g. plate thickness. |
| Sketch   | Parameters inside a sketch, e.g. a circle diameter. |
| Feature  | Parameters of a feature, e.g. an extrude length or chamfer size. |

## Expressions

An expression is a formula computed from other parameters.

```text
bolt_circle_radius = plate_width / 2 - 15 mm
```

Expressions must be unit-aware and must create dependency-graph edges to every parameter they read, so a change to an input re-evaluates the expression.

## Cross-part parameter usage (top-down design)

Central design parameters are defined on the assembly and consumed by several parts. This lets related parts change together.

```text
AssemblyParameters
  bolt_circle_radius      = 50 mm
  bolt_count              = 8
  bolt_clearance_diameter = 6.6 mm
  plate_thickness         = 8 mm
  gasket_thickness        = 2 mm

Part BasePlate   uses Assembly.bolt_circle_radius, Assembly.bolt_count, Assembly.bolt_clearance_diameter
Part CoverPlate  uses Assembly.bolt_circle_radius, Assembly.bolt_count, Assembly.bolt_clearance_diameter
Part Gasket      uses Assembly.bolt_circle_radius, Assembly.bolt_count
```

Changing `bolt_count` from 8 to 12 must recompute every dependent bolt circle in every affected part. This only works if assembly parameters participate cleanly in the dependency graph. Cross-part flow must be controlled and traceable: a part references an assembly parameter explicitly; the reference is an edge in the graph.

Cyclic parameter dependencies must be detected and rejected (for example part A using a parameter of part B while part B depends on part A). See the cycle-detection requirement in `docs/dependency-graph-mvp1-data-model.md`.

## Current implemented scope

- typed, named, validated parameters on a single `PartDocument`.
- three parameter types: positive `Length` (`mm`), positive whole `Count` (`1`) and signed finite
  `Angle` (`deg`). Formula evaluation supports Length and Count.
- two scopes: `part` and `assembly`; `AssemblyDocument` owns assembly-scoped parameters and pushes them into member parts through explicit `ParameterBinding` records (`docs/assembly-parameters-mvp4.md`).
- `PartDocument::set_parameter_value` marks dependents and drives incremental recompute (`docs/parameter-update-mvp1.md`).
- part-scope expression parameters: unit-aware formulas over other part parameters with graph-driven topological re-evaluation (`docs/parameter-expression-mvp.md`).

## Replacement-shell Part parameter manager — Block 136

The Manage ribbon's **Manage Parameters** command (`blcad.action.parameters`) opens the owned
`ShellParameterTools` task (`blcad.shell.parameters_task`). It requires a fresh Part, no repair
session, no active command and no planar Sketch editor. Project/Assembly parameter management is
outside this bounded command. Existing parameter property and Sketch-dimension editors remain
available through their existing contracts.

The read-only catalog shows every Part parameter's name, stable ID, type, evaluated value/unit,
formula and persisted scope. Select a row or use the parameter selector to edit a literal's value
or a computed parameter's formula. The detail label lists direct dependency-graph consumer IDs;
it is not a recursive usage navigator. Selection discards unsubmitted form input while retaining
already staged changes. Existing identity, name, type, scope and literal/formula category stay fixed.

For a new parameter, enter an ASCII expression identifier (`[A-Za-z_][A-Za-z0-9_]*`), choose
Length/Count/Angle and supply a finite bare number in the indicated unit, or choose a Length/Count
formula. Names must be unique. IDs use the first free `parameter.user.N` in the **entire dependency
graph**, including non-parameter nodes. No rounding or clamping occurs; editing uses 17 significant
digits for Length/Angle and integer Count text. Formula names/units/arithmetic/cycles retain Core
validation. Angle formulas, conversion between literal and formula, rename and delete are not
implemented by Core and are explicitly unavailable here.

**Lokal übernehmen** validates on a disposable Part copy, stages an ordered operation and refreshes
all evaluated catalog values and consumers. Invalid input retains its text and all earlier staged
operations. Pending input clears the viewport preview and blocks Preview/Apply until staged or
discarded by selection. **Lokale Änderungen verwerfen** resets the complete draft to committed intent.

**Vorschau** replays the complete batch through Core mutation APIs in one copied session transaction,
validates/recomputes the complete downstream model and displays disposable geometry. Positive typed
values that fail geometry remain local and correctable. Fixed DatumPlane records retain their normal
wire presentation so their unchanged planes do not cover previewed bodies. Neither typing, staging
nor preview changes live intent, dirty state, history, caches or files. **Anwenden** requires a current successful preview,
rechecks it and commits all operations in one Part transaction/Undo entry. Existing GUI constraint
and dimension sidecars are retained by that transaction; persistence uses the existing schema.

Editable STEP Parts follow the same batch contract (Block 141 integration). Candidate and committed
preview scenes share the saved document's verified STEP source context; sources are checked again
before preview publication. Downstream parameter edits retain the immutable imported base and
recompute its consumers. Changed or unavailable source bytes reject the candidate without publishing
intent, geometry or history. Native `[native-step-editable-parts]` proves the modeled Body changes
while the original imported base volume remains unchanged. Numbered completion belongs to
`docs/mvp-plan.md`.

Escape from child controls, dock close, Cancel, Undo/Redo, Save, changed presentation revision,
foreign task, document replacement or entering another context discard the entire local task and
preview. A foreign task is never cancelled by cleanup. Save writes only committed intent. The task
owns no timers, cross-component widget discovery or rewired actions.

Proof: `[block136-parameters]` in `tests/gui/gui_shell_parameter_tests.cpp` and actual-input
`[shell-combined-parameters]` in `tests/gui/gui_shell_combined_workflow_tests.cpp`. Commands and final
evidence live in `docs/development-setup.md` and `docs/gui-shell-acceptance-mvp9r.md`.

## Implementation sequence and wider target

1. Add a `scope` field to the parameter model and enforce allowed scopes.
2. Add an `Expression` type with unit-aware evaluation over other parameters.
3. Create dependency-graph edges from an expression to each parameter it reads.
4. Add re-evaluation of expressions during recompute in topological order.
5. Add assembly-scoped parameters once the assembly document exists (`docs/assembly-system.md`).
6. Add cross-part parameter references and the corresponding graph edges.
7. Add cycle detection across parameter, expression, and cross-part edges.
8. Surface parameter usage sites (`docs/user-interface.md`). The Part table now lists direct
   consumer IDs; recursive and cross-document navigation remains a wider target.

## Out of scope for the first versions

- global/Assembly parameter table navigation and parameter identity/category changes.
- global project-level parameter sharing before the multi-document project file exists (`docs/file-format.md`).


## Model-state authority and effective evaluation — 189/190

`ModelState` adds sparse named siblings to a permanent implicit `primary` identity. Primary is the
existing Part or Assembly definition; named states carry stable ids and unique display names.
The reserved id/name cannot be authored, renamed or removed. Empty catalogs are absent from
legacy files. `replace_model_states` validates the whole table against Primary before publication;
there is no state inheritance and no duplicated complete document per inactive state.

Part overrides address stable parameters, executable history ids and material assignments. A typed
literal may replace only a literal parameter of the same quantity category. An explicit formula
string may replace only an existing expression parameter. Final formulas are rebuilt together
through the existing unit-aware dependency authority: changed dependencies, missing names and
cycles are checked on the complete candidate, including simultaneous rewiring that would be
invalid as a sequence of intermediate edits. Parameter identity, authored table order, scope and
category survive evaluation. Feature suppression may explicitly suppress or reactivate an existing
row; normal history ownership rules still apply. A material record selects a pinned resource for
a Body or the Part default; a null assignment explicitly clears it. Missing targets or resources
fail validation. Reference Parts require explicit conversion before named-state authoring.

Inactive state references protect their executable, Body and pinned material targets from removal.
Primary parameter edits validate every named state before publication and roll back atomically if a
state domain becomes invalid. Named Assembly results clear sibling definitions and carry their
effective state identity; they cannot be reinterpreted as Primary by evaluating a default id.

Assembly states may override existing Assembly literals, component/subassembly suppression and
selected child states. Part features and material assignments remain Part-owned. Component and
subassembly selections persist independently of display visibility, grounding and authored poses;
all copy-style changes preserve the selection. Project validation resolves selected child states
in Primary and every named Assembly state, including suppressed occurrences. Removing or renaming
a referenced state requires a coordinated Project candidate that repairs every reference.
The existing Assembly hierarchy cycle validation remains mandatory; no recursive state
inheritance or shape/pose feedback solver is introduced.

`evaluate_model_state` returns a disposable effective definition while preserving Primary and its
catalog. Named Part results clear the sibling catalog because it belongs to the original Primary,
and carry a transient `effective_model_state()` identity. `model_state_source_key(Primary, state)`
includes the complete authored source and selected id; numerically identical siblings therefore
remain distinct contexts. Hierarchy traversal evaluates the selected Assembly at each occurrence;
leaf descriptors retain the selected Part state. Activation and derived geometry stay session data.
A caller must retain Primary for Save and state editing, and certify Geometry/cache results against
the effective document and explicit selected state. Assembly shape definitions combine each state's
visible, available Solid and Surface Bodies in stable Body-id order after complete cache validation.
Suppressed creators contribute no unavailable Body; hidden source Bodies remain available to their
downstream operations and physical mass. Presentation geometry, scene and STEP use the visible
definition; mass integrates every available participating solid, and Project physical validation
includes every available Body.
Common and adaptive Project validators use physical-active posed definitions independently of
display visibility, including an all-hidden design view or all-hidden Bodies. Reference and
zero-Body legacy Parts
retain their established complete-result authority. The separate historical final-shape API keeps
its single-available-Solid compatibility contract. Focused coverage is Geometry
`[assembly-multi-body]`, including distinct states, a surface from a hidden solid, physical mass and
flat/structured STEP reopening. Full Geometry, export and native acceptance remain separate
numbered gates in [the MVP plan](mvp-plan.md).

Focused Core proof is `tests/core/model_state_tests.cpp` (`[model-state]`): sparse typed evaluation,
formula rewiring/cycles, atomic invalid tables, materials/suppression, strict JSON, repeated nested
child selections, hidden physical participation and dangling-reference rejection. These tests do
not claim physical or native acceptance for 190/191/198.


### Native table, selected views and independent member copies — 191

Manage → Model States owns a native table for the current Part, the root Assembly or an owned
editable Project Part. New/Duplicate/Rename/Delete operate on stable state identities. Parameter
rows expose an explicit Use checkbox and the existing quantity category or authored formula;
unchecked rows inherit Primary. Separate selectors edit executable suppression, pinned Body/default
materials and child occurrence suppression/state. Invalid cells remain editable. Accept Locally
checks the Core table; Preview evaluates every state and the complete candidate; Apply publishes
one `Manage Model States` history entry. Apply, Undo and Redo retain a valid selected view; Undo
returns to Primary when it precedes creation of the selected state.

Assembly Insert Part, Insert Subassembly and Place expose a Model State selector for the exact
shared definition. New occurrences default to Primary; changing the definition resets that choice
to Primary, while editing an occurrence loads its authored selected state. Labels display names;
combo item data retains stable state IDs. Preview evaluates a disposable occurrence candidate;
Apply stores its state and placement in one existing root transaction/Undo. A missing state rejects
rather than silently selecting a sibling. This route authors Primary occurrence selections; named
root-state child overrides remain in Model States. Geometry, mass, STEP and member export consume
that same checked selection without rewriting shared Part/child Assembly definitions. GUI
`[occurrence-state-placement]` and native `[occurrence-state-placement-native]` cover both kinds of
insertion/edit, typed rejection, exact history, equal-shape context freshness and physical exchange.

The browser's Model States group and the table's View/Primary controls select transient effective
views. Activation rejects a pending foreign task or partial Part workflow. A named view is read-only
for ordinary modeling; return to Primary to author its definition, or edit the named overrides in
Model States. The viewport, browser values, Measure, Physical Properties, analysis and ordinary STEP
export consume the same selected effective document. Save writes Primary and the sparse catalog;
it never persists the currently activated view or changes dirty/Undo state merely because a view
was selected. Root Assembly state literals propagate through changed existing parameter bindings
before occurrence Part states are evaluated; explicit Part state overrides then apply to that
effective definition. The authored Project and shared Part definitions remain unchanged.

A state-table change validates inactive geometry before publication. Primary parameter edits also
validate inactive state formulas and roll back if a sibling becomes invalid. State suppression and
material records protect referenced executable/Body/resource identities against direct removal;
remove or repair those overrides explicitly in a disposable complete candidate first. Project
state-reference validation protects externally selected state identities independently of labels.

Save Member Copy chooses a new document identity, evaluates the explicitly selected Part state and
writes an independent Part without sibling states. Pinned resources survive, and standalone Sketch
sidecar catalogs are rebound to the new document owner. The existing atomic Save As transaction and
STEP asset-root rules apply: imported assets must remain available at matching relative paths in
the destination root. Existing document/sidecar paths and the live source document are preserved on
failure. This is an explicit member copy, without a factory or release/BOM authority.

Implementation proof lives in `tests/gui/gui_model_state_tests.cpp`, `[model-state]` and native-only
`[model-state-native]`. Native pointer input covers table editing, Preview/Apply, view switching and
reopening, with optional original captures under `BLCAD_UI_CAPTURE_DIR`. Current executed acceptance
and any remaining integration obligations belong to [the MVP plan](mvp-plan.md); source presence
alone does not close a numbered gate.


Undo/Redo stages the restored model, selected IDs and certified caches before publication. If the
active manufacturing state disappears, it falls back to Primary and recomposes every surviving
named position and design view. Missing representations fall back to Master/Default; repair or
missing-source results reset all effective scopes consistently. A failed selected-state rebuild
retains the accepted model, selected contexts and certified geometry. GUI `[history-scope-fallback]`
checks both surviving/deleted catalogs and an actual failed cut under the retained state.

### In-context Part editing and root publication — 198

Edit Part retains an immutable authored root-session checkpoint and opens the selected rooted
editable Part in a disposable Part session. Entry requires the complete idle Project in Primary,
Master position and Default design view, and the occurrence must actively select the Part's
Primary state. Reference Parts require explicit conversion. The initial definition comes from the
checked normalized Project so downstream editing sees current linked constructions and coordinated
Hole placements. Root/Part breadcrumb identity and the exact local-to-root transform chain remain
transient context data; ordinary Part tools author in Part coordinates. Adaptive source selectors
use the rooted occurrence identities and the Geometry frame-conversion authority.

Save during this context writes the committed root checkpoint while retaining the Part draft.
Root Save/Save As metadata survives nested Part/Sketch workflow cancellation and publication.
Cancel restores the complete root definition, resource/catalog ownership, history, visibility and
saved-state boundary; only presentation revisions advance. Finish requires complete fresh Part
geometry and no pending child task or partial history workflow, then replaces that Part and its
owned Sketch catalogs in one normalized Project candidate. Every affected downstream Part and
Assembly relationship must validate before one clearly labelled root Undo entry is published.
Failure retains the root checkpoint and the editable draft for correction.
The negative downstream-rebuild proof starts with an independently verified finite fixed-datum
cut, changes its adaptive source thickness from 1 to 8 mm, and rejects Finish when the moved target
no longer intersects that finite cut. The test retains the exact root checkpoint and saved bytes;
Through All cuts keep their normal target-following semantics.

Project-linked constructions/derived paths and Assembly-owned Hole recipes are read-only through
ordinary child editing: edit the owning Project command or explicitly detach at the root. Hole
placement coordinates continue to be derived by their existing layout authority. Part parameters
bound to Assembly parameters route to that Assembly driver. Ordinary candidate publication checks
these ownership rules before any child history change, and Finish repeats the check against the
initial normalized definition. Named view and representation switching cannot change the active
manufacturing/edit scope mid-context.

Child constraint/dimension catalogs have a durable Project-owned GUI sidecar and follow the root
history/dirty/publication boundary; they are never dropped on return to the Assembly. The
[file-format contract](file-format.md#project-owned-child-sketch-catalogs--198-gui-sidecar) owns the
exact envelope and atomic publication. Focused proof is `[project-part-context]`; native-only
`[project-part-context-native]` reopens the child Source Sketch and its owned catalogs. These proof
sources do not by themselves claim the complete 198 acceptance gate; current executed evidence
and remaining combinations stay in [the MVP plan](mvp-plan.md).

The connected partial X02 proof in `tests/gui/gui_configurable_housing_tests.cpp` uses the imported
advanced housing's actual boss-Hole Entry to drive the adaptive cover support. The housing owns
the patterned blind-Thread mounting layout; coordinated cover clearance consumes that layout.
Both cross-Part edges therefore run from housing to cover. A reverse cover-driven machining
command rejects atomically as a whole-Part dependency cycle. This proof does not introduce
feature-level feedback or snapshot authority to bypass the directed graph.
Within the housing, the mounting Hole and its feature Pattern precede the Body Pattern through
the current Body dependency. Both translated copies inherit the two bores. The proof checks this
history order, bore voids and adjacent retained material in all three solids; the hand volume
formula subtracts the analytic pair-cut volume from each solid.

The same connected Project combines sparse boss-size/material state edits, driver-spacing updates,
compatible and lost-face supplier revisions, a failed downstream participant, native state-table
and context/Source Sketch navigation, root Undo/Redo, save/open and structured STEP reimport.
Hand volume/density formulas and independently converted bore-axis frames supply physical oracles;
Primary intent and the immutable imported base remain separately checked. Block 204's complete
X02 scope, including real text and the later interaction/acceptance combinations, remains assigned
there. Registration of this proof does not claim that its native runs have executed; the MVP plan
records actual verification and numbered acceptance.
