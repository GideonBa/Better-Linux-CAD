# STEP Import Sequence MVP-10

Status: Blocks 137–139 complete; Blocks 140–143 planned. The sequence follows GUI Shell Reset MVP-9R (Blocks 132–136,
`docs/gui-shell-reset-sequence-mvp9r.md`); `docs/mvp-plan.md` is the status authority. An earlier
revision numbered these blocks 132–138, before MVP-9R took that range; only the numbering moved,
never the content or the ordering.

This document is the canonical numbered implementation sequence for importing STEP parts and
structured STEP assemblies in one of two explicit modes:

```text
Reference
EditableBody
```

The sequence begins after Block 136 because editable imports depend on the multi-body,
feature-result, semantic-topology, downstream-feature, and STEP exchange authorities established by
Blocks 48–94, and because the interactive surfaces from Blocks 95–131, rehosted on the MVP-9R shell in
Blocks 132–136, give every import block a mature host workbench.

## User-visible import modes

### Reference

The STEP source remains external geometry authority. BLCAD creates a read-only imported
`PartDocument` definition that may be instantiated repeatedly in local or nested assemblies.
Imported faces, edges, vertices, axes, circles, and points may be selected through stable semantic
import references for assembly relationships and joints.

Reference mode permits:

- component placement, visibility, suppression, grounding, relationships, and joints;
- repeated instances and nested BLCAD assemblies;
- explicit source refresh after freshness validation;
- flattened and structured STEP re-export.

Reference mode rejects Part feature-history edits. Assembly placement never rewrites the source
STEP file.

### EditableBody

BLCAD creates a normal `PartDocument` whose first geometry-producing feature is an
`ImportedBodyFeature`. Each imported solid or surface result receives persistent `BodyId` authority.
Later BLCAD features may consume those bodies and imported semantic topology exactly like other
supported feature results.

EditableBody mode permits new parametric BLCAD operations after the imported base, including the
supported Extrude/Cut, Hole, Boolean, Pattern, Mirror, Fillet, Chamfer, Shell, Draft, Sweep, Loft,
surface, and body-transform families once their Blocks 48–94 contracts are available.

EditableBody does not infer the source CAD feature history and does not make the source BRep itself
parametric. Feature recognition, direct face push/pull, and reconstruction of foreign sketches,
dimensions, constraints, patterns, or design tables remain separate future work.

## Non-negotiable architecture rules

1. Import mode is explicit persistent intent; no heuristic silently chooses Reference or
   EditableBody.
2. The source is addressed by a canonical project-relative asset path plus a cryptographic content
   digest. Absolute machine-local paths are not portable model identity.
3. STEP bytes, OCCT shapes, XDE labels, STEP entity numbers, traversal indices, and memory identity
   are never embedded as Core model identity.
4. Missing or digest-mismatched sources fail closed. Refresh is an explicit command that produces a
   freshness report before mutation.
5. Source units are normalized to millimeters at the Geometry boundary and recorded diagnostically;
   ambiguous or unsupported unit state fails explicitly.
6. One STEP product definition maps to one shared imported Part definition. Repeated occurrences do
   not duplicate geometry or persistent Part intent.
7. Structured STEP assembly import preserves product hierarchy and direct occurrence transforms;
   it does not flatten unless the user explicitly requests a flattened Part import.
8. Imported topology uses BLCAD semantic import identities and recovery metadata, never raw STEP or
   OCCT topology identity.
9. Topology recovery must report Resolved, Lost, or Ambiguous. It never silently remaps an assembly
   constraint or downstream feature input.
10. Reference mode is immutable at the Part-feature boundary. EditableBody mode mutates only through
    new BLCAD feature intent after `ImportedBodyFeature`.
11. Import, refresh, and conversion between modes are transactional: validation and Geometry work
    complete on a copy before Project/Part intent is committed.
12. Imported source files are never overwritten by modeling, assembly motion, recompute, or export.
13. Because the GUI infrastructure exists before this phase, each import block adds the matching
    task/browser/viewport access where user interaction is meaningful. Widgets remain clients of the
    same headless Core/Geometry command and transaction paths.

