# 0003: Einheiten und Laengenwerte

Status: akzeptiert fuer MVP 1

## Kontext

MVP 1 benoetigt nur Laengen in Millimetern. Trotzdem ist Parametrik ein
Kernziel. Das Einheitenmodell darf deshalb nicht aus losen `double`-Werten
bestehen, die spaeter schwer zu migrieren sind.

Die erste Referenzplatte verwendet:

```text
width         = 120 mm
height        = 80 mm
thickness     = 8 mm
hole_diameter = 20 mm
```

## Entscheidung

BLCAD verwendet fuer Laengen eine `Quantity`-artige Struktur mit internem Wert
in Millimetern.

Fuer MVP 1 gilt:

- unterstuetzter Typ: `length`
- unterstuetzte Einheit: `mm`
- interner Zahlenwert: `double`
- interne Basiseinheit: Millimeter

Ein Parameter speichert weiterhin Typ und Einheit:

```text
Parameter
  type = "length"
  value_mm = 120.0
  display_unit = "mm"
```

## Regeln

- Berechnungen im Core verwenden Millimeter.
- UI- oder Dateiformatnamen wie `mm` sind Anzeige- und Eingabeinformationen.
- Fuer MVP 1 sind Umrechnungen zwischen Einheiten nicht erforderlich.
- Negative und nullwertige Laengen sind fuer Platte, Bohrung und Extrusion
  ungueltig.
- Geometrieadapter erhalten bereits normalisierte Millimeterwerte.

## Konsequenzen

- Der erste Code bleibt einfach.
- Spaetere Einheiten wie `cm`, `m`, `inch` oder Formelauswertung koennen
  nachgeruestet werden.
- Tests koennen direkt mit Millimeterwerten arbeiten.
- Es gibt keine versteckten einheitenlosen Laengen im Core.

## Verworfene Alternativen

### Direkt `double`

Ein einzelner `double` waere minimal, verliert aber die Bedeutung des Werts.
Das ist fuer ein parametrisches CAD-System zu kurz gedacht.

### Vollstaendige Einheitenbibliothek

Eine vollstaendige Einheitenbibliothek ist fuer MVP 1 zu viel. Sie wuerde den
ersten Geometriepfad verlangsamen, ohne den MVP-Nachweis wesentlich zu staerken.

