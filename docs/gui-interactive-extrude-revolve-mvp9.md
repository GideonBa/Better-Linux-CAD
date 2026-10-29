# GUI Interactive Extrude, Path Extrude, and Revolve MVP-9

Status: headless controllers implemented in Block 124; their replacement-shell binding is implemented
in Block 135. The old Qt binding was deleted with Block 133
(`docs/gui-shell-reset-sequence-mvp9r.md`); `ShellFeatureTools` now hosts this surface on
`ShellWindow`. Every sentence about `MainWindow` ownership is historical.

Block 124 upgrades the MVP-7 form-driven Extrude and Revolve validation commands
(`docs/gui-part-foundation-workbench-mvp7.md`) into direct-manipulation authoring. It consumes the
Block-122 selection-first workspace and the Block-123 candidate-only manipulator layer. No new Core or
Geometry intent is introduced: every one of the seven extrude extent modes, taper, thin intent, path
direction, and the three revolve extents are the frozen Block-52..62 contracts
(`docs/part-extrude-extent-intent-mvp6.md`, `docs/part-revolve-intent-mvp6.md`,
`docs/part-feature-body-operation-mvp6.md`).

## Authority boundary

```text
profile-region preselection / Finish-Sketch handoff
  + driving Length parameter / revolve axis reference / result Body
  -> GuiInteractiveExtrudeController | GuiInteractiveRevolveController (headless)
     candidate feature parameter set
     Block-123 extent / taper / thin / angle handles
     disposable PartDocument clone + recompute plan (fail closed)
  -> one commit_part_transaction (seed driving parameters + add feature)
  -> existing recompute / freshness / undo authority
```

The controllers own no `PartDocument`, `ShapeCache`, or save-format field. They compose existing public
Core operations (`set_parameter_value`, `add_feature`, `add_revolve_feature`, `create_recompute_plan`)
on a throwaway clone for preview and inside one transaction for Apply.
`GuiInteractiveFeatureCoordinator` is a Qt-free connector owned by `ShellFeatureTools`; it publishes
a controller's handles to the shared `GuiViewportManipulatorLayer` and routes manipulator candidates
back into the active controller. The shell-owned overlay forwards its candidate callback into the
coordinator, so a viewport drag folds into the candidate without any Core authority living in a
widget.

## Extrude authoring

`GuiInteractiveExtrudeController` begins from a materialized profile region, an existing `Length`
distance parameter (whose current value seeds the extent arrow), and an existing result `Body`. It
exposes:

- operation toggle `NewBody | Join | Cut | Intersect` (modifying modes target an existing Body);
- all seven extent modes: `Distance`, `Symmetric`, and `TwoSided` drag the driving distance
  parameter(s); `ThroughAll` and `ToNext` carry no scalar handle; `ToFace` and `Between` consume
  pre-resolved semantic `FaceReference`s and fail closed when a limit face is missing;
- optional taper (an angle-wheel handle) and optional thin-wall intent (`OneSided | TwoSided |
  MidPlane`) driven by an existing `Length` thickness parameter with a non-negative thickness handle;
- path extrude: `set_path_curve(...)` switches to `ExtrudeDirection::Path` and authors a
  `create_additive_path_extrude` feature previewing the trajectory (no scalar extent).

The extent arrow (`extrude.extent`, Linear), taper wheel (`extrude.taper`, Angular), and thin handle
(`extrude.thin`, Linear) are only offered when the current mode uses them. Apply seeds the driving
distance and thickness parameter values and adds the feature in one transaction, so undo reverts the
parameter change and the feature together.

## Replacement-shell extent editor — Block 136, 2026-10-24

The existing Extrusion task now exposes all seven scalar/face termination modes, `NewBody`,
`Join`, `Cut` and `Intersect`, a visible recomputed solid target, normal/opposite direction, the
second distance for `TwoSided` and optional literal taper. The normal extent HUD retains its
last-valid-value rollback contract. New second-distance/taper fields retain rejected text and
disable Apply until corrected; incomplete text cannot apply an earlier candidate.
Inward taper that collapses/reverses a corresponding profile edge is rejected before lofting;
typed and handle-driven candidates share this Geometry check.

