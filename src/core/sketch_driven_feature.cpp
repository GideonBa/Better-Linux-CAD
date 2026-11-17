#include "blcad/core/sketch_driven_intent.hpp"
#include "emboss_validation.hpp"
#include <algorithm>
#include <cmath>
#include <set>
namespace blcad {
namespace {
bool valid(RibWebKind v) {
  return v == RibWebKind::Rib || v == RibWebKind::Web;
}
bool valid(RibExtrusionPlane v) {
  return v == RibExtrusionPlane::PerpendicularToSketch || v == RibExtrusionPlane::ParallelToSketch;
}
bool valid(RibThicknessSide v) {
  return v == RibThicknessSide::Symmetric || v == RibThicknessSide::First ||
         v == RibThicknessSide::Second || v == RibThicknessSide::Asymmetric;
}
bool valid(RibCrossingPolicy v) {
  return v == RibCrossingPolicy::Union || v == RibCrossingPolicy::Reject;
}
} // namespace
Result<SketchDrivenFeature> SketchDrivenFeature::create_rib_web(FeatureId id, std::string name,
                                                                BodyId body, RibWebIntent intent) {
  return create(std::move(id), std::move(name), std::move(body),
                SketchDrivenDefinition{std::move(intent)});
}
Result<SketchDrivenFeature> SketchDrivenFeature::create(FeatureId id, std::string name, BodyId body,
                                                        SketchDrivenDefinition definition) {

  const auto fail = [&](std::string message) {
    return Result<SketchDrivenFeature>::failure(Error::validation(id.value(), std::move(message)));
  };
  if (id.empty() || name.empty() || body.empty())
    return fail("Sketch-driven feature requires an identity, name and owned target Body");
  if (const auto* emboss = std::get_if<EmbossIntent>(&definition.intent)) {
    auto valid = detail::validate_emboss_structure(*emboss, id);
    if (!valid)
      return Result<SketchDrivenFeature>::failure(valid.error());
  } else {
    auto& intent = std::get<RibWebIntent>(definition.intent);
    if (!valid(intent.kind) || !valid(intent.plane) || !valid(intent.side) ||
        !valid(intent.crossings))
      return fail("Unknown Rib/Web mode");
    if (intent.source.sketch.empty() ||
        intent.source.profile.has_value() == !intent.source.curves.empty())
      return fail("Rib/Web source selects exactly one owned profile or an ordered curve list");
    if (intent.source.profile && intent.source.profile->empty())
      return fail("Rib/Web profile identity must not be empty");
    std::set<std::string> curves;
    for (const auto& curve : intent.source.curves)
      if (curve.empty() || !curves.insert(curve.value()).second)
        return fail("Rib/Web curves require unique nonempty identities");
    const auto& d = intent.direction;
    const double length = std::hypot(d.x, d.y, d.z);
    if (!std::isfinite(length) || length < 1e-12)
      return fail("Rib/Web direction must be finite and nonzero");

    if (intent.thickness.empty() ||
        (intent.side == RibThicknessSide::Asymmetric) != intent.second_thickness.has_value() ||
        (intent.second_thickness && intent.second_thickness->empty()) ||
        (intent.draft && intent.draft->empty()))
      return fail("Rib/Web thickness and draft bindings are incompatible with its side mode");
    const bool extent = std::visit(
        [](const auto& e) {
          using T = std::decay_t<decltype(e)>;
          if constexpr (std::is_same_v<T, RibDistanceExtent>)
            return !e.distance.empty();
          else if constexpr (std::is_same_v<T, RibBodyExtent>)
            return !e.body.empty();
          else
            return e.face.role() == PartFeatureInputRole::ExtrudeLimitFace;
        },
        intent.extent);
    if (!extent)
      return fail("Rib/Web termination requires a supported owned reference");
  }
  auto owned = std::make_shared<const SketchDrivenDefinition>(std::move(definition));
  return Result<SketchDrivenFeature>::success(
      SketchDrivenFeature(std::move(id), std::move(name), std::move(body), std::move(owned)));
}
SketchDrivenFeature::SketchDrivenFeature(FeatureId id, std::string name, BodyId target,
                                         std::shared_ptr<const SketchDrivenDefinition> definition)
    : id_(std::move(id)), name_(std::move(name)), target_(std::move(target)),
      definition_(std::move(definition)) {}
const FeatureId& SketchDrivenFeature::id() const noexcept {
  return id_;
}
const std::string& SketchDrivenFeature::name() const noexcept {
  return name_;
}
std::string_view SketchDrivenFeature::kind_name() const noexcept {
  if (const auto* e = std::get_if<EmbossIntent>(&definition_->intent))
    return e->operation == EmbossOperation::Raise ? "Emboss" : "Engrave";
  return std::get<RibWebIntent>(definition_->intent).kind == RibWebKind::Rib ? "Rib" : "Web";
}
const BodyId& SketchDrivenFeature::target_body() const noexcept {
  return target_;
}
const SketchDrivenDefinition& SketchDrivenFeature::definition() const noexcept {
  return *definition_;
}
std::vector<ParameterId> SketchDrivenFeature::parameter_dependencies() const {
  if (const auto* e = std::get_if<EmbossIntent>(&definition_->intent))
    return detail::emboss_parameters(*e);
  const auto& rib = std::get<RibWebIntent>(definition_->intent);
  std::vector<ParameterId> out{rib.thickness};
  if (rib.second_thickness)
    out.push_back(*rib.second_thickness);
  if (rib.draft)
    out.push_back(*rib.draft);
  if (const auto* distance = std::get_if<RibDistanceExtent>(&rib.extent))
    out.push_back(distance->distance);
  std::vector<ParameterId> unique;
  for (const auto& id : out)
    if (std::find(unique.begin(), unique.end(), id) == unique.end())
      unique.push_back(id);
  return unique;
}
std::vector<std::string> SketchDrivenFeature::input_nodes() const {
  if (const auto* e = std::get_if<EmbossIntent>(&definition_->intent))
    return detail::emboss_inputs(*e);
  const auto& rib = std::get<RibWebIntent>(definition_->intent);
  std::vector<std::string> out{rib.source.sketch.value()};
  if (const auto* face = std::get_if<RibFaceExtent>(&rib.extent)) {
    out.push_back(face->face.source_node_id());
    if (const auto* captured = std::get_if<ResultFaceReference>(&face->face.source()))
      out.push_back("body:" + captured->body().value());
  }
  if (const auto* body = std::get_if<RibBodyExtent>(&rib.extent))
    out.push_back("body:" + body->body.value());
  return out;
}
} // namespace blcad
