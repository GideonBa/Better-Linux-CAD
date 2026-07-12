#include "blcad/core/assembly_exchange_graph.hpp"

#include <catch2/catch_test_macros.hpp>

#include <set>
#include <string>

using namespace blcad;

namespace {

AssemblyDocument assembly(const char* id) {
  auto value = AssemblyDocument::create(DocumentId(id), id);
  REQUIRE(value);
  return value.value();
}

ComponentInstance component(const char* id) {
  auto value = ComponentInstance::create(ComponentInstanceId(id), id, DocumentId("part/a%"));
  REQUIRE(value);
  return value.value();
}

SubassemblyInstance occurrence(const char* id, const char* child) {
  auto value = SubassemblyInstance::create(SubassemblyInstanceId(id), id, DocumentId(child));
  REQUIRE(value);
  return value.value();
}

void add_leaf(AssemblyDocument& document, const char* component_id) {
  REQUIRE(document.add_member_part(DocumentId("part/a%")));
  REQUIRE(document.add_component_instance(component(component_id)));
}

} // namespace

TEST_CASE("Assembly exchange generated names percent-encode path separators and reserve root",
          "[core][assembly-exchange-graph]") {
  AssemblyDocument root = assembly("assembly.root");
  REQUIRE(root.add_subassembly_instance(occurrence("a/b", "assembly.slash")));
  REQUIRE(root.add_subassembly_instance(occurrence("a", "assembly.mid")));
  REQUIRE(root.add_subassembly_instance(occurrence("root", "assembly.root_named")));

  AssemblyDocument slash = assembly("assembly.slash");
  add_leaf(slash, "component/x");

  AssemblyDocument mid = assembly("assembly.mid");
  REQUIRE(mid.add_subassembly_instance(occurrence("b", "assembly.nested")));

  AssemblyDocument nested = assembly("assembly.nested");
  add_leaf(nested, "component/x");

  AssemblyDocument root_named = assembly("assembly.root_named");
  add_leaf(root_named, "component%root");

  auto part = PartDocument::create(DocumentId("part/a%"), "EncodedPart");
  REQUIRE(part);
  auto project = Project::create(DocumentId("project.exchange.names"), "ExchangeNames", root);
  REQUIRE(project);
  REQUIRE(project.value().add_child_assembly_document(slash));
  REQUIRE(project.value().add_child_assembly_document(mid));
  REQUIRE(project.value().add_child_assembly_document(nested));
  REQUIRE(project.value().add_child_assembly_document(root_named));
  REQUIRE(project.value().add_part_document(part.value()));

  auto graph = AssemblyExchangeGraph::build(project.value());
  REQUIRE(graph);

  std::set<std::string> assembly_names;
  for (const auto& occurrence_value : graph.value().assembly_occurrences()) {
    REQUIRE(assembly_names.insert(occurrence_value.product_name).second);
  }
  CHECK(assembly_names.contains("blcad:assembly-occurrence:root"));
  CHECK(assembly_names.contains("blcad:assembly-occurrence:a%2Fb"));
  CHECK(assembly_names.contains("blcad:assembly-occurrence:a/b"));
  CHECK(assembly_names.contains("blcad:assembly-occurrence:%72oot"));

  std::set<std::string> component_names;
  for (const auto& occurrence_value : graph.value().component_occurrences()) {
    REQUIRE(component_names.insert(occurrence_value.occurrence_name).second);
  }
  CHECK(component_names.contains("blcad:component-occurrence:a%2Fb/component%2Fx"));
  CHECK(component_names.contains("blcad:component-occurrence:a/b/component%2Fx"));
  CHECK(component_names.contains("blcad:component-occurrence:%72oot/component%25root"));

  REQUIRE(graph.value().part_product_definition_count() == 1U);
  CHECK(graph.value().part_product_definitions().front().product_name ==
        "blcad:part-definition:part%2Fa%25");
}
