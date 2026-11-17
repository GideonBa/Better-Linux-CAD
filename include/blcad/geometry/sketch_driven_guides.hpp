#pragma once
#include "blcad/core/sketch_driven_intent.hpp"
#include "blcad/geometry/finishing_selection.hpp"
namespace blcad::geometry {
struct SketchDrivenFaceTarget {
  FinishingSelectionTarget selection;
  std::optional<EmbossMapping> mapping;
  Point3 anchor;
  Vector3 normal;
  std::string label;
  std::optional<double> axial_center_mm{};
};
struct SketchDrivenGuide {
  std::string id, label;
  std::optional<ParameterId> binding;
  Point3 origin;
  Vector3 axis, reference_direction{1, 0, 0};
  double value{}, model_units_per_value{1};
  bool angular{};
};
class SketchDrivenGuideQuery {
public:
  static Result<std::vector<SketchDrivenFaceTarget>> faces(const PartDocument&, const ShapeCache&,
                                                           BodyId,
                                                           StepImportTopologyContext* = nullptr,
                                                           std::optional<std::size_t> prefix = {});
  static Result<std::vector<SketchDrivenGuide>>
  guides(const PartDocument&, const SketchDrivenFeature&, const ShapeCache&,
         StepImportTopologyContext* = nullptr, std::optional<std::size_t> prefix = {});
};
} // namespace blcad::geometry
