# 0004: Fehlerbehandlung im Core

Status: akzeptiert fuer MVP 1

## Kontext

MVP 1 muss erwartbare Fehler sichtbar machen:

- ungueltige Parameter
- ungueltige Profile
- fehlschlagende OCCT-Operationen
- fehlschlagender STEP-Export

Diese Fehler sind Teil des CAD-Modells und duerfen nicht still ignoriert werden.
Sie sind keine Programmierfehler, sondern normale Zustaende eines parametrischen
Systems.

## Entscheidung

BLCAD verwendet Ergebnisobjekte fuer erwartbare Fehler.

Konzeptuell liefert eine Operation entweder:

```text
Success(value)
```

oder:

```text
Failure(error)
```

Ein Fehler enthaelt mindestens:

```text
Error
  object_id
  category
  message
```

Fuer MVP 1 sind diese Kategorien ausreichend:

- `validation`
- `dependency`
- `geometry`
- `export`
- `internal`

Exceptions sind fuer erwartbare Core-Fehler nicht der normale Kontrollfluss.
Exceptions duerfen an Bibliotheksgrenzen abgefangen und in Fehlerobjekte
uebersetzt werden.

## Regeln

- Validierungsfehler werden vor OCCT-Aufrufen gemeldet.
- Recompute bricht bei einem Fehler kontrolliert ab und speichert den Fehler am
  betroffenen Objekt.
- Der Shape-Cache wird nur als gueltig markiert, wenn der komplette Recompute
  erfolgreich war.
- STEP-Exportfehler werden als `export`-Fehler gemeldet.
- Programmierfehler duerfen mit Assertions oder klaren internen Fehlern sichtbar
  werden.

## Konsequenzen

- Tests koennen Fehlerfaelle direkt pruefen.
- Die spaetere UI kann Fehler am betroffenen Feature anzeigen.
- Der Core bleibt deterministischer als bei breitem Exception-Einsatz.
- Bibliotheksfehler von OCCT brauchen eine kleine Uebersetzungsschicht.

## Verworfene Alternativen

### Exceptions fuer alles

Exceptions sind fuer Bibliotheks- und Programmierfehler nuetzlich, aber fuer
normale CAD-Validierungsfehler zu grob.

### Fehler nur loggen

Logging allein reicht nicht. Fehler muessen im Modell greifbar bleiben, damit
Recompute, Tests und spaetere UI sinnvoll reagieren koennen.

