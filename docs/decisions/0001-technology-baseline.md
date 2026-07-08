# 0001: Technische Startbasis

Status: akzeptiert fuer Projektstart

## Kontext

Die Zielarchitektur verlangt einen eigenen parametrischen CAD-Core mit OCCT als
Geometriekernel, spaeterer Linux-GUI und langfristiger Erweiterbarkeit.

## Entscheidung

- C++20 als Hauptsprache fuer Core, OCCT-Anbindung und spaetere GUI.
- CMake als Buildsystem, Ninja als Standardgenerator.
- OCCT als BRep-, Boolean-, Extrude-, STEP- und Visualisierungsbasis.
- Qt 6 als vorbereitete GUI-Basis.
- Eigen fuer Vektor- und Matrixoperationen.
- nlohmann-json fuer fruehe Dateiformat-Prototypen.
- Catch2 fuer spaetere Unit-Tests.

## Konsequenzen

- Das Projekt kann plattformnah und performant mit OCCT arbeiten.
- Die erste Implementierung sollte im Core beginnen, nicht in der UI.
- Die CMake-Struktur existiert schon, erzeugt aber noch keine Targets.

