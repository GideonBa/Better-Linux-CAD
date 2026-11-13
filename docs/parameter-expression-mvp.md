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

## Planned curve-local extension — Block 160

This extension is assigned after the verified G2 backend delivery; it is not implemented here.
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

## Still deferred

- converting a plain parameter into an expression parameter (and back).
- expressions over assembly-scoped parameters and cross-part references (`docs/parameter-model.md` steps 6-7).
- functions (`min`, `max`, trigonometry), angle-typed expressions, and additional units.
