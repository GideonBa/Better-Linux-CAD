#include "blcad/gui/gui_hole_conversion.hpp"
#include "blcad/core/part_document_json.hpp"

namespace blcad::gui {
namespace {
Result<PartDocument> fail(std::string message) {
  return Result<PartDocument>::failure(
      Error::validation("gui.hole_conversion", std::move(message)));
}
} // namespace
Result<PartDocument> GuiHoleConversionCommands::candidate(const PartDocument& part, FeatureId id) {
  const auto* cut = part.find_feature(id);
  if (part.is_reference_part() || !cut || cut->type() != FeatureType::SubtractiveExtrude)
    return fail("Select an editable legacy subtractive Extrude to convert to Hole.");
  if (cut->path_curve() || !cut->extrude_intent().is_historical_subtractive_default() ||
      cut->direction() == ExtrudeDirection::Path)
    return fail("Conversion currently requires a straight, flat Through All legacy cut without "
                "draft or thin intent. The legacy cut remains editable.");
  const auto& context = cut->body_result_context();
  if (!context || context->operation_mode() != FeatureBodyOperationMode::Cut ||
      !context->target_body() || context->effective_produced_body() != *context->target_body())
    return fail("Conversion requires an explicit Cut target that preserves its Body identity. Keep "
                "editing this legacy cut or assign a compatible Body target first.");
  const auto* sketch = part.find_sketch(cut->input_sketch());
  if (!sketch)
    return fail("The legacy source Sketch is unavailable.");
  const CircleProfile* circle = nullptr;
  for (const auto& profile : sketch->circle_profiles())
    if (!profile.construction() && ((cut->profile() && *cut->profile() == profile.id()) ||
                                    (!cut->profile() && sketch->profile_count() == 1)))
      circle = &profile;
  if (!circle)
    return fail(
        "Conversion requires one selected Circle profile. Circular Hole Pattern radius/count "
        "bindings and other profile families cannot be transferred to independent Hole positions; "
        "keep the legacy cut or author an explicit Hole/Pattern.");
  auto history = part.executable_history();
  if (!history)
    return Result<PartDocument>::failure(history.error());
  for (const auto& row : history.value())
    if (row.node_id == id.value() && row.state == PartFeatureState::Suppressed)
      return fail("Unsuppress this legacy cut before previewing its Hole conversion.");
  auto support = PlaneReference::create_datum_plane(PartFeatureInputRole::HoleSupportPlane,
                                                    sketch->workplane());
  if (!support)
    return Result<PartDocument>::failure(support.error());
  auto center = circle->center();
  if (const auto* origin = part.find_sketch_origin_override(sketch->id())) {
    center.x += origin->local_origin().x;
    center.y += origin->local_origin().y;
  }
  auto hole = HoleFeature::create(
      id, cut->name(), HoleKind::Plain, *context->target_body(), support.value(),
      {{HolePlacementId(id.value() + ".position"), center}}, circle->diameter_parameter(),
      HoleThroughAllExtent{},
      cut->direction() == ExtrudeDirection::SketchNormal ? HoleDirection::SupportNormal
                                                         : HoleDirection::OppositeSupportNormal);
  if (!hole)
    return Result<PartDocument>::failure(hole.error());
  auto converted = replace_part_extrude_with_hole(part, hole.value());
  if (!converted)
    return fail("Conversion cannot preserve the current downstream mapping: " +
                converted.error().message() +
                ". Keep the legacy cut or explicitly repair its consumers before converting.");
  return converted;
}
} // namespace blcad::gui
