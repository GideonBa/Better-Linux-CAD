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
multiple tools feeds each result into the next operation.

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
- OCCT does not complete the operation;
- the operation produces no solid result; or
- a cache update fails validation.

Missing inputs identify the relevant `BodyId`. OCCT/operation failures identify the Boolean
`FeatureId`. No partially updated cache is exposed.

## Proof

Focused tag:

```text
[geometry][body-boolean]
```

The focused suite proves Add, Subtract, Intersect, two independent solids, canonical multi-tool
order, both result modes, both tool-retention modes, incremental recompute with a consumed tool,
unchanged Core intent, contextual errors, and transactional failure.

## Handoff

Block 56 introduces persistent `BodyTransform` and `SketchOwnership` Core intent plus JSON. It does
not broaden Block 55 into transform Geometry, non-uniform scaling, GUI editing, or direct modeling.
