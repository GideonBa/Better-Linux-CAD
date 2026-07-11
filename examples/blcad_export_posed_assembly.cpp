#include "blcad/core/assembly_constraint_graph.hpp"
#include "blcad/core/error.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/geometry/assembly_rigid_body_solver.hpp"
#include "blcad/geometry/assembly_step_exporter.hpp"

#include <algorithm>
#include <cstddef>
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

  std::size_t solved_component_count = 0U;
  std::size_t applied_proposal_count = 0U;
  const auto groups = graph.value().connected_components();
  const auto group = std::find_if(groups.begin(), groups.end(), [](const auto& candidate) {
    return candidate.size() > 1U;
  });
  if (group != groups.end()) {
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
    solved_component_count = group->size();
    applied_proposal_count = applied.value();
  }

  const blcad::geometry::AssemblyStepExporter exporter;
  const auto written = exporter.write_step(project.value(), argv[2]);
  if (written.has_error()) {
    print_error(written.error());
    return 1;
  }

  std::cout << "Solved " << solved_component_count << " component(s), applied "
            << applied_proposal_count << " transform proposal(s), recomputed "
            << written.value().recomputed_part_count << " part(s), and exported "
            << written.value().exported_component_count << " posed component(s) to " << argv[2]
            << " (" << written.value().written_bytes << " bytes)\n";
  return 0;
}
