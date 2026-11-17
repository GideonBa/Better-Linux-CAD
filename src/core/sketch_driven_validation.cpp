#include "sketch_driven_validation.hpp"
#include "blcad/core/sketch_driven_intent.hpp"
#include "emboss_validation.hpp"
#include <cmath>
namespace blcad::detail {
Result<std::size_t> validate_sketch_driven_parameters(const PartDocument& p,
                                                      const SketchDrivenFeature& feature) {
  if (const auto* e = std::get_if<EmbossIntent>(&feature.definition().intent))
    return validate_emboss_parameters(p, *e, feature.id());
  const auto& r = std::get<RibWebIntent>(feature.definition().intent);
  std::vector<ParameterId> sizes{r.thickness};
  if (r.second_thickness)
    sizes.push_back(*r.second_thickness);
  if (const auto* extent = std::get_if<RibDistanceExtent>(&r.extent))
    sizes.push_back(extent->distance);
  for (const auto& id : sizes) {
    const auto* parameter = p.find_parameter(id);
    if (!parameter || parameter->type() != ParameterType::Length ||
        !parameter->value().is_positive_length())
      return Result<std::size_t>::failure(
          Error::validation(id.value(), "Rib/Web size requires a positive Length parameter"));
  }
  if (r.draft) {
    const auto* parameter = p.find_parameter(*r.draft);
    if (!parameter || parameter->type() != ParameterType::Angle ||
        !std::isfinite(parameter->value().degrees()) ||
        std::abs(parameter->value().degrees()) >= 45)
      return Result<std::size_t>::failure(Error::validation(
          r.draft->value(), "Rib/Web draft requires an Angle strictly between -45 and 45 degrees"));
  }
  return Result<std::size_t>::success(feature.parameter_dependencies().size());
}
Result<std::size_t> validate_sketch_driven_inputs(const PartDocument& p,
                                                  const SketchDrivenFeature& feature) {
  const auto fail = [&](const std::string& id, const char* message) {
    return Result<std::size_t>::failure(Error::validation(id, message));
  };
  const auto* target = p.find_body(feature.target_body());
  if (!target || target->kind() != BodyKind::Solid)
    return fail(feature.target_body().value(),
                "Sketch-driven features require an owned Solid target Body");
  if (const auto* e = std::get_if<EmbossIntent>(&feature.definition().intent))
    return validate_emboss_inputs(p, *e, feature.target_body(), feature.id());
  const auto& r = std::get<RibWebIntent>(feature.definition().intent);
  const auto* sketch = p.find_sketch(r.source.sketch);
  if (!sketch)
    return fail(r.source.sketch.value(), "Rib/Web source Sketch is unavailable");
  if (r.source.profile) {
    auto profile = ProfileRegionReference::create(r.source.sketch, *r.source.profile);
    if (!profile)
      return Result<std::size_t>::failure(profile.error());
    auto valid = validate_part_feature_input_reference(p, profile.value());
    if (!valid)
      return valid;
  } else
    for (const auto& curve : r.source.curves)
      if (sketch->curve_is_construction(curve) ||
          (!sketch->find_line_segment(curve) && !sketch->find_arc_segment(curve) &&
           !sketch->find_spline_segment(curve) && !sketch->find_equation_curve(curve)))
        return fail(curve.value(), "Rib/Web source curve is unavailable in its owned Sketch");
  if (const auto* body = std::get_if<RibBodyExtent>(&r.extent)) {
    const auto* limit = p.find_body(body->body);
    if (!limit || limit->kind() != BodyKind::Solid)
      return fail(body->body.value(), "Rib/Web Body termination requires an owned Solid");
  }
  if (const auto* face = std::get_if<RibFaceExtent>(&r.extent)) {
    auto valid = validate_part_feature_input_reference(p, face->face);
    if (!valid)
      return valid;
  }
  return validate_sketch_driven_parameters(p, feature);
}
} // namespace blcad::detail
