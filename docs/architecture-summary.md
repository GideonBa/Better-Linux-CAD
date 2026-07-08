# Architekturzusammenfassung

Quelle: `zielarchitektur-parametrisches-cad-system.tex`.

Ausgelagerte Teilarchitekturen (per `\input` eingebunden):

- `zielarchitektur-hole-wizard.tex`: Hole Wizard
- `zielarchitektur-feature-mirror-und-muster.tex`: Spiegeln und Muster
- `zielarchitektur-rundungen-und-phasen.tex`: Rundungen und Phasen
- `zielarchitektur-assembly-system.tex`: Assembly-System mit Zwangsbedingungen

## Ziel

BLCAD soll ein eigenes parametrisches CAD-System fuer Linux werden. Das Modell
speichert nicht nur fertige BRep-Geometrie, sondern die Konstruktionsabsicht:
Parameter, Skizzen, Features, Abhaengigkeiten und semantische Referenzen.

## Grundsatzentscheidung

- Kein FreeCAD-Fork.
- OCCT dient als Geometriekernel.
- Der eigene CAD-Core liegt oberhalb von OCCT.
- Dune 3D kann als technische Referenz fuer Sketching und UI dienen, aber nicht
  als dauerhaftes Fundament.

## Schichten

- Benutzeroberflaeche
- Command-System
- Parametrischer Core
- Sketch- und Constraint-Schicht
- Assembly-Schicht
- Engineering-Module
- Geometriekernel OCCT
- Datei- und Austauschformate

## Kernobjekte

- `Document`
- `PartDocument`
- `AssemblyDocument`
- `Parameter`
- `Expression`
- `Sketch`
- `DatumPlane`
- `DatumAxis`
- `Feature` (inklusive `FilletFeature` und `ChamferFeature`)
- `FeatureReference`
- `DependencyGraph`
- `ShapeCache`
- `ComponentInstance`
- `AssemblyConstraint`
- `Joint`

## Kritische Architekturthemen

- Parameter muessen echte Objekte sein.
- Features speichern Regeln, nicht nur Ergebnisgeometrie.
- Recompute laeuft ueber einen Dependency Graph.
- Sketches auf Flaechen brauchen stabile semantische Referenzen.
- OCCT-Shapes sind Cache, nicht das primaere Modell.
- Der OCCT-Pfad liegt in einem optionalen `blcad_geometry`-Target: Adapter fuer
  Rechteckextrusion und zentrischen Cut, ein kleiner ShapeCache, die
  Recompute-Ausfuehrung fuer `AdditiveExtrude` und `SubtractiveExtrude`, ein
  vollstaendiger Dokument-Recompute sowie der STEP-Export des finalen Shapes.
- Der ShapeCache bleibt in der Geometry-Schicht; `PartDocument` bleibt OCCT-frei
  und wird ueber `execute_document` in den Cache gerechnet.
- Parameterwerte sind ueber `PartDocument::set_parameter_value` aenderbar; eine
  Aenderung markiert die Abhaengigen und treibt den inkrementellen Recompute.
- Assembly-Parameter muessen kontrolliert in Parts einwirken.
- Rundungen und Phasen sind eigene parametrische Features mit semantischen
  Kantenreferenzen, nicht nur nachtraegliche BRep-Korrekturen.
- Das Assembly-System beschreibt Lagebeziehungen ueber Zwangsbedingungen: ein
  Constraint-Graph und ein Solver bestimmen Komponentenpositionen und
  verbleibende Freiheitsgrade; Joints erlauben spaeter kontrollierte Bewegung.
- Kanten- und Assembly-Referenzen bleiben semantisch, damit sie Modelländerungen
  ueberstehen.
