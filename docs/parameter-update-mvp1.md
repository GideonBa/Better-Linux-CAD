# MVP 1 Parameterwert-Update und numerischer Recompute

Status: Core-Operation fuer die Aenderung eines Parameterwerts

Dieses Dokument beschreibt den Schritt, der den inkrementellen Recompute
inhaltlich vervollstaendigt: ein Parameterwert kann im `PartDocument` geaendert
werden, und der abhaengige Teil des Modells wird daraufhin neu berechnet.

## Ziel

Bis zu diesem Schritt waren Parameter nach ihrer Erzeugung unveraenderlich. Der
Recompute-Lebenszyklus konnte nur strukturell gezeigt werden. Jetzt kann ein
Wert wirklich geaendert werden, sodass sich die berechnete Geometrie sichtbar
aendert. Damit ist das MVP-1-Erfolgskriterium erfuellt, dass eine Aenderung von
`hole_diameter` oder `thickness` die Shape neu berechnet.

## Oeffentliche Schnittstelle

Core-Header:

```text
include/blcad/core/parameter.hpp
include/blcad/core/part_document.hpp
```

Neue Operationen:

```text
Parameter::with_value(value)
PartDocument::set_parameter_value(id, value)
```

`Parameter::with_value` liefert eine Kopie des Parameters mit neuem Wert und
derselben Identitaet. Der neue Wert durchlaeuft dieselbe Validierung wie bei der
Erzeugung. `Parameter` bleibt damit unveraenderlich; eine Aenderung erzeugt einen
neuen Wert statt einen bestehenden zu mutieren.

`PartDocument::set_parameter_value` setzt den Wert eines vorhandenen Parameters
neu und markiert den Parameter und seine Abhaengigen als geaendert. Die Methode
gibt wie `mark_parameter_changed` die betroffenen Graphknoten zurueck.

## Ablauf

`set_parameter_value` arbeitet in dieser Reihenfolge:

1. Parameter-ID validieren.
2. Parameter im `PartDocument` suchen.
3. Neuen Wert ueber `Parameter::with_value` validieren.
4. Den Parameter im Dokument durch den neuen Wert ersetzen.
5. Den Parameter und seine transitiven Abhaengigen als `changed` bzw. `dirty`
   markieren.

Der abhaengige Teil des Modells wird dadurch fuer den naechsten Recompute-Plan
`dirty`. Der eigentliche geometrische Recompute bleibt in der Geometry-Schicht
(`GeometryRecomputeExecutor::execute_plan`).

## Zusammenspiel mit dem Recompute

Ein typischer numerischer Ablauf fuer das Referenzbauteil:

1. Dokument vollstaendig rechnen: `execute_document(document, cache)`.
2. Dokument als sauber markieren: `document.mark_all_clean()`.
3. Wert aendern: `set_parameter_value("part.hole_diameter", 40 mm)`.
4. Aus dem `dirty`-Zustand einen `RecomputePlan` ableiten. Er enthaelt nur den
   betroffenen Cut, nicht den Basiskoerper.
5. Nur die betroffenen Features mit `execute_plan(...)` neu rechnen. Der Cut
   liest den unveraenderten Basiskoerper aus dem `ShapeCache`.
6. Danach erneut `mark_all_clean()` setzen.

Ein groesserer Bohrungsdurchmesser entfernt mehr Material, das finale Volumen
wird also kleiner.

## Validierung

Aktuelle Fehlerfaelle:

- leere Parameter-ID
- Parameter existiert nicht im Dokument
- Wert verletzt die Parametervalidierung (fuer MVP 1 Laenge groesser als `0`)

Ein nicht positiver Laengenwert kann fuer MVP 1 bereits bei der Erzeugung der
`Quantity` nicht entstehen. Die Wertvalidierung in `with_value` bleibt trotzdem
als einziger Validierungspfad erhalten, damit sie bei spaeteren Parametertypen
weiterhin greift.

## Testabdeckung

Aktuelle Tests pruefen:

- `set_parameter_value` aktualisiert den Wert und markiert Sketch und Feature als
  `dirty`, waehrend nicht betroffene Parameter `clean` bleiben
- `set_parameter_value` lehnt fehlende und leere Parameter-IDs ab
- das Referenzbauteil rechnet nach einer echten Durchmesseraenderung nur den Cut
  neu, und das finale Volumen wird bei groesserer Bohrung kleiner

## Bewusste Begrenzung

Noch nicht enthalten:

- Formeln oder Expressions zwischen Parametern
- automatischer geometrischer Recompute direkt aus `set_parameter_value`
- Serialisierung des Dokuments als JSON oder eigenes Dateiformat
- Parametertypen ausser `length`

## Naechster sinnvoller Schritt

Der naechste kleine Schritt sollte die Modellabsicht persistierbar machen:

1. eine JSON-Serialisierung fuer `PartDocument` vorbereiten
   (Parameter, Sketches, Features)
2. das Dokument aus JSON wieder aufbauen und erneut rechnen
3. den `ShapeCache` weiterhin nur als berechnetes Ergebnis behandeln
4. weiter keinen allgemeinen Solver und keine GUI bauen
