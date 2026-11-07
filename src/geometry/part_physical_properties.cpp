#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/body_result_inspector.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/step_import_topology_context.hpp"
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
double number(long double value) {
  const auto result = static_cast<double>(value);
  if (!std::isfinite(value) || !std::isfinite(result) || (value != 0 && result == 0))
    throw std::runtime_error("Physical total exceeds the representable numeric range");
  return result;
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
  if (material)
    result.material = material->key();
  if (result.kind == BodyKind::Surface) {
    if (result.geometry.volume || result.geometry.area_mm2 <= 0)
      return Result<BodyPhysicalProperties>::failure(
          error(result.name, "Surface Body requires surface geometry without solid volume"));
    return Result<BodyPhysicalProperties>::success(std::move(result));
  }
  if (!result.geometry.volume || result.geometry.free_face_count != 0)
    return Result<BodyPhysicalProperties>::failure(
        error(result.name, "Solid Body requires closed solids without free faces"));
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
Result<PartPhysicalPropertiesData> aggregate(std::vector<BodyPhysicalProperties> bodies) {
  try {
    PartPhysicalPropertiesData result;
    result.bodies = std::move(bodies);
    long double area = 0;
    std::vector<detail::WeightedPhysicalProperties> volumes, masses;
    for (const auto& body : result.bodies) {
      area += body.geometry.area_mm2;
      result.estimated_relative_area_error = std::max(result.estimated_relative_area_error,
                                                      body.geometry.estimated_relative_area_error);
      result.estimated_relative_volume_error = std::max(
          result.estimated_relative_volume_error, body.geometry.estimated_relative_volume_error);
      if (body.kind == BodyKind::Surface) {
        ++result.surface_body_count;
        continue;
      }
      ++result.solid_body_count;
      const auto& v = *body.geometry.volume;
      volumes.push_back({v.volume_mm3, v.centroid_mm, v.central_inertia_mm5});
      if (body.mass) {
        const auto& m = *body.mass;
        masses.push_back({m.mass_kg, m.centroid_mm, m.central_inertia_kg_mm2});
      } else
        ++result.missing_density_body_count;
    }
    result.area_mm2 = number(area);
    if (!volumes.empty()) {
      const auto total = detail::sum_physical_properties(volumes);
      if (!total)
        return Result<PartPhysicalPropertiesData>::failure(total.error());
      result.volume_mm3 = total.value().weight;
      result.volume_centroid_mm = total.value().centroid;
      result.mass_status = PhysicalMassStatus::Unavailable;
    }
    if (!masses.empty()) {
      const auto summed = detail::sum_physical_properties(masses);
      if (!summed)
        return Result<PartPhysicalPropertiesData>::failure(summed.error());
      auto mass = detail::physical_mass_properties(summed.value());
      if (!mass)
        return Result<PartPhysicalPropertiesData>::failure(mass.error());
      result.known_mass = std::move(mass.value());
      result.mass_status = result.missing_density_body_count ? PhysicalMassStatus::Partial
                                                             : PhysicalMassStatus::Complete;
    }
    return Result<PartPhysicalPropertiesData>::success(std::move(result));
  } catch (const std::exception& e) {
    return Result<PartPhysicalPropertiesData>::failure(error("physical.part", e.what()));
  }
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
    auto data = aggregate(std::move(bodies));
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
