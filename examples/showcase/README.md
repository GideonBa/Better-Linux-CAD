# BLCAD gallery models

Open these files in BLCAD with **File → Open**. Each example has an editable feature tree.

| Model | Features on display |
| --- | --- |
| [Planetary stage](planetary-stage.blcad.json) | Six solid bodies with schematic gear teeth, stepped hubs, bores and a parameter-driven flange hole pattern. The stage is a static multibody Part. |
| [Exhaust manifold](exhaust-manifold.blcad.json) | Four short [hollow sweeps](../../docs/part-sweep-geometry-mvp6.md) rise from a scalloped engine flange to a compact collector with a [loft-cut passage](../../docs/part-loft-geometry-mvp6.md) and a rectangular turbo flange. The flange bolt holes use multi-position Hole features. |
| [Swept airfoil](swept-airfoil.blcad.json) | One solid loft through five spline sections on separately oriented workplanes. |

The [gallery](../../README.md#gallery) shows these models in the Qt/OCCT desktop. The banner comes
from the [startup splash](../../src/gui/startup_splash.cpp), cropped below the title. The
[loft workbench capture](../../docs/images/showcase/loft-workbench.png) is the original screenshot
supplied for the gallery.

The exhaust manifold takes its shape from the [MaxFab FWD 4AGE turbo manifold](https://maxfab.co.nz/products/fwd-4age-t3-turbo-manifold).

To regenerate a model's STEP export, build the `blcad_export_step` target with the
[GUI or Geometry preset](../../docs/development-setup.md#build-and-test-presets) and pass the
`.blcad.json` file and an output `.step` path.

The three examples were reopened in the native Qt/OCCT desktop and exported with
[`blcad_export_step`](../blcad_export_step.cpp). Reopening the STEP exports in OCCT yielded six
valid solids for the planetary stage, seven for the manifold and one for the airfoil. For the
manifold, point classification after STEP reopening verified material at the tube walls and open
space at the runner ends, collector passage and flange holes.
