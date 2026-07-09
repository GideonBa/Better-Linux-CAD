# Parameter Model, Expressions, and Top-Down Design

Status: target architecture. MVP-1 implements typed parameters and numeric updates on a single part; scopes, expressions, and cross-part flow are future blocks.

The parametric behavior of the system depends on parameters being first-class objects from the start. If features stored only fixed numbers, the system could not later become a real parametric CAD system. The graphical parameter table can come later; the internal parametrics must exist first.

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
- `PartDocument::set_parameter_value` marks dependents and drives incremental recompute (`docs/parameter-update-mvp1.md`).

## Proposed implementation sequence

1. Add a `scope` field to the parameter model and enforce allowed scopes.
2. Add an `Expression` type with unit-aware evaluation over other parameters.
3. Create dependency-graph edges from an expression to each parameter it reads.
4. Add re-evaluation of expressions during recompute in topological order.
5. Add assembly-scoped parameters once the assembly document exists (`docs/assembly-system.md`).
6. Add cross-part parameter references and the corresponding graph edges.
7. Add cycle detection across parameter, expression, and cross-part edges.
8. Surface each parameter's usage sites (which parts/features consume it) for the later parameter table (`docs/user-interface.md`).

## Out of scope for the first versions

- the graphical parameter table (UI comes after the internal model works).
- global project-level parameter sharing before the multi-document project file exists (`docs/file-format.md`).