`ToFace` and `Between` offer explicitly selected, resolved semantic source-feature planes from the
existing construction-source boundary. They are not arbitrary OCCT subshape ordinals or references
to a modified final Body. Sources are the six box roles of untapered, normal `Distance` rectangle
extrusions, including rectangles drawn as four lines. Symmetric/reversed/tapered features do not
implicitly gain generated box roles. Transverse planes, coincident limits, limits behind a `ToFace`
direction and unsupported profile/taper combinations fail in the disposable complete-document recompute.
`ThroughAll` and `ToNext` require a modifying operation and a target Body. They have no distance
input or extent arrow; the three distance modes retain the primary extent handle, and taper adds
its own angle handle. Direction reversal also reverses the extent handle frame.

Only inputs used by the final mode are provisioned: two distances for `TwoSided`, one for
`Distance`/`Symmetric`, none for face/target terminations; a new Body is added only for `NewBody`.
New IDs reserve both distance identities against the full document dependency namespace. Preview
replaces only the modifying target Body; supporting sketches and planes retain their presentations.
Apply publishes the feature and its used inputs in one transaction, preserving exact Undo/Redo,
sidecars and canonical reopening. Existing Feature Edit can edit retained parameter/reference
fields; it does not change extent discriminators.

### Cut outcome and curved-profile follow-up — 2026-10-28, issue #113

The scalar/face extent executor now accepts selected circles and line/arc/spline closed profiles.
It keeps exact circular arcs and rational cubic spans, translates the contour to the resolved start
plane and extrudes through the resolved end distance. Offset and oblique workplane frames retain
their authored coordinates. Circular outer boundaries retain analytic center/radius during hole
classification; selected curved contours retain their immediate inner material-region boundaries.
Nonzero Draft on a curved outer profile is explicitly unsupported and asks the user to clear the
Draft angle.

This repairs the runtime failure where a circle accepted for historical Through All was rejected
by the richer Distance/Two Sides executor. The existing schema, selected profile identities and
extent/limit resolvers remain authoritative. Selected curved Through-All features use the same
exact span path; unselected historical hole-pattern features retain their aggregate executor.

Cut also requires measurable material removal. Disjoint/tangent tools, full target removal and
invalid geometry have distinct task diagnostics; the exact policy and numerical resolution are
owned by `docs/part-body-boolean-geometry-mvp6.md`. The task hint names target, Normal/Opposite
direction and depth/Through All as correction choices. An invalid preview clears the replacement
presentation and disables Apply. The existing primary numeric HUD still rolls back rejected values
visibly; a rejected Cut cannot commit through a forced Apply call. Correcting target/direction/
extent and cancelling a recovered preview both retain the existing transaction contracts.

`[cut-material-diagnostics]` checks disjoint/tangent tools at three scales, aggregate tools and hole
patterns, tiny cuts, full removal, invalid solid inputs, source-cache immutability, pre-Cut replay,
exact circular/arc spans with inner holes, independent material-point classification and persistence.
`[cut-material-workflow]` uses real widgets, keyboard input and native model picks for multiple
Bodies, wrong-target/full-removal/existing-void diagnostics, offset/oblique circle Cuts after Fillet,
direction/Two Sides/Through All recovery, cancellation, failed-Apply atomicity, analytic volumes,
upstream depth edits, exact history, canonical reopening and STEP export. Native hole probes align
and magnify the view until the center is beyond the boundary's screen-space selection tolerance.

