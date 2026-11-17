# Variable Fillet groups and radius stations — Block 175

`FilletFeature` retains its legacy constant factory, radius binding and six-field JSON record.
Optional immutable advanced intent adds variable laws to the same normal feature family and Body
history. It is copied by value into shared const storage; editing an input draft cannot mutate a
record already owned by the document. `parameter_dependencies()` returns every distinct radius
binding in authored order. The legacy radius accessor continues to describe the constant binding;
advanced callers inspect the complete typed intent.

## Ordered intent

Each group has a stable ID, a name, an ordered list of semantic edge references with explicit
member reversal, a propagation rule, an interpolation rule and stable radius stations.

- `Explicit` requires the complete declared contour. `TangentChain` permits deterministic expansion
  on the current target Body. Geometry must reject ambiguous branches, missing/split references,
  disconnected members and expansion inconsistent with the authored rule.
- Station positions measure normalized **true curve length** along the complete oriented resolved
  contour. A fraction is never a kernel curve parameter. `Start` and `End` identify the semantic
  contour endpoints with canonical fractions 0/1. The group explicitly records whether its
  radius-station basis opposes its member direction.
- Station IDs are unique throughout the feature, group IDs are unique and every member belongs
  to exactly one group. Fractions must be finite and in [0,1]; effective positions are strictly
  increasing without duplicates and cover the complete contour from zero to one.
- `Linear` defines a continuous piecewise linear radius law. `MonotoneCubic` specifies positive
  shape-preserving cubic Hermite interpolation, without the overshoot of an unconstrained spline.
  Native cap construction uses endpoint tangent continuation; nonpositive/singular continuation
  fails rather than substituting another radius. Closed-contour seam feasibility belongs to 176.

`reverse_fillet_group` reverses member order/orientation and toggles the station-basis direction,
retaining the authored station table, IDs and physical radius attachments. It does not replace
any authored fraction literal with `1-f`. Reversing twice therefore restores the
original payload exactly, including high-precision station fractions and radius bindings.

Each radius references an existing Length parameter, including expression parameters and shared
bindings. Updating a binding or its formula invalidates the Fillet through the ordinary dependency
graph. Core validates target ownership/history provenance and intent shape; 176 decides whether
the resolved geometry and evaluated radii can actually be constructed.

## Normal history and persistence

Variable intent uses the existing add/update/remove Fillet commands and executable inventory,
with the same in-place Solid target and normal suppression/history behavior. Foreign producers
are rejected before publication. A replacement member retains all unaffected group/station IDs,
fractions and parameter bindings; Geometry must revalidate the repaired contour. Failed updates
leave the owned document and dependency graph unchanged.

Part version **14** is necessary exactly while advanced Fillet intent exists, including suppressed
records. Lower versions cannot carry it and an unnecessary version-14 envelope is rejected.
A variable record stores `id`, `name`, `type: fillet`, `target_body`, `mode: variable` and `groups`;
it has no competing top-level constant `radius_parameter` or flat edge list. Group/member/station
objects reject unknown fields, malformed types, unsupported rules and duplicate identities.
Project persistence delegates the same typed Part record. Legacy constant Fillet and all three
Chamfer records keep their existing meaning and serialized form.

The native adapter dispatches advanced intent through the [checked variable engine](geometry-variable-fillet-mvp8.md),
including physical station mapping after OCCT guide approximation. It never substitutes the first
radius for the complete law. Face/full-round roles and corner management remain 177–179.

## Verification status

Core foundation verified on 2026-11-17: all five development targets built successfully; the
focused variable-Fillet/edge-treatment/history-intent run passed **28 cases / 1,244 assertions**
and the full Core suite passed **791 cases / 35,193 assertions**. Coverage includes tiny literal
fractions and exact double reversal, malformed/needless/downgraded envelopes, all radius
dependencies, failed formula/value rollback of the complete document and graph, ownership/repair,
suppression/removal, Part/Project roundtrip and legacy six-field constant compatibility. Logs
are in `build/verification/block175/`. Resolved tangent expansion, branches and split/lost edges
remain part of the joint 175/176 Geometry gate; whole-block status is owned by `mvp-plan.md`.
