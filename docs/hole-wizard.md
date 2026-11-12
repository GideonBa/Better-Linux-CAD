# Hole Wizard

Status: staged target architecture. Numbered implementation and delivery status belong to
`mvp-plan.md`; the semantic Core foundation starts at 152, execution at 153 and Wizard UI at 157.

Execution planning is consolidated in [Blocks 152–157 and 196–198](inventor-parity-sequence.md).
Blocks 149–151 own shared revisioned resources/materials; Block 156 owns integration with the existing
Pattern authority. The conceptual `pattern_data` below must not create an independent pattern
executor. The original MVP sequence below is superseded for scheduling by those blocks; this file
retains Hole/Thread semantics and standards provenance. The new plan explicitly brings seat types
into the Part phase and coordinated Assembly holes into their later cross-document phase.

The replacement shell implements bounded plain cylindrical through-all single/bolt-circle cuts
through `CircleProfile` / `CircularHolePattern` and `SubtractiveExtrude`; see
`docs/gui-part-foundation-workbench-mvp7.md`. Those commands do not introduce the semantic
standards-oriented `HoleFeature`, threads, countersinks or assembly hole features described here.

The hole wizard creates holes as semantic, parametric, standards-oriented features, not as a bare circle plus cut. A `HoleFeature` carries hole type, standard, diameter, depth, thread, counterbore/countersink, fit, fastener type, and manufacturing intent. This makes later drawings, BOMs, fastener placement, collision checks, and manufacturing notes possible.

## Typed Core intent (Block 152)

`HoleFeature` is a separate semantic record. `HoleKind` initially distinguishes Plain and Clearance;
seats and threads extend it in their assigned later blocks. It owns one existing solid target Body,
a typed `PlaneReference` with role `hole_support_plane`, explicit SupportNormal/OppositeSupportNormal
direction, diameter intent, ordered stable placements, a discriminated extent and drill tip.
OppositeSupportNormal is the default; support orientation remains semantic, without kernel face IDs.
Reference Parts reject authored Hole intent. A limit Body never becomes an implicit cut participant.

`HolePlacementId` is unique within a Hole. Independent owned `Point2` positions are finite signed
millimeter coordinates in the support frame; a referenced placement retains its existing Sketch ID
and free/projected point entity ID. Sketch-driven points retain the existing dimension/constraint
and workplane authorities. No coordinates are copied from referenced Sketches. Geometric
coplanarity, finite support boundaries and duplicate/effectless bores are checked at execution, not
inferred by the Core factory. Repetition remains assigned to the shared Pattern authority in 156.

A custom diameter references a positive Length parameter. A standard diameter stores an immutable
`EngineeringResourceKey`, row key and explicitly selected Length column. Resolution uses only the
Part's owned 149 catalog snapshot and returns the exact cell value and cell provenance. Missing
revision/row/column/cell, a Material in place of a table or a differently typed column is an error;
there is no latest-revision fallback, numeric default or invented attribution for custom parameters.
Lookup does not mutate the document or executable lifetime. JSON stores this intent in 152B rather
than serialize a resolved numeric cache; revision adoption/removal must then guard actual consumers.

Extent intent distinguishes Blind (positive depth parameter), ThroughAll, ToNext, ToFace (typed
`hole_limit_face`), UpToBody (a distinct existing solid Body) and Symmetric (positive total-depth
parameter). Flat tip is the default. A conical tip is limited to Blind and requires an Angle
parameter strictly between 0° and 180°. Execution in 153 defines entry/bottom coordinates and uses
shared limit queries; Symmetric splits total depth about the support. For a conical blind bore,
depth denotes the full-diameter cylinder depth and the apex extends beyond it by the drill-tip
height. This intent does not itself execute geometry or publish an executable Part record.

`HoleFeature::create` validates structural intent; `validate_hole_feature_inputs` resolves existing
Core bodies, typed support/limit references, actual Sketch point entities and typed dimensions.
Document/history/graph/repair/JSON registration remains 152B; geometry and visible authoring remain
153/157. Legacy CircleProfile and CircularHolePattern cuts keep their existing contracts and bytes.

152B registers Hole in the existing executable family inventory and Body-modifier chain. Support,
Sketch sources, dimensional parameters, exact standard revision and explicit limits are dependency
inputs. Updates retain the history slot and downstream edges; guarded removal restores the previous
Body producer. Suppression retains typed intent and revision pins; scoped prefixes remain partial.
The derived resource node spells `resource:<ID length>:<ID>:<revision>`, preventing delimiter
collisions, and cannot alias authored/imported model identity. Unused table nodes are pruned after
consumer updates/removal while unrelated authored nodes remain intact.

Standard revision adoption rebuilds every affected Hole against the candidate table and publishes
only after all consumers validate. A late missing row/type rolls back the insertion and earlier
remaps. Old snapshots remain owned until explicitly removed; referenced snapshots, including those
of suppressed Holes, cannot be removed. Project adoption retains the existing explicit member list,
shared immutable catalog and all-or-nothing behavior. See `file-format.md` for V4 Part/V2 Project
spelling and offline extraction. Existing prefix/suppressed evaluation and browser history retain
this Core intent without claiming Wizard authoring before 157.

153A introduces one-direction ThroughAll and Blind on fixed datum/construction supports and current,
unposed rectangular primitive faces. Positive direction follows the support normal; the default
cuts opposite it. ThroughAll starts at the support and extends beyond the forward target bound,
without extending backwards. Blind depth measures the full-diameter cylinder from the support;
the conical apex adds `radius / tan(included_angle / 2)`. A blind tool that reaches the forward
exit is rejected with a Through All diagnostic. The complete Blind tool must also lie inside the
target material, preventing a side breakout that an axis-only exit test would miss.
Multiple placements are separate effective cuts;
effectless or target-erasing tools fail, and no intermediate Body/cache result is published.
Referenced Sketch points resolve in their own current frame and must lie on the support plane.
Rectangular supports require the full circular opening inside their bounds. Posed/imported/result
supports are implemented in 153B2; stable generated bore/seat references and combined acceptance
remain 153C. The shared history preview, Apply,
Undo/Redo and portable save/reopen use this execution without adding a second authoring workflow.

153B1 adds actual-solid axis intervals, excluding tangencies and boundary-only contacts. ToNext
clips the cylindrical tool to the target and selects its first separated forward material component,
retaining curved exits rather than substituting a center-line depth plane. If that component wraps
around an opening into a later axis interval, the limit is ambiguous and fails atomically.
UpToBody subtracts the explicit limit Body from the tool and retains the first separated forward
component; the limit Body itself is unchanged. It must begin ahead of the support and fully separate
the tool, otherwise it is ineffective or ambiguous. ThroughAll still traverses all forward material.

ToFace reuses the existing Extrude planar-limit resolution. B1 supports current unposed rectangular
primitive limits perpendicular to the bore axis, and requires the entire circular footprint inside
one actual trimmed face. Posed/imported face and broader support integration remain B2. Symmetric
uses its persisted positive **total depth**, split equally about the support; it never means an
unbounded ThroughAll cut. Blind depth remains measured from the support even when material starts
later: entry is resolved separately, the depth must reach material, and the remaining cylinder/tip
must fit fully before the first material exit.

