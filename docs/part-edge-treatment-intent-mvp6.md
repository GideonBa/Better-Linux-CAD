# Part Edge Treatment Intent MVP-6

Status: implemented in Block 68.

Block 68 adds persistent semantic intent for constant-radius fillets and the first three chamfer
modes. Blocks 69–70 implement Fillet and Chamfer Geometry.

## Core model

```text
FilletFeature: target_body, ordered edges[], radius Length parameter
ChamferFeature: target_body, ordered edges[], mode, one or two parameters
```

`EqualDistance` owns one Length parameter. `TwoDistance` owns two Length parameters.
`DistanceAngle` owns one Length and one Angle parameter; its angle is strictly between 0 and 90
degrees. Block 68 adds persistent `ParameterType::Angle`/`deg`; angle expressions remain deferred.

Edges accept Block-35/36 semantic linear or circular generated-edge identity. Empty lists,
duplicate identities, incorrect roles, missing or unsupported producers, missing Bodies, and wrong
parameter types fail before document state is published. Variable-radius fillets and
setback/corner-management intent remain deferred.

## Body history, invalidation, and JSON

Both features modify their target Body in place. The preceding Body producer feeds the edge
treatment, which becomes the new producer of the same Body. Edge producers and dimensional
parameters are explicit dependencies.

Part JSON uses one additive ordered `edge_treatments[]` array with strict `fillet`/`chamfer` and
`semantic_linear_edge`/`semantic_circular_edge` spellings. Missing arrays deserialize as empty;
unknown fields and malformed mode/parameter combinations fail closed.

```text
./build/blcad_core_tests "[core][edge-treatment]"
```

Fillet and Chamfer execution plus semantic OCCT-edge recovery are canonical in
`docs/part-fillet-geometry-mvp6.md` and `docs/part-chamfer-geometry-mvp6.md`. Block 71 ShellFeature
Core intent and JSON is the next boundary.
