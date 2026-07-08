# MVP 1 Spezifikation: Einzelteilmodellierung

Quelle: `zielarchitektur-parametrisches-cad-system.tex`

Status: Spezifikation mit erstem Core-Skeleton und Dependency-Graph-Datenmodell

## Ziel

MVP 1 beweist den kleinsten sinnvollen Kern des CAD-Systems:

Ein `PartDocument` kann ein einzelnes parametrisches Bauteil beschreiben, aus
Parametern und Features eine OCCT-Geometrie berechnen und diese Geometrie als
STEP-Datei exportieren.

Das Referenzbauteil ist eine rechteckige Platte mit zentraler Bohrung:

```text
width         = 120 mm
height        = 80 mm
thickness     = 8 mm
hole_diameter = 20 mm
```

Der wichtigste Nachweis ist nicht die grafische Darstellung. Der wichtigste
Nachweis ist, dass das Modell die Konstruktionsabsicht speichert:

- die Platte ist ein Rechteck, das extrudiert wurde
- die Bohrung ist ein Kreis, der als Cut durch den Koerper geht
- alle relevanten Werte sind Parameter
- die fertige OCCT-Shape ist nur ein berechneter Cache

## Nicht-Ziele

Folgende Themen gehoeren ausdruecklich nicht zu MVP 1:

- keine vollstaendige GUI
- kein Feature Tree in einer UI
- keine Assembly-Dokumente
- keine Assembly Constraints
- kein Top-Down-Design
- keine Skizzen auf vorhandenen Koerperflaechen
- kein Lochkreis
- keine Formeln zwischen Parametern
- kein vollstaendiger Constraint Solver
- keine semantischen Face-Referenzen ausser einfachen Feature-Ausgaben
- keine Materialdatenbank
- keine Normteilbibliothek
- keine technischen Zeichnungen
- kein DXF-Import
- kein STL-Export als Pflichtziel

Diese Begrenzung ist wichtig. MVP 1 soll den Core klein halten und nur die erste
belastbare vertikale Linie durch Dokument, Parameter, Feature-Recompute,
OCCT-Geometrie und STEP-Export liefern.

## Erfolgskriterium

MVP 1 gilt als erreicht, wenn ein automatisierter Ablauf folgendes schafft:

1. Ein `PartDocument` fuer eine Platte wird erzeugt.
2. Die vier Parameter `width`, `height`, `thickness`, `hole_diameter` werden
   gesetzt.
3. Eine Basisskizze auf der XY-Ebene beschreibt ein zentriertes Rechteck.
4. Ein `AdditiveExtrude` erzeugt daraus einen Volumenkoerper.
5. Eine zweite Skizze auf derselben XY-Ebene beschreibt einen zentrierten Kreis.
6. Ein `SubtractiveExtrude` schneidet die Bohrung durch den Koerper.
7. Das Ergebnis wird als gueltige STEP-Datei exportiert.
8. Wenn `hole_diameter` oder `thickness` geaendert wird, wird die Shape neu
   berechnet.

## Koordinaten und Einheiten

### Koordinatensystem

MVP 1 verwendet ein festes Weltkoordinatensystem:

- X-Achse: Breite der Platte
- Y-Achse: Hoehe der Platte
- Z-Achse: Dicke der Platte
- Ursprung: Mittelpunkt der unteren Rechteckflaeche

Die Basisskizze liegt auf der XY-Ebene bei `z = 0`.

Die Extrusion der Platte verlaeuft in positive Z-Richtung.

### Einheitenmodell

Alle Laengen werden intern in Millimetern gespeichert.

Ein Parameter speichert trotzdem Einheit und Typ, damit spaeter echte
Einheitenunterstuetzung moeglich bleibt.

```text
Parameter
  id        = "part.width"
  name      = "width"
  type      = "length"
  value     = 120
  unit      = "mm"
  scope     = "part"
```

Fuer MVP 1 sind nur diese Parametertypen noetig:

- `length`

Fuer MVP 1 ist nur diese Einheit noetig:

- `mm`

## Objektmodell

Aktueller Implementierungsstand:

- `Quantity`, `Error`, `Result`, typisierte IDs, `Parameter` und
  `PartDocument` sind als erster Core-Skeleton vorhanden.
