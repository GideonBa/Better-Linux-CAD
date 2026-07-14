# Part Fillet Geometry MVP-6

Status: implemented in Block 69.

Block 69 executes the Block-68 `FilletFeature` intent as constant-radius OCCT geometry. The
persistent model continues to store ordered semantic edge references and a Length parameter;
derived OCCT edge identity never enters the document or JSON.

## Geometry and topology resolution

`FilletAdapter` resolves every authored edge against the current target solid immediately before
execution. Linear rectangle-extrude edges are matched by their recomputed semantic endpoints.
Circular subtractive-extrude rims are matched by profile radius, axis, center line, and selected
source/opposite offset. Unique OCCT edges are collected in authored order and passed together to
`BRepFilletAPI_MakeFillet` with one constant radius.

Missing and ambiguous matches fail explicitly. The adapter also rejects an empty target, a
non-positive/non-finite radius, an incomplete OCCT operation, and an invalid or non-solid result.
Variable radii, setbacks, corner-management policies, and arbitrary raw edge picks remain
deferred.

## Recompute and transaction boundary

The geometry executor selects the latest cached producer of the target Body from the dependency
order, evaluates the radius parameter, and runs the fillet in a working cache. Feature, Body, and
single-solid final-shape products are published only after the complete result validates.
Radius-only edits recompute the fillet alone; upstream dimension edits recompute the producer and
then recover the semantic edge on its refreshed topology. Broken references and excessive radii
leave the previous valid cache unchanged.

## Verification

```text
./build/blcad_geometry_tests "[geometry][fillet-feature]"
```

The focused suite covers single- and ordered multi-edge fillets, a semantic circular hole rim,
parameter-driven radius changes, semantic edge recovery after an upstream dimension edit,
broken-reference failure, excessive-radius failure, and transactional cache preservation.

Block 70 Chamfer Geometry is implemented in `docs/part-chamfer-geometry-mvp6.md`. Block 71
ShellFeature Core intent and JSON is the next boundary.
