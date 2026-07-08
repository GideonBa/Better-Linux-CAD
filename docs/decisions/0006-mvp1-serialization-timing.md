# 0006: Zeitpunkt und Umfang der Serialisierung

Status: akzeptiert fuer MVP 1

## Kontext

Die Zielarchitektur verlangt langfristig ein eigenes Dateiformat, das die
Konstruktionsabsicht speichert. MVP 1 soll aber zuerst den vertikalen Pfad durch
Parameter, Features, Recompute, OCCT-Shape und STEP-Export beweisen.

Eine zu fruehe Dateiformat-Implementierung wuerde das Projekt vom Kernnachweis
ablenken.

## Entscheidung

JSON-Serialisierung wird fuer MVP 1 geplant, aber nicht als erster Codepfad
gebaut.

Reihenfolge:

1. internes Modell
2. Validierung
3. Recompute
4. OCCT-Geometriepfad
5. STEP-Export
6. JSON-Serialisierung der Modellabsicht

Die Serialisierung speichert Modellabsicht, nicht OCCT-Shapes.

## Regeln

- Der Shape-Cache ist kein primaerer Bestandteil des JSON-Modells.
- JSON enthaelt Parameter, Skizzen, Features und Referenzen.
- JSON darf fuer MVP 1 Namen verwenden, muss intern aber auf IDs abgebildet
  werden.
- Versionierung beginnt bei `version = 1`.
- Import/Export von BLCAD-Projektdateien ist erst nach funktionierendem
  Recompute verpflichtend.

## Konsequenzen

- MVP 1 bleibt auf den Geometrienachweis fokussiert.
- Das Dateiformat wird nicht aus einer unfertigen ersten Klassenstruktur
  versteinert.
- Die Spezifikation enthaelt trotzdem genug Struktur, um spaeter gezielt zu
  serialisieren.

## Verworfene Alternativen

### JSON sofort zuerst bauen

Das wuerde schnell sichtbare Dateien erzeugen, beweist aber noch nicht, dass das
CAD-Modell berechnet werden kann.

### Keine Serialisierung in MVP 1 planen

Das waere zu kurz gedacht, weil die Objektstruktur schon jetzt serialisierbar
entworfen werden muss.

