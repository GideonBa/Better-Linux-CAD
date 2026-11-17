#include "emboss_validation.hpp"
#include <cmath>
#include <set>
namespace blcad::detail {
std::vector<ParameterId> emboss_parameters(const EmbossIntent& e) {
  std::vector<ParameterId> out{e.depth};
  if (e.orientation)
    out.push_back(*e.orientation);
  if (const auto* text = std::get_if<EmbossTextSource>(&e.source)) {
    out.push_back(text->text.height_parameter());
    if (text->text.rotation_parameter())
      out.push_back(*text->text.rotation_parameter());
    for (const auto& binding : text->text.bindings())
      out.push_back(binding.parameter);
  }
  std::vector<ParameterId> unique;
  for (const auto& id : out)
    if (std::find(unique.begin(), unique.end(), id) == unique.end())
      unique.push_back(id);
  return unique;
}
std::vector<std::string> emboss_inputs(const EmbossIntent& e) {
  std::vector<std::string> out{emboss_source_sketch(e).value()};
  for (const auto& face : e.supports) {
    if (std::find(out.begin(), out.end(), face.source_node_id()) == out.end())
      out.push_back(face.source_node_id());
    if (const auto* captured = std::get_if<ResultFaceReference>(&face.source())) {
      const auto body = "body:" + captured->body().value();
      if (std::find(out.begin(), out.end(), body) == out.end())
        out.push_back(body);
    }
  }
  return out;
}
Result<std::size_t> validate_emboss_structure(const EmbossIntent& e, FeatureId id) {
  const auto fail = [&](const char* message) {
    return Result<std::size_t>::failure(Error::validation(id.value(), message));
  };
  if ((e.operation != EmbossOperation::Raise && e.operation != EmbossOperation::Recess) ||
      (e.mapping != EmbossMapping::Planar && e.mapping != EmbossMapping::Cylindrical &&
       e.mapping != EmbossMapping::Conical))
    return fail("Unknown Emboss/Engrave mode");
  if (const auto* profile = std::get_if<ProfileRegionReference>(&e.source);
      profile && profile->role() != PartFeatureInputRole::ProfileRegion)
    return fail("Emboss source requires a Sketch profile-region reference");
  if (e.depth.empty() || e.supports.empty() || emboss_source_sketch(e).empty() ||
      (e.orientation && e.orientation->empty()))
    return fail("Emboss/Engrave requires source, target supports and a depth binding");
  if (e.mapping == EmbossMapping::Planar && e.seam_degrees != 0)
    return fail("Planar Emboss has no periodic seam; use its orientation binding");
  if (!std::isfinite(e.anchor_mm.x) || !std::isfinite(e.anchor_mm.y) ||
      !std::isfinite(e.seam_degrees))
    return fail("Emboss mapping anchor and seam must be finite");
  std::set<std::string> supports;
  for (const auto& face : e.supports) {
    if (face.role() != PartFeatureInputRole::SurfaceSourceFace)
      return fail("Emboss supports require surface-face references");
    const auto key = std::visit(
        [](const auto& source) -> std::string {
          using T = std::decay_t<decltype(source)>;
          if constexpr (std::is_same_v<T, ResultFaceReference>)
            return source.identity();
          else if constexpr (std::is_same_v<T, ImportedTopologyReference>)
            return std::to_string(source.catalog().value().size()) + ":" +
                   source.catalog().value() + "/" + source.entry().value();
          else if constexpr (std::is_same_v<T, SemanticFaceReference>)
            return source.source_feature().value() + "/" + std::string(to_string(source.face()));
          else
            return source.source_feature().value() + "/" + source.source_profile().value() + "/" +
                   std::string(to_string(source.face()));
        },
        face.source());
    if (!supports.insert(key).second)
      return fail("Emboss support faces must be unique");
  }
  if (const auto* text = std::get_if<EmbossTextSource>(&e.source)) {
    const auto expected = bundled_outline_font(text->text.requested_font());
    if (!expected || expected->asset != text->font.asset || expected->sha256 != text->font.sha256)
      return fail("Emboss text requires a supported pinned outline font and exact asset digest");
  }
  return Result<std::size_t>::success(e.supports.size());
}
Result<std::size_t> validate_emboss_parameters(const PartDocument& p, const EmbossIntent& e,
                                               FeatureId) {
  const auto* depth = p.find_parameter(e.depth);
  if (!depth || depth->type() != ParameterType::Length || !depth->value().is_positive_length())
    return Result<std::size_t>::failure(
        Error::validation(e.depth.value(), "Emboss/Engrave depth requires a positive Length"));
  if (e.orientation) {
    const auto* angle = p.find_parameter(*e.orientation);
    if (!angle || angle->type() != ParameterType::Angle || !std::isfinite(angle->value().degrees()))
      return Result<std::size_t>::failure(
          Error::validation(e.orientation->value(), "Emboss orientation requires an Angle"));
  }
  if (const auto* text = std::get_if<EmbossTextSource>(&e.source)) {
    auto resolved = text->text.resolve(p);
    if (!resolved)
      return Result<std::size_t>::failure(resolved.error());
  }
  return Result<std::size_t>::success(emboss_parameters(e).size());
}
Result<std::size_t> validate_emboss_inputs(const PartDocument& p, const EmbossIntent& e,
                                           BodyId target, FeatureId id) {
  if (!p.find_sketch(emboss_source_sketch(e)))
    return Result<std::size_t>::failure(
        Error::validation(emboss_source_sketch(e).value(), "Emboss source Sketch is unavailable"));
  if (const auto* profile = std::get_if<ProfileRegionReference>(&e.source)) {
    auto valid = validate_part_feature_input_reference(p, *profile);
    if (!valid)
      return valid;
  }
  for (const auto& support : e.supports) {
    auto valid = validate_part_feature_input_reference(p, support);
    if (!valid)
      return valid;
    if (const auto* result = std::get_if<ResultFaceReference>(&support.source());
        result && result->body() != target)
      return Result<std::size_t>::failure(
          Error::validation(id.value(), "Emboss support belongs to a different target Body"));
    if (const auto* planar = std::get_if<SemanticFaceReference>(&support.source())) {
      const auto* source = p.find_feature(planar->source_feature());
      if (!source || !source->body_result_context() ||
          source->body_result_context()->effective_produced_body() != target)
        return Result<std::size_t>::failure(
            Error::validation(id.value(), "Emboss support belongs to a different target Body"));
    }
    if (const auto* cylindrical =
            std::get_if<SemanticCylindricalFaceReference>(&support.source())) {
      const auto* source = p.find_feature(cylindrical->source_feature());
      if (!source || !source->body_result_context() ||
          source->body_result_context()->effective_produced_body() != target)
        return Result<std::size_t>::failure(
            Error::validation(id.value(), "Emboss support belongs to a different target Body"));
    }
    if (const auto* imported = std::get_if<ImportedTopologyReference>(&support.source())) {
      const auto* catalog = p.find_imported_topology_catalog(imported->catalog());
      const auto* entry = catalog ? catalog->find(imported->entry()) : nullptr;
      bool owned = false;
      if (entry)
        for (const auto& base : p.imported_body_features())
          if (base.definition().catalog() == catalog->id())
            for (const auto& out : base.definition().outputs())
              if (entry->parent_body == out.imported_body && out.body == target)
                owned = true;
      if (!owned)
        return Result<std::size_t>::failure(Error::validation(
            id.value(), "Emboss imported support belongs to a different target Body"));
    }
  }
  return validate_emboss_parameters(p, e, id);
}
} // namespace blcad::detail
