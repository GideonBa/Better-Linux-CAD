#include "assembly_occt_rigid_transform.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/geometry/body_result_inspector.hpp"
#include "blcad/geometry/step_import_topology_context.hpp"
#include "occt_failure_message.hpp"
#include "physical_property_math.hpp"

#include <Standard_Failure.hxx>
#include <gp_Pnt.hxx>

#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace blcad::geometry {
namespace {
Error error(std::string id, std::string message) {
  return Error::geometry(std::move(id), std::move(message));
}
double number(long double value) {
  const double result = static_cast<double>(value);
  if (!std::isfinite(value) || !std::isfinite(result) || (value != 0 && result == 0))
    throw std::runtime_error("Posed physical property exceeds the representable numeric range");
  return result;
}
Point3 pose_point(Point3 value, const gp_Trsf& pose) {
  const gp_Pnt point = gp_Pnt(value.x, value.y, value.z).Transformed(pose);
  return {number(point.X()), number(point.Y()), number(point.Z())};
}
std::array<double, 9> rotate_tensor(const std::array<double, 9>& value, const gp_Trsf& pose) {
  const auto r = pose.VectorialPart();
  std::array<double, 9> result{};
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j) {
      long double sum = 0;
      for (int k = 0; k < 3; ++k)
        for (int l = 0; l < 3; ++l)
          sum += static_cast<long double>(r.Value(i + 1, k + 1)) * value[3 * k + l] *
                 r.Value(j + 1, l + 1);
      result[3 * i + j] = number(sum);
    }
  return result;
}
Result<BodyPhysicalProperties> pose_body(BodyPhysicalProperties value, const gp_Trsf& pose) {
  if (value.geometry.volume) {
    auto& volume = *value.geometry.volume;
    volume.centroid_mm = pose_point(volume.centroid_mm, pose);
    volume.central_inertia_mm5 = rotate_tensor(volume.central_inertia_mm5, pose);
  }
  if (value.mass) {
    const auto& mass = *value.mass;
    auto posed = detail::physical_mass_properties(
        detail::WeightedPhysicalProperties{mass.mass_kg, pose_point(mass.centroid_mm, pose),
                                           rotate_tensor(mass.central_inertia_kg_mm2, pose)});
    if (!posed)
      return Result<BodyPhysicalProperties>::failure(posed.error());
    value.mass = std::move(posed.value());
  }
  return Result<BodyPhysicalProperties>::success(std::move(value));
}
} // namespace

ProjectPhysicalProperties::ProjectPhysicalProperties(ProjectPhysicalPropertiesData data,
                                                     std::string source,
                                                     std::vector<PartSnapshot> parts)
    : data_(std::move(data)), source_(std::move(source)), parts_(std::move(parts)) {}

Result<std::size_t>
ProjectPhysicalProperties::validate_current(const Project& project,
                                            StepImportTopologyContext* imports) const {
  const auto source = serialize_project_to_json(project);
  if (!source)
    return Result<std::size_t>::failure(source.error());
  if (source.value() != source_)
    return Result<std::size_t>::failure(Error::dependency(
        project.id().value(),
        "Physical properties do not match the current Project source and occurrence poses"));
  for (const auto& snapshot : parts_) {
    const auto* part = project.find_part_document(snapshot.document);
    if (!part)
      return Result<std::size_t>::failure(
          Error::dependency(snapshot.document.value(), "Physical Part source is missing"));
    const auto valid = snapshot.properties.validate_current(*part, imports);
    if (!valid)
      return valid;
  }
  if (imports)
    return imports->validate_sources();
  return Result<std::size_t>::success(parts_.size());
}

