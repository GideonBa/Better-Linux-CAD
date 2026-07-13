# Part Extrude Extent Intent MVP-6

Status: implemented in Block 59.

## Boundary

Block 59 freezes persistent AdditiveExtrude/SubtractiveExtrude breadth. Geometry execution for the
new modes is implemented by Block 60 in `docs/part-extrude-extent-geometry-mvp6.md`. The historical
additive Distance and subtractive ThroughAll paths continue to execute unchanged.

`Feature` owns one `ExtrudeFeatureIntent` with:

```text
extent
optional taper_angle_deg
optional thin intent
```

The existing `FeatureBodyResultContext` remains the operation authority for `NewBody`, `Join`,
`Cut`, and `Intersect`; Block 59 does not duplicate body-result intent inside the Extrude record.

## Extent matrix

| Mode | Required persistent inputs | Forbidden inputs |
| --- | --- | --- |
| `distance` | one Length `ParameterId` | second distance, limit faces |
| `symmetric` | one Length `ParameterId`; total symmetric extent intent | second distance, limit faces |
| `two_sided` | two distinct Length `ParameterId` values | limit faces |
| `through_all` | none | distances, limit faces |
| `to_next` | none | distances, limit faces |
| `to_face` | one typed `FaceReference` with `ExtrudeLimitFace` role | distances, second face |
| `between` | two typed semantic planar `FaceReference` values with `ExtrudeLimitFace` role | distances |

The face references reuse Block 58 identity and validation. Their source Feature is a dependency
of the consuming Extrude. `Between` deliberately requires planar targets; `ToFace` also retains a
typed cylindrical face source for later Geometry validation rather than persisting kernel identity.

Distance and thickness references must resolve to existing Length parameters when the Feature is
added to a `PartDocument`. Every referenced parameter and semantic face producer contributes a
dependency edge, so invalidation reaches the Extrude deterministically.

## Taper and thin intent

`taper_angle_deg`, when present, is a finite signed degree value strictly between -90 and +90.
The sign is persistent user intent; Block 60 resolves it together with Extrude direction and extent.

Thin mode is optional:

```text
one_sided -> first thickness Length parameter
mid_plane -> first thickness Length parameter
two_sided -> two distinct thickness Length parameters
```

One-sided and mid-plane modes reject a second thickness. Two-sided mode requires it. Open-profile
eligibility, offset construction, self-intersection, and solid validity are Geometry concerns for
Block 60, now implemented in `docs/part-extrude-extent-geometry-mvp6.md`.

## JSON

Historical JSON remains byte-shape compatible at the Feature-record boundary:

```json
{
  "type": "additive_extrude",
  "length_parameter": "part.depth"
}
```

still restores `distance` with no taper and no thin intent, while historical subtractive
`"depth": "through_all"` restores the exact ThroughAll default. Serialization keeps those legacy
fields and omits `extrude` for the exact defaults.

Every richer combination writes an `extrude` object. Conditional fields are emitted only for the
selected mode:

```json
{
  "extrude": {
    "extent": "two_sided",
    "first_distance_parameter": "front",
    "second_distance_parameter": "back",
    "taper_angle_deg": 3.0,
    "thin": {
      "mode": "two_sided",
      "first_thickness_parameter": "wall.front",
      "second_thickness_parameter": "wall.back"
    }
  }
}
```

`to_face` and `between` embed the Block-58 semantic face source, capability, and role. Loading is
dependency ordered, so a referenced source Feature may appear later in the JSON array. Unsupported
modes, malformed conditional fields, invalid roles/capabilities, missing Length parameters, and
unresolvable face producers fail closed.

## Compatibility and deferrals

- Existing Feature factories remain source-compatible and construct the historical defaults.
- Existing `length_parameter()` and `subtractive_depth()` access remain compatibility views.
- Straight historical Geometry results are unchanged.
- New extent, taper, thin, and all four body-operation Geometry proofs are implemented in Block 60.
- Path-following Extrude/Cut remains Blocks 80–83.
- No raw OCCT face identity is persistent.

Focused proof:

```text
./build/blcad_core_tests "[core][extrude-extent]"
./build/blcad_geometry_tests "[geometry][extrude-extent]"
```

The proof covers stable spellings, factory invariants, Feature retention, document validation and
dependency edges, every extent's JSON roundtrip, legacy defaults, malformed JSON, taper, and thin
intent. The Geometry proof covers the implemented Block-60 execution boundary.
