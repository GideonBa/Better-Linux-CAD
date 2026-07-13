#include "blcad/core/error.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/geometry/body_result_inspector.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/shape_cache.hpp"
#include "blcad/geometry/step_exporter.hpp"

#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

namespace {

void print_usage(std::string_view program_name) {
  std::cerr << "Usage: " << program_name
            << " <input.blcad.project.json> <assembly-parameter-id> <value> <output-dir>\n";
}

void print_error(const blcad::Error& error) {
  std::cerr << "error[" << blcad::to_string(error.category()) << "] " << error.object_id() << ": "
            << error.message() << '\n';
}

blcad::Result<blcad::Quantity> quantity_from_parameter(const blcad::Parameter& parameter,
                                                       double value) {
  if (parameter.type() == blcad::ParameterType::Count) {
    return blcad::Quantity::count(value, parameter.id().value());
  }
  return blcad::Quantity::length_mm(value, parameter.id().value());
}

std::filesystem::path output_path_for_part(const std::filesystem::path& output_dir,
                                           const blcad::PartDocument& part) {
  return output_dir / (part.id().value() + ".step");
}

} // namespace

int main(int argc, char** argv) {
  if (argc != 5) {
    print_usage(argc > 0 ? argv[0] : "blcad_export_project");
    return 2;
  }

  auto project = blcad::read_project_json_file(argv[1]);
  if (project.has_error()) {
    print_error(project.error());
    return 1;
  }

  const blcad::ParameterId assembly_parameter_id(argv[2]);
  const blcad::Parameter* assembly_parameter =
      project.value().assembly().find_parameter(assembly_parameter_id);
  if (assembly_parameter == nullptr) {
    print_error(blcad::Error::validation(assembly_parameter_id.value(),
                                         "assembly parameter must exist in project"));
    return 1;
  }

  double raw_value = 0.0;
  try {
    raw_value = std::stod(argv[3]);
  } catch (const std::exception&) {
    print_error(blcad::Error::validation(argv[3], "assembly parameter value must be numeric"));
    return 1;
  }

  auto quantity = quantity_from_parameter(*assembly_parameter, raw_value);
  if (quantity.has_error()) {
    print_error(quantity.error());
    return 1;
  }

  auto update =
      project.value().set_assembly_parameter_value(assembly_parameter_id, quantity.value());
  if (update.has_error()) {
    print_error(update.error());
    return 1;
  }

  const std::filesystem::path output_dir(argv[4]);
  std::error_code create_error;
  std::filesystem::create_directories(output_dir, create_error);
  if (create_error) {
    print_error(blcad::Error::validation(output_dir.string(),
                                         "could not create project export output directory"));
    return 1;
  }

  const blcad::geometry::GeometryRecomputeExecutor executor;
  const blcad::geometry::StepExporter exporter;
  for (const blcad::PartDocument& part : project.value().part_documents()) {
    auto cache = blcad::geometry::ShapeCache::create(
        blcad::ShapeCacheId("shape_cache.project_export." + part.id().value()));
    if (cache.has_error()) {
      print_error(cache.error());
      return 1;
    }

    const auto summary = executor.execute_document(part, cache.value());
    if (summary.has_error()) {
      print_error(summary.error());
      return 1;
    }

    const auto final_shape =
        blcad::geometry::BodyResultInspector{}.resolve_compatible_final_shape(part, cache.value());
    if (final_shape.has_error()) {
      print_error(final_shape.error());
      return 1;
    }

    const auto output_path = output_path_for_part(output_dir, part);
    const auto written = exporter.write_step(final_shape.value(), output_path.string());
    if (written.has_error()) {
      print_error(written.error());
      return 1;
    }
    std::cout << "Exported " << part.id().value() << " to " << output_path << " ("
              << written.value() << " bytes)\n";
  }

  std::cout << "Updated " << update.value().updated_part_count()
            << " project part(s) from assembly parameter " << assembly_parameter_id.value() << '\n';
  return 0;
}
