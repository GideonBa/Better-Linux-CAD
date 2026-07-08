# Lokaler Ubuntu-Paketcache

Dieser Ordner enthaelt heruntergeladene `.deb`-Pakete fuer den Projektstart auf
Ubuntu 24.04.

Die Pakete wurden nur heruntergeladen. Sie sind in dieser Sitzung nicht
installiert, weil `sudo` ein interaktives Passwort verlangt.

Installation:

```bash
sudo apt-get install ./downloads/apt/*.deb
```

Alternativ aus den Ubuntu-Repositories:

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build pkg-config git clang-format clang-tidy \
  libocct-foundation-dev libocct-modeling-data-dev libocct-modeling-algorithms-dev \
  libocct-data-exchange-dev libocct-visualization-dev libocct-ocaf-dev libocct-draw-dev \
  qt6-base-dev libeigen3-dev libtbb-dev nlohmann-json3-dev libfmt-dev libspdlog-dev catch2 \
  doxygen graphviz
```

