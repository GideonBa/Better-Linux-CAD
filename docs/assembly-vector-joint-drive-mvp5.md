# Assembly Vector Joint Drive MVP-5

Status: implemented as Block 43 of `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

This document is canonical for role-addressed joint drives, selected/non-selected holding semantics, complete coordinate-slot freshness, local/cross-hierarchy parity, atomic result application, and the Block-44 family handoff.

## Scope

Block 43 generalizes the Geometry motion request/result/application boundary. It adds no joint family, target capability, residual family, transform variable, persistence field, or JSON spelling.

The public transient request is:

```text
AssemblyJointDrive
  selected AssemblyJointId
  requested_coordinates[]

AssemblyJointCoordinateDrive
  stable AssemblyJointCoordinateRole
  typed Quantity requested_value
```

Both `AssemblyJointMotionSolver` and `AssemblyCrossHierarchyJointMotionSolver` accept this request. Their historical `(joint_id, Quantity)` Revolute overloads construct one `rotation` drive and delegate to the vector boundary.

## Drive validation and order

A request must:

- identify the selected joint exactly;
- contain at least one requested coordinate;
- contain no duplicate role;
- use only roles defined by the selected joint family;
- use the physical Quantity kind required by that slot;
- keep every requested value inside its authored optional bounds.

Input request order is not authoritative. Validation restores deterministic family slot order in result state and numeric adaptation.

Current Revolute therefore accepts exactly one explicit Angular `rotation` value. The generic boundary is ready for partial multi-coordinate requests, but no richer family is introduced in Block 43.

## Holding semantics

The selected joint resolves a complete transient drive in family order:

```text
explicitly requested role -> requested typed value
omitted selected role     -> authored typed value
```

Every other active joint in the same combined motion closure is held at every authored coordinate slot. Suppressed/inactive relationships retain the established graph-participation rules.

Current Revolute numeric builders consume the resolved `rotation` value through an explicit family adapter. Authority variables remain six direct rigid-transform variables per unique free authority. Joint-coordinate values remain drive parameters and never become transform-authority variables.

The existing central finite-difference and Gauss-Newton engine remains authoritative.

## Result and freshness contract

Solver-generated results retain the scalar Revolute source/request fields as compatibility views and additionally own canonical `requested_coordinates`.

Every participating local or Project-level joint snapshot includes its complete ordered `AssemblyJointCoordinateSlot` vector. This protects:

- role and kind;
- authored value;
- optional lower and upper limits;
- slot count and family order;
- selected explicit drive roles and values.

Application rejects stale or tampered slot definitions, authored values, limits, selected drives, relationship participation, authority inputs, hierarchy boundaries, or semantic PartDocument inputs.

## Atomic application

Application first revalidates every freshness boundary. It then works on one Project copy and applies:

```text
one direct component transform per free authority proposal
+
selected-joint authored values for exactly the explicit requested roles
```

Omitted selected roles and every non-selected joint remain authored-state unchanged. Any transform or coordinate failure discards the candidate Project, so no partial transform/coordinate state escapes.

## Local and cross-hierarchy parity

Local and Project-level solvers share the same drive validation and family-order restoration. They retain distinct target identity and result authority scopes:

```text
local             -> AssemblyConstraintTarget + local component proposals
cross-hierarchy   -> rooted endpoints + ComponentTransformAuthority proposals
```

No endpoint scope is merged.

## Acceptance coverage

Focused tags:

```text
[geometry][assembly-vector-joint-drive]
[geometry][assembly-cross-hierarchy-vector-joint-drive]
[geometry][assembly-vector-joint-drive-application]
```

Coverage proves scalar/vector adapter equivalence, typed role validation, duplicate/unknown-role rejection, deterministic requested-role state, complete slot snapshots, holding of other active joints, limit-definition freshness, successful local/cross application, and atomic stale/tampered failure.

## Block 44 handoff

Blocks 44–46 Prismatic, Cylindrical, and Planar plus Block 47 passive Spherical are implemented in
their canonical documents. Spherical joins holding/motion closure as a zero-slot relationship but
is explicitly rejected as the selected drive. Blocks 48–55 Body identity, body-scoped
recompute/inspection, BodyBooleanFeature intent, and Boolean Geometry are implemented; Block 56
BodyTransform and SketchOwnership Core intent plus JSON is the next technical step.

Block 44 must add one `translation` Linear coordinate, persistent local/Project intent and JSON spelling, an explicit oriented target compatibility contract, deterministic residual order, shared local/root-space equations, graph participation, vector-drive execution, complete freshness, and atomic application. It must reuse the Block-43 drive boundary rather than adding a family-specific motion API.
