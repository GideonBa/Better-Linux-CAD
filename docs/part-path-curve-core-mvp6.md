# Part PathCurve Core MVP-6

Status: implemented in Block 79.

Block 79 introduces reusable persistent path identity for Sweep, path-following Extrude/Cut, Loft,
and later Surface features. A `PathCurve` owns ordered semantic segment references; it never owns
OCCT edges or traversal indices.

## Persistent model

Each PathCurve stores:

```text
id, name
segments[]
closure = open | closed
orientation_rule = profile_normal | minimum_twist | fixed_up_vector
optional fixed_up_vector
optional continuity_hint = c0 | g1 | g2
connection_tolerance_mm
```

Each segment also stores its authored `reversed` direction. Supported sources are explicit planar
Sketch lines/arcs/splines, projected and reference-generated planar lines, ConstructionLines, 3D
Sketch lines/polylines/arcs/splines/helices, and semantic generated edges. Source kind and exact
owner/entity identity are explicit in JSON.

## Validation and dependencies

- ids, names, segment lists, tolerance, and FixedUpVector intent are validated strictly;
- every referenced source and its declared curve kind must exist;
- finite authored planar and 3D curves are checked in order within the stored positive tolerance;
- authored reversal participates in endpoint ordering;
- closed paths must close within the same tolerance;
- repeated source segments, branch points, and repeated internal junctions fail closed;
- unresolved infinite or derived sources are valid as standalone semantic paths, but cannot be
  silently joined without a provable endpoint connection;
- source nodes feed the PathCurve dependency node, so source removal is protected and invalidation
  propagates without copying geometry.

## JSON compatibility

`path_curves` is always emitted and optional on read. Every path and segment record has a strict
source-dependent field set; malformed, unknown, mistyped, missing-source, disconnected, and
ambiguous records fail loading. Files from before Block 79 load with zero PathCurves.

No resolved endpoints, sampled curves, or OCCT identities are persisted.

## Explicit execution policy — 166

`PathExecutionPolicy::legacy()` is the default for every historical constructor/record. It retains
the existing interpretation, including fixed spline/Helix subdivisions and the existing equation
curve behavior. Reading, saving or editing an unrelated source never upgrades a path implicitly.

The explicit `curve_preserving(tolerance_mm)` policy owns a finite modeling tolerance in
**1e-9…1 mm**. It is separate from connection tolerance, solver tolerances and display tessellation.
Changing only this policy preserves path identity, ordered typed segment references/reversal,
closure, orientation/up-vector, continuity hint and connection tolerance. The atomic Part setter
invalidates the path and its dependent features without replacing source entities, dependency edges
or executable lifetime. Repeating the same policy is a no-op; Reference Parts remain immutable.

Only non-legacy policy intent adds the strict V10 `execution_policy` record described in
[the format contract](file-format.md#curve-preserving-path-policy--part-v10). Returning the final
upgraded path to legacy restores its prior necessary envelope and canonical fields. Typed Core
intent is the first slice; curve-preserving Geometry, explicit native comparison/preview/Apply and
the complete G01 gate remain in progress under 166. No sampled curve product is save-file intent.

Resource-aware Project members retain V10 through the existing Project-owned revision-reference
adapter, including empty local subsets. Material pins/Body overrides remain unchanged and portable
standalone extraction restores only that member's values. Explicit policy downgrade restores the
original necessary member envelope without adopting or discarding unrelated Project resources.

### Native curve construction

The Geometry slice under verification preserves planar rational cubic Bézier poles/weights and
the existing Sketch3D control-point or fit-point B-spline definition. Sketch3D and upgraded paths
share that constructor; changing path tolerance does not redefine the source fit. Lines, ordered
polyline spans and three-point arcs retain their exact interpretation. Reversal reverses the native
curve and its endpoint roles together, independently of subdivision counts.

An upgraded Helix uses its analytic cylinder pcurve and a C2 3D approximation with the authored
modeling tolerance, at most 4,096 spans/degree 14 and **1–128 turns**. Both the kernel completion
status and reported maximum error must satisfy the tolerance; a partial result is rejected.
The historical PathCurve radial frame/phase is retained, including its start/end positions in a
mixed chain. This does not silently replace the separate historical Sketch3D display frame.

Native Loft-section compatibility trims native curves without converting them to chords. The
derived vertex map retains authored section vertices. Degenerate or singular path starts reject;
failed construction never changes the committed document or previously certified cache. Independent
distance/tangent/length and native refinement proofs live in
[the path Geometry tests](../tests/geometry/path_execution_policy_tests.cpp). Equation twist,
native comparison/Apply and the remaining consumer combinations still require their own gates.

## Equation source contract

Both planar and spatial sources add the `equation_curve` kind in the same owner/entity reference.
Current parameter/domain inputs resolve endpoint witnesses through the shared Core program; no
OCCT enters source validation. A closed single-source path requires the equation's closed intent
and exact closure proof. Owner/ID loss, mismatched closure and invalid endpoints reject publication.
Sketch replacement revalidates its equation paths atomically. Physical execution uses the immutable
Geometry curve, preserving reversal and stable source roles. Existing non-equation path policies
and their later 166 upgrade boundary remain unchanged. Equation paths support the current
orientation/guide executor using the actual curve and start tangent. Equation inputs set the Pipe
kernel's 3D/boundary approximation to OCCT confusion resolution and angular tolerance to `1e-6`;
the historical non-equation default remains unchanged. Nonzero twist is rejected
explicitly until the curve-preserving twist policy in 166, rather than constructing a chord guide.

## Verification

```text
./build/dev-geometry/blcad_core_tests "[core][path-curve]"
```

The focused suite covers tolerance-sensitive planar connectivity, mixed authored direction,
3D-Sketch dependency/removal protection, all four source families, all orientation metadata,
strict JSON, and backward compatibility.

Blocks 48–94 are implemented. Block 80 Sweep feature Core intent and JSON is implemented; Block 81 Basic Sweep Geometry is implemented; Block 82 3D path, twist, and guide-controlled Sweep is implemented; Block 83 Path-following Extrude and Extruded Cut is implemented; Block 84 ProfileSectionReference and Loft Core intent plus JSON is implemented; Block 85 Two-section Loft Geometry on arbitrary planes is implemented; Block 86 Multi-section Loft is implemented; Block 87 Guided and continuity-controlled Loft is implemented; Block 88 Surface feature Core intent and JSON is implemented; Block 89 Boundary and Fill Surface Geometry is implemented; Block 90 Trim and Extend Surface Geometry is implemented; Block 91 Stitch/Knit/Sew shell Geometry is implemented; Block 92 Closed shell to solid conversion is implemented; Block 93 multi-body STEP export is implemented; Block 94 integrated Part Construction MVP acceptance is implemented; Block 95 Qt application shell, GUI document session, and command architecture is implemented; Block 96 document lifecycle, transactions, recompute, and diagnostics is implemented; Block 97 OCCT viewport, navigation, display, and semantic picking is implemented; Block 98 browser, property editor, and selection synchronization is next.