Final issue #113 verification (2026-10-28): all five GUI-preset targets build. Full Core passes
**545 cases / 17,037 assertions**, full Geometry **459 / 35,243**, and full offscreen GUI
**552 passed / 20 expected native-only skips / 76,150 assertions** (572 cases). The expanded native
Cut/extent/Revolve/Body/finishing/Union regression including required shell coverage passes
**20 / 7,596**, and the new Cut plus associative-tool workflows at 150% pass **3 / 1,918**;
neither native run skips cases. A separate native application startup reaches the intentional
six-second timeout (124). Inspected ordinary-scale diagnostic/hole captures and verified all
**737** frozen source/test/asset/build-definition hashes after testing. Builds, source edits, GUI
suites and startup were serial; final native/scaled/startup logs contain no High-severity,
BadDrawable or BadWindow errors. The available X11 visual retains its known OCCT Medium depth/stencil
warning. Reproduction commands live in `docs/development-setup.md`; local evidence is
`/tmp/blcad-issue113-{build,core-full,geometry-full,gui-full,native-regression,native-150,startup}.log`
and `/tmp/blcad-issue113-native{,-150}/`.

The initial simple-box regressions fail on the previous adapter, and the new native offset-circle
workflow exposed the richer executor's unsupported-profile failure before its fix. The broader
Body history case now checks rejected disjoint tool movement and a valid associative overlap,
rather than accepting an upstream edit that turns its Cut into a no-op. These reproduce concrete
failures; they do not claim every possible reported model failure has the same cause.

Focused proof: `[block136-extrude-extents]` in `gui_shell_extrude_extent_tests.cpp`. The actual-input
`[shell-combined-extrude-extents]` workflow creates a plate and offset sketch, cuts with two sides,
corrects transverse/coincident face bounds, cuts to/between source planes, checks exact history,
cancels a symmetric tapered candidate, reopens canonically and exports STEP.

The bounded thin, path-directed and expanded Revolve surfaces are described below.

### Replacement-shell viewport limit-face collection — Block 136, 2026-10-24

For `ToFace` and `Between`, `blcad.extrude.pick_role` explicitly activates the first or second
limit role. `Auswahl beenden` exits collection; the second role exists only for `Between`. The
existing face combos remain usable and synchronize the highlight of the active role. Collection
publishes disposable source-plane presentations from the same `build_construction_sources` result
that populates those combos. Face-only filtering plus an exact semantic-ID scope excludes Bodies,
sketches, datums, edges, arbitrary final-Body faces and disposable result previews. The selected
source plane is highlighted even when it makes the candidate invalid. Source planes are translucent
and use outline-only selection highlighting so their historical boundary does not cover the modified
Body preview. Scalar handles cannot capture a reference click while collection is active.

Transverse, coincident and otherwise invalid limit combinations retain their local typed references
and active role, clear the result preview and disable Apply. A subsequent valid pick recomputes the
complete candidate. No parameter, Body, feature or history entry is written by collection. Changing
extent/direction to a mode without face bounds exits collection and removes the source presentations.
Exiting collection restores the previous selection filter/scope and viewport selection; Apply,
Cancel, Escape, Undo and document replacement remove all task-owned source presentations. Command
completion restores the saved selection, including the Finish Sketch fallback when it had no viewport
presentation. Escape retains the existing whole-command cancellation behavior.

The source boundary stays limited to the six generated box roles described above; collecting a
historical source plane does not reference a modified final Body subshape. Focused proof:
`[block136-extrude-limit-picking]`, including actual native Right/Top/Bottom clicks. The combined
`[shell-combined-extrude-limit-picking]` workflow authors three Bodies through Qt controls, preserves
an earlier pocket, rejects a transverse cut limit and coincident wall limits, collects `ToFace` and
Thin `Between` bounds, changes source depth, edits an earlier feature, verifies five-step history,
reopens canonically and exports STEP. Native runs click actual source-plane locations; offscreen uses
the same visible combo controls and verifies their synchronized semantic highlight.

### Replacement-shell open-line Thin Extrude — Block 136, 2026-10-24

