#include "blcad/core/error.hpp"
#include "blcad/core/project_json.hpp"

#include <iostream>
#include <string_view>

namespace {

void print_usage(std::string_view program_name) {
  std::cerr << "Usage: " << program_name << " <input.blcad.project.json>\n";
}

void print_error(const blcad::Error& error) {
  std::cerr << "error[" << blcad::to_string(error.category()) << "] " << error.object_id() << ": "
            << error.message() << '\n';
}

void print_vector(const blcad::Vector3& vector) {
  std::cout << '(' << vector.x << ", " << vector.y << ", " << vector.z << ')';
}

} // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    print_usage(argc > 0 ? argv[0] : "blcad_inspect_project_components");
    return 2;
  }

  auto project = blcad::read_project_json_file(argv[1]);
  if (project.has_error()) {
    print_error(project.error());
    return 1;
  }

  const auto valid_structure = project.value().validate_assembly_structure();
  if (valid_structure.has_error()) {
    print_error(valid_structure.error());
    return 1;
  }

  std::cout << "Project " << project.value().id().value() << " has "
            << project.value().assembly().component_instance_count() << " component instance(s)\n";

  for (const blcad::ComponentInstance& instance : project.value().assembly().component_instances()) {
    std::cout << instance.id().value() << " \"" << instance.name() << "\" -> "
              << instance.referenced_part_document().value()
              << " visibility=" << blcad::to_string(instance.visibility())
              << " suppression=" << blcad::to_string(instance.suppression_state())
              << " grounding=" << blcad::to_string(instance.grounding_state())
              << " translation_mm=";
    print_vector(instance.transform().translation_mm);
    std::cout << " rotation_deg=";
    print_vector(instance.transform().rotation_deg);
    std::cout << '\n';
  }

  return 0;
}
