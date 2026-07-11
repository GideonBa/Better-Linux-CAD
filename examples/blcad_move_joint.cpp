#include "blcad/core/error.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/core/quantity.hpp"
#include "blcad/geometry/assembly_joint_motion_solver.hpp"

#include <exception>
#include <iostream>
#include <string>
#include <string_view>

namespace {

void print_usage(std::string_view program_name) {
  std::cerr << "Usage: " << program_name
            << " <input.blcad.project.json> <joint-id> <angle-deg> <output.blcad.project.json>\n";
}

void print_error(const blcad::Error& error) {
  std::cerr << "error[" << blcad::to_string(error.category()) << "] " << error.object_id()
            << ": " << error.message() << '\n';
}

} // namespace

int main(int argc, char** argv) {
  if (argc != 5) {
    print_usage(argc > 0 ? argv[0] : "blcad_move_joint");
    return 2;
  }

  auto project = blcad::read_project_json_file(argv[1]);
  if (project.has_error()) {
    print_error(project.error());
    return 1;
  }

  double requested_degrees = 0.0;
  try {
    requested_degrees = std::stod(argv[3]);
  } catch (const std::exception&) {
    print_error(blcad::Error::validation(argv[3], "joint motion angle must be numeric"));
    return 1;
  }

  const blcad::AssemblyJointId joint_id(argv[2]);
  auto requested = blcad::Quantity::angle_deg(requested_degrees, joint_id.value());
  if (requested.has_error()) {
    print_error(requested.error());
    return 1;
  }

  const blcad::geometry::AssemblyJointMotionSolver solver;
  auto motion = solver.move(project.value(), joint_id, requested.value());
  if (motion.has_error()) {
    print_error(motion.error());
    return 1;
  }
  if (!motion.value().converged()) {
    print_error(blcad::Error::validation(joint_id.value(),
                                         "joint motion solve did not converge"));
    return 1;
  }

  const blcad::geometry::AssemblyJointMotionResultApplier applier;
  auto applied = applier.apply(project.value(), motion.value());
  if (applied.has_error()) {
    print_error(applied.error());
    return 1;
  }

  const auto written = blcad::write_project_json_file(project.value(), argv[4]);
  if (written.has_error()) {
    print_error(written.error());
    return 1;
  }

  std::cout << "Moved " << joint_id.value() << " from " << motion.value().source_coordinate_deg
            << " deg to " << motion.value().requested_coordinate_deg << " deg, applied "
            << applied.value() << " transform proposal(s), and wrote " << written.value()
            << " bytes to " << argv[4] << '\n';
  return 0;
}