A Sketch containing exactly one nondegenerate real line and no profiles or additional curves is
eligible for Thin Extrude. Finish Sketch hands it to the existing Extrusion task; browser Sketch/
line selection and semantic viewport line selection use the same bounded predicate. Revolve still
requires a closed profile. General chains, arcs, splines, projected/reference lines and closed
profiles do not gain thin semantics. Standalone points do not contribute to the wall region.

The controller's auto-provisioning entry accepts an absent ProfileId only for this validated open
line; it never invents or materializes a closed profile. Such a candidate requires thickness intent
before Preview/Apply. Closed-profile callers and their existing begin contract remain unchanged.
The shell offers `Einseitig links`, `Mittig` and `Zwei Seiten`, a positive new first thickness and,
only for TwoSided, a positive new second thickness. Left/right follow the authored Sketch-line
direction, independent of normal/opposite extrusion. MidPlane uses half the first (total) thickness
on each side. Path direction is unavailable for open-line candidates.

The seven retained straight extent modes, optional taper and four Body operations use the same
disposable whole-model recompute and target replacement as closed Extrude. Used thickness inputs,
used distances, the feature and a NewBody result publish in one transaction. Switching wall modes
does not publish an unused second thickness; target operations do not add a pending new Body.
Generated IDs/names reserve both thickness inputs against the global dependency namespace.
New thickness text retains invalid values and disables Apply until corrected. Editing another
field does not silently replace a pending invalid thickness. Cancel and document replacement
discard all candidate inputs and leave Sketch topology and intent sidecars untouched.

The first-thickness handle represents the full first value, including MidPlane's total thickness.
Its origin is the line midpoint and its axis is the resolved left direction in the Sketch plane.
Depth, taper and thickness have independent rollback values and editors; failed geometry candidates
and Escape restore the last accepted thickness without overwriting the extrusion depth.
The second thickness remains a numeric input. Existing persistence records the optional `thin`
intent with Length parameter references and omits `profile` for the open line; there is no schema
change. Later parameter edits recompute downstream Body operations through existing dependencies.

Proof: `[block136-thin-extrude]` and `[shell-combined-thin-extrude]`. The combined actual-input
workflow authors two open lines, machines a mid-plane groove and one-sided through slot, creates
an independent two-sided symmetric wall, edits an earlier thickness through downstream cuts,
renames the earlier feature while retaining thin intent, checks exact history, cancels a
tapered/reversed draft, reopens and exports STEP.

### Replacement-shell path Extrude — Block 136, 2026-10-24

The same Extrusion task offers `Entlang Pfad` alongside normal/opposite direction. Its explicit
`blcad.extrude.path` selector lists existing open PathCurve names and retains stable IDs as references;
closed paths are excluded. Path authoring and orientation remain in the existing PathCurve task. Extrusion
consumes that orientation without adding Sweep-specific twist/guide controls. Supported profiles
and planar/construction/Sketch3D/generated-edge sources retain the Geometry boundary documented in
`docs/part-path-extrude-geometry-mvp6.md`; composite regions with holes remain unsupported.

All four Body operations use the existing additive Extrude identity and Body result context,
including Cut. Path mode disables the extent/taper controls, hides scalar/face inputs and publishes
no extent, taper or thin manipulator. Stored straight-field text is retained locally and validated
again on return to normal/opposite direction. Missing or incompatible path/profile candidates
disable Apply and clear the preview without changing the document. Selecting a valid path recovers
the candidate. Preview recomputes the complete document and replaces only the modifying target.
Conditional rows hide their field and label together, avoiding Qt 6.4 hidden-row occlusion after
mode/operation changes; the path popup remains reachable after revealing a target Body.

Path Apply adds no distance parameters and provisions a Body only for NewBody. In particular, a
previously staged TwoSided distance is not published. Returning to a straight direction clears the
controller's path reference and restores the retained extent/taper inputs. Direct stale manipulator
candidates are rejected in path mode. Apply, Cancel, history and document replacement retain the
same atomic publication and profile-sidecar contracts as straight Extrude. Persistence uses the
existing `direction: path` / `path_curve` fields, with no straight extent object or new schema.

