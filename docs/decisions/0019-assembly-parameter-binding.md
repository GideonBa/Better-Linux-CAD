# 0019: Assembly-Parameter-Binding

Status: akzeptiert fuer den MVP-4-Seed

## Kontext

MVP 4 verlangt bauteiluebergreifende Parametrik: ein gemeinsamer
Assembly-Parameter soll Features in mehreren Part-Dokumenten treiben. Die
Kernfrage war, wie der Wert vom Assembly in die Parts gelangt, ohne die
Dokumentgrenzen aufzuweichen.

## Entscheidung

Ein `AssemblyDocument` besitzt ausschliesslich assembly-skopierte Parameter,
registriert Mitglieds-Parts nur per `DocumentId` und speichert explizite
`ParameterBinding`-Records (Part-Dokument, Part-Parameter,
Assembly-Parameter). Ein Part-Parameter kann hoechstens einmal gebunden
werden.

Die Wertuebertragung ist ein expliziter Aufruf
`apply_bindings_to(PartDocument&)`. Er prueft Mitgliedschaft, Existenz und
Typgleichheit (Laenge/Count) und laeuft pro Binding ueber
`PartDocument::set_parameter_value`. Dadurch nutzt die Propagation exakt den
vorhandenen Invalidierungs- und Recompute-Pfad des Parts.

Assembly-Dokumente werden separat serialisiert
(`blcad.assembly_document.mvp4`); das Part-JSON bleibt unveraendert.

## Alternativen

- Parts halten direkte Referenzen auf Assembly-Parameter: verworfen, weil ein
  Part-Dokument dann nicht mehr alleinstehend validierbar und ladbar waere.
- Automatische Push-Propagation bei jedem `set_parameter_value` im Assembly:
  aufgeschoben; dafuer fehlt ein Projekt-Container, der Assembly und Parts
  gemeinsam besitzt (`docs/file-format.md`).
- Gemeinsamer globaler Parameterspeicher: verworfen, widerspricht dem
  Scope-Modell aus `docs/parameter-model.md`.

## Konsequenzen

- Der Datenfluss Assembly -> Part ist kontrolliert und nachvollziehbar; jede
  Anwendung liefert die betroffenen Graphknoten des Parts zurueck.
- Ein spaeterer Projekt-Container kann `apply_bindings_to` automatisch nach
  Assembly-Aenderungen aufrufen, ohne das Modell zu aendern.
- MVP 5 (Component Instances und Constraints) baut auf demselben
  `AssemblyDocument` auf.