## Frozen phase order

```text
137 STEP source identity, import modes, JSON, and freshness
138 OCCT STEP/XDE reader and deterministic imported body definitions
139 stable imported topology identity, recovery, and target resolution
140 Reference Part integration with assemblies
141 EditableBody ImportedBodyFeature and downstream modeling
142 structured STEP assembly import
143 integrated import, refresh, edit, assembly, and re-export acceptance
```

Do not merge Blocks 137–143 into one importer. Each block crosses a different persistent or Geometry
authority boundary.

The existing GUI grows with the same sequence:

| Import block | GUI extension |
|---|---|
| 137 | import-mode/source task, portable-path and freshness diagnostics |
| 138 | STEP inspection preview with product/body/unit diagnostics |
| 139 | imported semantic topology display, filtering, and picking |
| 140 | insert/refresh/convert commands for immutable Reference Parts |
| 141 | EditableBody creation and downstream Part-workbench integration |
| 142 | structured product-tree preview and Project import |
| 143 | end-to-end GUI/headless equivalence and failure-atomicity acceptance |

## Block 137 — STEP source identity, modes, JSON, and freshness

Primary boundary: Core external-source intent.

Introduce persistent intent conceptually equivalent to:

```text
StepImportSourceId
StepImportMode = Reference | EditableBody

StepImportSource
  id
  name
  project_relative_asset_path
  sha256_digest
  mode
```

Freeze canonical lowercase JSON spellings `reference` and `editable_body`, path normalization,
digest spelling, duplicate-source rejection, missing-asset diagnostics, and explicit refresh intent.
The JSON stores no STEP payload or derived product/body/topology cache.

The import loader must distinguish:

```text
source available and digest matches
source missing
source changed
source unreadable
```

No OCCT reader is added in this block.

Focused tags:

```text
[core][step-import-source]
[core][step-import-json]
[core][step-import-freshness]
```

### Block 137 source and GUI contract — 2026-10-26

`StepImportSource` stores exactly an id, name, canonical relative file path, lowercase SHA-256
digest and explicit `Reference` or `EditableBody` mode. Both standalone `PartDocument` and `Project`
own an ordered registry; embedded Parts keep separate registries. Ids and canonical paths must be
unique within a registry. Names, equal file content and hardlink aliases do not establish identity.
Refresh preserves identity, path, mode and position, replacing only the accepted digest after another
file inspection.

Paths use `/` separators. Constructors normalize harmless `.` components and repeated separators;
parent traversal, absolute/drive paths, backslashes, controls and nonportable filename spellings are
rejected. Digest constructors normalize hexadecimal case. JSON requires canonical path and lowercase
digest spelling, exact lowercase mode strings, and exactly five source fields. Optional
`step_import_sources` arrays are omitted when empty, preserving legacy canonical output. Unknown
source fields, malformed records and duplicates fail closed. See `docs/file-format.md`.

All paths resolve relative to the containing saved BLCAD document directory, including embedded Part
sources. Absolute filesystem roots remain runtime context. Inspection streams the complete file
through OpenSSL SHA-256 and distinguishes `available`, `missing`, `changed` and `unreadable`.
It requires regular files and real directories: symlinks in the root or asset traversal are rejected,
including links inside the root. File identity, size and timestamps are checked before/after reading
and against a newly opened path. A file changing during inspection yields no accepted fingerprint.
STEP format validation, units and geometry transfer follow in later blocks.

`GuiStepImportWorkbench` prepares disposable registration, refresh or removal evidence and applies
one Part/Project transaction. Evidence binds canonical document intent, saved path, original source,
availability and observed digest. Apply rereads and rejects changed evidence without partial mutation;
a new inspection can recover. Unchanged refresh adds no history and preserves Redo. Removing an
inspected missing/unreadable source is allowed and deletes only intent, never the file.