Proof: `[block136-path-extrude]` covers oriented Body operations, a mixed line/spline/reversed-arc
route, mode changes, unused inputs, stale handles, missing selection, excluded closed paths,
incompatible orientation with recovery, cancellation and reopening.
`[shell-combined-path-extrude]` authors its sketches and three-segment Sketch3D path through actual
widgets, cuts a previously machined Body, edits the earlier pocket and path endpoint, recovers a
degenerate route, checks exact history, cancels a switched draft, reopens and exports STEP.

## Revolve authoring

`GuiInteractiveRevolveController` begins from a materialized profile region, an `AxisReference`
(datum axis, construction line, or semantic axis/edge), and a result `Body`. It exposes the
`Revolve | RevolveCut` kind, the operation toggle, and the three frozen extents: `Full` (360°, no
handle), signed partial `Angle` with an explicit side, and `Symmetric` total angle. Partial and
symmetric modes offer the `revolve.angle` angle-wheel handle seeded with the current magnitude.
Revolve angles are literal Core values, so no parameter is seeded; Apply adds the revolve feature in
one transaction.

### Replacement-shell Revolve controls — Block 136, 2026-10-24

The existing Drehung task now exposes `Full`, positive/negative `Angle` and total-angle `Symmetric`
extents, a compatible typed axis and the four Body operations. Choosing Cut authors `RevolveCut`;
the other operations author `Revolve`, preserving Core's kind/operation pairing. The default remains
a positive 180-degree new Body. Full has no angle field or wheel; partial/symmetric modes retain the
last valid magnitude. The existing HUD restores rejected numeric text. Negative Angle reverses the
wheel frame normal; Symmetric uses the canonical axis and stores no side.
Inapplicable target/direction controls stay visible and disabled, preserving reliable form geometry
when the mode changes. The full-angle HUD is hidden.

Axis choices include explicit/line-derived DatumAxis, ConstructionLine, the semantic primary axis
of an eligible single circular Cut and supported semantic generated linear edges. Geometry's
`resolve_feature_axis` exposes the same transformed frame used by execution. Only resolvable axes
in the selected profile plane are offered; an axis-crossing profile remains a correctable geometry
error. A compatible construction axis also enables Revolve on a plane away from all origin axes.
Generated edges retain the existing rectangular-distance source-role boundary; arbitrary final-Body
topology is not offered as an axis.
The initial invalid geometry candidate stays open so another offered axis can be selected.

Target choices are visible recomputed solid Bodies. Preview replaces only the modifying target;
all geometry is recomputed in a disposable document. Only NewBody provisions the pending Body;
angles remain literals and no parameter is created. Apply publishes once, preserving supporting
sketch catalogs and exact history. Cancel/document replacement discard local choices.
Mixed Extrude/Revolve allocation reserves both global identities and the Body dependency-node
identity, including existing Bodies created by another feature family.

Proof: `[block136-revolve]` and `[shell-combined-revolve-controls]`. The combined actual-input
workflow machines negative, symmetric and full grooves, corrects invalid axes/units, edits an earlier
angle through downstream features, checks exact history, reopens canonically and exports STEP.

## Manipulator coupling

Dragging an offered handle produces a Block-123 candidate whose scalar value the controller folds into
the matching parameter (`apply_manipulator`). A numeric HUD override on the manipulator layer is
authoritative in exactly the same way. The proof shows a dragged extent and a dragged revolve angle
committing a byte-identical document to the equivalent typed edit, establishing GUI/headless
equivalence for the new commands.

## Failure policy

Every controller fails closed before any document mutation: missing or non-materialized profile
region, a missing or non-`Length` distance parameter, a missing result Body, a missing second
distance parameter for `TwoSided`, missing limit faces for `ToFace`/`Between`, a missing revolve axis,
and any extent whose value is rejected by the Core contract (for example a zero or 360° partial
revolve angle). Preview validates a disposable clone through the recompute plan and never publishes a
partial `ShapeCache`; Apply re-validates and commits exactly one transaction.