- `DatumPlane`, `Sketch`, `RectangleProfile` und `CircleProfile` sind als reine
  Datenmodelle vorhanden und in `PartDocument` integriert.
- `Feature`, `AdditiveExtrude` und `SubtractiveExtrude` sind als reine
  Datenmodelle vorhanden und in `PartDocument` integriert.
- `DependencyGraph` ist als reines Datenmodell vorhanden und in `PartDocument`
  integriert.
- `InvalidationState` ist als reines Datenmodell vorhanden und in
  `PartDocument` integriert.
- `RecomputePlan` ist als reines Datenmodell vorhanden und in `PartDocument`
  ableitbar.
- `ShapeCache` ist als optionales Geometry-Datenmodell vorhanden, aber noch
  nicht in `PartDocument` integriert.
- `GeometryRecomputeExecutor` kann `AdditiveExtrude` fuer genau ein
  Rechteckprofil und `SubtractiveExtrude` fuer genau ein Kreisprofil im
  optionalen Geometry-Target ausfuehren.
- `GeometryRecomputeExecutor` kann mit `execute_document` ein ganzes
  `PartDocument` in topologischer Reihenfolge in einen `ShapeCache` rechnen.
- `StepExporter` kann einen berechneten `GeometryShape` als STEP-Datei
  exportieren.
- Ein Ende-zu-Ende-Test erzeugt das Referenzbauteil und exportiert es als
  gueltige STEP-Datei.
- `PartDocument::set_parameter_value` aendert einen Parameterwert und markiert die
  Abhaengigen als geaendert, sodass ein numerischer inkrementeller Recompute
  moeglich ist.

Details: `docs/core-mvp1-skeleton.md`

### PartDocument

`PartDocument` ist das zentrale Dokumentobjekt fuer MVP 1.

Mindestinhalt:

```text
PartDocument
  id
  name
  parameters
  datum_planes
  sketches
  features
  dependency_graph
  shape_cache
```

Regeln:

- Ein MVP-1-Dokument enthaelt genau ein Bauteil.
- Es gibt keine Baugruppe.
- Es gibt keine externen Part-Referenzen.
- Der finale Shape-Cache darf geloescht und neu berechnet werden koennen.

Aktueller Stand:

- `PartDocument` speichert `DocumentId`, Name und Parameter.
- Parameter koennen hinzugefuegt werden.
- Parameter-IDs und Parameternamen muessen eindeutig sein.
- Parameter koennen per ID oder Name gesucht werden.
- Datum-Planes koennen hinzugefuegt werden und muessen eindeutige IDs haben.
- Sketches koennen hinzugefuegt werden und muessen eindeutige IDs haben.
- Sketch-Workplanes muessen im Dokument existieren.
- Profil-Parameterreferenzen muessen im Dokument existieren.
- Features koennen hinzugefuegt werden.
- Feature-IDs muessen eindeutig sein.
- Feature-Sketchreferenzen muessen im Dokument existieren.
- Additive-Extrude-Laengenparameter muessen im Dokument existieren.
- Subtractive-Extrude-Zielfeatures muessen im Dokument existieren.
- `PartDocument` erzeugt Dependency-Graph-Knoten fuer Parameter, Sketches und
  Features.
- `PartDocument` erzeugt Dependency-Graph-Kanten aus Profil- und
  Feature-Referenzen.
- `PartDocument` markiert betroffene Graphknoten nach einer Parameteraenderung.
- `PartDocument` kann `dirty`-Knoten als Recompute-Plan in topologischer
  Reihenfolge ableiten.
- `PartDocument` kann per `mark_all_clean()` nach einem Recompute wieder in den
  sauberen Zustand gesetzt werden.
- `PartDocument` kann per `set_parameter_value()` einen Parameterwert aendern, den
  neuen Wert validieren und die Abhaengigen als geaendert markieren.
- Der Shape-Cache bleibt bewusst in der Geometry-Schicht und wird ueber
  `execute_document` aus dem Dokument erzeugt, nicht als Feld eingebettet.

### Parameter

Parameter sind benannte Werte mit Typ, Einheit und Scope.