Documents with unavailable assets remain openable for recovery. Open, Save and history restoration
report freshness diagnostics without accepting a new digest or claiming imported geometry. Existing
authored geometry remains usable because Block 137 has no source-consuming feature yet. Future
consumers must require matching source evidence. Ordinary Save retains the registered digest even
when bytes changed. Save As into another directory requires matching assets at every new relative
location; it neither copies files nor silently rebases references. Document, Sketch-sidecar and STEP
outputs cannot overwrite registered assets, including missing paths, canonical aliases and hardlinks.

The browser exposes source identity, name, mode, path, digest and inspected state. **STEP-Quellen**
on the model/Assembly ribbon opens one shell-owned task. Save first, choose an explicit mode, inspect
the file, prepare a change, then Apply. Existing fields stay read-only; refresh changes only the digest.
Embedded Part entries show their owner and are read-only; the source chooser addresses only the
current document's registry. Derived owner scope distinguishes root and embedded registries even
when Project and Part ids are equal. Browser keys include owner kind and length-encoded ids, so
slashes within legal ids cannot alias another source. Input changes discard preview evidence. Save
during a draft writes only accepted intent; Save As, changed model intent, Undo/Redo, New/Open, Escape
and task close discard drafts. Undo/Redo remain reachable during a source draft when the corresponding
history step exists; cancellation precedes history restoration. Escape inside the file chooser
dismisses only that chooser. At 1280×800 the task and source table fit without horizontal scrolling;
full paths remain available in tooltips, and read-only SHA fields wrap without changing copied text.
Action identity:
`blcad.action.step_sources`; task/widget prefix: `blcad.step_import.`.

Loading a Part definition into an Assembly requires matching registered assets at the saved target
document's relative paths. Loading a Project as a child Assembly rejects Project-owned sources,
whose owner cannot survive that operation. Embedded Part registries remain intact and undergo the
same target-root check. No source is silently retargeted.

Focused GUI tags: `[step-import-workbench]`, `[block137-step-import]`,
`[shell-step-import-workflow]`, `[native-shell-step-import]`. `docs/mvp-plan.md` owns numbered completion.

### Block 137 acceptance — 2026-10-26

All twenty-one new cases pass: nine Core cases, seven headless GUI cases and five shell cases. The
shell workflows combine authored Sketch/Extrude geometry with both source modes, supplier byte
changes, stale Apply rejection, exact Undo/Redo, removal and canonical reopen; a nested Project
workflow relocates the complete directory and recovers missing/unreadable files without changing
occurrence or definition identity. Lifecycle cases cover Save during drafts, Save As, New/Open,
Escape, task close, busy Sketch guards and real Undo/Redo actions. Ownership cases include equal
Project/Part ids, equal source ids and slash-containing ids. Native input covers file chooser accept
and Escape, long paths, full fingerprint display and reachable controls at 1280×800.

| Suite | Cases | Assertions | Result |
| --- | ---: | ---: | --- |
| Focused Core | 9 | 405 | All passed |
| Focused GUI and shell inventory | 15 | 2,158 | 14 passed; one native-only skip |
| Focused native source workflows | 5 | 1,569 | All passed; no skips |
| Full Core | 539 | 16,579 | All passed |
| Full Geometry | 441 | 30,227 | All passed |
| Full offscreen GUI | 547 | 71,155 | 527 passed; 20 native-only skips |
| Full canonical native shell plus Block 137 | 356 | 67,620 | All passed; no skips |
| Shell closure gate after advancing the plan | 3 | 299 | All passed |

Build all four `dev-gui` targets, then use the focused commands in `docs/development-setup.md` and
the canonical native filter in `docs/gui-shell-acceptance-mvp9r.md` with `[block137-step-import]`
appended. Full Core/Geometry binaries and the unfiltered offscreen GUI binary establish broad
regression coverage. Native coverage includes the twenty intentionally skipped offscreen cases.
Production/test sources remained frozen during completed final runs. Separate native application
startup stayed running until the intentional six-second timeout (124).

