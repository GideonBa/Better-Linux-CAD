#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/body_result_inspector.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/step_import_topology_context.hpp"
#include "material_geometry_source.hpp"
#include "physical_property_math.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace blcad::geometry {
namespace {
Error error(std::string id, std::string message) {
  return Error::geometry(std::move(id), std::move(message));
}
Result<BodyPhysicalProperties> assign_mass(BodyPhysicalProperties result,
                                           const EngineeringMaterial* material) {
  result.material = material ? std::optional(material->key()) : std::nullopt;
  result.mass.reset();
  result.mass_status = BodyPhysicalMassStatus::NotSolid;
  if (result.kind == BodyKind::Surface)
    return Result<BodyPhysicalProperties>::success(std::move(result));
  result.mass_status = material ? BodyPhysicalMassStatus::MissingDensity
                                : BodyPhysicalMassStatus::UnassignedMaterial;
  if (material && material->properties().density) {
    auto mass = detail::physical_mass_properties(*result.geometry.volume,
                                                 material->properties().density->value);
    if (!mass)
      return Result<BodyPhysicalProperties>::failure(mass.error());
    result.mass = std::move(mass.value());
    result.mass_status = BodyPhysicalMassStatus::Known;
  }
  return Result<BodyPhysicalProperties>::success(std::move(result));
}
Result<BodyPhysicalProperties> body_properties(const GeometryShape& shape, const Body* body,
                                               const EngineeringMaterial* material) {
  auto measured = inspect_shape_physical_properties(shape);
  if (!measured)
    return Result<BodyPhysicalProperties>::failure(measured.error());
  BodyPhysicalProperties result;
  result.geometry = std::move(measured.value());
  result.name = body ? body->name() : "Part result";
  result.kind = body ? body->kind() : result.geometry.volume ? BodyKind::Solid : BodyKind::Surface;
  if (body) {
    result.body = body->id();
    result.visibility = body->visibility();
  }
  if (result.kind == BodyKind::Surface) {
    if (result.geometry.volume || result.geometry.area_mm2 <= 0)
      return Result<BodyPhysicalProperties>::failure(
          error(result.name, "Surface Body requires surface geometry without solid volume"));
    return assign_mass(std::move(result), material);
  }
  if (!result.geometry.volume || result.geometry.free_face_count != 0)
    return Result<BodyPhysicalProperties>::failure(
        error(result.name, "Solid Body requires closed solids without free faces"));
  return assign_mass(std::move(result), material);
}

} // namespace

PartPhysicalProperties::PartPhysicalProperties(PartPhysicalPropertiesData data, std::string source,
                                               std::uint64_t generation)
    : data_(std::move(data)), source_(std::move(source)), generation_(generation) {}

Result<std::size_t>
PartPhysicalProperties::validate_current(const PartDocument& document,
                                         StepImportTopologyContext* imports) const {
  const auto source = serialize_part_document_to_json(document);
  if (!source)
    return Result<std::size_t>::failure(source.error());
  if (source.value() != source_ || document.executable_generation() != generation_)
    return Result<std::size_t>::failure(Error::dependency(
        document.id().value(),
        "Physical properties do not match the current Part source and lifetime generation"));
  const auto observed = observe_imported_part_sources(document, imports);
  if (!observed)
    return observed;
  if (imports)
    return imports->validate_sources();
  return Result<std::size_t>::success(0);
}

