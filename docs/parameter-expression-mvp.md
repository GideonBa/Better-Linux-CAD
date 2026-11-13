# Parameter Expression Seed

Status: implemented the first computed-parameter block from `docs/parameter-model.md`: a part parameter whose value is a unit-aware formula over other part parameters, re-evaluated through the existing dependency graph.

## Grammar and unit rules

```text
expression := term (('+' | '-') term)*
term       := factor (('*' | '/') factor)*
factor     := number ['mm'] | parameter-name | '(' expression ')' | '-' factor
```

Parameter references use the parameter name. Unit dimension is a length power: bare numbers and Count parameters are dimensionless; `mm` literals and Length parameters have power one. Addition/subtraction require equal powers; multiplication adds and division subtracts powers; any intermediate or final power outside `{0, 1}` is rejected (`length * length`, `scalar / length`). Division by zero, unknown names, malformed input, and trailing content are rejected. The final power must match the target type (Length → 1, Count → 0) and the result passes that type's value validation.

## Model and document integration

- `Parameter` optionally carries a formula string; `Parameter::create_expression` validates the already-evaluated value through the normal type path. `with_value` preserves the formula.
- `PartDocument::add_expression_parameter(id, name, type, formula)` evaluates against the current document, stores the parameter, and adds dependency edges from every referenced parameter to the expression parameter. Cycles are rejected through the existing graph cycle detection.
- `PartDocument::set_parameter_value` rejects direct writes to expression-driven parameters; after any accepted parameter write it re-evaluates every affected expression parameter in topological order, so chained formulas read already-updated inputs, before returning the affected node list.
- `PartDocument::set_parameter_formula` is the mutation path for expression parameters: the new formula is evaluated first, the parameter's old input edges are replaced through `DependencyGraph::remove_dependencies_of_dependent`, direct and indirect cycles are rejected before any state changes, and dependents (including chained expressions) are invalidated and re-evaluated. Invalid edits leave the document untouched.
- JSON persists the formula string; deserialization re-derives values and edges through
  `add_expression_parameter` in dependency-ready passes. Formula edits may reference parameters
  appearing later in the file. Loading restores the authored parameter order after validation;
  missing names and cycles remain strict errors in normal and reference-repair modes. No schema
  change or persisted evaluated-value authority is introduced.
- Block 59 Extrude `distance`, `symmetric`, `two_sided`, and thin-wall inputs reference normal
  Length parameters. Formula-driven Length parameters therefore participate through the same
  dependency chain; Extrude intent does not persist duplicate evaluated lengths.

## Covered by tests

The replacement shell exposes standalone Part literal/formula creation and existing formula editing
with local batches, complete downstream Preview and one Apply transaction. The exact availability,
input, usage, history and persistence contract lives in `docs/parameter-model.md`. Formula validation
continues to use these Core APIs; there is no separate GUI evaluator or schema.

`tests/core/parameter_expression_tests.cpp`: formula evaluation (mm literals, parentheses, unary minus, count inputs), unit-violation and malformed-input rejection, graph edges, direct-write rejection, input-change re-evaluation, chained topological re-evaluation, duplicate/self-reference rejection, JSON roundtrip with re-derived edges, and formula editing (value/edge replacement, detached old inputs, chained re-evaluation, transactional invalid-edit rejection, indirect cycle rejection).

`tests/geometry/parameter_expression_pipeline_tests.cpp`: a plate thickness driven by `width / 10 - 2 mm` recomputes exactly (volume `120*80*10` → `160*80*14`) through the incremental recompute plan after one width change.

## Curve-local extension — Block 160

This extension adds typed equation intent to the existing Sketch authorities. Numbered acceptance
belongs to `mvp-plan.md`; Geometry and native authoring remain 161/162.
Keep the general parameter grammar, positive Length/Count validation and deferred scope below
unchanged. Curve coordinates require signed real lengths, with a separately typed real independent
variable and finite domain. Cartesian/polar/parametric 2D and parametric 3D inputs must distinguish
coordinate, angle and domain units explicitly. Curve-local products may need intermediate powers
outside `{0,1}` without making those expressions valid general parameter formulas.

Extend the expression subsystem with bounded parsing/compiled evaluation and an explicit supported
function grammar. Parameter references must retain stable IDs and join the existing Sketch/Sketch3D
dependency graph; independent variables cannot silently shadow persistent references. Define rename,
unit/range, malformed input, cycle, unsupported-function and singularity failures before the writer
changes. No general language evaluation or filesystem/network capability is part of this grammar.

Persist expression/domain/modeling-policy intent in the existing Sketch and
[Sketch3D](part-sketch-3d-curves-core-mvp6.md) authorities with strict version gates and historical
canonical-byte compatibility. Samples and compiled derivatives remain derived. Core S02 owns
parsing/JSON/dependency proof in 160; tolerance-controlled Geometry/downstream profiles remain 161,
and native authoring remains 162. [The numbered plan](mvp-plan.md) owns progress and serial delivery.

### Curve-local expression contract

Curve expressions use **`@{ParameterId}`** references, resolved in the owning Part. Bare identifiers
name only the declared independent variable, `pi`, `e` or a supported function. These lexical
namespaces cannot hide a persistent reference, and parameter rename cannot change expression
meaning. Reference tokens contain **1–256 bytes**, excluding braces and control characters.
Domains have unit **scalar**, **length** (mm) or **angle** (radians), plus lower/upper
expression strings evaluated without an independent variable. Bounds must be finite and ordered.