Native captures under `/tmp/blcad137-native-focused` and `/tmp/blcad137-native-full` were inspected,
including source registration, changed-source refresh, relocated Project recovery, wrapped SHA fields,
registry inspection and the existing combined Loft workflow. No high-severity graphics error,
BadDrawable or fatal lifecycle error occurred; the known medium OCCT incomplete-visual depth/stencil
warning remains. Changed C++ formatting and working/staged whitespace checks pass. Evidence logs
use `/tmp/blcad137-` with `core-focused.log`, `gui-focused.log`, `native-focused.log`, `core-full.log`,
`geometry-full.log`, `gui-full.log`, `native-full.log`, `startup.log`, `closure.log` and `build-final.log`.
Earlier development failures are superseded by these completed final runs.

Block 137 is complete. The Block-136 manifest remains a historical accepted milestone; the closure
gate checks that later plan progress still covers it. STEP geometry reading, units, imported topology,
Reference Part immutability and ImportedBodyFeature consumers remain explicitly outside this block.

The closure command is `BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 QT_QPA_PLATFORM=offscreen
./build/dev-gui/blcad_gui_tests "[integration][gui-feature-coverage]"`. It passed after setting
`implemented_through: Block 137` and `current_block: 138`; the historical manifest was not rewritten.

## Block 138 — OCCT STEP/XDE reader and imported body definitions

Primary boundary: deterministic STEP-to-Geometry translation.

Implemented on `feat/step-xde-inspection` from verified main `61e60b6`
(2026-10-29), after the issue queue is closed. The reader consumes a secure immutable
SHA-256-verified snapshot through OCCT's stream path, rather than reopening an unchecked
asset filename. External STEP references are rejected before transfer. Product-local
body definitions and direct occurrence transforms remain separate derived data; equal
definitions are not silently merged because their geometry or names match. Runtime ordering
and ambiguity diagnostics do not establish the persistent topology authority of Block 139.

The existing source registry task keeps Block-137 metadata registration/refresh/removal
semantics. A separate geometry-inspection action shows product/body/unit diagnostics
and disposable occurrence previews without creating Part features or accepting a changed
source digest. The acceptance evidence below establishes completion of this boundary.

Add a Geometry adapter using the OCCT STEP/XDE reader path. It must:

- read supported STEP files without mutating Core intent;
- normalize declared source units to millimeters;
- distinguish solid, shell/surface, compound, product definition, and occurrence structure;
- derive deterministic product/body definition order independent of OCCT traversal order;
- preserve supported product/body names as metadata without making names unique identity;
- reject empty transfer, invalid geometry, unsupported unit state, and non-finite transforms;
- publish diagnostics for any narrowly frozen sewing/healing operation rather than silently changing
  geometry.

`GeometryShape`, imported XDE documents, transfer maps, and BRep bodies remain derived products.

Focused tag:

```text
[geometry][step-import-reader]
```

### Block 138 reader and inspection contract — 2026-10-29

`read_verified_step_import_source` returns a disposable immutable byte snapshot using the
same no-symlink descriptor walk, regular-file/readability checks, SHA-256 stream and
before/after/reopened identity/size/timestamp checks as Block-137 inspection. Its digest must
match the accepted `StepImportSource`; no model or source bytes are changed. The Geometry
reader parses this snapshot with the STEP stream API and rejects recognized external-file
references before XDE transfer. It never opens another source by an unchecked filename.

`StepImportReader` returns product-local solid/surface bodies, supported names, original
Solid/Shell/Face/Compound product classification, direct occurrence matrices, root occurrences,
unit diagnostics and transfer diagnostics. Declared SI-prefix and recursively conversion-based
length units are normalized to millimeters for both geometry and translation. Missing,
unsupported, repeated/ambiguous length declarations and non-finite values fail. Placements must
be proper rigid transforms; scaling and reflection are rejected explicitly. Finite direct matrices
must also remain finite after rooted hierarchy and body-local placement composition, before either
the reader result or posed preview is published. Empty, unsupported
wire-only and invalid transfers publish no partial result. Optional OCCT ShapeFix/healing and
additional sewing are disabled; transferred BRep validity is checked rather than silently
repairing geometry. Reader settings, processing policy, auto-naming and unit globals are scoped
and restored after success and failure; no background Geometry worker is introduced.