Mindestfelder:

```text
Parameter
  id
  name
  type
  value
  unit
  scope
```

Validierung:

- `name` ist innerhalb des Part-Dokuments eindeutig.
- `type` ist fuer MVP 1 immer `length`.
- `unit` ist fuer MVP 1 immer `mm`.
- `value` muss numerisch sein.
- Laengenparameter muessen groesser als `0` sein.

Reservierte Felder fuer spaeter:

```text
formula
description
locked
```

Diese Felder duerfen im Datenmodell vorbereitet werden, muessen in MVP 1 aber
nicht ausgewertet werden.

Aktueller Stand:

- `Parameter` ist unveraenderlich; `Parameter::with_value` liefert eine Kopie mit
  neuem, erneut validiertem Wert.
- `PartDocument::set_parameter_value` aendert einen Wert und markiert den
  Parameter und seine Abhaengigen als geaendert.

### DatumPlane

MVP 1 benoetigt nur Standardebenen.

Pflicht:

```text
DatumPlane XY
  origin = (0, 0, 0)
  x_axis = (1, 0, 0)
  y_axis = (0, 1, 0)
  normal = (0, 0, 1)
```

Optional fuer interne Vollstaendigkeit:

```text
DatumPlane YZ
DatumPlane XZ
```

Fuer MVP 1 muessen Features nur mit `XY` funktionieren.

Aktueller Stand:

- `DatumPlane::xy()` erzeugt die Standardebene `XY`.
- ID und Name werden validiert.
- Datum-Planes koennen in `PartDocument` gespeichert und per ID gefunden werden.
- Freie oder abgeleitete Ebenen sind noch nicht implementiert.

### Sketch

Eine Skizze liegt auf einer `DatumPlane` und enthaelt einfache 2D-Geometrie.

Mindestfelder:

```text
Sketch
  id
  name
  workplane
  entities
  constraints
```

MVP-1-Entitaeten:

- `RectangleProfile`
- `CircleProfile`

`Line` kann intern als Baustein fuer ein Rechteck existieren, muss aber noch
nicht als frei editierbare Entitaet nach aussen angeboten werden.

MVP-1-Constraints:

- keine allgemeine Constraint-Loesung
- Profile werden direkt aus Parametern erzeugt
- Zentrierung wird durch die Profildefinition beschrieben

Aktueller Stand:

- `Sketch` speichert `SketchId`, Name und `DatumPlaneId`.
- Rechteck- und Kreisprofile koennen hinzugefuegt werden.
- Profil-IDs muessen innerhalb einer Skizze eindeutig sein.
- Workplane-Referenzen werden beim Hinzufuegen zu `PartDocument` validiert.
- Profil-Parameterreferenzen werden beim Hinzufuegen zu `PartDocument`
  validiert.

### RectangleProfile

Das Rechteck beschreibt die Grundplatte.

```text
RectangleProfile
  id
  center = (0, 0)
  width_parameter = part.width
  height_parameter = part.height
```

Abgeleitete Eckpunkte:

```text
(-width / 2, -height / 2)
( width / 2, -height / 2)
( width / 2,  height / 2)
(-width / 2,  height / 2)
```

Validierung:

- `width > 0`
- `height > 0`

Aktueller Stand:

- `RectangleProfile` speichert `ProfileId`, Mittelpunkt,
  Breitenparameter-Referenz und Hoehenparameter-Referenz.
- Fehlende Profil-IDs und Parameterreferenzen werden abgelehnt.
- Parameterreferenzen werden beim Hinzufuegen des Sketches zu `PartDocument`
  gegen vorhandene Parameter validiert.
- Parameterwerte werden noch nicht aufgeloest.

### CircleProfile

Der Kreis beschreibt die zentrale Bohrung.

```text
CircleProfile
  id
  center = (0, 0)
  diameter_parameter = part.hole_diameter
```

Validierung:

- `hole_diameter > 0`
- `hole_diameter < min(width, height)`

Aktueller Stand:

- `CircleProfile` speichert `ProfileId`, Mittelpunkt und
  Durchmesserparameter-Referenz.