The bounded grammar supports numeric/scientific literals, `mm`/`rad`/`deg` literal suffixes, unary
`+/-`, parentheses, `+ - * /`, integer literal powers **-8…8**, and `sin`, `cos`, `tan`, `sqrt`, `exp`,
`log`, `abs`. Products retain length/angle powers within **-8…8**. Addition requires identical units;
trig accepts scalar radians or angles; exp/log require scalars; sqrt requires even unit powers.
Coordinates must end as signed real lengths. General parameter formulas retain their old grammar.
Parsing is bounded to **4,096 bytes**, **512 nodes** and **64 nested levels** per expression.
Unknown symbols/functions, wrong units, nonfinite/overflowing evaluation, zero divisors and unsupported
singular derivatives return validation errors. Constant zero is valid; a variable-dependent sqrt/abs
singularity is not assigned fabricated derivatives. Values and first/second parameter derivatives
are derived, never serialized. Whole-domain regularity/error bounds belong to Geometry in 161.

Modes use the existing curve entity ID and owner workplane/model frame:

| Mode | Independent domain | Coordinate intent |
| --- | --- | --- |
| `cartesian_2d` | length | `y(x)`; x is the independent variable |
| `polar_2d` | angle | signed radius `r(theta)`; x/y derive by cosine/sine |
| `parametric_2d` | scalar, length or angle | x and y expressions |
| `parametric_3d` | scalar, length or angle | x, y and z expressions |

The independent-variable name is a **1–32 character ASCII identifier**, excluding reserved
functions/constants.
Planar Sketch rejects 3D mode and Sketch3D accepts only 3D mode. Domain, expressions, positive finite
modeling tolerance, `adaptive_v1` policy, `open`/`periodic` closure and construction intent belong to
one EquationCurve. `periodic` requests a closure proof in 161; it never authorizes snapping a gap.
Source/domain parameter IDs feed the existing Sketch/Sketch3D dependency node. Editing ordinary
entities or materializing solved topology must preserve equation intent and IDs exactly.

### Expression/intent proof

[Analytic/unit/safety tests](../tests/core/equation_curve_tests.cpp) and
[ownership/graph/persistence tests](../tests/core/equation_curve_document_tests.cpp) pass **12 Core
cases / 368 assertions**. [Qt session proof](../tests/gui/gui_equation_curve_tests.cpp) passes
**2 cases / 88 assertions**, including final release, exact history/reopen and definition-only or
parameter-only stale drags on both commit paths.

The five-target build passes. Full regression: Core **732 / 29,322**, Geometry **682 / 58,224**,
and offscreen Qt **900 cases: 833 passed, 67 native-only skipped / 101,345 assertions**. The
[relevant native Sketch consumer selection](development-setup.md#find-and-select-focused-tests)
passes **36 cases / 1,592 assertions at 100%** and **36 / 1,593 at 150%**, without skips. The 100%/150% endpoint
captures were reviewed after GUI exit; separate startup proved a PID-matched `IsViewable` window
(intentional timeout 124). This verifies current consumers; equation Geometry and native authoring
retain their 161/162 acceptance gates.

All runs use **1,079 frozen runtime inputs**, SHA256
`4a7fb09f6a30e6d69728e3fa3586616a437a4829d2c7a40abd4d1a5ec32c1895`.
Final source/binary hashes, GUI/app dependency records, formatting and documentation links pass.

### Geometry analysis boundary — 161

The same immutable AST supplies interval jets and conservative analytic recognition; no second
formula parser or persisted compiled representation is introduced. Whole-domain bounds must resolve
singular divisions/functions and zero-speed intervals within fixed work limits. Generic approximation
bounds analytic-to-chord error by `max_norm(second_derivative) * span^2 / 8`, adds the modeled
smooth-curve deviation from that chord and numeric resolution, then subdivides until the sum is within
half the authored modeling tolerance. Midpoint agreement alone cannot accept an interval.

Exact polynomial spans and structurally recognized circles may use native analytic Geometry.
`periodic` requires a supported structural periodicity or exact polynomial endpoint identity.
Closure alone is G0; tangent/curvature continuity at the seam needs its own diagnostics. Endpoint proximity
cannot authorize closing a gap. Curve/start/end roles retain source identity independently of
subdivision or display tessellation. Existing profile/path authorities consume immutable derived
curves, while equations and tolerance/policy remain the sole authored definition. The historical
non-equation path policies remain assigned to 166. Implementation and acceptance are tracked only
in the numbered plan.

The [interval/analytic tests](../tests/core/curve_expression_analysis_tests.cpp) use independent
values, derivatives and exact closure oracles. The
[profile/path tests](../tests/core/equation_curve_consumer_tests.cpp) exercise existing ownership,
closure, atomic replacement and V8 replay. Geometry and downstream evidence is indexed in
[the shared construction contract](part-sketch-3d-geometry-mvp6.md#equation-construction-contract).

## Still deferred

- converting a plain parameter into an expression parameter (and back).
- expressions over assembly-scoped parameters and cross-part references (`docs/parameter-model.md` steps 6-7).
- functions (`min`, `max`, trigonometry), angle-typed expressions, and additional units.