Definitions retain XDE sharing only in transient maps. Canonical recursive geometry/topology
signatures and sorted direct child-transform signatures determine runtime ordering. Names are
metadata/secondary presentation order, never unique product identity. Distinct equal products
and bodies remain separate records with explicit ambiguity/multiplicity diagnostics. Runtime
keys and vector indices are inspection data, not persisted Body/topology identities and not an
across-refresh recovery contract. Block 139 owns that later authority. A headless preview helper
composes rooted direct occurrence chains onto local body shapes without flattening persistent
Part intent or applying a definition location twice. Reading and preview share the same rooted
placement checks and reject expansion beyond 100,000 posed body occurrences without a partial result.

The existing **STEP Sources** task adds **Inspect Geometry** separately from metadata inspection,
Register/Refresh/Remove and Apply. Registered sources must match their saved digest; a new draft
can be inspected without being registered. A disposable product/body/occurrence tree, millimeter
unit report and diagnostic panel accompany non-selectable posed viewport bodies. The authored
scene, JSON, dirty state and Undo/Redo remain unchanged. Draft edits, Cancel/Escape/task close,
Save As/Open/history restoration and changed/unavailable source notifications discard the
inspection and restore its camera. Ordinary Save writes only accepted intent and keeps a valid
read-only inspection. Changed bytes require an explicit source refresh before another geometry
inspection. Atomic replacement with identical accepted bytes keeps the inspection valid and renews
its file/directory watches, so a subsequent in-place change still discards the preview. Existing
Block-137 opaque-byte metadata registration remains available; geometry
inspection requires valid supported STEP. Imported semantic picking and persistent import commands
remain Blocks 139–143.

New focused tags are `[core][step-import-snapshot]`, `[geometry][step-import-reader]`,
`[step-import-inspection]`, `[block138-step-inspection]` and `[native-step-inspection]`.
An explicit opt-in `[step-import-profile]` observes six reads of the existing connected
20-blade curved rotor, with fixture recompute/export outside timing and source-volume checks.
It records snapshot, full reader and posed-preview timings without a portable performance budget.
Raw observations and host/method details are retained in
`docs/benchmarks/block138-step-inspection.json`.

### Block 138 acceptance — 2026-10-29

The same frozen runtime passes all five build targets and the following checks. Counts include
existing affected workflows; skipped cases are the explicit native-only guards under offscreen Qt.

| Check | Cases | Assertions | Result |
| --- | ---: | ---: | --- |
| Focused source identity, freshness, JSON and byte snapshot | 12 | 681 | Passed |
| Focused STEP reader, units, ordering, sharing and invalid placement | 12 | 634 | Passed |
| Focused inspection/source task and required Shell inventory | 20 | 2,402 | 17 passed; 3 native-only skips |
| Full Core | 560 | 17,987 | Passed |
| Full Geometry | 491 | 36,718 | Passed |
| Full offscreen GUI | 625 | 82,197 | 597 passed; 28 native-only skips |
| Required Shell inventory after final status update | 3 | 306 | Passed |
| Native 100% source/inspection/viewer/required inventory | 21 | 3,267 | Passed; no skips |
| Native 150% inspection | 2 | 673 | Passed; no skips |
| Isolated connected 20-blade rotor inspection profile | 1 | 64 | Passed; observations only |

Native captures show a visible, correctly fitted preview in empty Part/Project documents and
explicit source-refresh recovery at both scales. Assertions retain exact document/history
invariance, cancellation/camera restoration, changed-byte rejection and file-watch continuity
after identical-byte atomic replacement. Two overflow regressions and the watcher regression
first failed against the unfixed implementation; their assertions are unchanged in the green run.

The final **904-file** runtime freeze is
`c465168447c31608584d4f7109c74b3ba6c1274d3f447c8d97808e961859118b`.
Commands, exact log/capture locations, intermediate fixture corrections and native startup
(intentional timeout **124**) live in `docs/development-setup.md`. The isolated 615,000-byte,
122-face rotor has source-volume agreement at **75,778.64478 mm³**; its five warm reader samples
have median **169.8 ms** and observed maximum **173.2 ms**. Reader time includes its verified
snapshot; separate stage columns are not additive, and five warm samples establish no portable
tail budget. No worker, persistent imported topology or imported feature is introduced.

