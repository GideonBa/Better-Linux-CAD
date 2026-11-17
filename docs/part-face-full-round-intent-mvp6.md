# Face blends, full rounds and setback intent — Block 177

Core intent is accepted. `mvp-plan.md` owns numbered acceptance; checked construction is
Block 178 and the shared native property panel is 179. These modes extend the existing immutable
`FilletFeature` authority, normal Body history and necessary Part version 14.

## Typed modes and ownership

`FaceBlendIntent` owns ordered, named first/second `FilletFaceSet` records, a continuity choice,
a typed size binding and optional hold lines. `FullRoundIntent` owns first/second/middle face sets,
continuity and optional hold lines. Every set has a stable ID and ordered semantic references.
First/second references use `FilletSupportFace`; middle references use `FilletMiddleFace`.
Set IDs must be distinct and a reference cannot appear in multiple roles. All sets are nonempty.

The in-place target must be an owned Solid Body. Semantic producers must belong to its history;
result-face references identify that exact Body. Imported inputs retain their exact immutable
parent-Body binding or selected ancestry, including rejection of another output of the same base.
Geometry rechecks actual resolved face identity, ownership and supported support combinations.
The factory stores an immutable copy, so changing an external draft cannot alter the owned record.

G1 face blends use a **radius** Length binding. G2 face blends use an explicit **contact-setback**
Length binding: curvature-continuous contact does not define a constant-radius circular section.
Unsupported size/continuity pairs reject in Core. Full-round has no radius binding or flat edge
list; Geometry must derive its cross-section from the selected supports and replace the selected
middle region. Selecting three faces does not authorize an ordinary constant edge Fillet.

Hold lines have stable IDs, explicit first/second side roles and owned `PathCurveId` inputs.
At most one distinct curve per side is supported. They enter normal dependencies and executable
history inputs. Geometry must verify trimmed contact and agreement with the requested size or
naturally derived full-round section; it cannot ignore an inconsistent hold line.

## Corner policy

Existing edge groups retain their normal checked native corner behavior. An optional typed
`FilletSetbackCorner` adds a stable corner ID, three group IDs and a Length distance binding.
The initial Core capability requires exactly three distinct explicit single-edge groups sharing
one constant radius parameter binding. All station IDs and group directions remain authored.
The setback distance participates in normal parameter invalidation and transactional domain checks.

The planned native capability for 178 is an orthogonal convex three-edge corner, with supported
setback greater than its radius and sufficient remaining edge length. Actual support/corner
feasibility, trimmed contact and resulting material belong to Geometry. The eventual capability
table must distinguish checked native rolling corners, supported G1 setbacks and unsupported
G2/support combinations; a generic cap does not establish contact continuity.

## Persistence and normal commands

Legacy constant Fillet and default variable records retain their exact fields and meanings.
A face record stores `id`, `name`, `type: fillet`, `target_body`, `mode: face_blend` and `definition`.
A full-round record uses `mode: full_round`. Definitions carry the complete named face sets,
continuity and hold lines; only face blends carry the typed size. Variable groups retain their
existing strict seven-field group objects. A setback adds the optional known top-level `corner`
record; its group identities and distance binding are never flattened into a BRep.

The V14 envelope remains necessary while any advanced Fillet exists, including suppression.
Unknown fields, malformed roles, invalid size/continuity pairs and ambiguous repeated references
reject. Part/Project reopening reconstructs the same typed mode. All modes use normal Fillet
add/update/remove and feature inventory. Face/path/size dependencies join the same atomic graph
update, and failed replacement preserves the owned document and graph.

The Geometry dispatcher recognizes these modes before legacy radius lookup. Until 178 supplies
checked construction, face/full-round and setback requests fail explicitly; they cannot use a
constant-radius fallback. Native create/edit/selection, handles and repair remain 179.

## Verification status

All five targets built successfully. Focused Core: **35 cases / 1,362 assertions**; complete Core:
**798 / 35,311**. Focused Geometry regression: **22 / 9,939**. Evidence is retained in
`build/verification/block177/`. Tests cover immutable typed sets, strict Part/Project roundtrip,
no competing full-round radius, repeated roles, foreign ownership, hold-line/parameter edits,
suppression/removal and unchanged legacy contracts.

The hold-line cycle regression constructs a derived curve from a downstream Fillet edge, then
tries to consume it in that same Fillet. Replacing a feature now checks the complete restored
DAG before publication, including documents without an authored construction sequence. Failure
preserves both document and graph. Native support, actual middle replacement, differential
continuity and shared interaction remain the separate 178/179 gates.
