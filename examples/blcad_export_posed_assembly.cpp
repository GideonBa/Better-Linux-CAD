#include "blcad/core/assembly_constraint_graph.hpp"
#include "blcad/core/error.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/geometry/assembly_rigid_body_solver.hpp"
#include "blcad/geometry/assembly_step_exporter.hpp"

#include <algorithm>
#include <iostream>
#include <string_view>
#include <vector>

namespace {

void print_usage(std::string_view program_name) {
  std::cerr << "Usage: " << program_name << " <input.blcad.project.json> <output.step>\n";
}

void print_error(const blcad::Error& error) {
  std::cerr << "error[" << blcad::to_string(error.category()) << "] " << error.object_id() << ": "
            << error.message() << '\n';
}

} // namespace

int main(int argc, char** argv) {
  if (argc != 3) {
    print_usage(argc > 0 ? argv[0] : "blcad_export_posed_assembly");
    return 2;
  }

  auto project = blcad::read_project_json_file(argv[1]);
  if (project.has_error()) {
    print_error(project.error());
    return 1;
  }

  const auto graph = blcad::AssemblyConstraintGraph::build(project.value().assembly());
  if (graph.has_error()) {
    print_error(graph.error());
    return 1;
  }

  const auto groups = graph.value().connected_components();
  const auto group = std::find_if(groups.begin(), groups.end(), [](const auto& candidate) {
    return candidate.size() > 1U;
  });
  if (group == groups.end()) {
    print_error(blcad::Error::validation(
        project.value().assembly().id().value(),
        "posed assembly export example requires one connected multi-component solve group"));
    return 1;
  }

  const blcad::geometry::AssemblyRigidBodySolver solver;
  const auto solved = solver.solve(project.value(), *group);
  if (solved.has_error()) {
    print_error(solved.error());
    return 1;
  }
  if (!solved.value().converged()) {
    print_error(blcad::Error::geometry(project.value().assembly().id().value(),
                                       "assembly solve did not converge before posed export"));
    return 1;
  }

  const blcad::geometry::AssemblySolveResultApplier applier;
  const auto applied = applier.apply(project.value(), solved.value());
  if (applied.has_error()) {
    print_error(applied.error());
    return 1;
  }

  const blcad::geometry::AssemblyStepExporter exporter;
  const auto written = exporter.write_step(project.value(), argv[2]);
  if (written.has_error()) {
    print_error(written.error());
    return 1;
  }

  std::cout << "Solved " << group->size() << " component(s), applied " << applied.value()
            << " transform proposal(s), recomputed " << written.value().recomputed_part_count
            << " part(s), and exported " << written.value().exported_component_count
            << " posed component(s) to " << argv[2] << " (" << written.value().written_bytes
            << " bytes)\n";
  return 0;
}
