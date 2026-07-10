# 0018: Count-Parameter und parametrischer Lochkreis

Status: akzeptiert fuer MVP 3

## Kontext

MVP 3 verlangt einen parametrischen Lochkreis: Radius, Lochanzahl und
Bohrungsdurchmesser als Parameter mit automatischem Recompute. Die Lochanzahl
ist eine dimensionslose ganze Zahl. `Quantity` und `Parameter` unterstuetzten
bisher nur positive Laengen in Millimetern.

Ausserdem musste entschieden werden, ob der Lochkreis als Menge unabhaengiger
Kreisprofile oder als eigenes semantisches Muster gespeichert wird.

## Entscheidung

`Quantity` erhaelt eine `QuantityKind`-Unterscheidung (`LengthMm`, `Count`).
`Quantity::count` validiert endliche, ganze Werte ab 1 und rundet auf die
naechste ganze Zahl innerhalb der Toleranz. `Parameter::create_count` erzeugt
`ParameterType::Count`; `with_value` haelt die Count-Validierung bei Updates.
Im JSON wird `"type": "count"` mit `"unit": "1"` gespeichert.

Der Lochkreis wird als `CircularHolePattern` im Sketch gespeichert: Zentrum,
Winkelversatz und drei Parameterreferenzen (Radius als Laenge, Anzahl als
Count, Bohrungsdurchmesser als Laenge). Er ist Modellabsicht, keine
Geometriekopie. `PartDocument::add_sketch` prueft Existenz und Typ der
Parameter und erzeugt Kanten von allen drei Parametern zum Sketch.

Der Geometry-Executor expandiert das Muster bei einem `SubtractiveExtrude` in
sequentielle Through-All-Kreisschnitte ueber den vorhandenen
`CircularCutAdapter`. Jede Bohrung wird einzeln gegen die aufgeloesten
Workplane-Bounds geprueft; die Fehlermeldung nennt `pattern.<id>.hole.<index>`.

## Alternativen

- Anzahl als Laengenparameter missbrauchen: verworfen, verletzt das
  Einheitenmodell aus ADR 0003.
- Ganzzahl direkt im Muster speichern statt als Parameter: verworfen, die
  Anzahl waere nicht parametrisch und nicht im Dependency Graph.
- Ein Boolean mit gemeinsamem Tool-Body aller Bohrungen: aufgeschoben als
  spaetere Optimierung; sequentielle Cuts sind fuer MVP-Groessen ausreichend
  und wiederverwenden den getesteten Adapterpfad.

## Konsequenzen

- Zukuenftige Parametertypen (z. B. Winkel) folgen demselben
  `QuantityKind`-Muster.
- Der Hole Wizard kann spaeter auf `CircularHolePattern` aufbauen und
  Bohrungssemantik ergaenzen.
- MVP 4 kann dieselben Parameter assembly-weit teilen, ohne das Muster zu
  aendern.
