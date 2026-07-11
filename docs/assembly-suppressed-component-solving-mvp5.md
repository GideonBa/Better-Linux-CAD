# Suppressed Components in Solved Groups MVP-5

Status: implemented the documented suppression policy inside the shared rigid-body solve path, replacing the previous hard rejection of suppressed components.

## Policy

A suppressed component stays part of the persistent group but leaves the numeric system:

- suppressed components contribute no solve variables and are neither fixed nor variable.
- every constraint touching a suppressed component vanishes from the collected constraint set. This falls out of the existing collector: constraints are only collected when **both** endpoints are in the passed subgroup, and the solver now passes the non-suppressed subgroup.
- suppressed components remain in the complete solve snapshots, so stale-result detection covers suppression exactly like grounding and transforms: unsuppressing (or suppressing) any snapshotted component after solving makes the result unapplable.
- proposals must match a **free active** component snapshot; a proposal for a suppressed component is rejected at application time.
- the remaining subgroup still requires at least one grounded non-suppressed component — but only if any constraints survive.
- a group whose constraints all vanish through suppression is trivially converged: zero residual components, zero iterations, and identity proposals for the remaining free components.
- diagnostics use the same filtered subgroup, so Jacobian rank and remaining DOF are computed only over non-suppressed free components and the surviving constraints.

## Proven behavior

`tests/geometry/assembly_suppressed_component_solver_tests.cpp`:

- a suppressed middle component in an `a - b - c` Mate chain removes both constraints; the group converges deterministically with zero residual components, full three-component snapshots, and an unchanged proposal for the remaining free component.
- a suppressed grounded component drops only its own constraint; the remaining grounded/free pair solves normally (four Mate residual components) and applies.
- a subgroup whose only grounded component is suppressed is still rejected with the established grounding error.
- unsuppressing a snapshotted component after solving makes the result stale and unapplable.
- diagnostics report constraint order, rank `3/6`, and three remaining DOF over the active subgroup only, deterministically.

The previous rejection test in `tests/geometry/assembly_rigid_body_solver_tests.cpp` now asserts the vanishing-constraint behavior instead.

## Still not implemented

- suppression cascade semantics (for example auto-suppressing constraints in the model intent layer; the numeric layer filters derived data only).
- joints, limits, motion, subassemblies.
- posed assembly-level STEP export (next block).
