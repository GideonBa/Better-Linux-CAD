# Engineering Modules

Engineering assistants remain target architecture. Block 149 supplies the shared typed resource
foundation, document pins, Part/Project persistence, explicit revision adoption and removal rules
below. Physical properties and material UI retain their assigned 150/151 gates. Exact implementation
status stays in `mvp-plan.md`; this foundation does not claim an assistant or the complete material UI.

The material/catalog foundation is now assigned to [Blocks 149–151](inventor-parity-sequence.md),
shared with semantic Hole/Thread standards in 152–157. This allocation supersedes the relative
order of the proposed sequence below for those resources: a standard-parts library is not a
prerequisite for material mass or the Hole Wizard. Bolt, shaft, bearing and gear assistants,
automatic fastener placement and the full parts library remain outside Blocks 145–204. The
already implemented bolt-circle/Assembly-binding seed is not reimplemented by this catalog.

Engineering modules combine calculation, standards logic, and geometry generation. They are the main distinction from FreeCAD or simple modelers: the user should not have to assemble every detail by hand from tables, standards, and catalogs. Each module produces a normal parametric feature in the feature tree and can use parameters.


## Shared engineering resources — Block 149A contract

Core owns one `EngineeringResourceCatalog` containing immutable `EngineeringMaterial` or
`EngineeringStandardTable` revisions. `EngineeringResourceKey` is a validated stable resource ID
plus a nonempty revision string; names, row positions and numerical equality never identify a
resource. The resource kind stays fixed for one ID across revisions. Inserting the same key and
identical canonical typed contents deduplicates; changed contents under that key fail. Different
revisions coexist, and catalog merge validates a disposable copy before replacing the target. No
in-place revision replacement or automatic revision selection is exposed.

`MassDensity` stores positive finite kg/m³ and converts explicitly from g/cm³ or kg/mm³; its kg/mm³
conversion uses 10⁻⁹. Values must remain positive/finite in both canonical and Geometry-facing
representations. `EngineeringPressure` stores positive finite Pa with explicit MPa/GPa conversions.
Conversion overflow/underflow fails validation. Density is optional: absent density remains absent,
never a guessed value or zero mass. Material properties can additionally carry Young's modulus,
yield strength and tensile strength as typed pressures. Physical mass/tensors remain Block 150.

Every present material property and table cell has `EngineeringProvenance`: UserDefined,
Manufacturer or Normative, explicit source label and optional reference, URI and license metadata.
Normative origin requires an explicit reference/edition. Resource-level provenance also describes
the assembled revision. URI is descriptive metadata, never an automatic fetch. Repository fixtures
use explicitly synthetic user-defined values; no normative values or assistants are shipped here.

Standards tables own stable column/row keys and labels. Columns declare required/optional cells
and one exact kind: length mm, signed displacement mm, angle degrees, positive integer count,
density, pressure, dimensionless scalar, text or boolean. Geometric cells reuse existing `Quantity`;
other kinds retain their typed values. Missing required cells, duplicate/unknown keys, wrong kinds,
invalid enums and non-finite scalars reject the whole table. Counts must be representable by the
existing count API before conversion. Cells normalize to declared column order; row display order
is retained. Per-row identity survives display reorder, while immutable revision content includes
the authored row order. The factory exposes only const contents.

A's public API lives in `engineering_quantity.hpp` and `engineering_resource.hpp`; factories enforce
invariants before a resource enters the catalog. Existing Part parameter/expression kinds remain
under their current authority. Part default → Body override assignment and explicit revision update
are B/C contracts, with Body override taking precedence and unassigned remaining unavailable.
Standalone/project persistence must use this catalog and preserve pinned values offline; its exact
schema is specified in `file-format.md` before B's writer. Material assignment does not author
appearance, and installing another library revision does not change an existing pin.

Focused Core proof is `[engineering-resource]` / `[parity-b149]`: conversions/extreme values,
provenance and optional density, every table kind/required cell, malformed/duplicate references,
identical-key deduplication, conflicting immutable revisions, kind collisions, copied snapshots
and late-conflict atomic merge. Legacy document bytes are covered by the unchanged full Core suite.
No Part/Project resource field is written by 149A; whole-block status stays in `mvp-plan.md`.


## Part material pins — Block 149B1 contract

An editable Part owns a snapshot of the same `EngineeringResourceCatalog`. `add_engineering_resource`
uses its collision rules; default and Body pins explicitly reference a Material key. Table keys,
unknown keys and unknown Bodies reject mutation. Effective assignment is Body override → Part
default → unassigned. Clearing an override restores default inheritance. Material without density
is assignable but still has no available mass. Names/appearance do not select a material.

Pins and unused immutable revisions survive copies, history reorder/suppression, JSON preview
cloning, Save/Open and result-face support staging. Removing an otherwise removable Body clears
that Body's override after all dependency checks pass; reusing the ID cannot resurrect the pin.
Reference Parts reject engineering-resource authoring; an Editable copy is the supported target.
Resource/pin edits do not change executable lifetime generation or geometric dependency edges.
Geometry recompute/source freshness still follows the existing session pipeline; B1 adds no mass
cache or metadata-only cache-certification shortcut.

Part default/Body setters publish a complete validated value or fail without mutation. Catalog
removal and coordinated Project updates remain 149C. B2 supplies Project-owned deduplication and
portable member snapshots; B1's Project-v1 serialization rejects resource-bearing members rather
than dropping or ambiguously duplicating their revision ownership. The exact codec/migration is
owned by `file-format.md`.

## Project resource ownership — Block 149B2 contract

