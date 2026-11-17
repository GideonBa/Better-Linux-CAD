# Parameter dependency tree — implementation plan after Block 204

Requested on 2026-11-17. **Planned Blocks 205–211; not implemented.** The
[main plan](mvp-plan.md) owns numbered progress. This extension follows completion of 204;
it does not extend the acceptance obligations of the original 145–204 program retroactively.
The [sequence](inventor-parity-sequence.md#post-204-parameter-dependency-navigation) reserves the
blocks; [PN01–PN07](inventor-parity-test-plan.md#post-204-parameter-navigation-scenarios)
own their acceptance scenarios. Type/action names below describe proposed responsibilities.

## Requested result

Press **E** in the modeling canvas to switch to a graphical, left-to-right Assembly/Part tree.
The view is rooted at the active document/edit context, with one card per occurrence:

```text
Assembly1
├─ Assembly2
│  ├─ Part3
│  └─ Part4
├─ Assembly3
│  ├─ Assembly4
│  └─ Part5
├─ Part1
└─ Part2
```

- On first entry the supplied small example is fully expanded. Subsequent entries restore its
  folds. Large trees use lazy expansion with explicit child counts, following measured budgets.
- Double-click an Assembly card header to fold/unfold all its descendants. The Assembly card
  remains visible so the branch can be reopened. Folding Assembly1 leaves its root card;
  folding Assembly2 hides only Part3/Part4. Reopening restores the branch's previous inner folds.
- Each card has a separate parameter dropdown. It opens a table on that card; several cards
  can have tables open for comparison. Parameter dropdowns and hierarchy folding are independent.
- Clicking a parameter row highlights cards whose formulas reference that exact parameter,
  including formulas in another Part. Matching parameter names or numeric values is insufficient.
- Opening Assembly2 as a separate root shows Assembly2 and its descendants only. Other assemblies
  that happen to reuse it do not appear as parents or extra branches.
- Folding the diagram changes presentation only. It does not hide/suppress CAD components or
  exclude their parameters from evaluation. Changing CAD visibility remains its existing command.

The table follows the supplied column-based sketch, using BLCAD controls and styling:

| Name | Formula | Value | Units | Dimensionality / type | Source / status |
| --- | --- | --- | --- | --- | --- |
| Dia | — | 20 | mm | Length | Local literal |
| Dia3 | Part2::Dia * 4 | 80 | mm | Length | Linked formula |
| bolt_count | — | 8 | 1 | Count | Assembly binding |

`Part2::Dia` is a proposed readable rendering of a qualified reference, not an already-supported
formula syntax. A source picker binds stable document/parameter identities; duplicate display
names cannot choose the source. Exact syntax and persistence are frozen in 206 before a writer
change. Units/type are derived from existing quantities and are not arbitrary editable strings.

## Existing foundations and actual gaps

| Authority already present at Block 184 | Reuse / bounded extension |
| --- | --- |
| [Parameter and quantity model](parameter-model.md), [Parameter](../include/blcad/core/parameter.hpp) | Typed IDs, values, scope and optional formulas; no parallel parameter registry. |
| [ParameterExpressionEvaluator](../include/blcad/core/parameter_expression.hpp), [formula contract](parameter-expression-mvp.md) | Existing local name grammar, Length/Count unit rules and returned referenced IDs. Extend resolution for qualified sources rather than implement a GUI evaluator. |
| [DependencyGraph](../include/blcad/core/dependency_graph.hpp) | Directed edges, direct/transitive dependents and topological/cycle checks; add qualified edge provenance to the Project query. |
| [Assembly bindings](assembly-parameters-mvp4.md), [Project](../include/blcad/core/project.hpp) | Explicit Assembly-to-Part bindings and candidate-based root parameter propagation. General sibling/subassembly formula evaluation is an extension, not an existing guarantee. |
| [AssemblyHierarchyTraversal](../include/blcad/core/assembly_hierarchy.hpp), [occurrence descriptors](../include/blcad/core/assembly_leaf_occurrence.hpp) | Stable subassembly/component paths and repeated definitions; add an explicit rooted projection and all-authored-occurrence query. |
| [ShellParameterTools](../src/gui/shell/shell_parameter_tools.cpp), [GuiDocumentSession](../include/blcad/gui/gui_document_session.hpp) | Owned parameter task, local staging, validated Preview/Apply and Part/Project transactions; share the draft/command layer with the new table. |
| Planned 189–198 | Reuse delivered model-state, reference, driving-context and atomic adaptive Project authorities when this extension starts. Audit their actual APIs in 205. |

Current general parameter formulas resolve names inside one Part and evaluate Length/Count.
Angle literals exist; general Angle formulas are not implemented. Curve-local expressions have a
separate documented grammar and stable parameter references; their parser/AST remains its owner.
The existing parameter manager edits a fresh Part only and lists direct consumer IDs. The current
Project parameter updater addresses the root Assembly's explicit bindings. Neither is already a
cross-document expression browser/editor. The current native Project owns embedded definitions
by value; native external Part loading is not supplied by the existing STEP asset registry.
Audit 194–195 first, then extend their native reference authority where needed. Reuse existing
portable-path, verified-read and atomic-publication services without treating a native Part as STEP.
Global project parameters, automatic parameter renaming and arbitrary new formula functions are
outside this extension unless a preceding block owns them.

## Identity, document scope and repeated assemblies

Keep the occurrence tree and parameter dependency graph separate. Hierarchy edges say “contains”;
formula edges say “reads this parameter”. Shared definitions form repeated cards in the occurrence
tree; a formula graph can cross sibling branches without becoming another assembly hierarchy.

Proposed query identities:

```text
ParameterKey      = owning DocumentId + ParameterId
OccurrenceKey     = viewed root/context + full SubassemblyInstanceId path
                    + optional ComponentInstanceId + referenced DocumentId
EvaluationContext = explicit model state / representation / driving occurrence, when applicable
```

Use structured typed keys, not labels, file basenames, row numbers or delimiter-split strings.
The owning Project/reference authority supplies asset identity/revision when crossing loaded-file
boundaries. An occurrence path identifies a card; it does not turn a definition parameter into
an independent occurrence parameter. Different effective state values require an explicit existing
state/context qualifier. Never infer a different parameter value merely from flexible placement.

Root selection is captured when entering the view from the active document/edit context. Selecting
or folding a card does not change it. Explicitly opening/activating another document changes the
root through the normal context command. The query starts at that root and follows child links
only; it does not walk reverse “used by assembly” links or scan the workspace for parents.
Extend the current root-only traversal with a read-only subtree/context projection; do not rewrite
the Project's persisted root just to display Assembly2. A standalone Part has one root card/table.

Build the tree from authored component/subassembly occurrences, not from the list of registered
member Part definitions. Unplaced registered definitions are not fabricated occurrences. Existing
visible-active leaf resolution is insufficient for the whole tree: hidden/suppressed occurrences
must remain inspectable with explicit status and optional filters. Their evaluated values follow
the actual state availability; hidden 3D geometry is not a reason to invent or drop parameter edges.

Expansion, table-open state, camera/zoom and column widths belong to UI/session state, keyed by
root and occurrence. They do not dirty CAD documents or create Undo entries. Repeated Assembly2
cards under different paths fold independently. Removing a path invalidates its UI state safely.

## Qualified formulas and one evaluation authority

1. Audit the reference/Project dependency authorities delivered in 194–195. Reuse their stable
   ownership, model-state and driver-context rules; implement only missing parameter capabilities.
2. Promote any missing Assembly formula/child-document update operations into that same Core
   authority, so Assembly and sibling-Part formulas use one validated resolver and evaluation plan.
3. Extend the existing parameter expression parser/resolver boundary for typed qualified inputs.
   The same unit/domain checks produce values and exact referenced IDs for both evaluation and
   usage tracking. Old local names keep their existing scope and canonical formula strings.
4. A parameter selected from another card inserts an explicit reference. The persisted identity is
   the document/parameter plus an authored state/driver qualifier where necessary. Human paths are
   labels. Reject ambiguous/unresolved names; do not reinterpret a bare local `Dia` globally.
   Supply that same resolution context to load/replay, edit, full and incremental evaluation;
   do not copy external values into hidden local parameters to make only one path succeed.
5. Direct bindings and formulas have explicit single-driver ownership. A target already driven by
   an Assembly binding cannot also acquire a conflicting formula or literal override silently.
   Show the source and route edits to that authority, or require an explicit supported rebind.
6. Topologically evaluate the affected Project candidate, including chains through local formulas,
   Assembly bindings, cross-document sources and relevant adaptive/state dependencies. Cycles
   reject before publication. There is no iterative feedback solver or “last writer wins”.
   Define supported expression/reference size, nesting and graph-work budgets before enabling
   the new input paths; unsupported limits return a diagnostic without partial publication.
7. Persist only new authored reference/formula intent through the existing Part/Assembly/Project
   adapters. Choose the necessary schema/version in 206 after auditing 185–204; do not reserve an
   arbitrary next version now. Legacy records keep their bytes without this new capability.
   Reverse indexes, resolved values, highlights, card layout and compiled products stay derived.

Existing Length/Count formulas are the initial supported formula types. Show Angle literals and
binding-owned values truthfully; do not enable Angle formulas or other unsupported quantities.
The read-only view may inspect structurally valid intent when geometry is unavailable; derived
values carry their actual stale/unresolved status and editing still requires a valid candidate.
Reference-mode Parts remain readable and immutable. Geometry-only STEP imports with no authored
parameters show an empty table/status; a measured diameter is not fabricated into a model parameter.

## Standalone source resolution and file relocation

A standalone Part5 must retain its own explicit Part3 source reference/catalog entry alongside its
formula, so it can resolve dependencies without opening their common Assembly or a Part3 editor.
Its evaluation context loads the dependency closure through the same expression/reference engine;
a hidden temporary Assembly or a second evaluator is not an authored model requirement.
An embedded Project definition remains owned by that Project; moving a separately exported Part3
file does not convert the embedded definition into an external live reference.

Separate three concerns in the native reference contract frozen in 206:

- **Identity:** the reference authority's asset namespace plus expected DocumentId and ParameterId,
  with any explicit state/driver qualifier. An ID records which document is intended, not its location.
- **Locator:** a canonical location relative to a declared project/source root, or an explicitly
  configured library root. Never rely on the current working directory or an open editor tab.
  Reuse existing path/root constraints; locating outside a root requires an explicit supported root
  mapping/import action, not an unchecked absolute path or parent-traversal bypass.
- **Revision:** accepted source revision/fingerprint and evaluation policy. Reuse verified reads and
  freshness checks. A digest proves observed bytes; it is not a replacement for document identity.

A project/reference catalog maps registered source identity to location. A source lookup checks the
recorded locator and explicitly registered current locations in its declared roots. The catalog is
not an omniscient file database: an operating-system move while BLCAD is closed can leave it stale.
Offer a bounded **Search project for source** action and **Locate source / Relink** file picker.
Search only configured roots for explicitly referenced documents, never the entire disk or parent
assemblies. Metadata/identity lookup must not evaluate every unrelated model or execute file content.

| User filesystem change | Planned behavior |
| --- | --- |
| Move the entire project directory, retaining its internal layout | Resolve project-relative locators from the new root; preserve all source/parameter identities. Library sources retain their separate declared root mappings. |
| Move/rename Part3 inside the project | Use an already registered new location when available; otherwise bounded project search or Locate Source finds candidates. A uniquely verified candidate with the expected identity and accepted revision may restore the locator without rewriting the formula. |
| Move Part3 outside all configured roots | Show Missing Source and retain authored references. The user locates it and explicitly maps a supported new root/location; no disk-wide guessing. |
| Find a different file also named Part3 | Reject relocation if expected document/parameter identity does not match. Replacing the source is a separate deliberate operation with compatibility/cycle/geometry validation. |
| Find several copies with the same IDs | Show ambiguity and candidate locations; require an explicit choice. OS copies can retain IDs and even identical bytes, so IDs/hash/name alone cannot establish which copy was intended. |
| Find the right document with changed content | Separate source refresh from locator repair. Review/validate the changed revision through the ordinary update policy; relocation must not silently accept new values. |

Inspection/search and automatic candidate discovery are read-only. A unique same-revision match
can be inspected as a validated runtime candidate and shown as **Source moved — locator repair pending**.
Explicit Relink publishes that location through the existing transaction/publication path; ordinary
Save persists committed intent and never adopts a discovered locator or revision implicitly.
Do not mutate a read-only consumer, dirty a document merely by opening it or rewrite formulas during
search. A Part5 opened alone can save its own verified reference repair without its Assembly.

Relink preview checks expected document/parameter IDs, unit/type, state/context, fingerprint and
cycles, then validates the complete affected candidate. Apply updates the canonical reference
catalog/locator and known affected bindings as one scoped transaction. A rejected/stale preview
preserves all committed intent, source bytes, results and history. External/read-only libraries are
never modified just to repair a consuming document's locator.
Verify the source's required dependency closure too. Moving Part3 must not reinterpret its own
relative links against an unrelated directory; missing nested Parts/assets retain their diagnostics
until their owning reference policy resolves them. File organization must preserve those roots or
repair their locators explicitly, not silently rebase an immutable source's internal references.

Discovery/repair does not move, delete or rewrite source files. Undo of Relink restores reference
metadata; it cannot undo an OS file move. If a later in-app file-organization command is included,
it must reuse 195's multi-file publication/recovery rules, define its filesystem undo boundary and
update registered locators explicitly; it is not implied by adding this picker.

For Save As, relocation and copying, preserve the distinction between the same document at a new
location and an independent Part definition. An explicit independent-copy operation must assign
new definition identity through the document-copy authority audited in 205. A raw filesystem copy
has no such signal and remains a potentially
ambiguous candidate. Never silently rewrite IDs inside a discovered source to make it fit.

Pending/missing sources remain inspectable with authored IDs and explicit stale/unresolved status;
retain the last committed result as historical evidence where available, without a fresh-value claim.
Successful same-revision relocation keeps formula text, qualified parameter identities and values
unchanged. Recheck source evidence before repair Preview/Apply and publication of its accepted
locator, including file replacement during read. Ordinary Save may retain unresolved authored
references under existing save rules, but cannot mark stale results fresh or accept changed bytes.

## Usage query and highlighting

The Core/Project query produces a revision-tagged read-only snapshot: occurrence nodes, parameter
rows, typed usage edges, direct consumers, transitive consumers and unresolved-reference diagnostics.
Local expression references come from the existing evaluator; curve-expression uses come from its
existing AST reference query. Assembly bindings contribute their explicit source/target IDs.
Do not use substring searches in formula text or treat the entire feature graph as formula usage.

Each usage site keeps its owning document, exact target parameter or Sketch/curve role and edge
kind: parameter formula, cross-document formula, binding, curve expression or direct feature use.
Curve-local grammar/references stay Part-owned; an externally driven owned parameter can feed
that existing grammar. This extension reads its reference metadata without inventing another
cross-document curve language. The default Formula filter highlights actual formula consumers;
binding links remain distinguishable and traversable. Direct feature-only use is available as a
separate filter. For a curve expression
without a target parameter row, the detail names the owning Sketch/curve and expression field.

Selecting a source gives it a distinct source marker. Direct formula consumers receive a solid
accent and their actual row/reference is marked; an “Include indirect uses” toggle adds transitive
consumers with a different style and their full dependency chain. Optional selected dependency
arrows differ from the neutral assembly containment connectors and have a legend. Color is paired
with labels/icons so the meaning remains usable without color perception.

Restrict displayed hits to occurrences reachable from the viewed root. Shared definition consumers
highlight every matching occurrence in that scope. Count unique affected occurrences separately
from matching parameter rows; repeated placements must not inflate a single definition's row count.

If a matching Part is under a folded Assembly, keep it folded and show an aggregate badge such as
“2 affected components inside”. That badge is not evidence that the Assembly itself owns a formula.
“Show uses” or clicking a search result may explicitly unfold the required path; selection alone
must not unfold the whole tree. Source selection can stay in a labeled info bar when folded away,
with an explicit “Show source” action. Clearing selection removes dependency highlighting.

A valid source outside the viewed subtree is shown as an external reference in the table/detail;
it does not add parent assemblies to the tree. A missing/unavailable source retains authored
identity and a clear unresolved/stale status, with no fresh-value claim or guessed replacement.
Resolve only explicitly authored sources through existing verified reference policies. Do not scan
other assemblies or silently load an unrelated parent to make a formula succeed.

## Native interaction and parameter editing

One explicitly owned ShellWindow tool controls a central Qt graphics view/scene and parameter table
models. Keep the accepted ribbon, docks, command search and 3D viewport. The existing CAD canvas is
restored on leaving the diagram with its camera, selection and filters intact. Qt draws the cards
and connectors; no CAD geometry computation is required to open/fold/highlight the graph.

**E** is the requested default shortcut and a discoverable ribbon/search action uses the same owner.
Audit the key map after 204 and resolve conflicts explicitly. E works from the CAD canvas/tree,
but remains text input in formula/value fields, search, dropdown editing and IME composition.
Ignore auto-repeat. Active Sketch tools, captured grips, modal dialogs and foreign tasks retain
control; entering this view cannot cancel or commit them implicitly. Disabled entry explains why.
E toggles back when no edits are pending. Esc first cancels cell editing/reference picking, then
closes a parameter dropdown, then leaves the view. Pending edits use an explicit Apply/Discard/Stay
choice owned by the edit task; entering/leaving is never an automatic Apply.

| Input | Meaning |
| --- | --- |
| Assembly-header double-click or branch chevron | Fold/unfold descendants, with the card retained. |
| Parameter dropdown | Open/close that node's table without changing child folds. |
| Parameter-row single-click | Select the exact source and highlight usage. |
| Editable value/formula cell double-click | Enter the existing validated parameter edit workflow. |
| Open/Activate context action | Explicit document/context navigation; not folding. |

Initially deliver the read-only diagram/table in 208, then shared editing in 209. Factor reusable
parameter rows, formatting, validation and draft commands out of the existing owned manager;
do not wrap its widgets by discovery or introduce competing actions/evaluators. Node tables use
Qt model/view and are created lazily only when opened/visible. Several open tables share immutable
query data; the diagram layout accounts for their size without overlapping cards/connectors.
Tables of folded descendants hide with their cards and restore their prior open state on unfolding.
Staged edits belong to the shared draft, not table widgets. An invalid active cell must be corrected
or cancelled before folding can hide that editor; folding never publishes or silently drops it.

Literal values and formulas use the existing mutation authorities, extended at Project level in
206 where necessary. Formula-driven result cells and binding-owned target cells remain read-only.
A formula source picker preserves the target edit while selecting a qualified source in another
node. Local type, unit and scope information stays visible; derived status cannot be edited away.

Standalone Parts retain the existing Part candidate and transaction. In a Project, Preview
re-evaluates all affected definitions and geometry/assembly consumers in a disposable
complete Project candidate, including consumers under folded branches. Editing a shared definition
lists the affected occurrences in the loaded Project; it does not become an occurrence override.
Apply rechecks revisions and publishes one root undo step with children, sidecars and caches in
sync. Invalid units, cycles, missing sources, immutable targets, failed downstream geometry or a
stale context preserve the complete committed state. Undo/Redo and Save/Open retain exact references.
Use 195's multi-document publication rules; inspection cannot require writing dependencies.

## Numbered implementation after 204

### Block 205 Rooted occurrence and parameter query

**Prerequisite:** 204 complete; audit the actual 189–198 parameter/state/context authorities.
Freeze root, identity, visibility/suppression and missing-source policies. Extend the existing
hierarchy query for explicit active roots and include Part occurrences and typed parameter rows.
Provide deterministic immutable snapshots without OCCT/UI dependencies. Include repeated
subassemblies, standalone Parts and standalone Assembly2. Record reusable APIs and remaining gaps.
**Proof:** PN01; strict root membership, distinct paths, identical-name definitions and zero document
mutation. No new writer is needed for this read-only projection.

### Block 206 Qualified parameter formulas and Project evaluation

**Prerequisite:** 205 and delivered 194–195. Extend the existing resolver, reference intent and
Project transaction/recompute commands for the missing cross-document/Assembly formula cases.
Freeze exact grammar, stable reference fields, single-driver rules, necessary versions, migration,
external-source policy and diagnostics before the first writer change. Include standalone native
source references, root-relative locators and relocation versus refresh/copy semantics from the
section above. Preserve local formula bytes.
**Proof:** PN02; local/cross-document chains, collisions, units, cycles, conflicting bindings,
shared-definition contexts, immutable sources, strict Part/Assembly/Project roundtrip and rollback.
Source moves/copies must preserve or deliberately repair the exact expected identities.

### Block 207 Typed reverse usage and impact query

**Prerequisite:** 206. Build an exact typed reverse index from authoritative expression/reference
and binding dependencies. Return direct/transitive consumers, occurrence projections, folded-branch
aggregates and reasons/paths. Include curve-expression reference metadata without reparsing it.
**Proof:** PN03; same-name/value decoys, cross-branch chains, repeated definitions, outside-root
sources, filtered/suppressed nodes and exact distinct counts. Ordinary feature usage is distinguishable.

### Block 208 Native tree view, E and per-node parameter tables

**Prerequisites:** 205,207. Integrate one owned Qt scene with the current shell. Deliver rooted
cards, deterministic left-to-right layout, independent fold/table controls, pan/zoom/fit, node/name
search, parameter-row highlighting and collapsed badges. Route E/ribbon/search through one action.
Keep document editing with the existing commands until 209 is accepted.
**Proof:** PN04; real double-clicks, dropdown/keyboard/focus events, sibling isolation, root folding,
multiple tables, text entry E, context restoration, task conflicts and 100%/compact 150% captures.

### Block 209 Shared parameter editing and source picking

**Prerequisites:** 206,208. Share parameter draft commands with the current manager. Add editable
literal/formula cells, a qualified source picker and complete Project Preview/Apply/Cancel.
Display binding/read-only ownership and shared-definition impact. Add bounded source search,
Locate Source and verified Relink through the normal repair/transaction owner. Retain source
selection and folds through correctable errors without publishing a partial value or child result.
**Proof:** PN05; 20 mm → 80 mm formula, source change 25 mm → 100 mm, independent physical geometry,
failed downstream consumer, one-step Undo/Redo, Save/Open and unchanged read-only resources.
Include standalone Part5 with closed Part3, locator repair, ambiguous copies and changed-source rejection.

### Block 210 State integration, freshness and large-tree behavior

**Prerequisites:** 209 and delivered 189–198/203. Key snapshots/indexes by model/reference revision,
root, model state and explicit driving context. Track source relocation, root mappings and observed
revision separately. Invalidate on formula/binding/state/source changes;
ignore stale asynchronous jobs. Preserve independent per-occurrence expansion and user filters.
Virtualize large trees/tables; expand children on demand, cache reverse queries and avoid creating
widgets/BRep scenes for hidden rows. Highlighting queries do not trigger recompute.
**Proof:** PN06; state/context changes, repeated/flexible definitions, external refresh, stale jobs,
10/100/1,000 occurrence fixtures, cold/warm build/expand/highlight timings, memory and cancellation.
Record machine/baseline and justified budgets before tuning; no unmeasured responsiveness claim.

### Block 211 Combined parameter-navigation acceptance

**Prerequisite:** all 205–210 complete. Run PN01–PN07 and affected existing parameter, expression,
hierarchy, adaptive/state and session regressions, complete relevant Core/Geometry/GUI suites,
native 100%/compact 150%, startup and real filesystem persistence/source cases. Recheck runtime
identity, inventory mapping, exact atomicity, measured budgets and actual skips before acceptance.
**Proof:** PN07 combines the supplied hierarchy, shared Assembly2, folded consumers, qualified
formulas, source edits, physical results, root switching, history and portable reopening. Update
numbered progress only after the entire extension passes its contracts.

## Expected application walkthrough

1. Edit Assembly1 and press E in the canvas. The small example opens fully expanded with
   Assembly1 as its root; later entries preserve the user's chosen folds.
2. Part3/Part4 appear under Assembly2. Double-click Assembly2 to hide just those descendants.
   Double-click Assembly1 to fold all branches; its root card remains available to reopen.
3. Open the parameter dropdown on Part2 and Part3. Part2 shows Dia = 20 mm; Part3 shows
   Dia3 = Part2::Dia * 4 = 80 mm. The source was explicitly picked, not inferred from “Dia”.
4. Click Part2.Dia. Part3 and its Dia3 formula highlight. With indirect uses enabled, a Part4
   parameter derived from Dia3 highlights differently. An unrelated equal 20 mm value does not.
5. If Assembly2 is folded, its badge announces a hidden affected component while it stays folded.
   Choose “Show uses” to reveal Part3 deliberately. Clicking Part5's diameter works identically
   when it is the actual selected source of a formula.
6. Change the source to 25 mm using the shared edit task. Preview shows Dia3 = 100 mm and the
   affected geometry. Apply publishes one undo step; a failed consumer preserves the saved model.
7. Press E to return to 3D. Open Assembly2 as its own document/root and press E again: only
   Assembly2, Part3 and Part4 appear. An authored source outside that root is labeled external or
   missing; its other parent assemblies are not pulled into the diagram.

## Delivery boundary

This planning change adds no runtime implementation, save-format fields or executed test evidence.
Blocks 185–204 retain their full scope/order. Future implementation uses focused professional commits
with at most 1,500 added lines each and the existing verification/freshness discipline. Reconcile
proposed responsibilities with the actual code at 205; never create a second parameter, hierarchy,
formula or transaction authority to fit this diagram.
