#include "blcad/geometry/step_import_topology_context.hpp"

#include "blcad/core/step_import_freshness.hpp"

#include <cmath>
#include <map>
#include <string>
#include <utility>

namespace blcad::geometry {
namespace {
Error context_error(std::string id, std::string message) {
  return Error::geometry(std::move(id), std::move(message));
}

std::pair<Vector3, Vector3> plane_axes(Vector3 normal) {
  const Vector3 seed = std::abs(normal.x) < 0.8 ? Vector3{1, 0, 0} : Vector3{0, 1, 0};
  auto x = blcad::cross(seed, normal);
  const double magnitude = std::sqrt(x.x * x.x + x.y * x.y + x.z * x.z);
  x = {x.x / magnitude, x.y / magnitude, x.z / magnitude};
  return {x, blcad::cross(normal, x)};
}
} // namespace

struct StepImportTopologyContext::Impl {
  struct VerifiedSource {
    StepImportSource source;
    StepImportGeometry geometry;
  };
  struct CatalogRecovery {
    ImportedTopologyCatalog catalog;
    ImportedTopologyRecovery recovery;
  };
  using Key = std::pair<std::string, std::string>;
  explicit Impl(std::filesystem::path root)
      : root_directory(root.empty() ? std::move(root)
                                    : std::filesystem::absolute(root).lexically_normal()) {}
  std::filesystem::path root_directory;
  std::map<Key, VerifiedSource> sources;
  std::map<Key, CatalogRecovery> catalogs;
};

StepImportTopologyContext::StepImportTopologyContext(std::filesystem::path root_directory)
    : impl_(std::make_unique<Impl>(std::move(root_directory))) {}
StepImportTopologyContext::~StepImportTopologyContext() = default;
StepImportTopologyContext::StepImportTopologyContext(StepImportTopologyContext&&) noexcept =
    default;
StepImportTopologyContext&
StepImportTopologyContext::operator=(StepImportTopologyContext&&) noexcept = default;

Result<std::size_t> StepImportTopologyContext::validate_sources() const {
  for (const auto& [key, cached] : impl_->sources) {
    (void)key;
    auto current = inspect_step_import_source(cached.source, impl_->root_directory);
    if (!current)
      return Result<std::size_t>::failure(current.error());
    if (current.value().state() != StepImportFreshness::Available)
      return Result<std::size_t>::failure(
          context_error(cached.source.id().value(), current.value().diagnostic()));
  }
  return Result<std::size_t>::success(impl_->sources.size());
}

Result<const ImportedTopologyRecovery*>
StepImportTopologyContext::recover(const PartDocument& document,
                                   ImportedTopologyCatalogId catalog_id) {
  const auto* catalog = document.find_imported_topology_catalog(catalog_id);
  if (catalog == nullptr)
    return Result<const ImportedTopologyRecovery*>::failure(
        context_error(catalog_id.value(), "imported topology catalog must belong to the Part"));
  const auto* source = document.find_step_import_source(catalog->source());
  if (source == nullptr)
    return Result<const ImportedTopologyRecovery*>::failure(
        context_error(catalog_id.value(), "imported topology source must belong to the Part"));
  const Impl::Key source_key{document.id().value(), source->id().value()};
  auto cached_source = impl_->sources.find(source_key);
  if (cached_source != impl_->sources.end() && cached_source->second.source != *source)
    return Result<const ImportedTopologyRecovery*>::failure(
        context_error(source->id().value(),
                      "STEP source intent changed; create a new import resolution context"));
  if (cached_source == impl_->sources.end()) {
    auto geometry = StepImportReader{}.read(*source, impl_->root_directory);
    if (!geometry)
      return Result<const ImportedTopologyRecovery*>::failure(geometry.error());
    cached_source =
        impl_->sources
            .emplace(source_key, Impl::VerifiedSource{*source, std::move(geometry.value())})
            .first;
  }
  const Impl::Key catalog_key{document.id().value(), catalog_id.value()};
  auto cached_catalog = impl_->catalogs.find(catalog_key);
  if (cached_catalog != impl_->catalogs.end() && cached_catalog->second.catalog != *catalog)
    return Result<const ImportedTopologyRecovery*>::failure(
        context_error(catalog_id.value(),
                      "topology catalog intent changed; create a new import resolution context"));
  if (cached_catalog == impl_->catalogs.end()) {
    auto recovery = StepImportTopology{}.recover(*catalog, cached_source->second.geometry);
    if (!recovery)
      return Result<const ImportedTopologyRecovery*>::failure(recovery.error());
    cached_catalog =
        impl_->catalogs
            .emplace(catalog_key, Impl::CatalogRecovery{*catalog, std::move(recovery.value())})
            .first;
  }
  auto fresh = validate_sources();
  if (!fresh)
    return Result<const ImportedTopologyRecovery*>::failure(fresh.error());
  return Result<const ImportedTopologyRecovery*>::success(&cached_catalog->second.recovery);
}

Result<ImportedTopologyResolution>
StepImportTopologyContext::resolve(const PartDocument& document,
                                   const ImportedTopologyReference& reference) {
  auto valid = validate_imported_topology_reference(document, reference);
  if (!valid)
    return Result<ImportedTopologyResolution>::failure(valid.error());
  auto current = recover(document, reference.catalog());
  if (!current)
    return Result<ImportedTopologyResolution>::failure(current.error());
  return StepImportTopology{}.resolve(*current.value(), reference);
}

Result<AssemblyGeometricTargetSourceKind>
imported_topology_assembly_source_kind(const ImportedTopologyDescriptor& descriptor) {
  using Kind = AssemblyGeometricTargetSourceKind;
  switch (descriptor.geometry) {
  case ImportedGeometryClass::Plane:
    if (descriptor.kind == ImportedTopologyKind::Face)
      return Result<Kind>::success(Kind::ImportedPlanarFace);
    break;
  case ImportedGeometryClass::Cylinder:
    if (descriptor.kind == ImportedTopologyKind::Face)
      return Result<Kind>::success(Kind::ImportedCylindricalFace);
    break;
  case ImportedGeometryClass::Line:
    if (descriptor.kind == ImportedTopologyKind::Edge)
      return Result<Kind>::success(Kind::ImportedLinearEdge);
    break;
  case ImportedGeometryClass::Circle:
    if (descriptor.kind == ImportedTopologyKind::Edge)
      return Result<Kind>::success(Kind::ImportedCircularEdge);
    break;
  case ImportedGeometryClass::Point:
    if (descriptor.kind == ImportedTopologyKind::Vertex)
      return Result<Kind>::success(Kind::ImportedVertex);
    break;
  case ImportedGeometryClass::Opaque:
    break;
  }
  return Result<Kind>::failure(context_error(
      "imported topology", "imported topology has no supported analytic Assembly capability"));
}

Result<AssemblyGeometricTargetDescriptor>
imported_topology_analytic_descriptor(const ImportedTopologyDescriptor& descriptor) {
  auto kind = imported_topology_assembly_source_kind(descriptor);
  if (!kind)
    return Result<AssemblyGeometricTargetDescriptor>::failure(kind.error());
  using ResultType = Result<AssemblyGeometricTargetDescriptor>;
  const auto finite_point = [](Point3 point) {
    return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
  };
  const auto& direction = descriptor.direction;
  const double norm =
      direction.x * direction.x + direction.y * direction.y + direction.z * direction.z;
  if (!finite_point(descriptor.origin) ||
      (descriptor.geometry != ImportedGeometryClass::Point &&
       (!std::isfinite(norm) || std::abs(norm - 1.0) > 1e-8)) ||
      ((descriptor.geometry == ImportedGeometryClass::Cylinder ||
        descriptor.geometry == ImportedGeometryClass::Circle) &&
       (!std::isfinite(descriptor.radius_mm) || descriptor.radius_mm <= 0)))
    return ResultType::failure(context_error("imported topology", "invalid analytic descriptor"));
  switch (descriptor.geometry) {
  case ImportedGeometryClass::Plane: {
    const auto [x, y] = plane_axes(descriptor.direction);
    return ResultType::success(
        AssemblyPlanarTargetDescriptor{descriptor.origin, x, y, descriptor.direction});
  }
  case ImportedGeometryClass::Cylinder:
    return ResultType::success(AssemblyCylindricalSurfaceTargetDescriptor{
        descriptor.origin, descriptor.direction, descriptor.radius_mm});
  case ImportedGeometryClass::Line:
    return ResultType::success(
        AssemblyLineTargetDescriptor{descriptor.origin, descriptor.direction});
  case ImportedGeometryClass::Circle: {
    const auto [x, y] = plane_axes(descriptor.direction);
    return ResultType::success(AssemblyCircularEdgeTargetDescriptor{
        descriptor.origin, x, y, descriptor.direction, descriptor.radius_mm});
  }
  case ImportedGeometryClass::Point:
    return ResultType::success(AssemblyPointTargetDescriptor{descriptor.origin});
  case ImportedGeometryClass::Opaque:
    break;
  }
  return ResultType::failure(context_error("imported topology", "unsupported analytic projection"));
}

} // namespace blcad::geometry