A Project owns the same immutable catalog authority. Inserting a standalone Part merges its
snapshot atomically, deduplicating identical keys and rejecting conflicting contents even if a
later entry conflicts after earlier new entries. Each member retains its portable local subset;
its pins retain exact revisions and are not silently updated to another member's default.
Project resources may also be unassigned or retained after member removal.

`synchronize_engineering_resources` reconciles existing mutable member edits into a candidate
catalog atomically. `validate_engineering_resources` rejects missing/conflicting ownership without
mutation; the writer uses it. GUI Project transactions synchronize their disposable candidate
before geometry work, preserving committed documents/caches/history if a conflict occurs.
Persistence stores shared values once and each Part's ordered resource keys; standalone extraction
restores complete snapshots offline through the existing Part codec. Exact fields/migration stay
in `file-format.md`; C owns explicit revision switching and referenced-resource removal.

## Explicit revision adoption and removal — Block 149C contract

`adopt_engineering_resource_revision` requires an existing source revision and a validated
replacement with the same logical resource ID/kind. It inserts an immutable revision and retains
the old one. Material adoption remaps the Part default and every Body override pinned to that
exact old revision; other revision pins stay unchanged. Same-key identical adoption is a no-op,
conflicting contents fail. A table adoption makes the replacement available locally; there are no
authored table consumers before 152. That block must extend the consumer remap/removal checks
when it introduces such references, using this same catalog.
The Part result counts inserted revisions plus remapped pins; an identical adoption returns zero.

Project adoption takes an explicit nonempty list of unique owned Part IDs. It validates existing
ownership and operates on a complete candidate, including the root catalog and every selected
member. Unknown/duplicate/Reference Parts, absent local source revisions and immutable conflicts
reject the whole update. Unselected Parts keep their pins; a later failing member cannot publish
earlier changes. Existing session transactions provide one Undo/Redo/save boundary for the batch.
The Project result counts changed selected members, rather than individual pins or stored copies.

Catalog removal removes one exact key (missing is a no-op). Part removal rejects revisions used
by its default or Body overrides; Reference Parts remain read-only. Project removal rejects any
member's local snapshot reference, even if unassigned, because that key is persisted in the member
reference list. Remove an unused local snapshot first, then remove its unreferenced root revision.
Retained Undo snapshots stay immutable. Resource edits do not change executable generation or
geometric dependency edges; ordinary session freshness/recompute still applies. Material-only
BRep reuse and mass invalidation remain 150, and visible management remains 151.

## Planned modules

| Module | Function |
|--------|----------|
| Bolt assistant | choose bolt size, clearance hole, countersink, nut, washer, engagement depth, safety factor. |
| Bolt-circle generator | parametric bolt circles: radius, count, angle offset, hole diameter, optional screw instances. |
| Shaft assistant | shafts with steps, keyways, bearing seats, and diameter sizing (`docs/shaft-wizard.md`). |
| Bearing assistant | select and place bearings by diameter, load, speed, and envelope. |
| Gear generator | simple gears from module, tooth count, pressure angle, width. |
| Material database | materials with density, yield, tensile strength, Young's modulus. |
| Standard-parts library | reusable standard parts: screws, nuts, washers, profiles, bearings. |

## Bolt assistant example

```text
Input:
  connection_type = bolted_plate_connection
  plate_1 = steel, 8 mm ; plate_2 = steel, 12 mm
  transverse_load = 2 kN ; safety_factor = 2

Output:
  recommended_bolt = M8
  clearance_hole_diameter = 9.0 mm
  minimum_edge_distance = 18 mm
  recommended_bolt_length = 25 mm
  generated_geometry = hole_pattern + bolts + washers
```

## Shared principles

- **Deterministic sizing, no AI**: bearing reactions, minimum diameters, bearing life, keys, retaining rings, and standard geometry must be deterministic, traceable, and checkable. AI may later assist with explanation, start values, catalog search, onboarding, and warning summaries — never with safety-relevant results. (Same rule as `docs/shaft-wizard.md`.)
- **Data with provenance**: material and standard/catalog data live in controlled databases that flag each value as normative, manufacturer, or user-defined. Standards data must not be copied uncontrolled; the system owns the structure but sources concrete values from licensed/manufacturer/user tables (see the standards database in `docs/hole-wizard.md`).
- **Integrated, not bolted on**: every module emits normal features/components, updates the dependency graph, and stays parametric and editable. Every automatic decision must be visible, justifiable, and editable — the system proposes a traceable, standards-oriented construction; the user keeps control.
- **Reuse across modules**: hole wizard, bolt assistant, bearing assistant, and standard-parts library share the same standards/material databases and the same fastener/bearing catalog.

## Relationship to other blocks

- Materials and standard parts become project-level `resources` in the file format (`docs/file-format.md`).
- Bolt circles combine the hole wizard (`docs/hole-wizard.md`), circular patterns (`docs/pattern-and-mirror-features.md`), and assembly insert constraints (`docs/assembly-system.md`).
- The gear generator and involute splines are later, more complex modules.

## Proposed implementation sequence

1. Add the material database as project-level resource data with provenance flags.
2. Add the standard-parts library structure (screws/nuts/washers) with sourced entries.
3. Add the bolt assistant driving the hole wizard and fastener placement.
4. Add the bearing assistant feeding the shaft wizard's bearing selection.
5. Add the gear generator (simple involute gears) as an isolated module.

## Out of scope for the first versions

- concrete normative standard values shipped in the repository (only the database structure).
- FEM/CAM coupling.
- gear/spline exact tooth geometry before the simpler modules are stable.