- Fehlende Profil-ID und Durchmesserreferenz werden abgelehnt.
- Die Durchmesserreferenz wird beim Hinzufuegen des Sketches zu `PartDocument`
  gegen vorhandene Parameter validiert.
- Die Groessenvalidierung gegen Breite und Hoehe folgt spaeter nach
  Parameterwertauswertung.

## Feature-System

MVP 1 benoetigt genau zwei Feature-Typen.

Aktueller Stand:

- `Feature` ist als reines Datenmodell implementiert.
- `AdditiveExtrude` und `SubtractiveExtrude` speichern nur Absicht und
  Referenzen.
- `PartDocument` validiert Feature-Referenzen gegen vorhandene Sketches,
  Parameter und Zielfeatures.
- Es gibt optionale OCCT-Adapter fuer die Rechteckextrusion und den zentrischen
  Cut sowie eine schmale Recompute-Ausfuehrung fuer beide Feature-Typen.

### AdditiveExtrude

Erzeugt aus einer geschlossenen Skizze einen Volumenkoerper.

```text
Feature BaseExtrude
  type = AdditiveExtrude
  input_sketch = Sketch_BaseRectangle
  length_parameter = part.thickness
  direction = +Z
  output_body = Body
```

Validierung:

- `input_sketch` muss ein geschlossenes Profil enthalten.
- `length_parameter > 0`
- Richtung ist fuer MVP 1 fest `+Z`.

Aktueller Stand:

- `input_sketch` muss als Sketch im `PartDocument` existieren.
- `length_parameter` muss als Parameter im `PartDocument` existieren.
- Ob der Sketch ein geschlossenes Profil enthaelt, wird noch nicht als
  Geometriebedingung ausgewertet.

### SubtractiveExtrude

Schneidet ein geschlossenes Profil aus einem bestehenden Koerper.

```text
Feature CenterHoleCut
  type = SubtractiveExtrude
  input_sketch = Sketch_CenterHole
  target_body = BaseExtrude.output_body
  depth = through_all
  direction = +Z
  output_body = Body
```

Validierung:

- `target_body` muss existieren.
- `input_sketch` muss ein geschlossenes Profil enthalten.
- `depth` ist fuer MVP 1 immer `through_all`.
- Der Kreis muss innerhalb des Rechtecks liegen.

Aktueller Stand:

- `input_sketch` muss als Sketch im `PartDocument` existieren.
- `target_feature` muss als Feature im `PartDocument` existieren.
- `depth` ist als `through_all` modelliert.
- Der optionale `GeometryRecomputeExecutor` wertet den Cut fuer genau ein
  Kreisprofil als zentrischen `through_all`-Boolean-Cut aus. Der Zielkoerper
  wird dabei aus dem `ShapeCache` gelesen.

## Recompute

MVP 1 braucht einen kleinen, aber echten Recompute-Ablauf.

### Abhaengigkeiten

Minimaler Dependency Graph fuer das Referenzbauteil:

```text
part.width
  -> Sketch_BaseRectangle
  -> BaseExtrude
  -> Part.Shape

part.height
  -> Sketch_BaseRectangle
  -> BaseExtrude
  -> Part.Shape

part.thickness
  -> BaseExtrude
  -> Part.Shape

part.hole_diameter
  -> Sketch_CenterHole
  -> CenterHoleCut
  -> Part.Shape
```

`CenterHoleCut` haengt zusaetzlich von `BaseExtrude` ab.

```text
BaseExtrude
  -> CenterHoleCut
  -> Part.Shape
```

Aktueller Stand:

- `DependencyGraph` speichert Knoten als stabile String-IDs.
- Eine Kante `A -> B` bedeutet: `B` haengt von `A` ab.
- Kanten koennen fuer Parameter-, Sketch- und Feature-Abhaengigkeiten
  gespeichert werden.
- Direkte und transitive Abhaengige koennen abgefragt werden.
- Eine topologische Ordnung kann erzeugt werden.
- Zyklen werden als `dependency`-Fehler gemeldet.
- `PartDocument` besitzt den Graphen und leitet Kanten aus seinen Objekten ab.
- `InvalidationState` nutzt den Graphen, um `changed` und `dirty` zu markieren.
- `RecomputePlan` nutzt den Graphen, um `dirty`-Knoten topologisch zu sortieren.
- Graph, Invalidierungsstatus und Plan fuehren keinen Recompute aus.

