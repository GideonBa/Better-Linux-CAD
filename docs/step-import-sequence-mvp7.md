# STEP Import Sequence MVP-7

Status: planned after the Block-94 Part Construction MVP. Blocks 95–101 are not implemented yet.

This document is the canonical numbered implementation sequence for importing STEP parts and
structured STEP assemblies in one of two explicit modes:

```text
Reference
EditableBody
```

The sequence begins only after Block 94 because editable imports depend on the multi-body,
feature-result, semantic-topology, downstream-feature, and STEP exchange authorities established by
Blocks 48–94.

## User-visible import modes

### Reference

The STEP source remains external geometry authority. BLCAD creates a read-only imported
`PartDocument` definition that may be instantiated repeatedly in local or nested assemblies.
Imported faces, edges, vertices, axes, circles, and points may be selected through stable semantic
import references for assembly relationships and joints.

Reference mode permits:

- component placement, visibility, suppression, grounding, relationships, and joints;
- repeated instances and nested BLCAD assemblies;
- explicit source refresh after freshness validation;
- flattened and structured STEP re-export.

Reference mode rejects Part feature-history edits. Assembly placement never rewrites the source
STEP file.

### EditableBody

BLCAD creates a normal `PartDocument` whose first geometry-producing feature is an
`ImportedBodyFeature`. Each imported solid or surface result receives persistent `BodyId` authority.
Later BLCAD features may consume those bodies and imported semantic topology exactly like other
supported feature results.

EditableBody mode permits new parametric BLCAD operations after the imported base, including the
supported Extrude/Cut, Hole, Boolean, Pattern, Mirror, Fillet, Chamfer, Shell, Draft, Sweep, Loft,
surface, and body-transform families once their Blocks 48–94 contracts are available.

EditableBody does not infer the source CAD feature history and does not make the source BRep itself
parametric. Feature recognition, direct face push/pull, and reconstruction of foreign sketches,
dimensions, constraints, patterns, or design tables remain separate future work.

## Non-negotiable architecture rules

1. Import mode is explicit persistent intent; no heuristic silently chooses Reference or
   EditableBody.
2. The source is addressed by a canonical project-relative asset path plus a cryptographic content
   digest. Absolute machine-local paths are not portable model identity.
3. STEP bytes, OCCT shapes, XDE labels, STEP entity numbers, traversal indices, and memory identity
   are never embedded as Core model identity.
4. Missing or digest-mismatched sources fail closed. Refresh is an explicit command that produces a
   freshness report before mutation.
5. Source units are normalized to millimeters at the Geometry boundary and recorded diagnostically;
   ambiguous or unsupported unit state fails explicitly.
6. One STEP product definition maps to one shared imported Part definition. Repeated occurrences do
   not duplicate geometry or persistent Part intent.
7. Structured STEP assembly import preserves product hierarchy and direct occurrence transforms;
   it does not flatten unless the user explicitly requests a flattened Part import.
8. Imported topology uses BLCAD semantic import identities and recovery metadata, never raw STEP or
   OCCT topology identity.
9. Topology recovery must report Resolved, Lost, or Ambiguous. It never silently remaps an assembly
   constraint or downstream feature input.
10. Reference mode is immutable at the Part-feature boundary. EditableBody mode mutates only through
    new BLCAD feature intent after `ImportedBodyFeature`.
11. Import, refresh, and conversion between modes are transactional: validation and Geometry work
    complete on a copy before Project/Part intent is committed.
12. Imported source files are never overwritten by modeling, assembly motion, recompute, or export.

## Frozen phase order

```text
95  STEP source identity, import modes, JSON, and freshness
96  OCCT STEP/XDE reader and deterministic imported body definitions
97  stable imported topology identity, recovery, and target resolution
98  Reference Part integration with assemblies
99  EditableBody ImportedBodyFeature and downstream modeling
100 structured STEP assembly import
101 integrated import, refresh, edit, assembly, and re-export acceptance
```

Do not merge Blocks 95–101 into one importer. Each block crosses a different persistent or Geometry
authority boundary.

## Block 95 — STEP source identity, modes, JSON, and freshness

Primary boundary: Core external-source intent.

Introduce persistent intent conceptually equivalent to:

```text
StepImportSourceId
StepImportMode = Reference | EditableBody

StepImportSource
  id
  name
  project_relative_asset_path
  sha256_digest
  mode
```

Freeze canonical lowercase JSON spellings `reference` and `editable_body`, path normalization,
digest spelling, duplicate-source rejection, missing-asset diagnostics, and explicit refresh intent.
The JSON stores no STEP payload or derived product/body/topology cache.

The import loader must distinguish:

```text
source available and digest matches
source missing
source changed
source unreadable
```

No OCCT reader is added in this block.

Focused tags:

```text
[core][step-import-source]
[core][step-import-json]
[core][step-import-freshness]
```

## Block 96 — OCCT STEP/XDE reader and imported body definitions

Primary boundary: deterministic STEP-to-Geometry translation.

Add a Geometry adapter using the OCCT STEP/XDE reader path. It must:

- read supported STEP files without mutating Core intent;
- normalize declared source units to millimeters;
- distinguish solid, shell/surface, compound, product definition, and occurrence structure;
- derive deterministic product/body definition order independent of OCCT traversal order;
- preserve supported product/body names as metadata without making names unique identity;
- reject empty transfer, invalid geometry, unsupported unit state, and non-finite transforms;
- publish diagnostics for any narrowly frozen sewing/healing operation rather than silently changing
  geometry.

`GeometryShape`, imported XDE documents, transfer maps, and BRep bodies remain derived products.

Focused tag:

