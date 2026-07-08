# MVP-Plan

Quelle: `zielarchitektur-parametrisches-cad-system.tex`.

## MVP 1: Einzelteilmodellierung

Ziel: ein einzelnes parametrisches Bauteil.

Detaildokument: `docs/mvp-1-specification.md`

- Part-Dokument
- Parameter mit Einheiten
- einfache 2D-Skizze auf Standardebene
- Rechteck, Kreis, Linie
- einfache Masse
- Extrude
- Cut
- STEP-Export

Beispiel: rechteckige Platte mit Breite, Hoehe, Dicke und zentraler Bohrung.

Aktueller Stand: Der Core besitzt die noetigen Datenmodelle bis zum
Recompute-Plan. Ein optionales `blcad_geometry`-Target erzeugt bereits eine
zentrierte Rechteckextrusion als OCCT-Solid und besitzt einen kleinen
`ShapeCache`. `AdditiveExtrude` kann fuer ein einzelnes Rechteckprofil bereits
aus einem Recompute-Plan ausgefuehrt werden; Cut und STEP-Export fehlen noch.

## MVP 2: Sketch auf planarer Flaeche

Ziel: Skizzen auf vorhandenen planaren Flaechen platzieren.

- Flaeche auswaehlen
- Workplane aus Flaeche erzeugen
- Sketch auf Workplane starten
- Cut durch vorhandenen Koerper

## MVP 3: Parametrischer Lochkreis

Ziel: erster aussagekraeftiger parametrischer Feature-Test.

- Lochkreisradius als Parameter
- Lochanzahl als Parameter
- Bohrungsdurchmesser als Parameter
- automatisches Recompute bei Aenderung

## MVP 4: Einfache Assembly-Parameter

Ziel: bauteiluebergreifende Parametrik.

- Assembly-Dokument
- zwei Part-Dokumente in einer Assembly
- gemeinsamer Assembly-Parameter
- beide Parts verwenden denselben Lochkreisparameter

## MVP 5: Einfache Assembly Constraints

Ziel: Bauteile ueber Constraints platzieren.

- Component Instances
- Achse-auf-Achse
- Flaeche-auf-Flaeche
- einfache Schraubenplatzierung
