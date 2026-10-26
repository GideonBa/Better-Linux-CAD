#include "blcad/geometry/body_boolean_adapter.hpp"

#include "geometry_shape_internal.hpp"
#include "occt_failure_message.hpp"

#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopoDS_Shape.hxx>

#include <exception>
#include <memory>
#include <string>
#include <utility>

namespace blcad::geometry {
namespace {

[[nodiscard]] Error boolean_error(const FeatureId& feature_id, std::string message) {
  return Error::geometry(feature_id.value(), std::move(message));
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
    return Result<TopoDS_Shape>::failure(
        boolean_error(feature_id, std::string(operation) + " operation produced an empty shape"));
  }
  if (!TopExp_Explorer(result, TopAbs_SOLID).More()) {
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
    for (const GeometryShape& tool : tools) {
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

    return Result<GeometryShape>::success(
        GeometryShape(std::make_shared<GeometryShape::Impl>(std::move(current))));
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

} // namespace blcad::geometry