Block 138 is complete and delivered in PR #130, merged as `ca2db6e`. Main exactly matches
tested head `a0aaa7c`, including all 904 runtime fingerprints; Block 139 begins below.

## Block 139 — Stable imported topology identity and recovery

Primary boundary: persistent selection identity over foreign geometry.

Implementation started on `feat/step-import-topology` from verified main `ca2db6e` after
Block 138 is merged as PR #130. Core owns persisted catalog identities and normalized recovery
metadata; Geometry owns current-shape matching and analytic capabilities; the existing source
inspection task supplies display, filtering and semantic picking. Block 139 is accepted on
2026-10-29 after focused, full, native, visual and startup verification. Reference Part and
ImportedBodyFeature integration remain Blocks 140–141.

Introduce semantic imported body/face/edge/vertex identities that are independent of STEP entity
numbers and OCCT traversal order. Initial import creates a persisted BLCAD import-topology catalog
containing stable ids plus normalized recovery descriptors. Geometry resolution maps catalog entries
to the current imported shape and reports exactly one of:

```text
Resolved
Lost
Ambiguous
```

Freeze capability projection for supported imported geometry:

```text
planar face       -> Plane
cylindrical face  -> Cylinder + Axis
linear edge       -> Line
circular edge     -> Circle + Axis + center Point
vertex            -> Point
```

Imported semantic references must work for downstream Part feature inputs and local or
cross-hierarchy Assembly target resolution. Unsupported analytic classification remains selectable
only where an explicit later capability exists; it is never approximated as another primitive.

Focused tags:

```text
[core][step-import-topology]
[geometry][step-import-topology-recovery]
[geometry][step-import-assembly-target]
```

### Block 139 accepted catalog and resolution contract

Each `ImportedTopologyCatalog` captures one selected source product definition. Its own BLCAD id,
source id, baseline digest, normalized product evidence and ordered entries are persistent intent.
Entries have BLCAD Body/Face/Edge/Vertex ids; subtopology names an enclosing Body entry in the same
catalog. These are catalog identities, not Block-141 authored `BodyId` outputs. The initial runtime
product selector and current product/body vector indices never enter JSON. Names, STEP entity ids,
XDE labels, OCCT addresses and enumeration order never resolve a saved reference.

Version-1 descriptors store product-local millimeter bounds, centroid, volume/area/length, exact
analytic classification where supported, canonical origins/directions/radii, samples and complete
trimmed boundary/equation witnesses. Physical equation and topology evidence is normalized before
hashing; the Block-138 runtime definition keys are not reused as recovery authority. Length-like
scalar evidence uses a `1e-7` grid and directions use `1e-10`; full descriptor comparisons additionally
check the frozen `1e-6` geometric tolerance. Closest-to-origin plane/axis/line origins and a positive
first nonzero direction component remove arbitrary analytic parameter frames. Catalog ids and entry
ids remain unchanged during explicit source-digest refresh; the baseline descriptors are immutable.

Recovery first checks complete Body evidence. A changed Body may instead recover through the
intersection of owners carrying its surviving complete trimmed-Face witnesses, with Body-kind and
product coherence checks. Edge/Vertex evidence alone never reidentifies a changed Body. Missing
evidence is Lost; conflicting or multiple indistinguishable owners are Ambiguous. Each saved entity
then requires exactly one full witness match inside the recovered owner. A changed face may be Lost
while an unchanged face on the same changed Body remains Resolved. There is no nearest, best-score,
name, traversal-position or multiplicity tie-break and no silent rewrite of saved references.

