# 0005: Grenze zwischen Core und OCCT

Status: akzeptiert fuer MVP 1

## Kontext

Die Zielarchitektur sagt klar: OCCT erzeugt Geometrie, besitzt aber nicht die
CAD-Logik. Der eigene Core verwaltet Parameter, Features, Dependency Graph und
Recompute.

Ohne klare Grenze wuerden OCCT-Typen schnell in Dokumentmodell, Parameterlogik
und Tests einsickern. Das wuerde spaetere Aenderungen und Fehlerbehandlung
erschweren.

## Entscheidung

OCCT wird hinter einem kleinen Geometry-Adapter gekapselt.

Der Core beschreibt Absicht:

- zentriertes Rechteck
- Kreisprofil
- additive Extrusion
- subtractive Extrusion
- STEP-Export des finalen Shapes

Der Geometry-Adapter uebersetzt diese Absicht in OCCT-Operationen.

Fuer MVP 1 soll der Adapter konzeptionell diese Operationen anbieten:

```text
make_extruded_rectangle(width_mm, height_mm, thickness_mm)
cut_centered_cylinder(body, diameter_mm, depth_mode)
export_step(body, path)
```

Die konkrete API wird erst beim ersten Code festgelegt. Die Grenze ist aber
entschieden: Core-Logik soll nicht direkt ueberall OCCT-Header einbinden.

## Regeln

- Dokument-, Parameter- und Dependency-Code kennt keine OCCT-Details.
- OCCT-Fehler werden im Adapter abgefangen und als BLCAD-Fehler gemeldet.
- Der Shape-Cache darf eine opaque Geometrie-Handle-Struktur halten.
- STEP-Export liegt im Geometry-Adapter oder in einer direkt angrenzenden
  Export-Schicht.
- Tests fuer reine Core-Logik sollen ohne OCCT moeglich bleiben.

## Konsequenzen

- Der erste Geometriepfad hat eine klare Schnittstelle.
- Reine Parameter- und Dependency-Tests bleiben schnell.
- OCCT bleibt austauschbarer und besser isoliert.
- Der Adapter muss bewusst klein gehalten werden, damit er nicht selbst zur
  CAD-Logik wird.

## Verworfene Alternativen

### OCCT direkt im Core verwenden

Das waere kurzfristig schneller, wuerde aber die wichtigste Architekturregel
verletzen: OCCT soll Geometrie berechnen, nicht das CAD-Modell definieren.

### Eigenen Geometriekernel abstrahieren

Eine vollstaendige Kernel-Abstraktion ist fuer MVP 1 zu viel. Es reicht, OCCT an
den konkreten Stellen zu kapseln, die der erste MVP braucht.

