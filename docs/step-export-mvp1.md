# MVP 1 STEP-Export

Status: optionaler OCCT-STEP-Export fuer den finalen Shape

Dieses Dokument beschreibt den STEP-Export fuer MVP 1. Der Code liegt im
optionalen Target `blcad_geometry` und bleibt damit ausserhalb des reinen
Core-Targets. Der Export schliesst die erste vertikale MVP-1-Linie ab: von
Parameter, Sketch und Feature ueber die OCCT-Shape bis zur STEP-Datei.

## Ziel

Der Schritt schreibt einen berechneten `GeometryShape` als STEP-Datei:

- Exportiert wird nur ein `GeometryShape`, in der Praxis der finale Shape aus
  dem `ShapeCache`.
- Der Exporter kapselt den OCCT-STEP-Writer hinter der Core/OCCT-Grenze.
- `blcad_core` bleibt frei von OCCT und kennt keinen STEP-Writer.
- Erwartbare Fehler werden als `ErrorCategory::Export` gemeldet.

## CMake-Target

Der Export wird nur mit dem Geometry-Preset gebaut:

```bash
cmake --preset dev-geometry
cmake --build --preset dev-geometry
ctest --preset dev-geometry
```

Der STEP-Writer braucht zusaetzlich die OCCT-Toolkits `TKSTEP` und `TKXSBase`.

## Oeffentliche Schnittstelle

Der oeffentliche Header ist:

```text
include/blcad/geometry/step_exporter.hpp
```

Aktueller Typ:

- `StepExporter`: OCCT-Adapter fuer den STEP-Export

Aktuelle Operation:

```text
write_step(shape, path)
```

`write_step` liefert die Groesse der geschriebenen Datei in Bytes. Wie die
anderen Adapter greift `StepExporter` als `friend` von `GeometryShape` ueber den
nicht oeffentlichen Header `src/geometry/geometry_shape_internal.hpp` auf die
OCCT-Shape zu.

## Operation

Aktuell implementiert:

```text
write_step(shape, path)
```

Regeln:

- Der Zielpfad `path` darf nicht leer sein.
- Der Shape muss ein nicht-leeres `GeometryShape` sein.
- Das STEP-Schema ist fest `AP214`.
- Der Shape wird als `STEPControl_AsIs` uebertragen.
- Nach dem Schreiben wird die Dateigroesse geprueft; eine leere Datei ist ein
  Fehler.
- Erwartbare OCCT- und Dateisystemfehler werden als `ErrorCategory::Export`
  gemeldet.

## Testabdeckung

Aktuelle Geometry-Tests pruefen:

- eine geschnittene Platte wird als nicht-leere STEP-Datei geschrieben, deren
  Groesse der gemeldeten Byte-Zahl entspricht und die mit dem STEP-Header
  `ISO-10303-21` beginnt
- ein leerer Shape wird als Export-Fehler abgelehnt, ohne eine Datei zu
  erzeugen
- ein leerer Pfad wird als Export-Fehler abgelehnt

## Bewusste Begrenzung

Noch nicht enthalten:

- STEP-Import
- Metadaten-Roundtrip
- Farben oder Materialien
- Assembly-STEP
- STL- oder anderer Export
- Aufruf des Exports direkt aus `PartDocument` oder Recompute

## Naechster sinnvoller Schritt

Mit dem Export ist die erste vertikale MVP-1-Linie technisch vollstaendig. Der
naechste sinnvolle Schritt ist, diese Bausteine zu einem durchgaengigen
Ablauf zu verbinden:

1. den `ShapeCache` in `PartDocument` integrieren
2. nach einem erfolgreichen Recompute automatisch `mark_all_clean()` setzen
3. das Referenzbauteil aus der Spezifikation als Ende-zu-Ende-Test erzeugen und
   exportieren
4. weiter keinen allgemeinen Solver und keine GUI bauen
