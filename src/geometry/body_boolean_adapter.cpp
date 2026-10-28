#include "blcad/geometry/body_boolean_adapter.hpp"

#include "geometry_shape_internal.hpp"
#include "occt_failure_message.hpp"

#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopoDS_Shape.hxx>

#include <exception>
#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <type_traits>

namespace blcad::geometry {
namespace {

[[nodiscard]] Error boolean_error(const FeatureId& feature_id, std::string message) {
  return Error::geometry(feature_id.value(), std::move(message));
}

[[nodiscard]] Error full_removal_error(const FeatureId& feature_id) {
  return boolean_error(feature_id, "Cut removes the entire target body. Reduce the tool size or "
                                   "extent, or choose a different target.");
}

[[nodiscard]] Error no_removal_error(const FeatureId& feature_id) {
  return boolean_error(feature_id, "Cut removes no measurable material from the target. Check the "
                                   "target body, tool position, direction and extent.");
}

struct VolumeMeasurement {
  double volume;
  double relative_error;
};

VolumeMeasurement solid_volume(const TopoDS_Shape& shape) {
  GProp_GProps properties;
  const double error = BRepGProp::VolumeProperties(shape, properties, 1e-10, true);
  return {std::abs(properties.Mass()), error};
}

template <typename Algorithm>
[[nodiscard]] Result<TopoDS_Shape> execute_algorithm(const FeatureId& feature_id,
                                                     const TopoDS_Shape& left,
                                                     const TopoDS_Shape& right,
                                                     std::string_view operation) {
  // GeometryShape copies share OCCT topology with earlier cached results.
  // Set safe-input mode before the first build: the two-shape constructors
  // execute eagerly, and simplification would otherwise mutate shared faces.
  Algorithm algorithm;
  TopTools_ListOfShape arguments, tools;
  arguments.Append(left);
  tools.Append(right);
  algorithm.SetArguments(arguments);
  algorithm.SetTools(tools);
  algorithm.SetNonDestructive(true);
  algorithm.Build();
  if (!algorithm.IsDone() || algorithm.HasErrors()) {
    return Result<TopoDS_Shape>::failure(
        boolean_error(feature_id, std::string(operation) + " operation did not complete"));
  }
  // Collapse shared coplanar faces and tangent edges so downstream selection
  // and finishing operate on the actual united boundary.
  algorithm.SimplifyResult(true, true);
  TopoDS_Shape result = algorithm.Shape();
  if (result.IsNull()) {
    if constexpr (std::is_same_v<Algorithm, BRepAlgoAPI_Cut>)
      return Result<TopoDS_Shape>::failure(full_removal_error(feature_id));
    return Result<TopoDS_Shape>::failure(
        boolean_error(feature_id, std::string(operation) + " operation produced an empty shape"));
  }
  if (!TopExp_Explorer(result, TopAbs_SOLID).More()) {
    if constexpr (std::is_same_v<Algorithm, BRepAlgoAPI_Cut>) {
      if (!TopExp_Explorer(result, TopAbs_FACE).More())
        return Result<TopoDS_Shape>::failure(full_removal_error(feature_id));
    }
    return Result<TopoDS_Shape>::failure(boolean_error(
        feature_id, std::string(operation) + " operation produced no solid result"));
  }
  if (!BRepCheck_Analyzer(result, true).IsValid())
    return Result<TopoDS_Shape>::failure(
        boolean_error(feature_id, std::string(operation) + " operation produced an invalid solid"));
  return Result<TopoDS_Shape>::success(std::move(result));
}

} // namespace

Result<GeometryShape> BodyBooleanAdapter::combine(FeatureId feature_id,
                                                  BodyBooleanOperation operation,
                                                  const GeometryShape& target,
                                                  const std::vector<GeometryShape>& tools) const {
  if (feature_id.empty()) {
    return Result<GeometryShape>::failure(
        Error::validation("body_boolean", "feature id must not be empty"));
  }
  if (target.empty()) {
    return Result<GeometryShape>::failure(
        boolean_error(feature_id, "body boolean target shape must not be empty"));
  }
  if (tools.empty()) {
    return Result<GeometryShape>::failure(
        boolean_error(feature_id, "body boolean requires at least one tool shape"));
  }
  for (const GeometryShape& tool : tools) {
    if (tool.empty()) {
      return Result<GeometryShape>::failure(
          boolean_error(feature_id, "body boolean tool shapes must not be empty"));
    }
  }

  try {
    TopoDS_Shape current = target.impl_->shape;
    if (operation == BodyBooleanOperation::Subtract &&
        !TopExp_Explorer(current, TopAbs_SOLID).More())
      return Result<GeometryShape>::failure(
          boolean_error(feature_id, "Cut target must be a valid solid body."));
    for (const GeometryShape& tool : tools) {
      if (operation == BodyBooleanOperation::Subtract &&
          !TopExp_Explorer(tool.impl_->shape, TopAbs_SOLID).More())
        return Result<GeometryShape>::failure(
            boolean_error(feature_id, "Cut tool must be a valid solid body."));
      Result<TopoDS_Shape> combined = [&]() {
        switch (operation) {
        case BodyBooleanOperation::Add:
          return execute_algorithm<BRepAlgoAPI_Fuse>(feature_id, current, tool.impl_->shape,
                                                     "add");
        case BodyBooleanOperation::Subtract:
          return execute_algorithm<BRepAlgoAPI_Cut>(feature_id, current, tool.impl_->shape,
                                                    "subtract");
        case BodyBooleanOperation::Intersect:
          return execute_algorithm<BRepAlgoAPI_Common>(feature_id, current, tool.impl_->shape,
                                                       "intersect");
        }
        return Result<TopoDS_Shape>::failure(
            boolean_error(feature_id, "unsupported body boolean operation"));
      }();
      if (combined.has_error())
        return Result<GeometryShape>::failure(combined.error());
      current = std::move(combined.value());
    }

    GeometryShape result(std::make_shared<GeometryShape::Impl>(std::move(current)));
    if (operation == BodyBooleanOperation::Subtract) {
      auto validation = validate_subtraction_result(feature_id, target, result);
      if (!validation)
        return Result<GeometryShape>::failure(validation.error());
    }
    return Result<GeometryShape>::success(std::move(result));
  } catch (const Standard_Failure& failure) {
    return Result<GeometryShape>::failure(
        boolean_error(feature_id, standard_failure_message(failure, "OCCT body boolean operation failed")));
  } catch (const std::exception& exception) {
    return Result<GeometryShape>::failure(boolean_error(feature_id, exception.what()));
  } catch (...) {
    return Result<GeometryShape>::failure(
        boolean_error(feature_id, "unknown body boolean geometry error"));
  }
}

Result<double> BodyBooleanAdapter::validate_subtraction_result(FeatureId feature_id,
                                                               const GeometryShape& target,
                                                               const GeometryShape& result) const {
  if (feature_id.empty())
    return Result<double>::failure(Error::validation("cut", "feature id must not be empty"));
  try {
    if (target.empty() || !TopExp_Explorer(target.impl_->shape, TopAbs_SOLID).More())
      return Result<double>::failure(
          boolean_error(feature_id, "Cut target must be a valid solid body."));
    if (result.empty())
      return Result<double>::failure(full_removal_error(feature_id));
    if (!TopExp_Explorer(result.impl_->shape, TopAbs_SOLID).More()) {
      if (!TopExp_Explorer(result.impl_->shape, TopAbs_FACE).More())
        return Result<double>::failure(full_removal_error(feature_id));
      return Result<double>::failure(
          boolean_error(feature_id, "Cut produced invalid geometry without a solid result."));
    }
    if (!BRepCheck_Analyzer(result.impl_->shape, true).IsValid())
      return Result<double>::failure(boolean_error(feature_id, "Cut produced an invalid solid."));
    if (result.impl_->shape.IsSame(target.impl_->shape))
      return Result<double>::failure(no_removal_error(feature_id));
    const auto before = solid_volume(target.impl_->shape);
    const auto after = solid_volume(result.impl_->shape);
    if (!std::isfinite(before.volume) || before.volume <= 0 || !std::isfinite(after.volume) ||
        after.volume <= 0 || !std::isfinite(before.relative_error) || before.relative_error < 0 ||
        !std::isfinite(after.relative_error) || after.relative_error < 0)
      return Result<double>::failure(
          boolean_error(feature_id, "Cut geometry has no valid measurable solid volume."));
    // Account for OCCT's integration error estimate and floating-point rounding.
    // This is numerical resolution, not a modeling minimum or a percentage cut.
    const double tolerance =
        before.relative_error * before.volume + after.relative_error * after.volume +
        64 * std::numeric_limits<double>::epsilon() * std::max(before.volume, after.volume);
    const double removed = before.volume - after.volume;
    if (removed < -tolerance)
      return Result<double>::failure(
          boolean_error(feature_id, "Cut produced an inconsistent increased solid volume."));
    if (removed <= tolerance)
      return Result<double>::failure(no_removal_error(feature_id));
    return Result<double>::success(removed);
  } catch (const Standard_Failure& failure) {
    return Result<double>::failure(boolean_error(
        feature_id, standard_failure_message(failure, "Could not validate Cut geometry")));
  } catch (const std::exception& exception) {
    return Result<double>::failure(boolean_error(feature_id, exception.what()));
  } catch (...) {
    return Result<double>::failure(boolean_error(feature_id, "Could not validate Cut geometry."));
  }
}

} // namespace blcad::geometry