Result<ProjectPhysicalProperties>
BodyResultInspector::inspect_physical_properties(const Project& project,
                                                 StepImportTopologyContext* imports) const {
  return inspect_project_physical_properties(project, nullptr, imports);
}
Result<ProjectPhysicalProperties>
BodyResultInspector::inspect_physical_properties(const Project& project,
                                                 const ProjectPhysicalProperties& previous,
                                                 StepImportTopologyContext* imports) const {
  return inspect_project_physical_properties(project, &previous, imports);
}
Result<ProjectPhysicalProperties>
BodyResultInspector::inspect_project_physical_properties(const Project& project,
                                                         const ProjectPhysicalProperties* previous,
                                                         StepImportTopologyContext* imports) const {
  try {
    const auto structure = project.validate_assembly_structure();
    if (!structure)
      return Result<ProjectPhysicalProperties>::failure(structure.error());
    const auto source = serialize_project_to_json(project);
    if (!source)
      return Result<ProjectPhysicalProperties>::failure(source.error());
    std::unordered_set<std::string> owned;
    for (const auto& part : project.part_documents())
      if (!owned.insert(part.id().value()).second)
        return Result<ProjectPhysicalProperties>::failure(Error::validation(
            part.id().value(), "Physical Project requires unique Part definitions"));
    const auto leaves = AssemblyLeafOccurrenceResolver{}.resolve(
        project, AssemblyLeafParticipation::PhysicalActive);
    if (!leaves)
      return Result<ProjectPhysicalProperties>::failure(leaves.error());
    std::vector<ProjectPhysicalProperties::PartSnapshot> parts;
    std::map<std::string, std::size_t> by_id;
    ProjectPhysicalPropertiesData data;
    std::vector<BodyPhysicalProperties> flattened;
    for (const auto& leaf : leaves.value()) {
      auto found = by_id.find(leaf.referenced_part_document.value());
      if (found == by_id.end()) {
        const auto* part = project.find_part_document(leaf.referenced_part_document);
        if (!part)
          return Result<ProjectPhysicalProperties>::failure(
              error(leaf.referenced_part_document.value(), "Physical Part definition is missing"));
        const auto* old = previous ? [&]() -> const PartPhysicalProperties* {
          for (const auto& snapshot : previous->parts_)
            if (snapshot.document == part->id())
              return &snapshot.properties;
          return nullptr;
        }()
            : nullptr;
        if (previous && !old)
          return Result<ProjectPhysicalProperties>::failure(Error::dependency(
              part->id().value(), "Physical reuse requires a matching integrated Part snapshot"));
        auto properties = old ? inspect_physical_properties(*part, *old, imports)
                              : inspect_physical_properties(*part, imports);
        if (!properties)
          return Result<ProjectPhysicalProperties>::failure(properties.error());
        found = by_id.emplace(part->id().value(), parts.size()).first;
        parts.push_back({part->id(), std::move(properties.value())});
      }
      const auto* assembly = project.find_assembly_document(leaf.assembly_document);
      const auto* component =
          assembly ? assembly->find_component_instance(leaf.component_instance) : nullptr;
      if (!component)
        return Result<ProjectPhysicalProperties>::failure(
            error(leaf.component_instance.value(), "Physical occurrence identity is unresolved"));
      gp_Trsf pose;
      for (const auto& transform : leaf.transforms_inner_to_outer)
        pose = detail::to_occt_rigid_transform(transform) * pose;
      PhysicalOccurrenceProperties occurrence{leaf, component->name(), {}};
      for (const auto& body : parts[found->second].properties.data().bodies) {
        auto posed = pose_body(body, pose);
        if (!posed)
          return Result<ProjectPhysicalProperties>::failure(posed.error());
        flattened.push_back(posed.value());
        occurrence.bodies.push_back(std::move(posed.value()));
      }
      data.occurrences.push_back(std::move(occurrence));
    }
    auto totals = detail::aggregate_physical_bodies(std::move(flattened));
    if (!totals)
      return Result<ProjectPhysicalProperties>::failure(totals.error());
    static_cast<PhysicalPropertiesTotals&>(data) =
        static_cast<const PhysicalPropertiesTotals&>(totals.value());
    data.computed_part_count = previous ? 0 : parts.size();
    data.reused_part_count = previous ? parts.size() : 0;
    ProjectPhysicalProperties result(std::move(data), source.value(), std::move(parts));
    const auto current = result.validate_current(project, imports);
    if (!current)
      return Result<ProjectPhysicalProperties>::failure(current.error());
    return Result<ProjectPhysicalProperties>::success(std::move(result));
  } catch (const Standard_Failure& e) {
    return Result<ProjectPhysicalProperties>::failure(
        error(project.id().value(), standard_failure_message(e)));
  } catch (const std::exception& e) {
    return Result<ProjectPhysicalProperties>::failure(error(project.id().value(), e.what()));
  }
}
} // namespace blcad::geometry