### Ablauf

1. Parameterwert wird geaendert.
2. Der Parameter wird validiert.
3. Abhaengige Skizzen und Features werden als invalid markiert.
4. Features werden in Reihenfolge neu berechnet.
5. Der finale Shape-Cache wird ersetzt.
6. Fehler werden am betroffenen Objekt gespeichert.

### Fehlerverhalten

Fehler duerfen nicht still ignoriert werden.

Beispiele:

- `width <= 0`: Parameterfehler
- `hole_diameter >= width`: Sketch- oder Featurefehler
- OCCT-Boolean schlaegt fehl: Featurefehler
- STEP-Export schlaegt fehl: Exportfehler

Jeder Fehler braucht mindestens:

```text
Error
  object_id
  category
  message
```

## ShapeCache

Der Shape-Cache speichert berechnete OCCT-Geometrie.

Regeln:

- Der Cache ist nicht die Quelle der Wahrheit.
- Der Cache darf geloescht werden.
- Der Cache muss aus Dokument, Parametern, Skizzen und Features neu erzeugbar
  sein.
- Fuer MVP 1 reicht ein finaler Shape-Cache pro `PartDocument`.
- Aktuell existiert ein optionaler `ShapeCache` im Geometry-Target, der
  Feature-Shapes und einen finalen Shape halten kann.
- Der `ShapeCache` bleibt in der Geometry-Schicht und ist kein Feld von
  `PartDocument`, damit der Core OCCT-frei bleibt.
- `GeometryRecomputeExecutor::execute_document` erzeugt den `ShapeCache`
  vollstaendig aus Dokument, Parametern, Skizzen und Features neu. Damit ist die
  Regel der Neu-Erzeugbarkeit konkret erfuellt.

Mindestfelder:

```text
ShapeCache
  owner_document_id
  valid
  source_feature_id
  occt_shape
```

## STEP-Export

MVP 1 muss eine STEP-Datei fuer das finale Bauteil exportieren koennen.

Anforderungen:

- Exportiert wird nur der finale Shape-Cache.
- Dateiformat: STEP AP214 oder AP242, je nach OCCT-Unterstuetzung.
- Der Exportpfad wird vom Aufrufer uebergeben.
- Exportfehler werden als Fehlerobjekt gemeldet.

Nicht erforderlich:

- Import von STEP
- Metadaten-Roundtrip
- Farben oder Materialien
- Assembly-STEP

Aktueller Stand:

- Der optionale `StepExporter` schreibt einen `GeometryShape` als STEP-Datei mit
  Schema `AP214`.
- Der Exportpfad wird vom Aufrufer uebergeben.
- Die geschriebene Datei wird auf eine Groesse groesser als `0` geprueft.
- Exportfehler werden als `ErrorCategory::Export` gemeldet.
- Der Aufruf des Exports direkt aus `PartDocument` folgt spaeter.

## Dateiformat fuer MVP 1

Ein eigenes dauerhaftes Dateiformat muss in MVP 1 noch nicht final sein.

Trotzdem soll ein internes serialisierbares Modell geplant werden. Ein
JSON-Prototyp ist fuer fruehe Tests ausreichend.

Beispielstruktur:

```json
{
  "type": "PartDocument",
  "version": 1,
  "name": "RectangularPlate",
  "parameters": [
    { "name": "width", "type": "length", "value": 120, "unit": "mm" },
    { "name": "height", "type": "length", "value": 80, "unit": "mm" },
    { "name": "thickness", "type": "length", "value": 8, "unit": "mm" },
    { "name": "hole_diameter", "type": "length", "value": 20, "unit": "mm" }
  ],
  "sketches": [
    {
      "name": "Sketch_BaseRectangle",
      "workplane": "XY",
      "profile": "centered_rectangle",
      "width": "width",
      "height": "height"
    },
    {
      "name": "Sketch_CenterHole",
      "workplane": "XY",
      "profile": "centered_circle",
      "diameter": "hole_diameter"
    }
  ],
  "features": [
    {
      "name": "BaseExtrude",
      "type": "additive_extrude",
      "input_sketch": "Sketch_BaseRectangle",
      "length": "thickness",
      "direction": "+Z"
    },
    {
      "name": "CenterHoleCut",
      "type": "subtractive_extrude",
      "input_sketch": "Sketch_CenterHole",
      "target": "BaseExtrude",
      "depth": "through_all",
      "direction": "+Z"
    }
  ]
}
```

