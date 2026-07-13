from pathlib import Path

path = Path("tests/core/assembly_generic_relationship_tests.cpp")
text = path.read_text(encoding="utf-8")

text = text.replace(
    'TEST_CASE("Generic assembly relationship intent is persistent-only before Block 39",\n'
    '          "[core][assembly-generic-relationship-intent]") {',
    'TEST_CASE("Generic assembly relationship intent participates in graphs after Block 39",\n'
    '          "[core][assembly-generic-relationship-intent]") {',
    1,
)
old_local = '''  CHECK(graph.value().edge_count() == 1U);
  REQUIRE(graph.value().edges().size() == 1U);
  CHECK(graph.value().edges().front().constraint() == AssemblyConstraintId("constraint.mate"));
'''
new_local = '''  CHECK(graph.value().edge_count() == 3U);
  REQUIRE(graph.value().edges().size() == 3U);
  CHECK(graph.value().edges()[0].constraint() == AssemblyConstraintId("constraint.coincident"));
  CHECK(graph.value().edges()[1].constraint() == AssemblyConstraintId("constraint.mate"));
  CHECK(graph.value().edges()[2].constraint() == AssemblyConstraintId("constraint.perpendicular"));
'''
if old_local not in text:
    raise RuntimeError("local generic relationship graph assertion marker not found")
text = text.replace(old_local, new_local, 1)

text = text.replace(
    'TEST_CASE("Generic Project relationships preserve rooted identity and stay out of current graphs",\n'
    '          "[core][assembly-generic-relationship-intent]") {',
    'TEST_CASE("Generic Project relationships preserve identity and participate in Block 39 graphs",\n'
    '          "[core][assembly-generic-relationship-intent]") {',
    1,
)
old_solve = '''  CHECK(solve_graph.value().relationship_count() == 0U);
  CHECK(solve_graph.value().authority_count() == 0U);
  CHECK(solve_graph.value().incidence_count() == 0U);
  CHECK(solve_graph.value().solve_group_count() == 0U);
'''
new_solve = '''  CHECK(solve_graph.value().relationship_count() == 3U);
  CHECK(solve_graph.value().authority_count() == 2U);
  CHECK(solve_graph.value().incidence_count() == 5U);
  CHECK(solve_graph.value().solve_group_count() == 1U);
'''
if old_solve not in text:
    raise RuntimeError("cross-hierarchy generic solve graph assertion marker not found")
text = text.replace(old_solve, new_solve, 1)
old_motion = '''  CHECK(motion_graph.value().relationship_count() == 0U);
  CHECK(motion_graph.value().authority_count() == 0U);
  CHECK(motion_graph.value().incidence_count() == 0U);
  CHECK(motion_graph.value().motion_group_count() == 0U);
'''
new_motion = '''  CHECK(motion_graph.value().relationship_count() == 3U);
  CHECK(motion_graph.value().authority_count() == 2U);
  CHECK(motion_graph.value().incidence_count() == 5U);
  CHECK(motion_graph.value().motion_group_count() == 1U);
'''
if old_motion not in text:
    raise RuntimeError("cross-hierarchy generic motion graph assertion marker not found")
text = text.replace(old_motion, new_motion, 1)

path.write_text(text, encoding="utf-8")
print("Block 39 Core graph participation tests normalized")
