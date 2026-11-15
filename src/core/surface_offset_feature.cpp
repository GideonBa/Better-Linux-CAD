#include "blcad/core/part_document.hpp"
#include "blcad/core/surface_feature.hpp"
#include <cmath>
#include <type_traits>
#include <utility>
namespace blcad {
SurfaceOffsetFeature::SurfaceOffsetFeature(FeatureId id, std::string name, SurfaceFeatureKind kind,
                                           SurfaceReference target,
                                           SurfaceOffsetParameters parameters,
                                           FeatureBodyResultContext context)
    : id_(std::move(id)), name_(std::move(name)), kind_(kind), target_(std::move(target)),
      parameters_(std::move(parameters)), context_(std::move(context)) {}
Result<SurfaceOffsetFeature> SurfaceOffsetFeature::create(FeatureId id, std::string name,
                                                          SurfaceFeatureKind kind,
                                                          SurfaceReference target,
                                                          SurfaceOffsetParameters parameters,
                                                          FeatureBodyResultContext context) {
  const auto fail = [&](std::string message) {
    return Result<SurfaceOffsetFeature>::failure(
        Error::validation(id.empty() ? "surface.offset" : id.value(), std::move(message)));
  };
  if (id.empty() || name.empty() || parameters.distance.empty() || parameters.tolerance.empty())
    return fail("Surface offset requires identity, target, distance and tolerance");
  if (parameters.side != SurfaceOffsetSide::AlongNormal &&
      parameters.side != SurfaceOffsetSide::OppositeNormal &&
      parameters.side != SurfaceOffsetSide::Symmetric)
    return fail("Unknown Surface offset side");
  if (parameters.approximation != SurfaceOffsetApproximation::ExactAnalytic &&
      parameters.approximation != SurfaceOffsetApproximation::VerifiedApproximation)
    return fail("Unknown Surface offset approximation policy");
  const bool approximate =
      parameters.approximation == SurfaceOffsetApproximation::VerifiedApproximation;
  if (approximate != parameters.maximum_deviation.has_value() ||
      (parameters.maximum_deviation && parameters.maximum_deviation->empty()))
    return fail("Verified approximation requires exactly one explicit maximum-deviation parameter");
  if (kind == SurfaceFeatureKind::OffsetSurface &&
      context.operation_mode() != FeatureBodyOperationMode::NewBody)
    return fail("OffsetSurface creates a new material-free Surface Body");
  return Result<SurfaceOffsetFeature>::success(
      SurfaceOffsetFeature(std::move(id), std::move(name), kind, std::move(target),
                           std::move(parameters), std::move(context)));
}
Result<SurfaceOffsetFeature> SurfaceOffsetFeature::create_offset(FeatureId id, std::string name,
                                                                 SurfaceReference target,
                                                                 SurfaceOffsetParameters parameters,
                                                                 FeatureBodyResultContext context) {
  return create(std::move(id), std::move(name), SurfaceFeatureKind::OffsetSurface,
                std::move(target), std::move(parameters), std::move(context));
}
Result<SurfaceOffsetFeature>
SurfaceOffsetFeature::create_thicken(FeatureId id, std::string name, SurfaceReference target,
                                     SurfaceOffsetParameters parameters,
                                     FeatureBodyResultContext context) {
  return create(std::move(id), std::move(name), SurfaceFeatureKind::Thicken, std::move(target),
                std::move(parameters), std::move(context));
}
Result<TrimSurfaceFeature>
TrimSurfaceFeature::with_options(std::optional<SurfaceTrimOptions> options) const {
  const auto fail = [&](std::string message) {
    return Result<TrimSurfaceFeature>::failure(Error::validation(id_.value(), std::move(message)));
  };
  if (options) {
    const auto& value = *options;
    if (value.tolerance.empty() ||
        (value.side != SurfaceTrimSide::KeepInside &&
         value.side != SurfaceTrimSide::RemoveInside) ||
        (value.projection != SurfaceTrimProjection::OnSurface &&
         value.projection != SurfaceTrimProjection::AlongDirection))
      return fail("Advanced Surface Trim requires a supported side, projection and tolerance");
    if ((value.projection == SurfaceTrimProjection::AlongDirection) != value.direction.has_value())
      return fail("Projected Surface Trim requires exactly one explicit direction");
    if (value.direction) {
      const auto d = *value.direction;
      const double norm = dot(d, d);
      if (!std::isfinite(norm) || norm <= 1e-24)
        return fail("Surface Trim direction must be finite and nonzero");
    }
  }
  auto copy = *this;
  copy.options_ = std::move(options);
  return Result<TrimSurfaceFeature>::success(std::move(copy));
}
Result<ExtendSurfaceFeature>
ExtendSurfaceFeature::with_options(std::optional<SurfaceExtendOptions> options) const {
  const auto fail = [&](std::string message) {
    return Result<ExtendSurfaceFeature>::failure(
        Error::validation(id_.value(), std::move(message)));
  };
  if (options) {
    const auto& value = *options;
    if (value.tolerance.empty() ||
        (value.mode != SurfaceExtendMode::Natural && value.mode != SurfaceExtendMode::Tangent) ||
        (value.mode == SurfaceExtendMode::Tangent) != value.maximum_deviation.has_value() ||
        (value.maximum_deviation && value.maximum_deviation->empty()))
      return fail("Advanced Surface Extend requires tolerance and an explicit deviation budget for "
                  "tangent extension");
  }
  auto copy = *this;
  copy.options_ = std::move(options);
  return Result<ExtendSurfaceFeature>::success(std::move(copy));
}
BodyKind surface_feature_result_kind(const SurfaceFeature& feature) noexcept {
  const auto kind = surface_feature_kind(feature);
  return kind == SurfaceFeatureKind::Thicken || kind == SurfaceFeatureKind::ClosedShellToSolid
             ? BodyKind::Solid
             : BodyKind::Surface;
}
const FeatureBodyResultContext*
surface_feature_body_context(const SurfaceFeature& feature) noexcept {
  const auto* offset = std::get_if<SurfaceOffsetFeature>(&feature);
  return offset ? &offset->body_result_context() : nullptr;
}
bool surface_feature_requires_v13(const SurfaceFeature& feature) noexcept {
  return std::visit(
      [](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, SurfaceOffsetFeature>)
          return true;
        else if constexpr (std::is_same_v<T, TrimSurfaceFeature> ||
                           std::is_same_v<T, ExtendSurfaceFeature>)
          return value.options().has_value() ||
                 value.target().source_kind() == SurfaceReferenceSourceKind::SelectedFaces;
        else if constexpr (std::is_same_v<T, SurfaceStitchFeature>) {
          for (const auto& input : value.surfaces())
            if (input.source_kind() == SurfaceReferenceSourceKind::SelectedFaces)
              return true;
          return false;
        } else if constexpr (std::is_same_v<T, ClosedShellToSolidFeature>)
          return value.shell().source_kind() == SurfaceReferenceSourceKind::SelectedFaces;
        else
          return false;
      },
      feature);
}
Result<std::size_t> validate_surface_feature_parameters(const PartDocument& part,
                                                        const SurfaceFeature& feature) {
  const auto fail = [&](std::string message) {
    return Result<std::size_t>::failure(
        Error::validation(surface_feature_id(feature).value(), std::move(message)));
  };
  const auto length = [&](const ParameterId& id, double lo, double hi) -> std::optional<double> {
    const auto* parameter = part.find_parameter(id);
    if (!parameter || parameter->type() != ParameterType::Length)
      return {};
    const double value = parameter->value().millimeters();
    return std::isfinite(value) && value >= lo && value <= hi ? std::optional(value) : std::nullopt;
  };
  if (const auto* offset = std::get_if<SurfaceOffsetFeature>(&feature)) {
    const auto& intent = offset->parameters();
    const auto d = length(intent.distance, 1e-6, 1e6), tol = length(intent.tolerance, 1e-9, 1);
    if (!d || !tol || *tol > .01 * (*d))
      return fail("Surface offset requires distance 1e-6..1e6 mm and tolerance 1e-9..1 mm, at most "
                  "1% of distance");
    if (intent.maximum_deviation) {
      const auto error = length(*intent.maximum_deviation, *tol, .01 * (*d));
      if (!error)
        return fail("Surface approximation budget must be at least the modeling tolerance and at "
                    "most 1% of distance");
    }
  } else if (const auto* trim = std::get_if<TrimSurfaceFeature>(&feature);
             trim && trim->options()) {
    if (!length(trim->options()->tolerance, 1e-9, 1))
      return fail("Advanced Surface Trim tolerance must be a Length from 1e-9 to 1 mm");
  } else if (const auto* extend = std::get_if<ExtendSurfaceFeature>(&feature);
             extend && extend->options()) {
    const auto& options = *extend->options();
    const auto d = length(extend->distance_parameter(), 1e-6, 1e6),
               tol = length(options.tolerance, 1e-9, 1);
    if (!d || !tol || *tol > .01 * (*d))
      return fail(
          "Advanced Surface Extend requires bounded distance and tolerance at most 1% of distance");
    if (options.maximum_deviation && !length(*options.maximum_deviation, *tol, .01 * (*d)))
      return fail("Tangent extension requires a bounded explicit maximum-deviation Length");
  }
  return Result<std::size_t>::success(1);
}
} // namespace blcad
