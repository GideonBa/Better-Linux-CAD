# Dependencies for Ubuntu 24.04

These packages prepare the development environment without already adding CAD code.

## Build tools

- `build-essential`
- `cmake`
- `ninja-build`
- `pkg-config`
- `git`
- `clang-format`
- `clang-tidy`

## CAD and geometry libraries

- `libocct-foundation-dev`
- `libocct-modeling-data-dev`
- `libocct-modeling-algorithms-dev`
- `libocct-data-exchange-dev`
- `libocct-visualization-dev`
- `libocct-ocaf-dev`
- `libocct-draw-dev`

## UI and helper libraries

- `qt6-base-dev`
- `libeigen3-dev`
- `libtbb-dev`
- `nlohmann-json3-dev`
- `libfmt-dev`
- `libspdlog-dev`
- `catch2`

## Documentation

- `doxygen`
- `graphviz`

## Installation

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build pkg-config git clang-format clang-tidy \
  libocct-foundation-dev libocct-modeling-data-dev libocct-modeling-algorithms-dev \
  libocct-data-exchange-dev libocct-visualization-dev libocct-ocaf-dev libocct-draw-dev \
  qt6-base-dev libeigen3-dev libtbb-dev nlohmann-json3-dev libfmt-dev libspdlog-dev catch2 \
  doxygen graphviz
```

## Local package download

The `.deb` packages resolved for this Ubuntu 24.04 environment are located under:

```text
downloads/apt/
```

Install from the local package directory:

```bash
sudo apt-get install ./downloads/apt/*.deb
```

If `apt` requests additional packages, use the normal repository command from the installation section instead.
