# MVP 1 ShapeCache-Datenmodell

Status: optionales Geometry-Datenmodell, von Additive-Recompute nutzbar

Dieses Dokument beschreibt den ersten kleinen `ShapeCache` fuer MVP 1. Der
Cache liegt im optionalen Target `blcad_geometry`, weil er `GeometryShape`
enthaelt. Der Core bleibt dadurch frei von OCCT-Headern und frei von
Geometry-Linking.

## Ziel

Der `ShapeCache` speichert berechnete Geometrie als Ergebnis des parametrischen
Modells. Er ist nicht die Quelle der Wahrheit. Quelle der Wahrheit bleiben
Dokument, Parameter, Skizzen, Features, Dependency Graph und Recompute-Plan.

Der aktuelle Cache unterstuetzt den ersten Additive-Recompute-Schritt:

- berechnete Feature-Shapes nach `FeatureId` speichern
- einen finalen Shape und dessen Quellfeature markieren
- Cache-Inhalte loeschen koennen
- leere IDs und leere Shapes ablehnen

## CMake-Target

Der Cache wird nur mit dem Geometry-Preset gebaut:

```bash
cmake --preset dev-geometry
cmake --build --preset dev-geometry
ctest --preset dev-geometry
```

Der Standard-Preset `dev` bleibt Core-only.

## Oeffentliche Schnittstelle

Header:

```text
include/blcad/geometry/shape_cache.hpp
```

Aktuelle Typen:

- `CachedFeatureShape`
- `ShapeCache`

`ShapeCache` verwendet:

- `ShapeCacheId`
- `FeatureId`
- `GeometryShape`
- `Result<T>` und `Error`

## Verhalten

Ein Cache wird ueber eine nicht-leere `ShapeCacheId` erzeugt:

```text
ShapeCache::create(shape_cache_id)
```

Feature-Shapes werden nach Feature-ID abgelegt:

```text
store_feature_shape(feature.base_extrude, geometry_shape)
```

Der finale Shape wird mit dem Feature verbunden, das ihn aktuell erzeugt:

```text
set_final_shape(feature.base_extrude, geometry_shape)
```

`clear()` entfernt alle Feature-Shapes und den finalen Shape.

## Validierung

Aktuelle Regeln:

- Cache-ID darf nicht leer sein.
- Feature-ID darf nicht leer sein.
- Leere `GeometryShape`-Handles duerfen nicht gespeichert werden.
- Erneutes Speichern derselben Feature-ID ersetzt den vorhandenen Shape statt
  einen zweiten Eintrag anzulegen.

## Testabdeckung

Aktuelle Tests pruefen:

- Cache startet leer mit stabiler ID.
- Leere Cache-IDs werden abgelehnt.
- Feature-Shapes koennen gespeichert und wiedergefunden werden.
- Ein gespeicherter Rechteckextrusions-Shape enthaelt genau einen Solid.
- Wiederholtes Speichern derselben Feature-ID erzeugt keinen Duplikateintrag.
- Finaler Shape und Quellfeature werden gespeichert.
- `clear()` leert Feature- und finalen Shape.
- Leere Feature-IDs und leere Shapes werden abgelehnt.
- `GeometryRecomputeExecutor` kann einen ausgefuehrten `AdditiveExtrude` im
  Cache ablegen.

## Bewusste Begrenzung

Noch nicht enthalten:

- Integration in `PartDocument`
- `SubtractiveExtrude` und Boolean Cut
- STEP-Export
- GUI

## Naechster sinnvoller Schritt

Als naechstes sollte der zentrische Cut fuer `SubtractiveExtrude` entstehen:

1. Zielshape aus dem `ShapeCache` lesen
2. Kreisprofil und Durchmesserparameter aus `PartDocument` aufloesen
3. OCCT-Boolean-Cut ausfuehren
4. Ergebnis im `ShapeCache` als neuen finalen Shape speichern
4. weiter keinen allgemeinen Solver und keine GUI bauen
