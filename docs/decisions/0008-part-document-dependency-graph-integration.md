# 0008: PartDocument erzeugt Dependency-Graph-Kanten

Status: akzeptiert fuer MVP 1

## Kontext

`DependencyGraph` existiert als separates Core-Datenmodell. Damit der Graph fuer
MVP 1 nuetzlich wird, muss er aus den tatsaechlichen Dokumentobjekten entstehen:
Parametern, Sketches und Features.

Die Integration darf noch keinen Recompute starten. Sie soll nur sicherstellen,
dass das Dokument seine Abhaengigkeitsstruktur mitfuehrt.

## Entscheidung

`PartDocument` besitzt einen internen `DependencyGraph`.

Beim Hinzufuegen von Objekten werden Knoten und Kanten abgeleitet:

- `Parameter` erzeugt einen Parameterknoten.
- `Sketch` erzeugt einen Sketchknoten.
- Profil-Parameterreferenzen erzeugen Kanten vom Parameter zum Sketch.
- `Feature` erzeugt einen Featureknoten.
- `Feature.input_sketch` erzeugt eine Kante vom Sketch zum Feature.
- `AdditiveExtrude.length_parameter` erzeugt eine Kante vom Parameter zum
  Feature.
- `SubtractiveExtrude.target_feature` erzeugt eine Kante vom Zielfeature zum
  Feature.

Der Graph ist ueber einen `const`-Accessor lesbar. Direkte externe Mutation des
Graphen ueber `PartDocument` wird nicht angeboten.

## Begruendung

`PartDocument` validiert bereits, dass referenzierte Parameter, Sketches und
Features existieren. Es ist deshalb der richtige Ort, die daraus entstehenden
Graphkanten kontrolliert anzulegen.

Die Graphaenderung wird erst uebernommen, wenn die Objektvalidierung erfolgreich
war. Fehlgeschlagene `add_*`-Operationen duerfen weder das Dokument noch den
Graphen teilweise veraendern.

## Konsequenzen

- Die MVP-1-Abhaengigkeitsordnung ist aus einem echten `PartDocument` pruefbar.
- Der Graph bleibt weiterhin frei von OCCT und GUI.
- Invalidierung und Recompute bleiben separate Folgeschritte.
- Datum-Planes werden noch nicht als Graphknoten modelliert; fuer MVP 1 reichen
  Parameter-, Sketch- und Feature-Abhaengigkeiten.

## Verworfene Alternativen

### Graph extern zum PartDocument pflegen

Das wuerde doppelte Buchhaltung erzeugen und koennte leicht vom
Dokumentzustand abweichen.

### Graph direkt mutierbar herausgeben

Das waere flexibler, wuerde aber die Dokumentinvarianten umgehen. Fuer MVP 1
soll `PartDocument` die einzige Stelle sein, die seine Abhaengigkeitsstruktur
veraendert.
