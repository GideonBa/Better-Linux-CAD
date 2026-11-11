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

Extend `GuiFeatureEditController` and the existing `ShellFeatureEditTools` with Hole and Thread
families; browser activation and History Edit use those same Core update commands. No save-schema,
resource-store, pattern or transaction authority changes. Preserve the feature/history slot and
placement/step IDs. Existing custom dimensions retain their parameter bindings; formula values
remain read-only until an explicit rebind. Pinned table keys, revisions, row and column mappings
are immutable in this slice; adoption continues through Manage resources.

Property rows wrap to the available panel width; nested source/profile groups must not require
horizontal scrolling at ordinary or compact high-DPI sizes. Present existing intent in the Hole order: name/positions, target/support/direction, bore/seat,
extent/tip, thread. Thread uses target/support/direction then its definition. Numeric fields use
the actual Length/Angle parameter type, including thread start/length/pitch and drill depth.
Handedness and cosmetic/modeled representation are explicit choices; geometry/cost failures clear
the candidate and block Apply. Retained unsupported recipe discriminants remain visible and fixed.
Adding a position or step assigns a fresh opaque UUID-based identity, retained through Apply and
reopen. New entries never recycle deleted identities across drafts or committed edits. Removing a consumed position must
fail through downstream validation rather than bind another one.

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
