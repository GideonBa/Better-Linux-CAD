# 0011: Erster Geometry-Adapter fuer Rechteck-Extrusion

Status: akzeptiert fuer MVP 1

## Kontext

Nach Parameter-, Sketch-, Feature-, Dependency-, Invalidierungs- und
Recompute-Plan-Datenmodell fehlt der erste konkrete OCCT-Pfad. Gleichzeitig
soll der Core weiterhin ohne OCCT baubar und testbar bleiben.

ADR `0005-geometry-adapter-boundary.md` legt bereits fest, dass OCCT hinter
einem kleinen Adapter gekapselt wird. Diese Entscheidung konkretisiert den
ersten Adapter.

## Entscheidung

Es wird ein eigenes optionales Target `blcad_geometry` eingefuehrt.

Dieses Target implementiert fuer MVP 1 zuerst nur:

```text
RectangleExtrusionAdapter::make_extruded_rectangle(width, height, thickness)
```

Die oeffentliche Schnittstelle gibt keinen OCCT-Typ direkt frei. Stattdessen
wird ein opaker `GeometryShape` verwendet. Eine kleine `ShapeSummary` dient
Tests und Diagnose, ersetzt aber keinen Shape-Cache.

Der Standard-Build `dev` bleibt Core-only. Der neue Preset `dev-geometry`
aktiviert `BLCAD_BUILD_GEOMETRY=ON` und baut die Geometry-Tests.

## Regeln

- `blcad_core` darf nicht gegen OCCT linken.
- Core-Header duerfen keine OCCT-Header inkludieren.
- Der Geometry-Adapter darf OCCT-Fehler in `ErrorCategory::Geometry`
  uebersetzen.
- `PartDocument` wird durch diesen Schritt nicht mit Geometrie erweitert.
- Der Adapter erzeugt nur Geometrie; er entscheidet nicht ueber
  Feature-Reihenfolge oder Modellgueltigkeit.

## Konsequenzen

- Der erste OCCT-Code ist testbar, ohne die Core-Tests zu verlangsamen.
- Die Architekturgrenze zwischen Modellabsicht und berechneter Shape wird
  praktisch erprobt.
- Spaeter kann ein `ShapeCache` einen `GeometryShape` halten, ohne den Core in
  Richtung OCCT zu oeffnen.
- Der naechste Recompute-Schritt kann auf einem kleinen, getesteten
  Rechteckextrusionspfad aufsetzen.

## Verworfene Alternativen

### OCCT direkt in `Feature` oder `PartDocument` einbauen

Das waere kurzfristig weniger Code, wuerde aber die zentrale Trennung zwischen
parametrischem Modell und Geometriekernel verletzen.

### Vollstaendige Geometrie-Abstraktion bauen

Eine allgemeine Kernel-Abstraktion ist fuer MVP 1 zu gross. Ein konkreter,
kleiner Adapter fuer die Referenzplatte reicht.

### Geometry-Target immer bauen

Das wuerde die schnelle Core-Entwicklung unnoetig von OCCT abhaengig machen.
Der Standard-Build soll weiter die reine Datenmodellschicht pruefen.
