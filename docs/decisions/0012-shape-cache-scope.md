# 0012: Umfang des ersten ShapeCache

Status: akzeptiert fuer MVP 1

## Kontext

Der erste OCCT-Adapter kann eine zentrierte Rechteckextrusion erzeugen. Damit
fehlt ein kleiner Ort, an dem berechnete Shapes abgelegt werden koennen, bevor
der eigentliche Recompute ausgefuehrt wird.

Gleichzeitig darf der Cache nicht zur Quelle der Modellwahrheit werden. Das
parametrische Modell liegt weiterhin in Dokument, Parametern, Skizzen, Features,
Dependency Graph, Invalidierungsstatus und Recompute-Plan.

## Entscheidung

Es wird ein optionaler `ShapeCache` im Target `blcad_geometry` eingefuehrt.

Der Cache speichert:

- Feature-Shapes nach `FeatureId`
- einen finalen Shape
- die `FeatureId`, aus der der finale Shape stammt

Der Cache verwendet den opaken `GeometryShape`. Er gibt keine OCCT-Typen in
Core-Headern frei.

Der Cache wird noch nicht in `PartDocument` eingebaut und noch nicht durch einen
Recompute automatisch beschrieben.

## Regeln

- `blcad_core` darf weiterhin nicht gegen OCCT oder `blcad_geometry` linken.
- `PartDocument` bleibt vorerst reine Modellabsicht.
- Der Cache darf geloescht und neu aufgebaut werden.
- Leere Feature-IDs und leere Shapes werden abgelehnt.
- Das Speichern derselben Feature-ID ersetzt den vorherigen Shape.
- Der finale Shape ist ein berechnetes Ergebnis, kein dauerhaftes
  Serialisierungsobjekt.

## Konsequenzen

- Die naechste Recompute-Ausfuehrung kann Shapes gezielt ablegen.
- Der erste STEP-Export kann spaeter klar aus dem finalen Shape lesen.
- Der Core bleibt schnell und ohne OCCT testbar.
- Die Grenze zwischen Modellabsicht und berechneter Geometrie bleibt erhalten.

## Verworfene Alternativen

### ShapeCache direkt in PartDocument speichern

Das waere fuer den Endzustand naheliegend, wuerde aber `PartDocument` jetzt zu
frueh an die Geometry-Schicht koppeln.

### Nur einen globalen finalen Shape speichern

Das reicht fuer den STEP-Export, ist aber fuer Recompute und Fehlerdiagnose zu
schwach. Feature-Shapes helfen, den Aufbau der Feature-Historie schrittweise zu
testen.

### Cache als primaeres Dokumentmodell behandeln

Das verletzt die Grundregel der Architektur. OCCT-Shapes sind berechnete
Ergebnisse, nicht die Konstruktionsabsicht.
