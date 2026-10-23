# Parameter Model, Expressions, and Top-Down Design

Status: implemented bounded Part parameters, expressions, Assembly bindings and replacement-shell
parameter management. The wider scope/navigation model below remains target architecture.

Parameters are first-class objects. Features reference them by identity, and the graphical table
uses the same validated model and dependency graph as recompute and persistence.

## Parameter object

A parameter is a named value with type, unit, scope, and an optional formula.

```text
Parameter
  id      = "asm.bolt_circle_radius"
  name    = "bolt_circle_radius"
  value   = 50
  unit    = "mm"
  type    = "length"
  scope   = "assembly"
  formula = optional
```

## Scopes

| Scope    | Meaning |
|----------|---------|
| Global   | Project-wide parameters usable across several documents. |
| Assembly | Parameters of an assembly; central for top-down design. |
| Part     | Parameters of a single part, e.g. plate thickness. |
| Sketch   | Parameters inside a sketch, e.g. a circle diameter. |
| Feature  | Parameters of a feature, e.g. an extrude length or chamfer size. |

## Expressions

An expression is a formula computed from other parameters.

```text
bolt_circle_radius = plate_width / 2 - 15 mm
```

Expressions must be unit-aware and must create dependency-graph edges to every parameter they read, so a change to an input re-evaluates the expression.

## Cross-part parameter usage (top-down design)

Central design parameters are defined on the assembly and consumed by several parts. This lets related parts change together.

```text
AssemblyParameters
  bolt_circle_radius      = 50 mm
  bolt_count              = 8
  bolt_clearance_diameter = 6.6 mm
  plate_thickness         = 8 mm
  gasket_thickness        = 2 mm

Part BasePlate   uses Assembly.bolt_circle_radius, Assembly.bolt_count, Assembly.bolt_clearance_diameter
Part CoverPlate  uses Assembly.bolt_circle_radius, Assembly.bolt_count, Assembly.bolt_clearance_diameter
Part Gasket      uses Assembly.bolt_circle_radius, Assembly.bolt_count
```

Changing `bolt_count` from 8 to 12 must recompute every dependent bolt circle in every affected part. This only works if assembly parameters participate cleanly in the dependency graph. Cross-part flow must be controlled and traceable: a part references an assembly parameter explicitly; the reference is an edge in the graph.

Cyclic parameter dependencies must be detected and rejected (for example part A using a parameter of part B while part B depends on part A). See the cycle-detection requirement in `docs/dependency-graph-mvp1-data-model.md`.

## Current implemented scope

- typed, named, validated parameters on a single `PartDocument`.
- three parameter types: positive `Length` (`mm`), positive whole `Count` (`1`) and signed finite
  `Angle` (`deg`). Formula evaluation supports Length and Count.
- two scopes: `part` and `assembly`; `AssemblyDocument` owns assembly-scoped parameters and pushes them into member parts through explicit `ParameterBinding` records (`docs/assembly-parameters-mvp4.md`).
- `PartDocument::set_parameter_value` marks dependents and drives incremental recompute (`docs/parameter-update-mvp1.md`).
- part-scope expression parameters: unit-aware formulas over other part parameters with graph-driven topological re-evaluation (`docs/parameter-expression-mvp.md`).

## Replacement-shell Part parameter manager — Block 136

The Model ribbon's **Parameter verwalten** command (`blcad.action.parameters`) opens the owned
`ShellParameterTools` task (`blcad.shell.parameters_task`). It requires a fresh Part, no repair
session, no active command and no planar Sketch editor. Project/Assembly parameter management is
outside this bounded command. Existing parameter property and Sketch-dimension editors remain
available through their existing contracts.

The read-only catalog shows every Part parameter's name, stable ID, type, evaluated value/unit,
formula and persisted scope. Select a row or use the parameter selector to edit a literal's value
or a computed parameter's formula. The detail label lists direct dependency-graph consumer IDs;
it is not a recursive usage navigator. Selection discards unsubmitted form input while retaining
already staged changes. Existing identity, name, type, scope and literal/formula category stay fixed.

For a new parameter, enter an ASCII expression identifier (`[A-Za-z_][A-Za-z0-9_]*`), choose
Length/Count/Angle and supply a finite bare number in the indicated unit, or choose a Length/Count
formula. Names must be unique. IDs use the first free `parameter.user.N` in the **entire dependency
graph**, including non-parameter nodes. No rounding or clamping occurs; editing uses 17 significant
digits for Length/Angle and integer Count text. Formula names/units/arithmetic/cycles retain Core
validation. Angle formulas, conversion between literal and formula, rename and delete are not
implemented by Core and are explicitly unavailable here.

**Lokal übernehmen** validates on a disposable Part copy, stages an ordered operation and refreshes
all evaluated catalog values and consumers. Invalid input retains its text and all earlier staged
operations. Pending input clears the viewport preview and blocks Preview/Apply until staged or
discarded by selection. **Lokale Änderungen verwerfen** resets the complete draft to committed intent.

**Vorschau** replays the complete batch through Core mutation APIs in one copied session transaction,
validates/recomputes the complete downstream model and displays disposable geometry. Positive typed
values that fail geometry remain local and correctable. Fixed DatumPlane records retain their normal
wire presentation so their unchanged planes do not cover previewed bodies. Neither typing, staging
nor preview changes live intent, dirty state, history, caches or files. **Anwenden** requires a current successful preview,
rechecks it and commits all operations in one Part transaction/Undo entry. Existing GUI constraint
and dimension sidecars are retained by that transaction; persistence uses the existing schema.

Escape from child controls, dock close, Cancel, Undo/Redo, Save, changed presentation revision,
foreign task, document replacement or entering another context discard the entire local task and
preview. A foreign task is never cancelled by cleanup. Save writes only committed intent. The task
owns no timers, cross-component widget discovery or rewired actions.

Proof: `[block136-parameters]` in `tests/gui/gui_shell_parameter_tests.cpp` and actual-input
`[shell-combined-parameters]` in `tests/gui/gui_shell_combined_workflow_tests.cpp`. Commands and final
evidence live in `docs/development-setup.md` and `docs/gui-shell-acceptance-mvp9r.md`.

## Implementation sequence and wider target

1. Add a `scope` field to the parameter model and enforce allowed scopes.
2. Add an `Expression` type with unit-aware evaluation over other parameters.
3. Create dependency-graph edges from an expression to each parameter it reads.
4. Add re-evaluation of expressions during recompute in topological order.
5. Add assembly-scoped parameters once the assembly document exists (`docs/assembly-system.md`).
6. Add cross-part parameter references and the corresponding graph edges.
7. Add cycle detection across parameter, expression, and cross-part edges.
8. Surface parameter usage sites (`docs/user-interface.md`). The Part table now lists direct
   consumer IDs; recursive and cross-document navigation remains a wider target.

## Out of scope for the first versions

- global/Assembly parameter table navigation and parameter identity/category changes.
- global project-level parameter sharing before the multi-document project file exists (`docs/file-format.md`).
