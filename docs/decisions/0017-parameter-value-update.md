# 0017: Parameterwert-Update im PartDocument

Status: akzeptiert fuer MVP 1

## Kontext

Der Geometry-Pfad kann ein `PartDocument` vollstaendig und inkrementell in einen
`ShapeCache` rechnen. Bisher waren Parameter nach ihrer Erzeugung aber
unveraenderlich. Der Recompute-Lebenszyklus konnte nur strukturell gezeigt
werden, nicht mit einer echten Wertaenderung.

Das MVP-1-Erfolgskriterium verlangt, dass eine Aenderung von `hole_diameter`
oder `thickness` die Shape neu berechnet. Dafuer muss ein Parameterwert im
Dokument aenderbar sein.

## Entscheidung

`Parameter` bleibt unveraenderlich. Es wird eine Operation `Parameter::with_value`
eingefuehrt, die eine Kopie mit neuem, erneut validiertem Wert liefert.

`PartDocument` erhaelt `set_parameter_value(id, value)`. Die Methode:

- validiert die Parameter-ID,
- sucht den vorhandenen Parameter,
- validiert den neuen Wert ueber `Parameter::with_value`,
- ersetzt den Parameter im Dokument,
- markiert den Parameter und seine Abhaengigen als geaendert.

Die Methode gibt wie `mark_parameter_changed` die betroffenen Graphknoten
zurueck.

## Regeln

- `Parameter` wird nicht mutiert; eine Aenderung erzeugt einen neuen Wert.
- Die Wertvalidierung bleibt in einem einzigen Pfad (`create_length`), den
  `with_value` wiederverwendet.
- `set_parameter_value` fuehrt keinen geometrischen Recompute aus. Es markiert
  nur den Invalidierungsstatus.
- Der geometrische Recompute bleibt in der Geometry-Schicht.

## Konsequenzen

- Ein echter numerischer inkrementeller Recompute ist testbar: eine groessere
  Bohrung verkleinert das finale Volumen.
- Die Trennung zwischen Modellabsicht (Core) und Geometrieausfuehrung (Geometry)
  bleibt erhalten.
- Der naechste Schritt kann die Modellabsicht als JSON serialisieren.

## Verworfene Alternativen

### Parameter direkt mutierbar machen

Ein Setter auf `Parameter` wuerde die zentrale Validierung umgehen koennen und
die Invariante "Wert ist immer gueltig" schwaechen. `with_value` behaelt die
Validierung an einer Stelle.

### set_parameter_value fuehrt den Recompute selbst aus

Das wuerde OCCT und den `ShapeCache` in den Core ziehen. Der Core bleibt darum
OCCT-frei und markiert nur den Invalidierungsstatus.

### Wertaenderung ohne Invalidierung

Ein reines Setzen ohne `mark_changed` wuerde einen veralteten `clean`-Zustand
hinterlassen. Der abhaengige Teil des Modells muss nach einer Aenderung `dirty`
werden.
