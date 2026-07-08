# 0002: Objekt-IDs und Namen

Status: akzeptiert fuer MVP 1

## Kontext

MVP 1 benoetigt stabile Referenzen zwischen Parametern, Skizzen, Features und
Shape-Cache. Gleichzeitig soll das fruehe Modell einfach serialisierbar bleiben.

Die MVP-1-Spezifikation nennt Objekte wie:

- `part.width`
- `Sketch_BaseRectangle`
- `BaseExtrude`
- `CenterHoleCut`

Diese Namen sind gut lesbar, aber nicht ausreichend als alleinige interne
Identitaet. Namen koennen spaeter umbenannt werden; Referenzen muessen trotzdem
stabil bleiben.

## Entscheidung

BLCAD verwendet intern typisierte Objekt-IDs und zusaetzlich menschenlesbare
Namen.

MVP 1 unterscheidet mindestens diese ID-Arten:

- `DocumentId`
- `ParameterId`
- `DatumPlaneId`
- `SketchId`
- `FeatureId`
- `ShapeCacheId`

Jedes persistente Modellobjekt besitzt:

```text
id
name
```

Die `id` ist die stabile technische Identitaet. Der `name` ist eine lesbare
Bezeichnung fuer UI, Logs, Tests und Serialisierung.

Fuer MVP 1 duerfen IDs als Stringwerte gespeichert werden, sollen im Code aber
typisiert werden. Das verhindert versehentliches Vertauschen von Parameter-,
Sketch- und Feature-IDs.

## Regeln

- IDs sind innerhalb ihres Typs und Dokuments eindeutig.
- Namen sind innerhalb ihres lokalen Scopes eindeutig.
- Referenzen im Modell verwenden IDs, nicht Namen.
- JSON-Prototypen duerfen Namen enthalten, muessen beim Laden aber auf IDs
  aufgeloest werden.
- Automatisch erzeugte Namen duerfen stabil und schlicht sein, zum Beispiel
  `BaseExtrude` oder `Sketch_CenterHole`.

## Konsequenzen

- Recompute- und Dependency-Graph koennen stabil auf Objekte zeigen.
- Spaetere Umbenennung wird moeglich, ohne Referenzen zu brechen.
- Der erste Code braucht etwas mehr Struktur als reine Strings.
- Tests koennen weiterhin lesbare Namen verwenden.

## Verworfene Alternativen

### Nur Strings

Reine String-IDs waeren am schnellsten umzusetzen. Sie erhoehen aber das Risiko,
dass falsche ID-Typen kombiniert werden.

### Nur Namen

Namen als Identitaet sind fuer MVP 1 verlockend, werden aber spaeter bei
Umbenennung, UI und Serialisierung problematisch.