`ImportedTopologyReference` contains catalog id, entry id and explicit capability. Part Axis/Plane/
Face/Edge input variants validate ownership and capability and depend on the catalog node; source
refresh invalidates dependent catalog nodes. Source removal is blocked while catalogs own it, and a
referenced catalog cannot be removed. Part/Project JSON omits empty catalog arrays and keeps all
legacy canonical output; exact fields, normalized descriptors and reference spellings are frozen in
`docs/file-format.md`. Missing external assets do not prevent loading saved intent.

Plane, cylindrical-face Axis/Cylinder, linear-edge Line, circular-edge Circle/Axis/center Point and
vertex Point capabilities use their exact recovered geometry. Explicit generic Face/Edge capabilities
also support existing topological Part consumers, including opaque surfaces/curves; these never
acquire an invented analytic primitive. A shared full trimmed-witness matcher checks imported
finishing references against actual target-body faces/edges rather than attaching an unrelated
external primitive with similar bounds.

`StepImportTopologyContext` receives the saved document's asset root explicitly and binds it once to
an absolute root. It shares verified reader snapshots within one operation, binds cached
source/catalog values to the actual owning Part,
rejects stale intent, and revalidates accepted source bytes before publication. An absent context
fails explicitly for imported inputs. Real Mirror/axis, planar extrusion limits and finishing paths
consume these references;
local and occurrence-qualified Assembly target resolution reuse the same recovery and exact
capability projection. Imported target metadata retains the typed catalog reference, with no fake
generated `FeatureId`. Nested placement applies the existing exact inner-to-outer transform chain.
Imported Part creation, feature-result authority and source-refresh commands for those imported
Parts remain Blocks 140–141; the Block-137 metadata source task keeps its explicit digest contract.

The source task adds an explicit **Prepare Topology Catalog** operation with existing Apply/Undo/Redo.
It revalidates document/path/source evidence and securely recreates the candidate before one catalog
transaction. Both preparation and Apply reject duplicate capture of the selected product; ambiguity
in an unrelated product does not block a distinct product with no matching evidence. Accepted
catalogs are inspected without replacing baseline ids/descriptors; no-op Apply preserves history.
Product-local Body previews, recovery rows, capability filters and native semantic
Face/Edge/Vertex picking show canonical `imported:` references through a separate disposable viewport
selection layer. Inspection focus stays fitted to preview bounds across standard-view changes until
the preview is cleared or the user chooses Fit All. Existing full occurrence inspection remains the
Block-138 action. Cancellation,
document lifecycle and source changes clear derived topology selection and restore prior scope/masks.

