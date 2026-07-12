# Assembly Reference Geometry Intent (MVP 5, Block 32)

Status: implemented.

This document is canonical for Block 32: assembly-selectable reference geometry Core intent, the first-class `DatumAxis` PartDocument model, and the frozen unambiguous semantic-reference grammar for DatumPlane, DatumAxis, ConstructionLine, and ConstructionPoint sources.

Serialization remains Block 33 (`docs/assembly-general-geometric-target-roadmap.md`). Geometry resolution into the Block-31 taxonomy remains Block 34.

## Scope

Block 32 makes persistent part reference geometry deliberately addressable by assembly semantic endpoints:

1. reuses existing persistent `DatumPlane`, `ConstructionLine`, and `ConstructionPoint` identities as semantic source authorities;
2. adds first-class `DatumAxis` PartDocument intent;
3. freezes the first two DatumAxis definition families with validation, ownership, dependency, and invalidation semantics;
4. freezes one canonical, provably unambiguous semantic-reference spelling per reference family;
5. keeps assembly endpoints as component/occurrence identity plus semantic-reference string;
6. performs no Geometry target resolution;
7. makes no JSON change.

Block 32 copies no resolved Plane/Axis/Line/Point geometry into assembly records.

## DatumAxis model intent

Public model: `include/blcad/core/datum_axis.hpp`.

```text
DatumAxisId       new typed id (id.hpp)
name              non-empty display name
kind              Explicit | FromConstructionLine
```

### Family 1 — Explicit

```text
origin       finite Point3
direction    finite unit Vector3 (1e-9 tolerance)
parameter_dependencies[]   optional, non-empty unique ParameterIds
```

Validation rejects empty id/name, non-finite origin/direction, zero-length or non-unit direction, and empty or duplicate parameter dependencies.

### Family 2 — FromConstructionLine

```text
source_construction_line   non-empty owned ConstructionLineId
```

The derived family stores identity only. `origin()`/`direction()` return zeroed placeholders; derived axis geometry is resolved later at the Block-34 Geometry boundary. Validation rejects empty id/name and an empty source line id.

### PartDocument ownership

`PartDocument::add_datum_axis`:

- rejects duplicate `DatumAxisId` within the document;
- requires every declared parameter dependency to exist;
- requires the `FromConstructionLine` source line to exist;
- registers one dependency-graph node under the raw axis id;
- adds `parameter -> datum_axis` edges for every declared dependency;
- adds one `construction_line -> datum_axis` edge for the derived family;
- synchronizes invalidation state exactly like existing construction geometry.

`mark_parameter_changed` therefore places dependent datum axes (directly parameter-driven, or transitively through their source construction line) into the recompute plan.

Accessors follow the existing PartDocument shape: `datum_axes()`, `datum_axis_count()`, `find_datum_axis()`.

## Frozen semantic-reference grammar

Public boundary: `include/blcad/core/assembly_reference_target.hpp`.

Canonical spelling:

```text
ref:<family>:<encoded-id>

family = datum_plane | datum_axis | construction_line | construction_point
```

Encoding: every id byte outside `[A-Za-z0-9_-]` is escaped as `%HH` with exactly two uppercase hex digits. `.`, `/`, `:`, `%`, spaces, and all other bytes are always escaped.

Examples:

```text
DatumPlaneId("datum.xy")            -> ref:datum_plane:datum%2Exy
DatumAxisId("datum_axis.spindle")   -> ref:datum_axis:datum_axis%2Espindle
ConstructionLineId("a/b")           -> ref:construction_line:a%2Fb
ConstructionPointId("a%b")          -> ref:construction_point:a%25b
```

### Unambiguity proof

BLCAD typed ids require only non-emptiness and may contain `.`, `/`, and `%`.

1. **Disjoint from feature spellings.** Every existing feature target spelling has the shape `<feature-id>.<role>` and therefore contains at least one `.`. A valid reference spelling escapes every `.` and thus contains none. The two grammars accept provably disjoint string sets, so no parse order or precedence rule is needed and no existing persisted feature target string changes meaning.
2. **Injective encoding.** Escaping is total on reserved bytes and forbidden on unreserved bytes, and decoding accepts only two-uppercase-hex escapes. Every identity has exactly one canonical spelling and every valid spelling decodes to exactly one identity. `a.b` and the literal id `a%2Eb` produce the distinct spellings `a%2Eb` and `a%252Eb`.
3. **Fail-closed parsing.** Missing prefix, unknown family, empty id, truncated escapes, lowercase hex, escaped unreserved bytes, and raw reserved bytes (including a raw `.` from a prefix-mimicking feature id such as `ref:datum_axis:x.axis`) are all rejected with validation errors.

`is_assembly_reference_target_spelling` is a cheap reserved-prefix check only; `parse_assembly_reference_target_spelling` remains the validating authority.

### Identity boundary

```text
AssemblyReferenceTargetFamily      DatumPlane | DatumAxis | ConstructionLine | ConstructionPoint
AssemblyReferenceTargetIdentity    variant<DatumPlaneId, DatumAxisId,
                                           ConstructionLineId, ConstructionPointId>

make_assembly_reference_target_spelling(identity)  -> canonical string
parse_assembly_reference_target_spelling(spelling) -> typed identity
```

Assembly endpoints (`AssemblyConstraintTarget`, `AssemblyHierarchyConstraintEndpoint`) continue to persist only component/occurrence identity plus the semantic-reference string and carry reference spellings byte-for-byte unchanged.

## Explicitly not in Block 32

- no JSON/serialization change (`cross_hierarchy_constraints[]`, `cross_hierarchy_joints[]`, and part JSON shapes are untouched);
- no Geometry resolution, no descriptor/capability projection, no resolver support for `ref:` spellings yet;
- no new relationship or joint family;
- no change to existing `<feature>.<top|bottom|right|left|front|back|axis|seat>` spellings or their resolvers.

Geometry resolution of these sources into the Block-31 taxonomy (`DatumPlane -> Plane`, `DatumAxis -> Axis + Line`, `ConstructionLine -> Line`, `ConstructionPoint -> Point`) is Block 34.

## Focused acceptance coverage

```text
[core][datum-axis]
[core][assembly-reference-target-intent]
```

`tests/core/datum_axis_tests.cpp` proves explicit/derived DatumAxis validation, PartDocument ownership/uniqueness, dependency-edge registration, and parameter plus source-line invalidation propagation into recompute plans.

`tests/core/assembly_reference_target_tests.cpp` proves canonical spellings per family, exact roundtrips for ids containing `.`, `/`, `:`, `%`, and role keywords, escape-like id distinctness, disjointness from feature spellings, fail-closed malformed parsing, and unchanged endpoint carriage.

## Handoff

The next block is Block 33: additive DatumAxis PartDocument JSON, historical-file compatibility, byte-for-byte endpoint spelling roundtrips, and load-time ownership/family validation. See `docs/assembly-general-geometric-target-roadmap.md`.
