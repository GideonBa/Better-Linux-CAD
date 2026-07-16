# Interactive Sketcher Planar Tutorial MVP-8

This tutorial is a deterministic acceptance document. Every step id, authored object id, and expected state is stable so that a GUI driver and a headless command script can execute the same scenario.

## Document

- document id: `tutorial.sketch.planar`
- Sketch id: `sketch.tutorial.planar`
- workplane: `datum.xy`

## Steps

1. `planar.001.open-workspace`: enter the contextual Sketch workspace on `datum.xy`; expect no persistent mutation before Apply.
2. `planar.002.rectangle`: create a two-corner rectangle from `(0,0)` to `(60,40)` with deterministic line ids `tutorial.bottom`, `tutorial.right`, `tutorial.top`, and `tutorial.left`.
3. `planar.003.constraints`: author horizontal/vertical constraints and coincident corner intent; expect one accepted transaction and no implicit snap constraint.
4. `planar.004.dimensions`: bind width `60 mm` and height `40 mm` as driving dimensions; expect the same solved coordinates through mouse placement and direct command input.
5. `planar.005.circle`: create a center-radius circle at `(30,20)` with radius `8 mm`.
6. `planar.006.modify`: chamfer the top-right rectangle corner with a `5 mm` setback; reject the candidate atomically when a deliberately stale source revision is supplied.
7. `planar.007.pattern`: create a three-instance exploded circular pattern of an ordinary construction line; expect ordinary copied entities and no associative pattern intent.
8. `planar.008.project`: project a construction axis, verify associative reference intent, then cancel a break-link preview and confirm the source remains unchanged.
9. `planar.009.region`: recognize the bounded outer region and select it with the point `(10,10)`.
10. `planar.010.finish`: Finish Sketch, materializing exactly one `ClosedProfile` for the selected region.
11. `planar.011.persistence`: save, reopen, recompute, and compare stable ids, dimensions, constraints, reference intent, and the selected profile.
12. `planar.012.history`: undo and redo the Finish Sketch transaction; expect exact authored-state restoration in both directions.
13. `planar.013.keyboard`: repeat one creation command with keyboard Apply and Cancel; Apply must match pointer acceptance and Cancel must restore the baseline.
14. `planar.014.high-dpi`: replay the rectangle pick sequence at scale factors `1.0`, `1.5`, and `2.0`; expect identical model-space coordinates.

## Acceptance

The GUI and headless script outputs are equivalent when their serialized authored state contains the same stable ids, values, references, and ordering. Transient hover, glyph, and preview presentation is excluded from serialized comparison, but stale previews must never be published after the source revision changes.
