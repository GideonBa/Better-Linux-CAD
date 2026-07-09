# Shaft Wizard

Status: target architecture. Not implemented yet. This is a large engineering module and should be built well after the part-modeling core.

The shaft wizard is a hybrid module of calculation, standards data, part generation, and parametric geometry. From a selected axis or sketch line it creates a parametric shaft: it takes loads and boundary conditions, evaluates bearing concepts, computes minimum diameters, suggests bearings, and generates standards-compliant shoulders, bearing seats, retaining elements, and shaft-hub connections. The designer describes functional intent; the system produces a technically sound, editable, parametric shaft.

## Separation of axis, calculation, and geometry

The wizard does not directly create a fixed solid of revolution. It owns a semantic shaft model; the geometry is the result of that model.

```text
ShaftFeature
  id, name
  axis_reference                 # selected sketch line / axis
  coordinate_system              # local shaft coordinate system
  length
  operating_data                 # speed, torque, application factor, required safety, bearing life
  material                       # from material database, with source flag
  load_cases                     # ShaftLoad[]
  supports                       # ShaftSupport[]
  bearing_arrangement            # fixed_floating, adjusted, O, X, floating, custom
  strength_results               # ShaftStrengthResult[]
  bearing_candidates
  selected_bearing_solution
  segments                       # ShaftSegment[]
  transitions                    # ShaftTransition[]
  retaining_elements
  shaft_hub_connections
  generated_shape                # cache
```

Segments describe the shaft along a local x coordinate; each has a function so later elements bind to semantic regions (`bearing_seat`, `hub_seat`, `threaded_end`), not arbitrary faces.

```text
ShaftSegment { id, x_start, x_end, diameter, function, tolerance, surface_roughness, fit, locked_by_user, generated_by }
ShaftTransition { id, left_segment, right_segment, type=shoulder, fillet_radius, undercut, notch_factor }
ShaftLoad { id, load_case, position_x, force_vector, moment_vector, rotating_or_static, description }
ShaftSupport { id, position_x, support_type, bearing_type, selected_bearing, radial_reaction_y, radial_reaction_z, axial_reaction }
```

## Input areas

- **Operating data**: speed n, torque T, direction, life/duration, load type, application factor K_A, required minimum safety S_min.
- **Material and manufacturing**: material (e.g. C45, 42CrMo4), heat treatment, tensile strength R_m, yield R_e, fatigue values, surface condition, notch sensitivity, temperature range. Material is data, not just text, and comes from a controlled database that flags values as normative / manufacturer / user.
- **Loads and application points**: distinguish external loads (gears, pulleys, chains, couplings, axial forces) from bearing reactions. Support radial force, axial force, torque, bending moment, force couple, gear force (tangential/radial/axial), belt/chain force.
- **Bearing concept**: fixed-floating, adjusted, O-arrangement, X-arrangement, floating, custom. Derives which bearings carry axial/radial load and which DOF are locked.

## Calculation model (deterministic, no AI)

Steps must be separable and traceable:

1. **Static beam model** from axis, supports, and external loads. Compute bearing reactions (y, z), axial reactions, shear diagrams, bending diagrams `M_y(x)` and `M_z(x)`, resultant `M_b(x) = sqrt(M_y^2 + M_z^2)`, and torque `T(x)`.
2. **Preliminary minimum diameters** per critical section from bending, torsion, axial force, notch data, and material, with equivalent stress and required safety. Orient the final check on an accepted method such as DIN 743, and produce a strength report, not just a number.
3. **Critical sections** detected automatically at shoulders, notches, keyways, retaining-ring grooves, thread starts, bearing seats, load application points, and maximum-bending locations.

```text
ShaftStrengthResult
  section = x = 65 mm
  bending_moment = 180 Nm, torque = 120 Nm
  notch_type = shoulder_with_fillet
  required_diameter = 28.4 mm, selected_diameter = 30 mm
  static_safety = 2.1, fatigue_safety = 1.9, status = valid
```

## Automatic bearing selection

Deterministic, from a bearing database (licensed / manufacturer / user). Hard filters (bore fits shaft diameter, load rating, speed limit, bearing type matches concept, axial capacity, envelope, required life) then scoring (safety reserve, envelope, cost class, availability, mountability, standardness, small diameter change, few extra retaining elements). Present several ranked candidates.

## Geometry generation and retaining elements

When a bearing combination is chosen, the system generates bearing seats, shoulders, undercuts, and axial retentions using construction rules (seat diameter = bore, shoulders must support the bearing axially without colliding with the bearing chamfer, manufacturable undercuts/radii, diameter steps within user limits). Retaining elements (shaft/bore retaining rings, lock nuts, spacer sleeves, shoulders) are bound to a segment function, and the system also creates the needed groove or thread.

```text
DiameterStepConstraints { min_delta_d = 2 mm, max_delta_d = 8 mm, preferred_step = 5 mm, allow_nonstandard_steps = false }

RetainingElement { type = shaft_retaining_ring, standard = DIN_471, position_x = 18 mm,
                   target_segment = Segment_02, purpose = axial_retention_of_bearing_inner_ring }
```

## Manual editing and shaft-hub connections

Every segment stays editable (length, diameter, insert/remove segment, function, tolerance/fit, undercut/transition radius, surface note). Editing triggers recompute of adjacent segments, bearing distances, and strength at critical sections, with a warning if safety drops too low. Shaft-hub connections (parallel key DIN 6885, woodruff key, splines, involute spline DIN 5480, press fit, clamp hub, later polygon) generate the matching keyway/geometry and check diameter range, standard data, load, and envelope. Fillets/chamfers at ends and shoulders use `docs/fillet-chamfer-features.md`.

## Pipeline

```text
Input axis + user data -> abstract shaft model -> static beam model -> bearing reactions
  -> bending/torque diagrams -> critical sections -> required diameters -> bearing candidates
  -> user selects combination -> shaft segments -> shoulders/grooves/threads/retaining elements
  -> OCCT geometry -> update CAD document and dependency graph
```

Each intermediate stage should be inspectable in the UI so the user understands why a shaft is proposed.

## Integration into the parametric core

The shaft is a normal feature in the feature tree and uses parameters. Changing `torque` recomputes loads and minimum diameters; if a segment is not `locked_by_user`, its diameter updates automatically; if locked, the diameter is kept and a fatigue-safety warning is shown.

## Role of AI

Engineering sizing must be deterministic, traceable, and checkable. AI may later assist with natural-language explanation of results, sensible start values, catalog search, onboarding help, and warning summaries — never with deciding safety-relevant diameters, bearing selection, or standard geometry.

## Proposed implementation sequence

1. Add the `ShaftFeature` semantic model (axis, segments, transitions) with no calculation.
2. Generate a stepped solid of revolution from segments in the geometry layer.
3. Add JSON serialization, roundtrip, and recompute tests for a manually defined stepped shaft.
4. Add the static beam model and bearing-reaction / diagram computation.
5. Add critical-section detection and minimum-diameter calculation with a strength report.
6. Add the bearing database and deterministic bearing selection.
7. Add automatic geometry generation (seats, shoulders, retaining elements).
8. Add shaft-hub connections, starting with parallel keys.

## Out of scope for the first versions

- full DIN 743 fatigue proof before the geometry/segment model is stable.
- involute-spline exact geometry (later module).
- motion analysis.