Dieses JSON ist Spezifikation fuer die Modellabsicht, nicht fuer
OCCT-Shape-Serialisierung.

## Testfaelle

MVP 1 sollte mit automatisierten Tests abgesichert werden.

### Parameter-Tests

- `width = 120 mm` ist gueltig.
- `width = 0 mm` ist ungueltig.
- `thickness = -1 mm` ist ungueltig.
- doppelte Parameternamen sind ungueltig.
- `set_parameter_value` aendert einen Wert und markiert die Abhaengigen als
  `dirty`.
- `set_parameter_value` lehnt fehlende und leere Parameter-IDs ab.

### Sketch-Tests

- Rechteck erzeugt vier Eckpunkte.
- Kreisradius ist `hole_diameter / 2`.
- Kreis mit Durchmesser kleiner als `min(width, height)` ist gueltig.
- Kreis mit Durchmesser groesser oder gleich Plattenbreite ist ungueltig.

### Dependency-Tests

- `part.width -> sketch.base` wird gespeichert.
- `part.thickness -> feature.base_extrude` wird gespeichert.
- `part.hole_diameter -> sketch.hole` wird gespeichert.
- `feature.center_hole_cut` kommt in topologischer Ordnung nach
  `feature.base_extrude`.
- Zyklen werden erkannt.
- Aenderung von `part.width` markiert `sketch.base`, `feature.base_extrude`
  und `feature.center_hole_cut` als `dirty`.
- Nicht betroffene Knoten bleiben `clean`.
- Der Recompute-Plan enthaelt `dirty`-Knoten in topologischer Reihenfolge.
- `changed`-Parameter stehen nicht im Recompute-Plan.

Spaetere Integrationstests sollen pruefen:

- `CenterHoleCut` wird nach `BaseExtrude` neu berechnet.

### Geometrie-Tests

- Der optionale `RectangleExtrusionAdapter` erzeugt einen nicht-leeren Solid.
- Der optionale `RectangleExtrusionAdapter` erzeugt genau einen Solid.
- Der optionale `ShapeCache` speichert Feature-Shapes und einen finalen Shape.
- `AdditiveExtrude` erzeugt ueber den optionalen Geometry-Recompute einen
  nicht-leeren Solid.
- `SubtractiveExtrude` erzeugt einen nicht-leeren Solid.
- Das finale Volumen ist kleiner als das Volumen ohne Bohrung.
- STEP-Export erzeugt eine Datei groesser als 0 Byte.
- `execute_document` rechnet das Referenzbauteil vollstaendig und exportiert eine
  gueltige STEP-Datei.
- Der Recompute folgt einem sauberen Clean/Dirty-Lebenszyklus.
- Nach `set_parameter_value` fuer einen groesseren Bohrungsdurchmesser rechnet ein
  inkrementeller Plan nur den Cut neu, und das finale Volumen wird kleiner.

## Aktuelle Implementierungsreihenfolge

Diese Liste zeigt die grobe MVP-1-Reihenfolge. Die ersten vier Punkte sind
inzwischen als reine Datenmodellschicht umgesetzt.

1. Datenklassen fuer `Parameter`, `PartDocument`, `Sketch`, `Feature`: erledigt.
2. Validierungslogik fuer Parameter und Profile: erledigt.
3. Dependency Graph fuer die feste MVP-1-Abhaengigkeitsstruktur: erledigt.
4. Dependency Graph in `PartDocument` integrieren: erledigt.
5. Invalidierungsstatus als Datenzustand modellieren: erledigt.
6. Recompute-Plan als Datenzustand modellieren: erledigt.
7. OCCT-Adapter fuer Rechteck-Extrusion: begonnen als optionales
   `blcad_geometry`-Target.
