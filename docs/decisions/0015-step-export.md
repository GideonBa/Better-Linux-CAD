# 0015: STEP-Export fuer den finalen Shape

Status: akzeptiert fuer MVP 1

## Kontext

Der optionale Geometry-Pfad kann bereits eine Rechteckextrusion und einen
zentrischen Cut berechnen und die Ergebnisse im `ShapeCache` halten. Fuer die
erste vertikale MVP-1-Linie fehlt nur noch der Export des finalen Shapes als
STEP-Datei.

Der Core kennt bereits `ErrorCategory::Export` und `Error::export_error`. Der
konkrete STEP-Writer soll aber wie die uebrige OCCT-Nutzung hinter der
Core/OCCT-Grenze bleiben.

## Entscheidung

Es wird ein `StepExporter` im Target `blcad_geometry` eingefuehrt.

Der Exporter kann aktuell:

- ein `GeometryShape` als STEP-Datei schreiben
- den OCCT-`STEPControl_Writer` mit Schema `AP214` verwenden
- den Shape als `STEPControl_AsIs` uebertragen
- die geschriebene Datei auf eine Groesse groesser als `0` pruefen
- die geschriebene Byte-Zahl zurueckgeben

## Regeln

- `blcad_core` bleibt frei von OCCT und `blcad_geometry`.
- Der Exporter liegt in der Geometry-Schicht.
- Exportiert wird nur ein `GeometryShape`, in der Praxis der finale Shape aus
  dem `ShapeCache`.
- Der Exportpfad wird vom Aufrufer uebergeben.
- Exportfehler werden als `ErrorCategory::Export` gemeldet.
- Das interne OCCT-Backing von `GeometryShape` wird nur ueber den nicht
  oeffentlichen Header mit dem Exporter geteilt.

## Konsequenzen

- Die erste vertikale MVP-1-Linie von Parameter/Sketch/Feature ueber
  OCCT-Shape bis zur STEP-Datei ist technisch vollstaendig und testbar.
- Der Export ist unabhaengig von `PartDocument` und kann jeden berechneten Shape
  schreiben.
- Der naechste Schritt kann die Bausteine zu einem durchgaengigen Ende-zu-Ende-
  Ablauf verbinden.

## Verworfene Alternativen

### STEP-Writer direkt in `PartDocument` aufrufen

Das wuerde OCCT in den Core ziehen. Der Export bleibt deshalb in der
Geometry-Schicht und arbeitet auf einem `GeometryShape`.

### Sofort STEP-Import und Roundtrip bauen

Das ist kein MVP-1-Ziel und wuerde den Schritt unnoetig vergroessern. MVP 1
braucht nur den Export des finalen Bauteils.

### Erfolg des Exports nur am OCCT-Rueckgabewert festmachen

Der OCCT-Status allein sagt nicht sicher, dass eine nutzbare Datei entstanden
ist. Der Exporter prueft zusaetzlich die Dateigroesse.
