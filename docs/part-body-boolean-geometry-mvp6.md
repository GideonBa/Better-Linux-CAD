# Body Boolean Geometry and Recompute MVP-6

Status: implemented in Block 55.

## Scope

Block 55 executes the persistent `BodyBooleanFeature` intent introduced by Block 54. The optional
`blcad_geometry` target now applies OCCT Fuse, Cut, and Common operations to body-scoped cached
shapes without moving BRep authority into Core.

Supported operations and result modes are:

```text
Add        -> sequential OCCT Fuse
Subtract   -> sequential OCCT Cut
Intersect  -> sequential OCCT Common

ModifyTarget -> replace the target Body result
NewBody      -> store a distinct result Body and retain the target
```

Tool Bodies execute in the canonical `BodyId` order frozen by `BodyBooleanFeature`. A Boolean with
multiple tools feeds each result into the next operation. OCCT error reports and final BRep validity
are checked before publishing a result. Shared coplanar faces and tangent boundary edges are unified
after each operation so a Union exposes its united boundary to later selection and finishing.

## Cache and recompute contract

Every successful Boolean stores both a feature-scoped backing shape and a Body-scoped result whose
source is the Boolean feature. `keep_tool_bodies=true` retains tool Body results.
`keep_tool_bodies=false` removes them from the active Body-result view while retaining their
feature-scoped backing. That backing permits deterministic incremental recompute without forcing an
otherwise clean, independent tool producer to execute again.

`ModifyTarget` resolves the pre-Boolean target producer rather than repeatedly applying an
operation to its previous Boolean result. `execute_document()` follows the Part dependency graph,
so normal features and Body Boolean features may form one ordered producer/consumer chain.
`execute_plan()` recognizes both feature families.

Full-document and plan execution use a working cache and commit only after every requested step
succeeds. Direct Boolean execution is transactional as well. The persistent `PartDocument` intent
is read-only throughout Geometry execution.

## Failure policy

Execution fails closed when:

- the Boolean feature is missing;
- the target or a tool Body has no cached or producer-backed shape;
- an input shape is empty;
- OCCT does not complete the operation or reports algorithm errors;
- the operation produces no solid result or an invalid BRep; or
- subtraction removes no measurable material from the target; or
- a cache update fails validation.

Missing inputs identify the relevant `BodyId`. OCCT/operation failures identify the Boolean
`FeatureId`. No partially updated cache is exposed.

## Cut outcome contract — 2026-10-28, issue #113

Subtraction is a material-removing operation. A disjoint or tangent tool, or a tool entirely inside
an existing void, is a rejected candidate with the diagnostic **"Cut removes no measurable
material"** and guidance to check the target, tool position, direction and extent. A completed
operation that removes every solid is diagnosed separately as **"Cut removes the entire target
body"**. Non-solid inputs, failed kernel operations, invalid solids, invalid volume measurements
and inconsistent volume increases remain geometry errors. These outcomes identify the feature and
never publish a partially changed cache, consume tools or add an interactive history entry.

`BodyBooleanAdapter::validate_subtraction_result` validates the complete feature result. Multi-tool
subtraction and legacy circular-hole patterns may contain individually ineffective tools as long
as their combined result removes material. Full-target removal cannot be repaired by a later
subtractive tool and fails immediately. Add and Intersect retain their existing result rules.

For nonempty valid solids, the check uses exact surface geometry and adaptive OCCT volume
integration (requested relative accuracy `1e-10`). A decrease must exceed the sum of the two
reported integration uncertainties plus `64 * machine_epsilon * max(before, after)` for rounding.
There is no fixed millimeter threshold or prescribed percentage removal. An identical topology
result is rejected directly. Changes below measurable numerical resolution are reported as such;
they are not presented as successful machining. A small `1e-5 mm³` cut in a `1000 mm³` target is
part of the regression proof.

The same validator guards modern Extrude/Revolve/other Boolean-backed Cuts and the legacy
Through-All circle, line/arc/spline and compound-profile executor paths. Legacy hole patterns are
checked after all holes. Primitive cut builders enable OCCT non-destructive mode before Build so
shared source/cache shapes survive previews and failed operations. A repeated legacy Cut resolves
its pre-Cut producer while retaining the existing requirement for a present target cache; it does
not subtract from its own previously published result.

This policy also applies when upstream edits make an existing Cut ineffective. Historical saved
intent containing such a Cut can therefore fail normal recomputation. **Open for Repair** retains
Core-valid failed intent without publishing partial Geometry; correct the upstream inputs or
target/operation before returning to a fresh model. The persistence schema is unchanged.

Retained tool Bodies remain associative, as required by the existing Core graph. A later tool
transformation therefore recomputes the earlier Boolean. Moving it entirely outside the target
rejects the candidate if it makes that Cut ineffective; the numeric HUD restores its last valid
value and history is unchanged. A partially overlapping move remains valid and updates the Cut.
`[shell-combined-bodies]` checks both outcomes, including branching history and reopening;
`[cut-material-removal]` checks transactional Geometry replay and JSON reconstruction.

The shell presents the diagnostic in its task summary, clears stale previews and disables Apply.
Apply rechecks through the same Geometry path, including forced command calls. Target/direction/
extent correction can produce a valid preview in the same task; Cancel preserves the original
document and model. Interactive proof and the additional curved-profile extent fix are described
in `docs/gui-interactive-extrude-revolve-mvp9.md`.

## Proof

Focused tag:

```text
[geometry][body-boolean]
```

The focused suite proves Add, Subtract, Intersect, two independent solids, canonical multi-tool
order, both result modes, both tool-retention modes, incremental recompute with a consumed tool,
unchanged Core intent, contextual errors, and transactional failure.

## Handoff

Block 56 now introduces persistent `BodyTransform` and `SketchOwnership` Core intent plus JSON.
Transform Geometry, non-uniform scaling, GUI editing, and direct modeling remain outside Block 55.

`[union-boundary]` proves that two overlapping equal-height boxes become one six-face solid rather
than retaining internal coplanar seams, including deterministic JSON replay. Native-capable
`[curved-union-workflow]` combines two overlapping curved bodies after fillet/chamfer operations and
checks one retained solid, tool consumption, preview isolation, history and Save/Open.

Cross-family history persistence now emits `feature_order` whenever an executable dependency would
be reversed by the historical JSON family insertion order, including dependencies through Body
alias nodes. Thus Fillet/Chamfer followed by Union replays in the authored order during Undo/Redo
and Save/Open even when no BodyTransform is present. Independent family ordering retains its
existing JSON form. `[core][edge-treatment][feature-order]` and `[curved-finishing]` regress this
finishing-to-Union chain and its exact recomputed volume.

Boolean execution enables OCCT non-destructive mode before the first build. This also enables the
safe-input mode used by result simplification. Cached `GeometryShape` copies share topology, so a
later Cut/Union must never rewrite faces belonging to an earlier feature or a disposable preview's
input cache. `[boolean-cache-isolation]` checks a directed groove, symmetric cut and full rotational
cut, then edits the first angle and replays the document, verifying both retained input copies and every
relevant cached intermediate volume.