Inventor workflow reference: [Autodesk's current Hole documentation](https://help.autodesk.com/cloudhelp/2027/ENU/Inventor-Help/files/GUID-94EB6947-0347-4888-870D-4BA22DB144D8.htm)
(checked 2026-11-08) confirms support-based distance, progressive positions/type/termination fields,
explicit solid participants and Create Another. Its Symmetric option is limited to simple ThroughAll;
BLCAD's already persisted finite symmetric extent is an explicit extension and must be labelled as
total depth in 157, rather than presented as identical to that Inventor option.

153B2 resolves semantic rectangular supports on the current Body through authored face lineage,
including preceding rigid transforms, scale and surviving modifier descendants. Internal face-center
aliases retain native placement coordinates; they are excluded from public selectable origins so
existing captured Sketch/workplane frames stay unchanged. Captured/derived support IDs remain their
existing Core workplane identities. A deleted contributor or split face fails explicitly.

Imported supports first verify their owned source/catalog and then follow the existing imported-face
origin on the current Body. Canonical in-plane coordinates are transported with the original frame;
they are not re-canonicalized after a pose. For owned outputs, the outward face normal defines drilling direction,
while the original in-plane coordinate convention is retained. Changed supplier bytes invalidate
the operation; no geometric-proximity fallback selects another face. Multiple features can reuse
surviving trimmed supports, provided each complete opening fits the actual selected face.

ToFace uses this same current native/imported resolution and accepts oblique planar faces. It
computes the complete plane section of the tool, checks its full area against the selected trimmed
face and clips to the side containing the start. Thus an oblique bore ends on that plane with an
elliptical section. Parallel, behind-start, start-crossing, missing and insufficient finite limits
fail atomically. The limit Body remains a nonparticipant; generated bore/seat references and whole
153 combinations remain assigned to C, and semantic Wizard authoring remains 157.

153C assigns every generated cylindrical wall, flat Blind bottom and planar ToFace termination a
producer-owned origin
`hole/<byte-length>:<placement-id>/wall` or `/bottom`. Placement IDs, rather than center order,
coordinates or OCCT traversal, distinguish multiple openings. The roots are attached to the
tool before the final Body cut and followed through surviving/modified kernel faces, including
subsequent Body poses and finishing. Through exits and conical tips do not pretend to be planar
bottom seats. A missing contributor or multiple descendants rejects the reference explicitly.

The existing `ResultFaceReference` authority also supplies generic Face inputs for Draft/Shell and
planar extent consumers. Finishing selection offers only uniquely resolved current faces. Bore
walls can drive a Draft; flat Blind bottoms participate in the existing direct face-Sketch route,
with one shared support/Sketch Undo action. A face that becomes curved cannot supply a workplane.
No extra Hole-reference table, duplicated Sketch geometry or new authoring panel is introduced.
The dedicated Inventor-like Hole property workflow remains assigned to 157.

## Manufacturing variants (Block 154 contract)

`HoleManufacturing` is one validated discriminator: simple (omitted), Counterbore, Countersink,
Spotface, Stepped, Fitted/Reamed, Center or Slot. It extends the existing Hole record, placement
identity, Body modifier and revision catalog. It does not create a parallel cut or standards store.
Counterbore and Spotface carry outer diameter and depth; Countersink and Center carry outer
diameter and included angle. Stepped seats carry stable authored step IDs, outer diameters and
absolute depths; evaluation orders steps by depth, requiring strictly decreasing diameters.

Every manufacturing dimension selects either an existing typed parameter or an exact table
revision/row/column. Length and Angle cells retain their actual cell provenance. All selected
revisions, including seat-only revisions, participate in dependency edges, guarded removal and
atomic revision adoption. Missing cells, incompatible types and invalid candidate dimensions are
errors; they never fall back to a nominal value or a newer table. See the Part V5 spelling in
`file-format.md`; ordinary semantic holes retain their V4 bytes.

Seat depths use the authored support plane as zero, including a support above the material.
Counterbore/Spotface shoulders must reach material, precede the termination and fit the target.
Conical seat depth is `(outer_radius - nominal_radius) / tan(included_angle / 2)`; the angle must
be strictly between 0° and 180°. Center holes require Blind; an optional existing conical drill
tip remains separate from the entry cone. One-sided seats reject finite Symmetric extents.
Intersecting entry geometry is evaluated against actual current material; partial side breakout,
insufficient seat material or a limit truncating the seat prevents a successful candidate.

Fitted/Reamed carries an explicit designation, process and absolute minimum/maximum diameter
dimensions containing the nominal diameter. Geometry uses that nominal diameter. The designation
is user/table intent, with no inferred ISO fit, machining allowance or invented normative table.
Fit geometry may use existing symmetric extents because it adds no one-sided seat.

Slot consumes one existing closed Sketch profile with two tangent parallel lines alternating with
two equal outward semicircles. Inward caps remain valid Sketch curves but do not form a Slot.
Its width must equal the nominal Hole diameter. The profile supplies position and
orientation; the Hole therefore retains exactly one owned zero placement, Blind or ThroughAll,
and a flat tip. Neither the executor nor mode edits create or duplicate Sketch entities.

Generated primary round wall roots survive meaningful mode edits. Seat walls/shoulders use
`seat/wall` and `seat/bottom`; stepped roots use authored step IDs and slot sides use the existing
Sketch curve IDs. Missing seats fail reference resolution; consumers retain their actual geometry
and capability checks rather than selecting a nearby face. Labels describe the seat, step or slot side in the existing selection
and property surfaces. The shared history preview/Apply, Undo/Redo and Save/Open routes apply;
the dedicated progressive Inventor-like Hole property panel remains scheduled for 157.

`hole_spacing_advisories` is a read-only query for round holes using an explicit caller-provided
minimum web recommendation. It reports actual and recommended web distances using the largest
opening diameter, without changing intent or blocking otherwise valid geometry. Invalid geometry
remains an error. No default spacing recommendation is fabricated or persisted as a standard.
Slot spacing is outside this circular query's scope and is rejected explicitly.

## Shared Thread Core inputs (Block 155A)

`ThreadDefinition` is the shared input value for internal Hole threads and standalone external
`ThreadFeature` records (`thread_feature.hpp`). The default representation is Cosmetic; Modeled
must be explicit. Handedness is Right or Left. Thread start and length are independent of Hole
drill depth. Length references an existing positive Length parameter; an omitted start means zero,
otherwise start also references a positive Length parameter. This reuses the existing parameter
authority without inventing a zero-valued Length parameter.

Workflow reference: [Inventor 2027 Thread](https://help.autodesk.com/cloudhelp/2027/ENU/Inventor-Help/files/GUID-C0848D58-12CB-4605-8165-0836232927F0.htm)
(checked 2026-11-10) uses cylindrical/conical face selection, sourced type/size/designation/class,
offset/length and cosmetic graphics. These guide 157's selectors; optional modeled geometry is an
additional BLCAD capability.

`ThreadParameterProfile` names an authored system, designation and tolerance plus existing typed
parameters for major/minor diameter, pitch and included flank angle. Those labels do not assert
normative conformity. `ThreadStandardProfile` selects one exact immutable table revision, row and
explicit columns for all seven fields: metadata requires nonblank Text, diameters/pitch require
Length, included angle requires Angle. Each selected cell retains its own provenance. A missing
revision/row/cell, a Material resource or a mismatched type fails; there is no latest-revision,
default-pitch or implicit tolerance lookup.

The optional signed taper half-angle is an Angle parameter or a selected Angle column. Omission
means cylindrical; an explicit taper must be nonzero and strictly between −90° and 90°. Positive
taper widens along the threaded interval, negative taper narrows. Both diameters are measured at
the start of that interval. Resolution requires major > minor > 0, pitch > 0, included angle in
(0°,180°), finite start/end and turn count, and a positive minor diameter throughout the interval.
It validates intent ranges, not support compatibility or manufacturability. Execution must still
check the complete physical profile, target material, trimmed support and modeling limits.

`resolve_thread_definition` is read-only: it returns current evaluated dimensions, labels, modes,
the exact revision pin and field provenance without modifying Part intent, history or resources.
Dependencies preserve field order and deduplicate repeated parameter identities. Explicit revision
remapping returns a new definition; the caller must validate every candidate consumer before an
atomic adoption. Standalone definitions do not themselves become registered resource consumers.

The external `ThreadFeature` input owns its Feature ID/name, editable solid target, typed
`thread_support_face`, definition and Along/OppositeSupportAxis direction. Core validates identity,
reference existence, Body kind and dimensions; actual cylindrical/conical compatibility and axial
placement require Geometry. Reference Parts reject authored Thread inputs.

155A delivered the input foundation. The document integration below is 155B; geometry/display
execution remains 155C. The complete B03 gate and dedicated Inventor-like authoring in 157 remain
required. Focused tests:
`tests/core/thread_feature_tests.cpp`, `[core][parity-b155]`.

## Thread document integration (Block 155B)

A plain round one-direction Hole may carry an optional `ThreadDefinition`. Clearance, fit/reamed,
center, slot and Symmetric combinations reject it. Entry seats remain allowed intent; physical seat
clearance is checked at execution. The explicit pilot diameter must equal the thread minor diameter
within 1e-7 mm. For Blind, start + length must not exceed the full-diameter drill depth (same numeric
tolerance); the conical tip does not extend the threaded interval. Geometry must additionally check
actual support/material and any taper over the whole interval.

External Threads register as the existing Body-modifier chain, with `PartFeatureFamily::Thread`.
Support, thread parameters and exact standard pins are graph inputs; the previous Body producer
precedes the Thread and the Thread becomes the next producer. Update preserves the history slot;
guarded removal restores its predecessor. The existing authored-order/suppression/prefix machinery
retains typed definitions, including pins of suppressed consumers.

Revision adoption uses the shared catalog and candidate-only updates across internal and external
consumers. Any missing row/type or invalid late consumer rejects catalog insertion and all earlier
remaps. Project adoption retains the explicit member list and offline ownership. Removal rejects
referenced snapshots, including suppressed Threads. See `file-format.md` for canonical Part V6;
legacy Parts without thread intent retain their previous bytes.

155B registered intent with a guarded unavailable execution boundary. The 155C executor below
replaces that boundary; dedicated Inventor-like Hole/Thread authoring remains 157.

## Thread execution and display (155C)

Geometry validates the current typed cylindrical/conical support, authored axis/seam orientation,
actual axial interval and profile diameter/taper at the thread start. Native circle-extrusion patches
are rooted by source-frame angular witnesses, never traversal ordinals. Their authored axis follows
Body transforms instead of inheriting an incidental kernel cylinder direction. Whole-envelope material
and opposite-side clearance checks reject breakout, covered supports, incomplete bores, seats and
blind/trimmed terminations within the threaded interval. Overlapping cosmetic definitions on one
coaxial interval reject. Hole pilot/thread depths remain independent; attached tapered Hole threads
reject until a matching conical pilot is supported.

Cosmetic remains the default. It preserves native BRep identity for external threads and plain
pilot geometry for internal threads. Derived, nonselectable graphics contain interval rims and at
most four illustrative turns at the actual pitch, clipped to surviving support faces. Very small
pitches use rims. These graphics follow Body transforms/copies, remain outside solid/surface
results and are never exported as physical threads. Their metadata is derived, not new persistence.

Modeled execution supports single-start straight symmetric ideal V profiles with the authored flank
angle and major/minor diameters. Labels/classes do not imply unprovided standard root radii or
allowances. Conical modeled threads, multi-start/rounded normative profiles and overlapping threads
are unsupported. The checked helix helper is independent of general Sweep and reusable by 166.
It constructs a surface-guided quarter-turn tool and joins at most 136 exact screw placements,
extending beyond both ends. Cuts use the authored interval; an unclipped tool is allowed only when
an aligned bound proves the whole Body already lies within that interval. Only the
selected cylinder's tangent patches may simplify in a safe candidate; every other edge is kept,
material invariance is checked and all face descendants are mapped through kernel history.

Modeled limits: 32 turns per thread; pitch, radial depth, minor radius and length at least 0.01 mm;
pitch/major radius/length at most 1e6 mm; separated flanks with more than 0.001 mm axial gap; at most
512 target faces. A multi-position Hole permits 16 modeled placements and 128 aggregate turns.
Curve/pipe construction requests 1e-7 mm; actual tool/result edge, vertex and face tolerances must
stay at or below 1e-4 mm. Unsupported/cost/numerical failures preserve prior cache/model results.
These are explicit execution limits, not standard dimensional defaults.

Native JSON retains the entire definition and exact resource pins. Existing STEP writes evaluated
Body geometry/names, including real modeled cuts, without semantic thread system/designation,
handedness/class or table provenance. Cosmetic STEP contains the unchanged shaft/pilot geometry.
Focused proof lives in `tests/geometry/thread_feature_geometry_tests.cpp` (independent material-point,
pitch/hand/radial/volume oracles and cosmetic identity) and `thread_feature_combined_tests.cpp`
(resource revisions, mass, history/prefix, mode switching, offline reopen, actual STEP readback,
immutable imports, conical cosmetic supports, rotated shafts and atomic cost failures).
`[thread-geometry-shell]` covers History preview/Apply/Undo/Redo/reopen, nonselection and component
visibility; `[thread-history-shell]` retains failed-preview rollback. `mvp-plan.md` owns acceptance
and native proof. B03's dedicated keyboard/mouse authoring completion remains assigned to 157.

## Shared placement execution and Assembly consumers (156)

Explicit placements extend the existing Linear/Circular Pattern classes; `part-pattern-core-mvp6.md`
owns layout/identity and `file-format.md` owns V7. A Hole source repeats the actual removed solid
(`predecessor − completed seed`), including all owned placements, seats, tips and modeled threads.
Occurrence `(0,0)` aliases surviving seed origins without another cut or duplicate cosmetic symbol.
Additional occurrences scope every origin by Pattern, source and both logical coordinates. Whole-Body
copies remain Body-source operations. Nested negative-operation Pattern sources reject explicitly.

The identical-cut path supports round Blind and ThroughAll seeds. Slots, ToNext/ToFace/UpToBody/
Symmetric recipes need a separately recomputed recipe and reject here; their original Hole execution
is unchanged. Actual seed/copy openings must remain complete and exposed. Each copied removed solid
must fit current material without breakout, overlap or duplicate cuts. Current pilot clearance proves
termination: a ThroughAll copy cannot silently become Blind in thicker local stock. Covered entries,
detached starts or changed ends reject atomically. Later seed Body translation/rotation/positive uniform
scale is mapped from current authored bore frames; incompatible placement transforms reject.

Limits are 1024 source-occurrences in the shared explicit layout and 256 actual Hole bores, including
seed/multiple placements. Modeled patterns retain the 16-bore/128-aggregate-turn budget and 1e-4 mm
actual tolerance cap from Thread execution. Cost/geometry/source-freshness failures publish no partial
Body/cache. Booleans retain non-destructive inputs and do not simplify helical copied cuts.

`HoleInstanceReference` separates HolePlacementId from Pattern coordinates and addresses axis, entry,
counterbore/spotface seat or authored Step ID. Resolution uses surviving actual Body faces: coaxial
cylindrical descendants may jointly prove one axis; a seat must be one planar descendant normal to
that axis. No nearest-face fallback or persisted coordinates exist. Bore frames follow later Body
poses, radius edits and unconsumed finishing. Consumed/ambiguous seats or entry rims diagnose explicitly.

The existing Assembly target catalog exposes each physical Hole once, with per-copy axis/entry/seat/
Step labels and component poses. Axis targets provide Axis capability; entry/seat/Step frames also
provide Plane and Frame. Existing Insert/Concentric/planar consumers share these typed descriptors.
Saved endpoints retain missing/excluded occurrence intent and diagnose the affected relationships;
they never silently bind another Hole. Canonical endpoint spelling is owned by Core.

Numeric solver iterations reuse at most 16 Part results within one request, keyed by exact canonical
intent and import context with current supplier-byte checks; nested solves share that request and
no derived geometry survives its scope. Component pose changes do not rebuild identical Hole solids.
Native Pattern authoring controls and whole B04 acceptance are verified in Block 156; dedicated
Hole/Thread authoring remains 157.

Proof owners: `tests/core/pattern_placement_tests.cpp`,
`tests/geometry/hole_pattern_{geometry,combined}_tests.cpp`, and
`[hole-pattern-consumer-shell]` in `tests/gui/gui_shell_assembly_tools_tests.cpp`.
`mvp-plan.md` records actual slice verification and whole-block status.

## Property editing — 157A design and boundary

`GuiFeatureEditController` and the existing `ShellFeatureEditTools` identify Hole and Thread
families; browser activation and History Edit use those same Core update commands. No save-schema,
resource-store, pattern or transaction authority changes. Preserve the feature/history slot and
placement/step IDs. Existing custom dimensions retain their parameter bindings; formula values
remain read-only until an explicit rebind. Pinned table keys, revisions, row and column mappings
are immutable in this slice; adoption continues through Manage resources.

Property rows wrap to the available panel width; nested source/profile groups must not require
horizontal scrolling at ordinary or compact high-DPI sizes. Present existing intent in the Hole
order: name/positions, target/support/direction, bore/seat,
extent/tip, thread. Thread uses target/support/direction then its definition. Numeric fields use
the actual Length/Angle parameter type, including thread start/length/pitch and drill depth.
Handedness and cosmetic/modeled representation are explicit choices; geometry/cost failures clear
the candidate and block Apply. Retained unsupported recipe discriminants remain visible and fixed.
Adding a position or step assigns a fresh opaque UUID-based identity, retained through Apply and
reopen. New entries never recycle deleted identities across drafts or committed edits. Removing a
consumed position must fail through downstream validation rather than bind another one.

The existing complete candidate recompute validates downstream patterns/finishing and Thread
execution, with current supplier freshness. Previews include derived cosmetic graphics and replace
the old annotations; no preview graphic becomes selectable or persisted. Apply is one transaction;
Cancel, failed edits, Save during preview, Undo/Redo and reopen retain the existing contracts.
A native numeric-field/representation workflow and independent physical volume/reference checks
must prove the bridge before delivery. Full creation, recipe switching, source/preset selectors,
Sketch return, anchored manipulators, legacy conversion and Create Another remain 157B/C.

157A visual proof: original [Hole/Pattern property preview](images/hole-property-edit.png),
[modeled Thread preview at 100%](images/thread-property-edit.png) and
[compact 150% Thread preview](images/thread-property-edit-compact.png). The native tests assert
that nested forms need no horizontal scrolling and dispatch keyboard/numeric input and mouse Apply.
The ordinary fit includes datum planes; these captures remain unmodified. Numbered acceptance and
executed counts are recorded only in `mvp-plan.md`.

## Semantic creation — 157B1 bounded design

`begin_new_hole` extends the existing GUI edit lifecycle with a Model Hole action through
explicit `ShellWindow`/ribbon ownership. The same panel creates actual `HoleFeature` Plain /
ThroughAll intent; the legacy Sketch-cut command remains available. This slice starts in a complete
editable Part without another task/workflow/End of Part context. Broader contexts remain 157B3.

Creation generates a fresh feature/placement identity and a pending custom Length dimension.
Pending inputs belong only to the candidate; rebinding to an existing parameter does not publish an
unused generated parameter. The existing canonical Part serializer seeds the form. A typed support override feeds the
Core Hole factory, avoiding another reference serializer. Existing Core add/update, session commit,
geometry execution and freshness checks remain authoritative; there is no JSON/schema change.

Target choices contain current visible solids. Support collection offers current resolvable datum /
construction planes and native semantic planar faces on the chosen Body. Lost/unsupported choices
produce explicit diagnostics. Imported/result-face workplane provision remains 157B2. Body changes
clear the required support choice; mismatched roles, invalid positions and ineffective cuts block
Apply while preserving the draft. Face/datum preselection and command-first collection converge on
one typed definition. A task-owned preview-selection scope routes actual model picks and restores
previous filters/scope on Apply/Cancel. Incoming selections must be copied before callbacks can
change shared selection state. When an enabled datum overlaps a task's bounded face hit, prefer the
bounded face; existing Sketch profile precedence and nearest-face ordering remain authoritative.
Datum support is also available explicitly in the selector.

Preview and Save leave the committed Part unchanged. Apply adds only the required new dimensions
and Hole in one transaction, with exact Undo/Redo/reopen. Create Another publishes that candidate
once, then opens a fresh draft retaining the complete recipe through the 157B3 lifecycle below. Cancel
restores consumed preselection and camera. Focused tests cover physical cut volumes, invalid
corrections, existing-parameter ownership, preselection versus collection, multiple Bodies, nested
form layout and actual native keyboard/model/mouse workflows at 100%/150%. Creation does not close
157B or parent 157; all remaining recipe/thread/source/preset/manipulator/conversion scope is retained.

Original native captures: [ordinary creation](images/hole-creation.png), 1280×800, and
[compact creation](images/hole-creation-compact.png), 1536×960 physical pixels / 1024×640 DIP at
150%. Numeric keyboard input follows actual target/face mouse picks. Apply and Create Another
remain visible below the scrolling form; tests reject horizontal form overflow. The shared native
regression also covers existing Sketch profile and imported STEP selection after the datum-overlap fix.

### 157B2 implementation plan

Extend the same create/edit lifecycle with explicit recipe/extent/tip choices and sourced/custom
dimensions. Form transitions operate on disposable typed Core records and reseed through the
canonical serializer. Keep role-specific pending dimensions stable within a draft and publish only
used inputs, including when an existing Hole acquires a new recipe. Explicit sources select an
existing pinned resource revision, row and compatible column with disclosed provenance; no guessed
normative mapping. Internal threads reuse ThreadDefinition and distinguish drill depth from thread
length/start. Preserve IDs, existing formula ownership and the current feature/history slot.

Complete external Thread authoring and native/imported/result support collection in the same assigned
157B2 scope. Geometry and supplier freshness decide capability; invalid transitions remain editable
and never publish incomplete inputs. Native acceptance includes source changes, recipe correction,
physical geometry and exact undo/reopen. All 157B3/C and subsequent numbered scope remains assigned.

157B2b support plan: translate selection roles explicitly into Hole support/limit or Thread support
references. Keep imported references tied to current supplier bytes and capability. A newly required
result-face workplane belongs to the candidate and is published only when used. Initial external
Thread dimensions need current analytic axis/radius/interval information from Geometry, sharing
the execution frame rules; Qt must not duplicate OCCT topology interpretation.

Standard Thread authoring must explicitly select revision, row and typed column mappings for each
profile field. Incomplete mappings block Apply while preserving the current committed definition.
Start omission remains zero; optional start/taper controls and cone interval/radius consistency must
preview through the same Thread execution. These are planned requirements, not 157B2a acceptance.
New modeled-thread templates must satisfy the separated-flank/turn limits themselves; a valid
cosmetic default does not establish a valid modeled profile. The current custom internal template
uses cosmetic representation; the full new-feature modeled workflow is part of 157B2b/C.

### Recipe and dimensional source controls — 157B2a

The shared Hole panel adds Plain/Clearance intent and explicit Simple, Counterbore, Countersink,
Spotface, Stepped, Fitted, Reamed, Center and Slot choices. The Slot transition starts from an existing
arc-closed profile and keeps its typed profile selector; an unavailable profile is diagnosed. Blind,
Through All, To Next, To Face, Up to Body and Symmetric choices use the existing extent definitions.
The initial To Face limit prefers a current native bottom face. Up to Body begins with another
visible solid. Explicit scoped native/imported/result selectors are described below under 157B2b.
Flat/Conical tip transitions enforce the existing Blind-only cone contract.

`GuiHoleRecipeCommands` operates only on disposable Part clones through Core factories and update
commands. The shell reseeds its form through the canonical serializer while retaining committed
freshness and shared-parameter edits. Fresh draft namespaces keep recipe role dimensions stable; unused
roles are reset to defaults when reintroduced. Recreated steps receive fresh IDs. Only inputs used
by the final definition are published, including new dimensions added while editing an existing Hole.
Rejected Core transitions restore the actual prior choice and block Apply until correction.
Recipe/source controls retain keyboard focus across the deferred form rebuild, including successive
arrow-key changes; Cancel/finish discard pending focus restoration for the old draft.

Every bore/seat/step dimension offers Custom or an explicit cell from an existing pinned table
revision. Choices disclose table, revision, row, column and provenance, and restrict Length versus
Angle cells. The resolved value and units are visible. Raw pin fields remain read-only. Returning
to Custom creates a fresh owned dimension initialized from the resolved cell; it does not edit the
table or overwrite another field's deliberate binding to an earlier custom dimension.
An internal Thread toggle creates the existing custom ThreadDefinition with separate bore, major /
minor diameter, pitch, angle and length controls. Omitted start is zero; creation of optional start /
taper, explicit standard Thread mappings and external Thread creation remain 157B2b.

Focused proof is `[hole-recipe-shell]` / `[native-hole-recipe]` in the current shell test file. It uses
independent analytic volumes for all nine recipes and six extent choices, separate limit-Body
invariance, pin/custom changes, angle units, invalid correction, new dimensions on existing features,
retired steps, unused Thread inputs and exact history/reopen. Parent 157B2 and 157 remain open.
Create Another retains the complete recipe/thread configuration with fresh owned inputs through
the 157B3 lifecycle below.

Original native recipe captures: [ordinary](images/hole-recipe.png), 1280×800, and
[compact](images/hole-recipe-compact.png), 1536×960 physical pixels at 150%. They follow repeated
keyboard recipe changes, numeric seat input and mouse Apply. Tests assert focus retention, footer
reachability and absence of horizontal form overflow. Numbered status and executed counts remain
in `mvp-plan.md`.

### Planar support collection — 157B2b

Hole creation now collects current native, imported and result-origin planar faces on its visible
solid target. Imported face selections are rebound explicitly as Plane-capable Hole support
references to their captured catalog/entry. The same supplier-byte checks and authored Body
placements apply to collection, preview and commit; labels disclose the current face center in mm.

Result-origin faces must resolve as planar on the current cache. Their support workplane is a
pending typed ResultFaceWorkplane, since the canonical Hole plane reference uses a DatumPlane ID.
The candidate adds that record before the Hole. Only a workplane used by the final support is
published, together with required dimensions and the Hole in one transaction. Recipe reseeding may
carry pending records; changing support, Cancel and Save during preview never leak them into the
committed Part. No sampled surface, topology index or viewer identity becomes persistent input.

Focused proof is `[hole-support-shell]` / `[native-hole-support]`: imported translation and axis
frames, supplier changes, pending/unused workplanes, exact history/reopen, and actual native model
picks on a derived Hole bottom and placed imported top. Existing-support editing and explicit
limit selection use the scoped input model described below.
Original captures: [derived bottom](images/hole-derived-support.png), 1280×800, and
[placed imported plane at compact 150%](images/hole-imported-support-compact.png), 1536×960.
Native helpers reveal the scrolled controls and verify hit ownership and on-screen coordinates.

### Exterior Thread support inspection — 157B2b

The Geometry authoring query `ThreadAdapter::inspect_support` is read-only and requires an editable
Part, a typed `ThreadSupportFace`, a solid target and a complete cache for the exact document.
Existing-feature authoring can instead supply an explicitly matching history-prefix cache and
count. An unqualified partial cache, mismatched prefix or downstream reference is rejected.
It shares the execution resolver and producer-axis/seam rules, returning the entry origin, unit
axis/seam, entry radius, finite axial span and signed taper in the selected direction. It rejects
planar faces, another Body's faces and inward-facing bore walls. Reversing direction moves the
origin to the other end and reverses the taper sign; the query does not create parameters or
claim that a complete thread profile fits. Execution still checks the authored interval, material,
breakout, overlap and representation limits. Imported support inspection checks supplier freshness.
Inspection includes the current terminal Body transform. It has no authored consumer yet; resolving
only a predecessor placement would lose an otherwise valid moved imported support.

Independent `[thread-support-query]` / `[thread-import]` Geometry tests cover both directions,
native rigid placement, imported cylinders/cones, radius/span/taper, stale caches, wrong targets,
bore rejection and changed supplier bytes after initial resolution. Queries preserve exact Part
intent, current BRep and supplier bytes. The full `[parity-b155]` regression protects execution
after sharing the resolver. External creation uses this query below; complete Thread selectors
remain assigned here.

### External creation and new modeled templates — 157B2b

Model → Design → Thread opens the shared property task for a complete editable Part. Choose a
visible solid and a current exterior cylindrical/conical face. Native, imported and result-origin
faces are explicitly rebound to `ThreadSupportFace` and filtered through Geometry inspection;
planar and inward bore faces are excluded. Face/Body preselection is scoped to that catalog.
Changing target clears the selected support and blocks Apply until a new support is chosen.
Profile dimensions remain authored inputs; choosing another support never rewrites shared values.

The disposable custom seed derives its major diameter and signed taper from the actual support.
For radius `r` and axial span `s`, radial profile depth is `min(1 mm, r/5, s/8)`, included angle is
60°, pitch is 1.5 times that depth and thread length is `min(4 mm, s)`. These are editable custom
values with authored labels, without an inferred standard/tolerance. The pitch separates ideal-V
flanks when the existing modeled limits permit execution. For a narrowing cone with signed radius
slope `m < 0`, the initial length is also bounded by `(r - depth) / (-2*m)`, retaining a positive
minor radius over the interval. An apical initial entry selects the inspected opposite direction
with a positive entry radius. This seeds only new intent; support changes never rewrite an
existing feature's authored direction or profile. Conical defaults remain cosmetic;
modeled taper and tiny/otherwise unsupported profiles fail with the existing Geometry diagnostic.
The new internal custom template also uses 1.5 mm pitch for its 1 mm radial depth, so its cylindrical
modeled switch is valid. Existing definitions and their dimensional bindings are unchanged.

Both creation kinds share the controller/task lifecycle and typed Core add commands. Required
owned dimensions and the feature publish in one transaction; rebinding to an existing parameter
prunes its unused draft counterpart. Invalid/corrected previews, supplier changes, Cancel and
committed Save during preview retain the established contracts. Thread creation has an Apply
footer and becomes the repeatable command after publication. Create Another now uses the 157B3
typed repeat lifecycle below; optional start/taper/source selectors and existing-support editing
are described below.

Focused proof is `[thread-create-shell]` / `[native-thread-create]`: pending ownership, dimensions,
invalid correction/cancellation, target/support changes, imported placement/freshness, new internal
and external modeled cuts, exact Undo/Redo/reopen and physical STEP roundtrip. Actual executed
counts and delivery status remain in `mvp-plan.md`.

Original native captures show [external creation](images/thread-creation.png) and
[new modeled internal creation](images/thread-internal-creation.png) at 1280×800, plus
[placed imported creation at compact 150%](images/thread-imported-creation-compact.png) at
1536×960 physical pixels. The native tests use real scoped model picks, keyboard length/mode
input and mouse Apply, assert no horizontal form overflow and replay exact history/reopening.
Fit includes the current origin planes; these captures are unmodified.

### Thread sources and optional intervals — 157B2b

Internal and external Thread definitions share a Profile Source selector. A standard profile
requires an explicit immutable table ID/revision, row and typed column for each system,
designation, tolerance, major/minor diameter, pitch and included angle. A tapered profile adds an
explicit Angle-column mapping. No column is guessed from its name or selected automatically.
Changing the table clears row/columns; changing the row clears columns. Existing pins preload
their persisted choices. Controls disclose keys, resolved values/units and cell provenance.

Incomplete mappings are transient form state: they block preview/Apply while preserving the
committed document and last canonical definition. A complete but invalid mapping retains its
specific Core diagnostic and remains editable. Selecting Custom abandons an unfinished mapping:
it retains the preceding custom definition or converts the preceding complete pin. Converting a
complete pinned profile creates fresh owned
dimensional parameters with the resolved values and authored labels; it retains length/start
bindings, handedness and representation. It never overwrites another field's intentional binding
to a formerly used profile parameter or changes a resource revision.

Start offset is optional: omission means exactly zero, enabling it creates a positive 1 mm Length
input. A custom taper toggle creates a signed Angle input initialized to 1°; straight/tapered
compatibility still comes from the actual support and complete execution. Explicit zero taper is
invalid; omission represents a cylinder. Unsupported modeled taper remains a diagnostic. Standard
taper uses the mapped Angle column instead of a custom parameter.

Typed commands apply numeric changes only to parameters used by the resulting Hole/Thread intent
before the Core family update. Removing a start/taper also discards an invalid unused numeric
draft without publishing it. Other explicitly shared bindings and expression ownership remain
intact. Source controls remain available when current custom input is invalid; an explicit profile
replacement discards superseded profile edits while retaining the remaining intent and used values.
Required new inputs publish in the same final transaction, with unused draft roles pruned.
When a complete mapping was rejected by Core because of another used numeric value or binding,
correcting that input retries the same explicit mapping. It does not require changing an already
selected column. Incomplete mappings still wait for their missing explicit choices.
Focused `[thread-source-shell]` / `[native-thread-source]` proof covers typed mappings, invalid
correction, revision changes, custom ownership, optional removal, history/reopen and a native
mapped modeled cut. Whole-block status and executed counts remain in `mvp-plan.md`.

Original mapped-profile previews: [100%](images/thread-standard-creation.png), 1280×800, and
[compact 150%](images/thread-standard-creation-compact.png), 1536×960 physical pixels. The native
fixture activates its window before keyboard input and asserts focus before/after selector rebuilds.
The field values, units and current origin planes are unmodified captures.

### Existing support editing — 157B2b

Editing a Hole or Thread on a currently visible solid collects target and support choices from
the exact executable history before that feature. The existing Geometry prefix evaluator and
typed reference resolvers own this input model; no second feature interpreter is introduced.
Later Bodies and the edited feature's own faces are excluded. Existing references remain unchanged
until an explicit rebind, including native cylindrical Thread references whose selectable result
counterpart has a different identity.

During a model pick, the viewport shows that predecessor geometry with transient scoped Body,
face and datum targets. Unique preview aliases avoid collisions with current presentation IDs;
only typed Core references enter the candidate. A result-face Hole support owns a pending
workplane in the same final transaction. Numeric preview and Apply still recompute the complete
downstream model. Invalid inputs block Apply and remain available for correction. Cancel restores
the entry camera, selection and pick scope while the entry revision remains current.

Focused proof is `[hole-support-edit-shell]`, `[native-hole-support-edit]` and Geometry
`[thread-support-prefix]`: reference preservation, explicit rebinding, downstream exclusion,
used-only workplanes, independent physical frames/volumes, exact history/reopen and actual native
predecessor picking. The [157B3 input-history workflow](#hidden-and-consumed-input-bodies--157b3)
extends these selectors to hidden/consumed targets. The
[prefix lifecycle](#end-of-part-and-shared-contexts--157b3) also uses exact predecessor selectors.

Original native previews: [100%](images/hole-support-edit.png), 1280×800, and
[compact 150%](images/hole-support-edit-compact.png), 1536×960 physical pixels. Both show the
selected predecessor bottom, complete candidate, scrolled selectors and reachable Apply/Cancel.

### Explicit Hole limits — 157B2b

To Face exposes a required Limit Face selector and scoped model pick. It collects native planar,
Plane-capable imported and planar result-origin faces from available visible solids in the input
model. The persisted input is a `HoleLimitFace` with its native/imported/result identity. Imported
limits retain Face capability; Plane capability is checked separately during collection. Result
limits use a temporary checked plane only for inspection and create no persistent workplane.
Core's existing typed extent command and serializer supply each face's canonical form.

Up to Body exposes a required Limit Body selector and Body pick. It excludes the target and Bodies
unavailable at the feature's input history. Changing target cannot silently keep it as its own
limit; the remaining selector allows an explicit correction. Clearing either limit blocks Apply
without publishing incomplete Core intent. Invalid parallel/behind/finite-footprint choices remain
selectable and report the existing Geometry diagnostic, so another limit can correct the draft.
Only the target is cut; the selected limit Body remains unchanged.

The native target's bottom remains the initial To Face choice when available; otherwise the first
collected plane seeds the editable choice. This seed does not promise a valid cut. Existing-feature
limits use the same predecessor cache and picking lifecycle as support edits. Imported source
freshness is checked during collection, preview and final Apply. Numeric edits are retained when
switching the limit face. No limit picker introduces a new cut or persistence implementation.

Focused proof: `[hole-limit-shell]` and `[native-hole-limit]` cover invalid/cleared correction,
native/imported/result identity, placement and supplier changes, distinct limit Bodies, target
changes, physical volumes, exact Undo/Redo/reopen and actual native planar model picks.
`[native-hole-body-limit]` additionally picks the actual separate Solid with the target excluded,
then checks its unchanged volume and the target's independently known termination depth.
The original [separate Body-limit preview at 100%](images/hole-body-limit.png), 1280×800,
shows the actual picked Solid, target cut and reachable footer.
Original captures show [a native bottom limit at 100%](images/hole-native-limit.png), 1280×800,
and [a placed imported limit at compact 150%](images/hole-imported-limit-compact.png), 1536×960
physical pixels, with scrolled limit selectors and pinned Apply/Create Another/Cancel.

### Conical Thread combinations — 157B2b

Thread dimensions use nine decimal places in the numeric editor, sufficient for the existing
`1e-7` support/profile checks when a conical start requires a nonintegral diameter or taper angle.
Native rectangle/line-profile Revolve supplies [authored source-edge roots](part-revolve-geometry-mvp6.md#selectable-polygon-profile-face-origins)
through the existing result-face lineage, so its analytic cone can be selected without importing
it or capturing a topology index.
Profile diameters describe the actual thread start, not the support's end. A start offset or
direction change therefore requires explicitly authored matching major/minor diameters and signed
taper; the panel reports incompatibility and keeps correction controls available. A reverse
direction reverses taper sign and moves the support entry to the opposite end. Modeled conical
intent remains rejected; returning to Cosmetic restores a valid unchanged solid when the profile
and interval fit.

`[thread-cone-shell]` checks both apex orientations, native and placed imported supports, bounded
positive seeds, pinned taper/custom conversion, corrected start/direction and a complete rejected
mapping retried after length correction. Native `[native-thread-cone]` uses actual keyboard/mouse
controls, asserts focus after bounded window activation, corrects an invalid interval/profile and
unsupported modeled choice, then verifies exact history/reopen and physical STEP volume. Existing
placed imported Hole/Thread support edits additionally exercise the qualified predecessor context.
The native mapped-profile fixture also corrects a rejected length through keyboard input and
asserts focus on the rebuilt numeric field before continuing to modeled Apply.
Source proof: [current shell tests](../tests/gui/gui_shell_hole_thread_edit_tests.cpp) and
[independent Geometry roots/frames](../tests/geometry/thread_feature_geometry_tests.cpp).
Original [compact conical preview](images/thread-conical-compact.png), 1536×960 physical pixels,
shows the signed nine-decimal taper, Cosmetic representation and reachable Apply/Cancel. Fit All
includes the origin plane; the small conical Body and its illustrative Thread graphics are unedited.

### Complete Create Another — 157B3

Creation tasks for both Hole and Thread expose Create Another. It commits the current candidate
once, then uses `GuiHoleThreadRepeatCommands` to clone the complete typed intent into a disposable
new task. The feature, Hole placement and step identities are fresh. Numeric parameters owned by
the current creation receive fresh identities while repeated bindings within that feature remain
aliased. Explicit existing parameter/formula bindings and immutable standard pins remain shared;
the command never converts them to resolved numeric snapshots. Only required new inputs publish.
The canonical Core factories, serializer and normal preview/transaction authorities remain in use.

Recipe, extent/limits, support/target, positions, tip, internal/external Thread profile, interval,
direction, handedness and representation are retained. Existing semantic support workplanes and
Slot profiles remain explicit model references. Repeating the same position, Slot profile or
Thread interval can be ineffective or overlap the committed feature; the new task remains open
with Apply disabled until the user chooses another position/profile/interval/target. No new
location is guessed. Cancel discards only this new draft and its inputs. Save during that draft
continues to save the previously committed model. Each later Apply is one independent undo step.

`[hole-repeat-shell]` covers all nine recipes and six extents, fresh owned dimensions/placements/
steps, internal bore aliases, custom/pinned external profiles, shared lengths, formula/pinned seat
bindings, physical cuts, unchanged limit Bodies and exact history/reopen. `[native-hole-repeat]`
uses mouse Create Another, numeric keyboard correction and a second mouse Apply. Full remaining
The remaining 157B3 conversion and extended contexts, plus 157C combinations, remain assigned.

Native evidence: [second Counterbore position](images/hole-repeat.png) and
[second Thread interval at compact 150%](images/thread-repeat-compact.png). The captures are
unmodified outputs of the asserted native workflows.

### Referenced position workflow — 157B3

Each placement selects an independent point in the support frame or an existing scoped Sketch
point. The latter retains `SketchId` and point identity; no resolved coordinate snapshot replaces
the reference. Multiple placements keep independent stable placement IDs when reordered or
rebound. Explicit point preselection seeds creation; the selector also offers actual viewport
point picks. The Geometry executor validates each point in its current Sketch frame against the
current support plane and rejects incompatible positions before publication.

Selecting Independent point explicitly breaks that placement's reference at its resolved current
support-local coordinates. It preserves the placement ID and leaves the source Sketch unchanged.
It resolves the selected typed support independently of unrelated invalid property values. A
referenced Slot origin can therefore return to its required independent origin without abandoning
the recipe; the complete candidate still decides whether Apply is valid.
`GeometryRecomputeExecutor::resolve_hole_support` shares the execution resolver and accepts only
the exact complete cache or an explicitly matching predecessor scope; stale caches fail closed.

Edit Source Sketch enters the existing Sketch workspace through the shared breadcrumb authority.
The enclosing Part workflow owns Sketch operations, sidecars and the Hole draft. Finish Sketch
returns to the retained Hole inputs and recomputes its complete candidate. Local Undo/Redo affects
the source operations; Save writes the committed entry state. Final Apply publishes the source
and Hole together as one Undo step. Cancel discards the complete workflow. End of Part source
navigation remains disabled while its broader lifecycle is being completed.

`[hole-position-shell]` covers multiple scoped explicit/projected points, current-frame rejection,
explicit independent conversion, retained inputs, source Save/Cancel/local Undo/Redo, actual
shifted bore axes and one combined publication. The existing `[source-sketch-workflow]` and
`[block136-feature-edit]` regression passes 15 cases / 1,967 assertions, with three native skips.
Geometry `[hole-support-query]` plus current support/Thread/scene regression passes 18 cases /
1,299 assertions. Native position/source/repeat/feature-edit regression passes five cases / 508
assertions at each of 100% and compact 150%, including real point picking, source point creation,
breadcrumb return and Apply. Unmodified captures show the [source visit](images/hole-source-sketch.png)
and [compact return](images/hole-position-return-compact.png). Full 157B3 acceptance still requires
legacy conversion, extended contexts and the complete native combination gate.

The shared Window keeps the idle model selection filter when entering a feature's source Sketch;
the task's temporary pick mask never becomes the idle filter. Hole Cancel restores the entry
selection/camera after the enclosing workflow rollback, whose revision deliberately advances.

### Support-anchored canvas controls — 157B3

The shared property editor's Canvas Control selects an authored position X/Y, bore/seat/step
dimension, depth or Thread start/length. The existing Linear/Radial manipulator layer owns pointer
mapping, frozen starts, numeric override and exact release. A valid candidate supplies its exact
predecessor cache for support resolution; the result cache supplies subsequent Body placement.
Grips follow rotation/translation/uniform scale while entered values retain their authored units.
Radial diameter grips explicitly map half the posed diameter to world radius.
Slot position/width remain driven by the selected source profile; those ineffective placement/
diameter grips are omitted. Its authored depth remains available as a canvas control.

Dragging changes only the disposable form/candidate. Invalid geometry disables Apply and remains
correctable during the same drag. Escape restores the preceding scalar, release uses the final
sample, and Apply publishes once. Canvas Control is locked during the drag. Selection pick modes
clear grips so model/point picks cannot be intercepted by a manipulation handle.
Placement and step grips use their semantic IDs. Reordering retains the selected logical grip;
removing it clears the selection instead of redirecting manipulation to another row.

Formula/shared values retain their bindings until Make Independent is explicitly chosen. It
creates an owned typed parameter from the displayed value without changing the original source;
unused pending inputs remain pruned. Pinned properties retain the explicit Custom source action.
Referenced placements likewise require the explicit Independent point transition before position
dragging. Thread length starts at the authored interval offset on its inspected cylinder/cone.

Geometry `[hole-canvas-frame]` plus Body-transform/result-face/support regression passes 21 cases /
1,195 assertions. Qt canvas/manipulator/position/source/editor regression passes 28 cases / 2,428
assertions, with five native skips. Native canvas/source/repeat/editor/body/face/limit regression
passes twelve cases / 1,111 assertions at 100%. The same binary at 150% passes disjoint eleven-case /
1,030-assertion and one-case / 81-assertion runs. The broader canvas/position/source/Hole/Thread/
editor/prefix/coverage/viewport regression passes 115 cases / 9,804 assertions, with 20 native
skips. The focused native selection covers eleven of those skips plus the all-platform post-Body-
scale case; complete native combination acceptance remains 157C. This component does not close 157B3.
Unmodified captures show [posed Hole depth](images/hole-canvas.png),
[diameter after Body scale/rotation](images/hole-canvas-scaled.png) and
[compact modeled Thread interval](images/thread-canvas-compact.png).
Proof sources: [shell/native interactions](../tests/gui/gui_shell_hole_thread_edit_tests.cpp),
[value mapping](../tests/gui/gui_shell_feature_tools_tests.cpp) and
[pose inversion](../tests/geometry/body_transform_pipeline_tests.cpp).

### Property presets — 157B3

Hole and Thread share the Property Presets group after the ordinary inputs in the scrolling form.
An editable name, Save Values, Load Values and Remove manage application preferences independently of model history.
Names are scoped by Hole/Thread kind; Save Values explicitly replaces the same kind/name. Cancel
discards the model draft while retaining an explicitly saved preset. Active canvas input, model
picking and source-Sketch visits suspend preset actions. Invalid model preview disables Save Values;
loading can correct scalar input without publishing a feature.

Save Values captures all recipe/extent/tip or Thread profile/interval/hand/direction/representation
properties. Numeric parameters, including formulas and shared inputs, become literal value snapshots
with retained aliases; source parameter names/identities and expressions are omitted. Standard choices
keep their exact immutable revision, selected row/columns and complete provenance snapshot. The UI states
this value-snapshot boundary explicitly.

Load Values retains the current feature's ID/name, target Body, support, placement identities/sources,
Slot profile and explicit Face/Body limit references. A Slot preset requires an existing Slot/profile;
To Face and Up to Body require their current role and selected limit first. Missing current references
or conflicting pinned revisions produce an inline diagnostic and preserve the previous draft.
Each loaded numeric binding receives a fresh owned parameter. Existing step identities are retained
by index, while added steps get fresh identities. Required missing revisions remain candidate-only
until Apply; changing to a recipe/profile that no longer needs them prunes their publication.

The preference file is `feature-presets.json` under Qt's application config location; the explicit
`BLCAD_PRESETS_PATH` override supports isolated automation. QSaveFile publishes the complete library
atomically. Missing files mean an empty library; malformed, unsupported, duplicate or noncanonical
entries fail without overwriting the existing file. Each value template uses the canonical Core Part
codec with fixed placeholder identities. Re-capture validation excludes document-specific bindings;
no new document intent, geometry interpreter or model persistence schema is introduced.

Proof sources: [typed preset/storage regression](../tests/gui/gui_hole_thread_preset_tests.cpp),
[shell and native interactions](../tests/gui/gui_shell_hole_thread_edit_tests.cpp), tags
`[hole-thread-presets]`, `[hole-preset-shell]` and `[native-hole-preset]`. The broader regression passes
122 cases / 10,463 assertions with 22 native skips; the focused native preset/canvas/source/repeat selection passes 10 cases / 824 assertions at both scales. This is targeted
native coverage. Runtime and delivery evidence is indexed in
the [current implementation plan](mvp-plan.md#current-implementation--157b3). Presets alone do not
accept parent 157; explicit conversion and extended contexts are described below, while combined
acceptance remains 157C.

### Explicit legacy conversion — 157B3

History → Convert to Hole and the selected feature's browser menu explicitly preview a legacy
subtractive Extrude as a semantic Plain ThroughAll Hole. Ordinary Edit Feature remains available;
opening old documents does not migrate their cuts. The conversion currently requires a straight,
flat ThroughAll cut, one selected materialized Circle profile (or the only profile), and an explicit
Cut target preserving its Body identity. Suppressed cuts must be unsuppressed first. Circular Hole
Pattern radius/count bindings, other profiles, taper/thin/path/other extents and Body aliases report
an actionable diagnostic; their legacy intent remains intact.

The Hole retains the feature ID/name, executable history position, target Body, diameter binding
(including formulas) and source workplane binding. Its circle center plus any Sketch-origin offset
becomes an explicitly disclosed independent position. The source Sketch and its inputs remain in
the Part; later source-center edits do not move this independent Hole. The canonical Core factory
and `replace_part_extrude_with_hole` codec operation own the replacement and complete dependency/
reference validation. The latter replaces the family atomically at its existing history position,
without appending an intermediate modifier behind downstream Body readers. Supported
consumers such as whole-Body Patterns and subsequent Body placement retain their mappings.
An old full-result feature Pattern can have a different material meaning from a semantic Hole
Pattern; unchanged feature IDs alone do not certify it. Such material changes, unsupported legacy
target-feature or semantic mappings fail before publication and identify the conversion boundary
instead of dropping or reinterpreting consumers.

Conversion begins in an idle complete editable Part without an enclosing workflow/End of Part.
The existing disposable Part workflow stages and recomputes the complete candidate before opening
the ordinary Hole editor. Every available Solid Body must retain its result availability and common
material: the existing Boolean intersection compares both original and candidate volume to their
shared volume within `1e-8 * max(1 mm³, original volume)`. Equal volume with a displaced bore therefore
fails certification. This derived check adds no geometry intent or topology identifiers.

The task identifies the conversion and its value/reference boundary. Subsequent edits use normal
Hole preview, support selection, canvas and presets. Save writes the committed legacy entry until
Apply; Cancel restores it. Final Apply publishes conversion and the accepted Hole properties as one
ordinary workflow transaction and one Undo step. Existing imported-source freshness and downstream
geometry validation still apply. The shared context lifecycle is described below; full native
combinations remain assigned to 157C. This bounded command alone does not accept parent 157.
The task captures the original camera, model/viewport selection and pick scope before internal
staging clears the selection. Cancel restores that entry context after rebuilding the legacy model;
a rejected task startup also restores it through the existing monotonic checkpoint authority.

Proof sources: [atomic Core family/schema regression](../tests/core/part_hole_feature_tests.cpp),
[conversion, shell and native regression](../tests/gui/gui_shell_hole_thread_edit_tests.cpp),
tags `[hole-conversion]`, `[hole-conversion-shell]`, `[native-hole-conversion]` and
`[native-hole-conversion-context]`. Full Core passes 707 cases / 26,935 assertions; focused Geometry
passes 26 / 2,186; final focused Qt passes seven / 670 with one native skip. The broader Qt selection
passes 150 / 12,297 with 25 native skips. Targeted native conversion/context/editor/preset/source/
History regression passes 14 / 920 at 100% and 14 / 926 at compact 150%. The
[implementation plan](mvp-plan.md#current-implementation--157b3) owns delivery/runtime evidence;
these focused results retain their component boundary; combined acceptance remains 157C.

### Hidden and consumed input Bodies — 157B3

Existing Hole/Thread support editing resolves the exact predecessor cache before deciding which
targets are available. A Body hidden by persistent intent or transient presentation, or consumed
by a later Boolean, remains selectable when it existed before the edited feature. Later Bodies,
the feature's own results and unavailable predecessors remain excluded. Creation still chooses
currently visible available Solids; this edit extension changes no creation eligibility.

Target/limit labels identify hidden inputs and Bodies available only in the input history. During
an explicit pick, the task temporarily displays the predecessor cache's available Body shapes,
including hidden inputs, through the existing candidate overlay. Canonical scope validation and
typed face/Body references still own picking; no visibility flag or persistent display override is
written. Closing the pick returns to normal complete candidate preview. Apply validates the full
downstream model, including consumption, and publishes one ordinary edit. Cancel clears the input
overlay and restores the entry camera, selection and scope under the existing revision policy.
The shared Geometry selector requires an explicit `IncludeHiddenInputs` policy for these queries;
its normal finishing default remains `VisibleOnly`. Scope/cache/source validation is unchanged.

If the target is unavailable even before the feature (for example an unavailable creator), the
existing numeric edit/failure policy remains; the task does not invent support geometry. Active
user-prefix edits use the same predecessor query within the enclosing prefix boundary below.

Proof sources: [input-history shell/native regression](../tests/gui/gui_shell_hole_thread_edit_tests.cpp)
and [explicit Geometry visibility/scope regression](../tests/geometry/finishing_selection_tests.cpp),
tags `[hole-input-history-shell]`, `[native-hole-input-history]` and `[hidden-input-selection]`. The
[implementation plan](mvp-plan.md#current-implementation--157b3) records verification and delivery.
Focused Geometry passes 11 cases / 801 assertions; focused Qt passes two / 246 with two native skips.
The broader relevant Qt selection passes 82 / 6,200 with 15 native skips. Targeted native input/
support/source/limit/context regression passes seven / 763 at each scale. These are bounded proofs;
complete native combination acceptance remains assigned separately.

### End of Part and shared contexts — 157B3

Hole/Thread creation, existing edits and Create Another use the existing session-owned prefix child
checkpoint. A child Apply compacts its source and feature edits into one local Undo operation;
Cancel restores only the child entry. Create Another starts a fresh child after the accepted row.
The outer End of Part context retains its marker, complete suffix, committed Save snapshot and
final Apply/Cancel authority. Its Apply stays disabled throughout a child or Source Sketch visit.
Final publication validates the complete suffix and produces one global Undo operation.

Creation inserts typed Hole/Thread intent atomically at the marker through the canonical Core
reader. Adding a modifier at the end and subsequently moving it can create a forbidden intermediate
Body-consumer order; the insertion operation validates the complete order and lineage together.
It retains existing suppression and all suffix records, rejects invalid positions/identities and
leaves the source unchanged on failure. The session still owns marker advancement and local history.
The candidate scene uses its evaluated prefix count, including newly inserted rows. Support and
position queries explicitly use either the edited feature's predecessor scope or the current marker.

Referenced Hole positions can enter Source Sketch under the same prefix child. Returning rebases
the pending Hole against current Sketch intent and refreshes its predecessor inputs; no nested
Part workflow is opened. Save, child Cancel and outer Cancel retain their existing boundaries.
Historical documents without explicit history retain a complete canonical history envelope when
the pending Hole requires a newer one; the source-return regression covers both history modes.
Ordinary complete-document source visits and explicit conversion keep their existing Part workflow.
Hole/Thread entry requires an editable current Part workspace; unrelated workflows and active
commands are rejected without taking ownership. A changed source or lost task/context ownership
rejects stale Apply and preserves independently committed document changes.

Proof sources: [canonical insertion](../tests/core/thread_feature_tests.cpp),
[shell/native lifecycle](../tests/gui/gui_shell_hole_thread_edit_tests.cpp), tags
`[hole-thread-insertion]`, `[hole-prefix-shell]`, `[hole-context-shell]` and `[native-hole-prefix]`.
Focused Qt passes five cases / 582 assertions with one native skip. Native prefix/context and
source/input/conversion-context selections pass ten cases / 1,134 assertions at each of 100% and
compact 150%, in disjoint six-plus-four runs with clean exits. Full Core passes 708 cases / 27,016
assertions; focused Geometry passes 37 / 2,987. These are targeted lifecycle proofs; the
[implementation plan](mvp-plan.md#current-implementation--157b3) records the broader regression and
delivery boundary separately from the remaining 157C combined gates.

Recipe/support/limit handoffs reselect the active feature by stable ID whenever Core returns
canonical JSON. A record index belongs only to its current template; it is never carried across
re-serialization. Source Sketch return can append a pending record to a template while executable
order places it before another Hole. The next recipe or reference change retains the pending
feature's name, bindings and positions and leaves the suffix intent unchanged.

`[hole-canonical-rebase-shell]` in the same shell test source covers Through All and To Face,
Source return followed by a Counterbore recipe, exact suffix records, independent combined cut
volume and complete Undo/Redo/reopen. The focused case passes 169 assertions. Native Source input
and keyboard recipe changes join the existing support/limit/Thread/preset selection: seven cases /
1,026 assertions at both scales, all with exit 0. Original captures show the retained
[100% prefix/recipe context](images/hole-prefix-rebased-100.png) and
[compact 150% context](images/hole-prefix-rebased-150.png); scrolled fields remain reachable and
outer/local Apply/Cancel remain visible. Combined B01–B04/UI01 acceptance stays assigned to 157C.

### Keyboard input and correction — 157C

Enter and Tab finish local field editing without publishing a feature. Ctrl+Enter first checks the
focused spin-box or text input, interprets acceptable numeric text or finishes the text edit, then
uses ordinary typed preview/Apply. An incomplete number is retained for correction; a geometric
failure also leaves the draft and committed document intact. Feedback stays in the current task,
preserving compact input space and the wrapped guidance and fixed Apply/Cancel actions. Finishing
the preceding accepted number still refreshes a preview after an incomplete edit.

Escape from an owned input cancels the task, or resets an active canvas drag under the existing
gesture contract. Source Sketch editing keeps its own key handling. These handlers are attached to
the current Hole/Thread inputs; they introduce no new document transaction or persistence rule.

Proof: `[hole-thread-keyboard-shell]` in
[combined shell acceptance](../tests/gui/gui_shell_hole_thread_acceptance_tests.cpp), run with the
existing `[property-controls-widget]` Extrude baseline. The case checks incomplete and oversized
values, focus/correction, Enter/Tab, pending names on Ctrl+Enter, committed Save, Repeat/Escape,
one Undo/Redo and exact reopen, with native OCCT availability and compact guidance/action bounds.
Verification: focused offscreen keyboard/Extrude **4 cases / 308 assertions**; affected shared Qt
selection **66 cases: 58 passed, 8 native-only skips / 5,403 assertions**; native keyboard/Extrude
**4 cases / 325 assertions** at each of 100% and compact 150%, all completed with exit 0. The
PID-matched native startup reached `IsViewable` before its intentional timeout (124). Original
[incomplete Hole input](images/hole-keyboard-invalid.png) and
[compact Thread preview](images/thread-keyboard-preview-compact.png) show the retained task controls.
All 156 GUI/app dependency records are valid. Frozen runtime: **1,063 files**, SHA256
`3867fa78784cbbce25642e22e2f17e40672c46cf4b717e3868c49d49760dc3eb`.
Failed and superseded runs contribute no proof. This component alone does not accept parent 157;
the combined authoring, Pattern/Assembly/STEP and inventory gates remain assigned below the plan.

## Why more than a cut

```text
Weak:
  Sketch001: circle diameter = 6.6 mm
  Cut001: depth = through_all

Strong:
  HoleFeature
    type = clearance_hole
    screw_size = M6
    fit = normal
    diameter = derived_from_standard_table
    depth = through_all
    placement = circular_pattern
    count = Assembly.bolt_count
    radius = Assembly.bolt_circle_radius
```

The strong model knows the hole is an M6 clearance hole, so that meaning survives into drawings and downstream modules.

## Hole types

Simple, through, blind, threaded, countersunk, counterbored (cylindrical), stepped, fitted/reamed (tolerance), slot, and center hole. Each is a semantic variant of `HoleFeature`.

## Data model

```text
HoleFeature
  id, name
  target_body
  placement_plane          # semantic face/datum reference
  placement_points
  hole_type
  standard_family, size, diameter
  depth_mode, depth_value
  thread_data
  counterbore_data, countersink_data
  tolerance_data
  pattern_data
  manufacturing_data
  generated_shape          # cache
```

Examples:

```text
HoleFeature M6_Clearance_Holes
  placement_plane = BasePlate.top_mounting_face
  hole_type = clearance_hole, screw_size = M6, fit = normal
  diameter = StandardTable.clearance_hole(M6, normal)
  depth_mode = through_all
  placement = circular_pattern
  pattern_radius = Assembly.bolt_circle_radius
  pattern_count = Assembly.bolt_count

HoleFeature M8_Threaded_Hole
  placement_plane = MountingBlock.top_face
  hole_type = threaded_hole, thread_size = M8, thread_pitch = standard_coarse
  thread_depth = 18 mm, drill_depth = 22 mm
  core_hole_diameter = StandardTable.tap_drill(M8, coarse)
  depth_mode = blind
```

## Placement

Single point, multiple free points, points from a sketch, rectangular pattern, circular pattern, along a construction line, concentric to an existing axis, or relative to edges/center lines/datum planes. Placement must be parametric and dimensionable, not only absolute coordinates. The placement face is a semantic reference (`docs/semantic-references.md`).

## End conditions

Blind, Through All, Up To Next, Up To Face, Up To Body, Symmetric, and Through Assembly (multiple bodies). A hole that affects several parts of an assembly must be distinguished from one that changes a single part (assembly hole feature, below).

## Assembly hole features

A hole defined in an assembly can create coordinated holes across parts, e.g. clearance holes in a cover and threaded holes in a base block from one bolt circle:

```text
AssemblyHoleFeature Housing_Bolt_Holes
  placement = circular_pattern
  radius = Assembly.bolt_circle_radius, count = Assembly.bolt_count, screw_size = M6
  affected_parts:
    CoverPlate: clearance_hole, through_all
    BaseBlock:  threaded_hole, thread_size = M6, thread_depth = 14 mm
```

This is central to top-down design: the bolt circle is defined once, several parts depend on it. See `docs/parameter-model.md` and `docs/assembly-system.md`.

## Standards database

Many dimensions (clearance holes, tap-drill sizes, thread sizes, counterbore/countersink dimensions, fit tolerances) come from a versioned standards database, not free numbers. Every auto-chosen dimension must have a source (licensed, manufacturer, or user data). Standards data must not be copied uncontrolled from the internet; the system owns the database structure but sources concrete values from licensed/manufacturer/user tables.

## Threads as semantic features

Threads should default to a cosmetic/semantic definition used in drawings, exports, and manufacturing info; real modeled thread geometry is optional (for 3D printing, rendering, or special simulation) because it creates many faces and slows large assemblies.

```text
ThreadData
  type = internal_thread, thread_system = metric
  nominal_size = M8, pitch = 1.25 mm, tolerance_class = 6H, thread_depth = 18 mm
  cosmetic_representation = true, modeled_thread_geometry = false
```

## Validation

Minimum edge distances, minimum hole-to-hole distances, sufficient thread depth, sufficient material for countersinks, collisions with other features, screw length vs. part thickness, countersink vs. head, hole fully on the selected face, sensible tolerance on fitted holes. Warnings are shown but do not always block; the user keeps control.

## Dependency-graph integration

A `HoleFeature` depends on the placement face, hole parameters, standards data, assembly parameters, and target body.

```text
Assembly.bolt_count -> HoleWizard_M6_ClearancePattern -> BasePlate.Shape
Assembly.bolt_circle_radius -> HoleWizard_M6_ClearancePattern -> BasePlate.Shape
BasePlate.top_mounting_face -> HoleWizard_M6_ClearancePattern -> BasePlate.Shape
```

## MVP scope and sequence

1. Add a `HoleFeature` intent model for a single clearance hole on a semantic face, `through_all`.
2. Add depth modes (blind, through all).
3. Add circular and rectangular placement patterns driven by parameters.
4. Add the standards-database structure with sourced entries and derive diameters from it.
5. Add threaded holes with cosmetic thread data.
6. Add validation warnings.
7. Later: assembly hole features, fastener generation, counterbore/countersink, slots, fitted holes.

## Role in the system

The hole wizard binds part modeling, assembly modeling, the standards/fastener library, and engineering assistants. It creates `HoleFeature`s (and optional `FastenerPattern`s, see `docs/pattern-and-mirror-features.md`) and updates the dependency graph, enabling later drawing derivation, BOMs, and screw placement.

## Out of scope for the first version

- automatic fastener/washer instances.
- drawing-note generation.
- through-assembly holes before the assembly document exists.