Additional focused tags are `[step-import-part-input]`, `[step-import-topology-workbench]`,
`[block139-step-topology]` and `[native-step-topology]`. Core, recovery, real Part/Assembly consumers,
GUI history/freshness and native picking tests pass. All five targets build; full Core passes
**566 / 18,234**, Geometry **506 / 38,596**, and GUI **631 cases — 602 passed, 29 expected
native-only skips / 83,143 assertions**. Native 100% passes **26 / 5,137**; the new native topology
case passes at 150% **1 / 934**, with actual Part and Project picks at both scales. Separate native
startup reaches intentional **124**. All **914** runtime fingerprints remain unchanged. Exact
commands, unchanged red-to-green regressions, local artifact repair and visually reviewed captures
live in `docs/development-setup.md`. Blocks 140–143 remain planned, and the next branch starts only
after this block is merged.
Delivery: [PR #131](https://github.com/GideonBa/Better-Linux-CAD/pull/131), verified implementation
commit `95f15a6`; the delivery-documentation follow-up changes no runtime source.

## Block 140 — Reference Part integration with assemblies

Primary boundary: immutable imported Part definitions as Assembly members.

Create a read-only imported `PartDocument` definition from one STEP product/body definition. It must
participate in:

- Project part ownership and shared component instances;
- repeated instances and nested BLCAD subassemblies;
- visible/active leaf traversal and exact transform chains;
- assembly target selection, compatibility, constraints, joints, solving, motion, freshness, and
  atomic application;
- contact/interference queries and flattened/structured STEP export.

Part feature additions, parameter edits, body edits, and mode changes fail explicitly in Reference
mode. Conversion to EditableBody is a separate transactional command that creates the Block-141
intent without altering the original source asset.

Focused tags:

```text
[core][step-reference-part]
[geometry][assembly-step-reference-part]
```

## Block 141 — EditableBody ImportedBodyFeature and downstream modeling

Primary boundary: imported geometry as a Part feature-result authority.

Add persistent `ImportedBodyFeature` intent referencing one `StepImportSource` and its imported body
definitions. Its ordered outputs are persistent Block-48 `BodyId` values with `Solid` or `Surface`
kind. Recompute reads the verified source, resolves the selected definitions, and seeds the normal
multi-body `ShapeCache`/feature-result path.

Freeze:

- deterministic body-output order and body mapping;
- dependency and invalidation edges from source to imported feature to downstream features;
- incremental recompute after downstream parameter changes;
- explicit source refresh with topology recovery diagnostics before commit;
- downstream semantic selection on imported topology;
- conversion from Reference to a new EditableBody Part without mutating existing assemblies unless
  the user explicitly replaces their referenced Part definition.

Prove representative downstream edits: an additive feature, a subtractive feature, a hole, a
fillet/chamfer or body boolean, JSON roundtrip, recompute, and re-export.

This block is history-based modeling after an immutable base feature. It does not implement feature
recognition or direct modeling/push-pull.

Focused tags:

```text
[core][imported-body-feature]
[geometry][imported-body-recompute]
[geometry][imported-body-downstream-feature]
```

## Block 142 — Structured STEP assembly import

Primary boundary: foreign product structure to BLCAD Project hierarchy.

Import an XDE/STEP product graph into:

```text
one BLCAD Project
one AssemblyDocument per unique imported assembly definition
one imported PartDocument per unique imported part definition
one ComponentInstance or SubassemblyInstance per occurrence
exact direct occurrence transform
```

Repeated STEP product definitions remain shared definitions with distinct occurrences. Import order,
generated ids, and collision handling are deterministic. Cycles, malformed product graphs, missing
definitions, invalid transforms, and duplicate generated identities fail closed.

The user chooses Reference or EditableBody for imported part definitions. Structured import creates
no inferred assembly constraints or joints; imported occurrence transforms are initial placement
only. BLCAD relationships and joints may be authored afterward using Block-139 semantic imported
targets.

Focused tags:

```text
[core][structured-step-import]
[geometry][structured-step-import]
```

## Block 143 — Integrated STEP import acceptance and application workflows

Primary boundary: end-to-end import proof and application commands.

Add shared commands/APIs and matching GUI tasks for:

```text
inspect STEP source
import Reference Part
import EditableBody Part
import structured STEP assembly
validate source freshness
refresh source explicitly
convert Reference Part to a new EditableBody Part
re-export imported or edited results
```

Representative acceptance must prove:

- one Reference Part can be inserted repeatedly, constrained, moved by a supported joint, analyzed,
  saved, loaded, refreshed, and re-exported;
- one EditableBody import accepts downstream additive and subtractive modeling, survives JSON
  roundtrip, recomputes deterministically, and re-exports the edited result;
- one structured STEP assembly preserves shared definitions, nested occurrences, names where
  supported, direct transforms, and geometric equivalence;
- missing/changed sources, lost/ambiguous imported topology, malformed structures, and stale refresh
  results fail atomically;
- source STEP files remain byte-for-byte unchanged;
- no raw STEP/XDE/OCCT identity appears in persistent JSON.
- GUI-created import intent and results are equivalent to the matching headless workflows.

Focused tag:

```text
[integration][step-import-mvp]
[integration][gui-step-import]
```

After Block 143, the first STEP Import MVP is complete.

## Explicit deferrals after Block 143

```text
automatic foreign feature-history recognition
foreign sketch/dimension/constraint reconstruction
direct face push/pull or offset editing
automatic pattern/fillet/hole recognition
bidirectional associative update to the source CAD system
editing or overwriting the source STEP asset
lossless vendor-specific metadata and appearance parity
arbitrary topology healing or silent geometric repair
```

The next phase after Block 143 must be planned from measured import and topology-recovery gaps.
