# Project Goal

BLCAD is intended to become an independent parametric CAD system for Linux.

The long-term goal is not a thin STEP exporter and not a wrapper around final BRep geometry. The project should store and recompute model intent:

- parameters
- expressions
- sketches
- features
- semantic generated-face references
- dependency and invalidation data
- later assembly relationships
- later engineering helpers

OCCT is used as the geometry kernel. OCCT shapes are computed cache data, not the primary model.

## Direction

The project should grow in controlled vertical slices:

1. single-part parametric model
2. stable recompute graph
3. generated-face workplanes
4. broader feature support
5. sketch constraints
6. assemblies
7. GUI
8. engineering modules

The current development rule is to keep every step narrow enough to test in the headless pipeline before adding UI or broad topology support.

## Non-goals for the current phase

The current phase should not build:

- a GUI
- a full topological naming system
- assemblies
- arbitrary face picking
- a general constraint solver
- engineering assistants

Those are long-term topics. The current MVP work should first prove that semantic model intent can be stored, recomputed, serialized, and exported reliably.