Result<PartPhysicalProperties>
BodyResultInspector::inspect_physical_properties(const PartDocument& document,
                                                 const PartPhysicalProperties& previous,
                                                 StepImportTopologyContext* imports) const {
  const auto source = serialize_part_document_to_json(document);
  if (!source)
    return Result<PartPhysicalProperties>::failure(source.error());
  const auto before = detail::material_independent_geometry_source(previous.source_);
  const auto after = detail::material_independent_geometry_source(source.value());
  if (!before || !after || before.value() != after.value() ||
      document.executable_generation() != previous.generation_)
    return Result<PartPhysicalProperties>::failure(Error::dependency(
        document.id().value(),
        "Physical reuse requires identical geometry inputs and executable lifetime"));
  std::vector<BodyPhysicalProperties> bodies;
  for (const auto& old : previous.data_.bodies) {
    const auto* material =
        old.body ? document.effective_material_for_body(*old.body)
        : document.default_material()
            ? document.engineering_resources().find_material(*document.default_material())
            : nullptr;
    auto row = assign_mass(old, material);
    if (!row)
      return Result<PartPhysicalProperties>::failure(row.error());
    bodies.push_back(std::move(row.value()));
  }
  auto data = detail::aggregate_physical_bodies(std::move(bodies));
  if (!data)
    return Result<PartPhysicalProperties>::failure(data.error());
  data.value().geometry_reused = true;
  PartPhysicalProperties result(std::move(data.value()), source.value(),
                                document.executable_generation());
  const auto fresh = result.validate_current(document, imports);
  if (!fresh)
    return Result<PartPhysicalProperties>::failure(fresh.error());
  return Result<PartPhysicalProperties>::success(std::move(result));
}

Result<PartPhysicalProperties>
BodyResultInspector::inspect_physical_properties(const PartDocument& document,
                                                 StepImportTopologyContext* imports) const {
  try {
    const auto source = serialize_part_document_to_json(document);
    if (!source)
      return Result<PartPhysicalProperties>::failure(source.error());
    auto cache = ShapeCache::create(ShapeCacheId("physical.part." + document.id().value()));
    if (!cache)
      return Result<PartPhysicalProperties>::failure(cache.error());
    const auto executed =
        GeometryRecomputeExecutor(imports).execute_document(document, cache.value());
    if (!executed)
      return Result<PartPhysicalProperties>::failure(executed.error());
    const auto complete = cache.value().validate_complete_history_result(document);
    if (!complete)
      return Result<PartPhysicalProperties>::failure(complete.error());
    std::vector<BodyPhysicalProperties> bodies;
    if (document.bodies().empty()) {
      if (const auto* shape = cache.value().final_shape()) {
        const auto* material =
            document.default_material()
                ? document.engineering_resources().find_material(*document.default_material())
                : nullptr;
        auto row = body_properties(*shape, nullptr, material);
        if (!row)
          return Result<PartPhysicalProperties>::failure(row.error());
        row.value().name = document.name();
        bodies.push_back(std::move(row.value()));
      }
    } else {
      const auto plan = document.create_history_evaluation_plan();
      if (!plan)
        return Result<PartPhysicalProperties>::failure(plan.error());
      for (const auto& body : document.bodies()) {
        const auto available =
            std::find_if(plan.value().bodies.begin(), plan.value().bodies.end(),
                         [&](const auto& entry) { return entry.body == body.id(); });
        if (available == plan.value().bodies.end() || !available->available)
          continue;
        const auto* shape = cache.value().find_body_shape(body.id());
        if (!shape)
          return Result<PartPhysicalProperties>::failure(
              error(body.id().value(), "Participating Body has no complete shape result"));
        auto row = body_properties(*shape, &body, document.effective_material_for_body(body.id()));
        if (!row)
          return Result<PartPhysicalProperties>::failure(row.error());
        bodies.push_back(std::move(row.value()));
      }
    }
    auto data = detail::aggregate_physical_bodies(std::move(bodies));
    if (!data)
      return Result<PartPhysicalProperties>::failure(data.error());
    PartPhysicalProperties result(std::move(data.value()), source.value(),
                                  document.executable_generation());
    const auto current = result.validate_current(document, imports);
    if (!current)
      return Result<PartPhysicalProperties>::failure(current.error());
    return Result<PartPhysicalProperties>::success(std::move(result));
  } catch (const std::exception& e) {
    return Result<PartPhysicalProperties>::failure(error(document.id().value(), e.what()));
  }
}
} // namespace blcad::geometry
