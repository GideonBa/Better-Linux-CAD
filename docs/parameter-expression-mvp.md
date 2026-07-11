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
- JSON persists the formula string; deserialization re-derives the value and the edges through `add_expression_parameter` (file order preserves input-before-expression validity).

## Covered by tests

`tests/core/parameter_expression_tests.cpp`: formula evaluation (mm literals, parentheses, unary minus, count inputs), unit-violation and malformed-input rejection, graph edges, direct-write rejection, input-change re-evaluation, chained topological re-evaluation, duplicate/self-reference rejection, JSON roundtrip with re-derived edges, and formula editing (value/edge replacement, detached old inputs, chained re-evaluation, transactional invalid-edit rejection, indirect cycle rejection).

`tests/geometry/parameter_expression_pipeline_tests.cpp`: a plate thickness driven by `width / 10 - 2 mm` recomputes exactly (volume `120*80*10` → `160*80*14`) through the incremental recompute plan after one width change.

## Still deferred

- converting a plain parameter into an expression parameter (and back).
- expressions over assembly-scoped parameters and cross-part references (`docs/parameter-model.md` steps 6-7).
- functions (`min`, `max`, trigonometry), angle-typed expressions, and additional units.
