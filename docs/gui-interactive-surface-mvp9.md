# GUI Interactive Surface Authoring and Surface-to-Solid MVP-9

Status: implemented in Block 128.

Block 128 upgrades the MVP-7 form-driven surface commands (`docs/gui-spatial-surface-workbench-mvp7.md`)
into direct-manipulation authoring over the frozen Block-88..92 `SurfaceFeature` contracts
(`docs/part-surface-feature-intent-mvp6.md`, `docs/part-boundary-fill-surface-geometry-mvp6.md`,
`docs/part-trim-extend-surface-geometry-mvp6.md`, `docs/part-surface-stitch-geometry-mvp6.md`,
`docs/part-closed-shell-to-solid-geometry-mvp6.md`). It consumes the Block-122 selection-first
workspace and the Block-123 candidate-only manipulator layer. No new Core or Geometry intent is
introduced.

## Authority boundary

```text
boundary curves / target surface + contour / surfaces + typed tolerance / shell
  [+ Length distance parameter]
  -> GuiInteractiveSurfaceController (headless)
     candidate SurfaceFeature parameter set
     Block-123 extend distance handle
     disposable PartDocument clone + real geometry recompute (diagnostics)
  -> one commit_part_transaction (seed distance/tolerance + add SurfaceFeature)
  -> existing recompute / freshness / undo authority
```

`GuiInteractiveSurfaceController` owns no `PartDocument`, `ShapeCache`, or save-format field. It is
owned by the Block-124 `GuiInteractiveFeatureCoordinator` alongside the other interactive commands.

## Surface authoring

The controller authors one of the six `SurfaceFeature` kinds:

- **Boundary / Fill** — collect an ordered, duplicate-free set of boundary curves (PathCurve or
  semantic edge) and produce a surface body.
- **Trim** — a target surface (Body or semantic face) and one trimming contour (boundary curve or
  profile region). The deterministic kept region is the Core contract's; no keep-side guess is made.
- **Extend** — a target surface, a linear boundary, and an existing `Length` distance parameter driven
  by the `surface.extend` linear handle.
- **Stitch** — an ordered set of surfaces and an existing `Length` tolerance parameter set as a typed
  HUD value (at least two surfaces are required).
- **Closed-shell-to-solid** — a closed shell surface converted to a solid body.

## Geometry diagnostics before commit

Stitch free-edge/gap and closed-shell closedness/manifold/orientation checks are geometric, so
`preview(...)` runs the real `GeometryRecomputeExecutor` on a throwaway document clone rather than only
a structural plan. A failing stitch surfaces the authoritative "stitched shell left free faces or gaps
outside tolerance" / "non-manifold" diagnostic, and a closed-shell conversion surfaces "requires a
closed shell with no free edges" — each before any document mutation. Raising the typed tolerance past
the gap resolves the stitch diagnostic. The color-coded free-edge overlay itself is a transient binder
presentation of these diagnostics.

## Manipulator coupling

The extend distance handle produces a Block-123 candidate the controller folds into the distance
parameter (`apply_manipulator`); a numeric HUD override is authoritative the same way. The proof shows
a coordinator-routed extend-distance drag committing a byte-identical document to the equivalent typed
edit.

## Failure policy

Every command fails closed before any document mutation: a missing result Body, a missing or
wrong-typed distance/tolerance parameter, fewer than the required inputs (no boundary, fewer than two
stitch surfaces, missing trim/extend references), and any geometry rejected by the recompute
(free edges, gaps, non-manifold, non-closed shell). Preview never publishes a partial `ShapeCache`.

## Focused proof

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][interactive-surface]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[integration][stitch-diagnostic-overlay]"
```

The proof covers fill authoring with duplicate rejection, extend distance driving and apply,
closed-shell-to-solid fail-closed reporting, fail-closed rejection of invalid inputs, the
tolerance-driven stitch free-edge/gap diagnostic surfaced before commit, a clean coplanar stitch, and
coordinator-routed extend-distance/typed document equivalence.

## Scope and deferrals

Bounded here and not introduced implicitly by this block:

- the **color-coded free-edge/gap overlay** and the trim **kept-region highlight** are transient
  viewport presentation the binder renders from the controller's diagnostics and the Core kept-region
  contract; the controller owns the validated candidate and diagnostic, not pixels;
- boundary/surface/contour and shell **picking** resolve to already-built references and Body ids;
  their interactive selection is the caller's selection responsibility;
- mapping the Block-122 `surface.boundary_fill` / `surface.trim_extend` / `surface.stitch` /
  `surface.closed_shell_to_solid` command ids to resolved inputs and a manipulator frame is a
  `MainWindow` command-handler policy consuming the coordinator's surface controller.

## Next boundary

Block 129 adds the feature edit lifecycle and the Core feature-update command boundary, so any of the
Block-124..128 interactive commands can also edit existing persistent feature intent.
