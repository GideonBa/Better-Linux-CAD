# MVP 1 Sketch-Datenmodell

Status: reines Datenmodell mit PartDocument-Integration, keine Geometrieerzeugung

Dieses Dokument beschreibt den aktuellen Stand von `DatumPlane`, `Sketch`,
`RectangleProfile` und `CircleProfile`.

Der Stand ist bewusst begrenzt:

- keine Features
- keine Extrusion
- kein Cut
- kein Recompute
- kein OCCT
- keine GUI

## Ziel

Das Sketch-Datenmodell bildet die Konstruktionsabsicht fuer die erste
Referenzplatte ab:

- eine feste XY-Arbeitsebene
- ein zentriertes Rechteck fuer die Grundplatte
- ein zentrierter Kreis fuer die spaetere Bohrung
- Referenzen auf Parameter, aber noch keine Auswertung zu OCCT-Geometrie
- Dokumentvalidierung fuer Workplane- und Parameterreferenzen

## Typen

### `Point2`

Ein einfacher 2D-Punkt fuer Skizzenkoordinaten.

```text
Point2
  x
  y
```

MVP-1-Profile verwenden aktuell `center = (0, 0)`.

### `Point3`

Ein einfacher 3D-Punkt fuer Datum-Plane-Urspruenge.

```text
Point3
  x
  y
  z
```

### `Vector3`

Ein einfacher 3D-Vektor fuer Achsen und Normalen.

```text
Vector3
  x
  y
  z
```

## `DatumPlane`

MVP 1 implementiert nur die Standardebene `XY`.

```text
DatumPlane XY
  id = "datum.xy"
  name = "XY"
  origin = (0, 0, 0)
  x_axis = (1, 0, 0)
  y_axis = (0, 1, 0)
  normal = (0, 0, 1)
```

Validierung:

- Datum-Plane-ID darf nicht leer sein.
- Name darf nicht leer sein.

Noch nicht enthalten:

- frei definierte Ebenen
- Offset-Ebenen
- Ebenen aus Flaechenreferenzen
- Validierung von Orthogonalitaet oder Normallaenge

## `RectangleProfile`

Das Rechteckprofil beschreibt die spaetere Grundplatte.

```text
RectangleProfile
  id = "profile.base_rectangle"
  center = (0, 0)
  width_parameter = "part.width"
  height_parameter = "part.height"
```

Validierung:

- Profil-ID darf nicht leer sein.
- Breitenparameter-ID darf nicht leer sein.
- Hoehenparameter-ID darf nicht leer sein.

Das Profil selbst prueft nicht, ob die referenzierten Parameter im
`PartDocument` existieren. Diese Pruefung geschieht beim Hinzufuegen eines
Sketches zu `PartDocument`.

## `CircleProfile`

Das Kreisprofil beschreibt die spaetere zentrale Bohrung.

```text
CircleProfile
  id = "profile.center_hole"
  center = (0, 0)
  diameter_parameter = "part.hole_diameter"
```

Validierung:

- Profil-ID darf nicht leer sein.
- Durchmesserparameter-ID darf nicht leer sein.

Das Profil selbst prueft nicht, ob der Durchmesserparameter im `PartDocument`
existiert. Diese Pruefung geschieht beim Hinzufuegen eines Sketches zu
`PartDocument`.

Die Groessenvalidierung `hole_diameter < min(width, height)` braucht Zugriff auf
Parameterwerte und folgt spaeter.

## `Sketch`

Eine Skizze speichert ID, Name, Workplane-Referenz und Profile.

```text
Sketch
  id = "sketch.base"
  name = "Sketch_BaseRectangle"
  workplane = "datum.xy"
  rectangle_profiles
  circle_profiles
```

Validierung:

- Sketch-ID darf nicht leer sein.
- Name darf nicht leer sein.
- Workplane-ID darf nicht leer sein.
- Profil-IDs muessen innerhalb einer Skizze eindeutig sein.
- Profil-IDs muessen auch zwischen Rechteck- und Kreisprofilen eindeutig sein.

Noch nicht enthalten:

- allgemeine Sketch-Constraints
- Linien, Boegen oder frei editierbare Entitaeten
- automatische Profilableitung
- Parameterwertauswertung
- OCCT-Konvertierung

## Integration in `PartDocument`

`PartDocument` speichert jetzt:

- Parameter
- Datum-Planes
- Sketches

Beim Hinzufuegen eines Sketches wird validiert:

- Die `workplane` des Sketches muss als Datum-Plane im Dokument existieren.
- Jeder `RectangleProfile.width_parameter` muss im Dokument existieren.
- Jeder `RectangleProfile.height_parameter` muss im Dokument existieren.
- Jeder `CircleProfile.diameter_parameter` muss im Dokument existieren.
- Sketch-IDs muessen im Dokument eindeutig sein.

Diese Pruefung stellt sicher, dass eine Skizze nicht isoliert auf nicht
existierende Dokumentobjekte zeigt.

## Testabdeckung

Aktuelle Tests pruefen:

- XY-Ebene mit Ursprung, X-Achse, Y-Achse und Normalenrichtung
- Datum-Plane-Validierung fuer ID und Name
- Rechteckprofil mit Breiten- und Hoehenparameter
- Kreisprofil mit Durchmesserparameter
- Profilvalidierung fuer fehlende IDs und Parameterreferenzen
- Sketch-Validierung fuer ID, Name und Workplane
- Hinzufuegen von Rechteck- und Kreisprofilen
- eindeutige Profil-IDs innerhalb einer Skizze
- Datum-Planes im `PartDocument`
- Sketches im `PartDocument`
- fehlende Workplanes beim Hinzufuegen von Sketches
- fehlende Breiten-, Hoehen- und Durchmesserparameter beim Hinzufuegen von
  Sketches

## Naechster Integrationsschritt

Die naechsten Integrationsschritte sind erledigt: Features sind als reine
Datenmodelle aufgenommen, und `PartDocument` erzeugt Dependency-Graph-Knoten
und -Kanten aus Parameter-, Sketch- und Feature-Referenzen. Der
Invalidierungsstatus markiert betroffene Sketches und Features nach einer
Parameteraenderung. Der Recompute-Plan listet die betroffenen `dirty`-Knoten
topologisch geordnet auf.

Der naechste sinnvolle Schritt liegt nun oberhalb dieser Skizzen-Schicht:

- Kreisprofil fuer `SubtractiveExtrude` auswerten
- Zielshape aus dem bestehenden Geometry-`ShapeCache` lesen
- zentrischen Cut als naechsten Geometry-Schritt erzeugen
- OCCT weiter hinter der Adaptergrenze halten
- noch keine GUI bauen
