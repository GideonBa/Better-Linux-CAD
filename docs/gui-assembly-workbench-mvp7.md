# GUI Assembly Authoring, Relationships, and Motion Workbench MVP-7

Status: headless workbench implemented in Block 103. The former window was removed by the
shell reset; root-local and occurrence-qualified cross-hierarchy replacement-shell commands are
restored by the Block-136 Assembly slices.

`GuiAssemblyWorkbench` exposes Project-owned shared Part definitions, child Assembly documents,
repeated component occurrences, rigid nested subassembly occurrences, authored placement,
visibility, suppression, grounding, Assembly parameters, and parameter bindings. All persistent
changes pass through one `GuiDocumentSession` Project transaction.

Local Mate/Distance/Concentric/Insert/Angle and generic relationship records, all Revolute,
Prismatic, Cylindrical, Planar, and passive Spherical joint records, and occurrence-qualified
cross-hierarchy relationships/joints reuse their exact Core constructors. Capability-specific
prompts distinguish first/second semantic targets, relationship compatibility, joint coordinate
roles, solve groups, and drivable coordinates.

Relationship/joint preview validates a Project copy and complete Assembly structure without
publishing transforms. Read-only solve and joint-motion calls use the existing numeric solvers.
DOF classification, remaining DOF, rank/redundancy, consistency, residuals, iterations, and proposed
component transforms remain derived result data. Only converged, fresh results pass through the
existing atomic solve/motion appliers in one GUI transaction; failed or stale results cannot partly
move an Assembly.

Block 130 adds the direct-manipulation layer over this validation workbench in
`docs/gui-interactive-assembly-mvp9.md`: component/subassembly triads, capability-filtered second
targets, solved relationship candidates, oriented joint frames, typed coordinate controls, and
in-viewport single-coordinate motion. The headless MVP-7 API remains available to its tests; the removed
window is no longer a GUI fallback. The replacement-shell contract also covers hierarchy validation,
cross-hierarchy relationship/joint editing, connected-group solving and typed coordinate motion.
Authority remains in the same Project transactions and solve/motion appliers.

## Verification

```bash
./build/dev-gui/blcad_gui_tests "[gui][assembly-authoring]"
./build/dev-gui/blcad_gui_tests "[gui][assembly-relationships]"
./build/dev-gui/blcad_gui_tests "[gui][assembly-motion]"
```
