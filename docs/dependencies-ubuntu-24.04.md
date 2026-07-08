# Abhaengigkeiten fuer Ubuntu 24.04

Diese Pakete bereiten die Entwicklung vor, ohne bereits CAD-Code zu schreiben.

## Build-Werkzeuge

- `build-essential`
- `cmake`
- `ninja-build`
- `pkg-config`
- `git`
- `clang-format`
- `clang-tidy`

## CAD- und Geometrie-Bibliotheken

- `libocct-foundation-dev`
- `libocct-modeling-data-dev`
- `libocct-modeling-algorithms-dev`
- `libocct-data-exchange-dev`
- `libocct-visualization-dev`
- `libocct-ocaf-dev`
- `libocct-draw-dev`

## UI- und Hilfsbibliotheken

- `qt6-base-dev`
- `libeigen3-dev`
- `libtbb-dev`
- `nlohmann-json3-dev`
- `libfmt-dev`
- `libspdlog-dev`
- `catch2`

## Dokumentation

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

## Lokaler Paket-Download

Die fuer diese Ubuntu-24.04-Umgebung aufgeloesten `.deb`-Pakete liegen unter:

```text
downloads/apt/
```

Installation aus dem lokalen Paketordner:

```bash
sudo apt-get install ./downloads/apt/*.deb
```

Falls `apt` weitere Pakete nachfordert, sollte stattdessen der normale
Repository-Befehl aus dem Abschnitt "Installation" genutzt werden.