```text
[geometry][step-import-reader]
```

## Block 97 — Stable imported topology identity and recovery

Primary boundary: persistent selection identity over foreign geometry.

Introduce semantic imported body/face/edge/vertex identities that are independent of STEP entity
numbers and OCCT traversal order. Initial import creates a persisted BLCAD import-topology catalog
containing stable ids plus normalized recovery descriptors. Geometry resolution maps catalog entries
to the current imported shape and reports exactly one of:

```text
Resolved
Lost
Ambiguous
```

Freeze capability projection for supported imported geometry:

```text
planar face       -> Plane
cylindrical face  -> Cylinder + Axis
linear edge       -> Line
circular edge     -> Circle + Axis + center Point
vertex            -> Point
```

Imported semantic references must work for downstream Part feature inputs and local or
cross-hierarchy Assembly target resolution. Unsupported analytic classification remains selectable
only where an explicit later capability exists; it is never approximated as another primitive.

Focused tags:

```text
[core][step-import-topology]
[geometry][step-import-topology-recovery]
[geometry][step-import-assembly-target]
```

## Block 98 — Reference Part integration with assemblies

Primary boundary: immutable imported Part definitions as Assembly members.

Create a read-only imported `PartDocument` definition from one STEP product/body definition. It must
participate in:

- Project part ownership and shared component instances;
- repeated instances and nested BLCAD subassemblies;
- visible/active leaf traversal and exact transform chains;
- assembly target selection, compatibility, constraints, joints, solving, motion, freshness, and
  atomic application;
- contact/interference queries and flattened/structured STEP export.

Part feature additions, parameter edits, body edits, and mode changes fail explicitly in Reference
mode. Conversion to EditableBody is a separate transactional command that creates the Block-99
intent without altering the original source asset.

Focused tags:

```text
[core][step-reference-part]
[geometry][assembly-step-reference-part]
```

## Block 99 — EditableBody ImportedBodyFeature and downstream modeling

Primary boundary: imported geometry as a Part feature-result authority.

Add persistent `ImportedBodyFeature` intent referencing one `StepImportSource` and its imported body
definitions. Its ordered outputs are persistent Block-48 `BodyId` values with `Solid` or `Surface`
kind. Recompute reads the verified source, resolves the selected definitions, and seeds the normal
multi-body `ShapeCache`/feature-result path.

Freeze:

- deterministic body-output order and body mapping;
- dependency and invalidation edges from source to imported feature to downstream features;
- incremental recompute after downstream parameter changes;
- explicit source refresh with topology recovery diagnostics before commit;
- downstream semantic selection on imported topology;
- conversion from Reference to a new EditableBody Part without mutating existing assemblies unless
  the user explicitly replaces their referenced Part definition.

Prove representative downstream edits: an additive feature, a subtractive feature, a hole, a
fillet/chamfer or body boolean, JSON roundtrip, recompute, and re-export.

This block is history-based modeling after an immutable base feature. It does not implement feature
recognition or direct modeling/push-pull.

Focused tags:

```text
[core][imported-body-feature]
[geometry][imported-body-recompute]
[geometry][imported-body-downstream-feature]
```

## Block 100 — Structured STEP assembly import

Primary boundary: foreign product structure to BLCAD Project hierarchy.

Import an XDE/STEP product graph into:

```text
one BLCAD Project
one AssemblyDocument per unique imported assembly definition
one imported PartDocument per unique imported part definition
one ComponentInstance or SubassemblyInstance per occurrence
exact direct occurrence transform
```

Repeated STEP product definitions remain shared definitions with distinct occurrences. Import order,
generated ids, and collision handling are deterministic. Cycles, malformed product graphs, missing
definitions, invalid transforms, and duplicate generated identities fail closed.

The user chooses Reference or EditableBody for imported part definitions. Structured import creates
no inferred assembly constraints or joints; imported occurrence transforms are initial placement
only. BLCAD relationships and joints may be authored afterward using Block-97 semantic imported
targets.

Focused tags:

```text
[core][structured-step-import]
[geometry][structured-step-import]
```

## Block 101 — Integrated STEP import acceptance and headless workflows

Primary boundary: end-to-end import proof and application commands.

Add headless commands/APIs for:

```text
inspect STEP source
import Reference Part
import EditableBody Part
import structured STEP assembly
validate source freshness
refresh source explicitly
convert Reference Part to a new EditableBody Part
re-export imported or edited results
```

Representative acceptance must prove:

- one Reference Part can be inserted repeatedly, constrained, moved by a supported joint, analyzed,
  saved, loaded, refreshed, and re-exported;
- one EditableBody import accepts downstream additive and subtractive modeling, survives JSON
  roundtrip, recomputes deterministically, and re-exports the edited result;
- one structured STEP assembly preserves shared definitions, nested occurrences, names where
  supported, direct transforms, and geometric equivalence;
- missing/changed sources, lost/ambiguous imported topology, malformed structures, and stale refresh
  results fail atomically;
- source STEP files remain byte-for-byte unchanged;
- no raw STEP/XDE/OCCT identity appears in persistent JSON.

Focused tag:

```text
[integration][step-import-mvp]
```

After Block 101, the first STEP Import MVP is complete.

## Explicit deferrals after Block 101

```text
automatic foreign feature-history recognition
foreign sketch/dimension/constraint reconstruction
direct face push/pull or offset editing
automatic pattern/fillet/hole recognition
bidirectional associative update to the source CAD system
editing or overwriting the source STEP asset
lossless vendor-specific metadata and appearance parity
arbitrary topology healing or silent geometric repair
```

The next phase after Block 101 must be planned from measured import and topology-recovery gaps.