## Focused proof

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][extrude-revolve]"
```

The dedicated `[gui][interactive-extrude]`, `[gui][interactive-revolve]`, and
`[integration][extrude-revolve-manipulator]` cases asserted against the deleted MainWindow shell and
its manipulator overlay binder. They were removed with Block 133 and are restored against
`ShellWindow` by Block 135.

The proof covers extent-parameter driving with one atomic transaction and exact undo, operation/taper/
thin/through-all modes with their offered handles, fail-closed rejection of invalid inputs, full/angle/
symmetric revolve authoring, manipulator/typed document equivalence for both features, and
`ShellWindow` ownership of the coordinator committing a viewport-style drag. The additional
`[gui][shell-features]` proof covers Finish-Sketch handoff, auto-provisioned candidate inputs,
recomputed geometry, mutation-free Cancel, and atomic Undo.

The replacement-shell hardening proof also covers strict `mm`/`deg` parsing, preservation of the
last valid value after rejected text, numeric-override precedence during a live pointer drag,
drag-start rollback on `Esc`/deactivation, rejection of mismatched candidates, collision-safe
generated inputs, default in-plane origin-axis choice, and atomic Revolve Undo.

## Scope and deferrals

Bounded here and not introduced implicitly by this block:

- the **bolt-circle hole** foundation (`GuiPartFoundationWorkbench::apply_hole_workflow`) now also
  has dedicated replacement-shell creation through `ShellHoleTools`, including a single-hole
  variant using the existing CircleProfile cut. Exact bounds are in
  `docs/gui-part-foundation-workbench-mvp7.md`; it does not enlarge the semantic Hole Wizard contract;
- `ToFace`/`Between` consume already-resolved semantic `FaceReference`s; interactive limit-face
  **picking/highlighting** is supplied by the replacement shell role selector above;
- continuous **shaded viewport preview** rendering is the existing recompute/presentation path after a
  validated candidate; the controllers publish the validated preview summary, not pixels;
- path Extrude is available through the Block-136 direction/path controls above. Block 135 maps
  `part.extrude` and `part.revolve` to an auto-provisioned candidate Body/Length parameter and a resolved profile-plane
  frame through `ShellFeatureTools`; those objects persist only with Apply.

## Next boundary

Block 125 adds interactive Fillet, Chamfer, Shell, and Draft over the same manipulator layer and the
supported-topology picking boundary.


## Sketch-line axis selectors — Issue #110

Revolve and Body Rotate offer eligible editable Sketch lines in their existing Rotation Axis
chooser, labelled with Sketch name, entity id and Geometry/Construction/Centerline role. Each item
carries the scoped persistent pair, independently of its position in the chooser. This accessible
selector is the Sketch-line collection route; existing model picking for datum/construction and
semantic-edge sources keeps its bounded scope. No extra Sketch ownership or detached coordinate
snapshot is inferred.

Revolve offers only axes currently in the profile plane; its Geometry preview/Apply still reject
profiles crossing the axis and all existing invalid combinations. Body Rotate accepts arbitrary
spatial Sketch planes. Both commands use the Geometry Sketch-line resolver, which reads current
endpoints and the cache-aware workplane exactly once. Revolve refreshes its manipulator frame from
the resolver during candidate configuration. Changing source endpoints or the plane after Apply
recomputes downstream features through the source Sketch dependency.

`[sketch-line-axis]` and `[sketch-line-axis-revolve]` cover Core identity/JSON/dependencies, posed and
edited Geometry, profile guards and the two GUI selectors. `[sketch-line-axis-native]` authors all
three line roles through native ribbon/numeric input, creates a partial oblique Revolve and 90°
Body Rotate, edits the original endpoint through a viewport drag, then creates a downstream linear
pattern and checks exact history, full recompute, canonical reopening and STEP. Acceptance results
are recorded in `development-setup.md`; tag existence alone is not native interaction proof.
