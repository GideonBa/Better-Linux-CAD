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
supports remain assigned to 153B2; stable generated bore/seat references and combined acceptance
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