8. ShapeCache als Geometry-Datenmodell: erledigt.
9. Recompute-Ausfuehrung fuer AdditiveExtrude: erledigt fuer ein einzelnes
   Rechteckprofil.
10. OCCT-Adapter fuer zentrischen Cut: erledigt als `CircularCutAdapter` mit
    Recompute-Ausfuehrung fuer ein einzelnes Kreisprofil.
11. STEP-Exporter: erledigt als `StepExporter` fuer einen `GeometryShape`.
12. Vollstaendiger Dokument-Recompute und Referenzbauteil-Pipeline: erledigt als
    `execute_document` mit Ende-zu-Ende-Test.
13. Parameterwert-Update und numerischer inkrementeller Recompute: erledigt als
    `set_parameter_value` mit Ende-zu-Ende-Test.
14. Tests fuer Parameter, Dependency Graph, Recompute und STEP-Export.

## Designentscheidungen vor dem ersten Code

Diese Punkte wurden fuer MVP 1 entschieden:

- Objekt-IDs und Namen: `docs/decisions/0002-object-identity-and-naming.md`
- Einheiten und Laengenwerte: `docs/decisions/0003-units-and-quantities.md`
- Fehlerbehandlung: `docs/decisions/0004-error-handling.md`
- Grenze zwischen Core und OCCT: `docs/decisions/0005-geometry-adapter-boundary.md`
- Zeitpunkt und Umfang der Serialisierung: `docs/decisions/0006-mvp1-serialization-timing.md`
- Umfang des Dependency Graph: `docs/decisions/0007-dependency-graph-scope.md`
- Dependency-Graph im PartDocument:
  `docs/decisions/0008-part-document-dependency-graph-integration.md`
- Umfang des Invalidierungsstatus:
  `docs/decisions/0009-invalidation-state-scope.md`
- Umfang des Recompute-Plans: `docs/decisions/0010-recompute-plan-scope.md`
- Erster Geometry-Adapter:
  `docs/decisions/0011-geometry-adapter-rectangle-extrusion.md`
- Umfang des ersten ShapeCache: `docs/decisions/0012-shape-cache-scope.md`
- Erste Additive-Recompute-Ausfuehrung:
  `docs/decisions/0013-additive-extrude-recompute-execution.md`
- Subtractive-Recompute-Ausfuehrung:
  `docs/decisions/0014-subtractive-extrude-recompute-execution.md`
- STEP-Export: `docs/decisions/0015-step-export.md`
- Dokument-Recompute und ShapeCache-Grenze:
  `docs/decisions/0016-document-recompute-and-shape-cache-boundary.md`
- Parameterwert-Update: `docs/decisions/0017-parameter-value-update.md`

Kurzfassung fuer MVP 1:

- typisierte IDs, aber einfache String-Namen fuer Serialisierung
- `Quantity` mit internem Wert in Millimetern
- Ergebnisobjekte fuer erwartbare Validierungsfehler
- Dependency Graph zuerst als reines Datenmodell ohne Recompute
- Invalidierungsstatus zuerst als reines Datenmodell ohne Recompute
- Recompute-Plan zuerst als reines Datenmodell ohne Ausfuehrung
- OCCT hinter einem kleinen Geometry-Adapter kapseln
- erster OCCT-Pfad als optionales `blcad_geometry`-Target
- ShapeCache zuerst optional in der Geometry-Schicht halten
- AdditiveExtrude zuerst fuer genau ein Rechteckprofil ausfuehren
- JSON erst nach funktionierendem Recompute und STEP-Export

## Abnahmekriterien

MVP 1 ist fertig, wenn alle folgenden Aussagen stimmen:

- Das Referenzmodell ist ohne GUI erzeugbar.
- Alle vier Parameter koennen geaendert werden.
- Ungueltige Parameter werden abgefangen.
- Die Platte wird als OCCT-Solid berechnet.
- Die zentrale Bohrung wird als Cut berechnet.
- Der finale Shape-Cache wird nach Parameteraenderung neu erzeugt.
- Eine STEP-Datei wird exportiert.
- Tests pruefen Parameter, Recompute und Export.
- Der Core enthaelt keine UI-Logik.
