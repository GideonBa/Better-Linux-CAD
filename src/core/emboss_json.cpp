#include "emboss_json.hpp"
#include "blcad/core/sketch_driven_intent.hpp"
#include "blcad/core/sketch_text_json.hpp"
#include <stdexcept>
namespace blcad::detail {
namespace {
using json = nlohmann::json;
void require(bool okay, const char* message) {
  if (!okay)
    throw std::invalid_argument(message);
}
template <class T> T take(Result<T> value) {
  if (!value)
    throw std::invalid_argument(value.error().message());
  return std::move(value.value());
}
json text_json(const SketchText& text) {
  auto catalog = take(SketchTextCatalog::create(DocumentId("embedded.text"), {text}));
  return json::parse(take(serialize_sketch_text_catalog_to_json(catalog))).at("texts").at(0);
}
SketchText parse_text(const json& text) {
  require(text.is_object() && text.size() == 8 && text.at("anchor").is_object() &&
              text.at("anchor").size() == 2 && text.at("bindings").is_array(),
          "Malformed Emboss Sketch text");
  for (const auto& binding : text.at("bindings"))
    require(binding.is_object() && binding.size() == 3, "Malformed Emboss text binding");
  json root{{"schema", "blcad.sketch_text.mvp8"},
            {"version", 1},
            {"document", "embedded.text"},
            {"texts", json::array({text})}};
  return take(deserialize_sketch_text_catalog_from_json(root.dump())).texts().front();
}
} // namespace
json emboss_to_json(const SketchDrivenFeature& feature, const SketchDrivenFaceWriter& face) {
  const auto& e = std::get<EmbossIntent>(feature.definition().intent);
  json source;
  if (const auto* profile = std::get_if<ProfileRegionReference>(&e.source))
    source = {{"kind", "profile"},
              {"sketch", profile->sketch().value()},
              {"profile", profile->profile().value()}};
  else {
    const auto& text = std::get<EmbossTextSource>(e.source);
    source = {{"kind", "text"},
              {"text", text_json(text.text)},
              {"font", {{"asset", text.font.asset}, {"sha256", text.font.sha256}}}};
  }
  json supports = json::array();
  for (const auto& support : e.supports)
    supports.push_back(face(support));
  return {{"id", feature.id().value()},
          {"name", feature.name()},
          {"target_body", feature.target_body().value()},
          {"mode", e.operation == EmbossOperation::Raise ? "emboss" : "engrave"},
          {"source", source},
          {"supports", supports},
          {"depth", e.depth.value()},
          {"mapping", e.mapping == EmbossMapping::Planar        ? "planar"
                      : e.mapping == EmbossMapping::Cylindrical ? "cylindrical"
                                                                : "conical"},
          {"anchor_mm", {e.anchor_mm.x, e.anchor_mm.y}},
          {"seam_degrees", e.seam_degrees},
          {"orientation", e.orientation ? json(e.orientation->value()) : json(nullptr)}};
}
Result<SketchDrivenFeature> emboss_from_json(const json& record,
                                             const SketchDrivenFaceReader& face) {
  try {
    require(record.is_object() && record.size() == 11, "Malformed Emboss/Engrave record");
    const auto& source = record.at("source");
    require(source.is_object() && source.size() == 3, "Malformed Emboss source");
    EmbossSource input =
        take(ProfileRegionReference::create(SketchId("placeholder"), ProfileId("placeholder")));
    if (source.at("kind") == "profile")
      input =
          take(ProfileRegionReference::create(SketchId(source.at("sketch").get<std::string>()),
                                              ProfileId(source.at("profile").get<std::string>())));
    else if (source.at("kind") == "text") {
      const auto& font = source.at("font");
      require(font.is_object() && font.size() == 2, "Malformed pinned Emboss font");
      input = EmbossTextSource{
          parse_text(source.at("text")),
          {font.at("asset").get<std::string>(), font.at("sha256").get<std::string>()}};
    } else
      throw std::invalid_argument("Unknown Emboss source mode");
    const auto& mode = record.at("mode");
    require(mode == "emboss" || mode == "engrave", "Unknown Emboss/Engrave operation");
    const auto& mapping = record.at("mapping");
    require(mapping == "planar" || mapping == "cylindrical" || mapping == "conical",
            "Unknown Emboss mapping");
    require(record.at("supports").is_array(), "Emboss supports must be an ordered array");
    std::vector<FaceReference> supports;
    for (const auto& support : record.at("supports"))
      supports.push_back(take(face(support, PartFeatureInputRole::SurfaceSourceFace)));
    const auto& anchor = record.at("anchor_mm");
    require(anchor.is_array() && anchor.size() == 2,
            "Emboss anchor requires two chart coordinates");
    std::optional<ParameterId> orientation;
    if (!record.at("orientation").is_null())
      orientation = ParameterId(record.at("orientation").get<std::string>());
    EmbossIntent intent{std::move(input),
                        std::move(supports),
                        mode == "emboss" ? EmbossOperation::Raise : EmbossOperation::Recess,
                        ParameterId(record.at("depth").get<std::string>()),
                        mapping == "planar"        ? EmbossMapping::Planar
                        : mapping == "cylindrical" ? EmbossMapping::Cylindrical
                                                   : EmbossMapping::Conical,
                        {anchor[0].get<double>(), anchor[1].get<double>()},
                        record.at("seam_degrees").get<double>(),
                        orientation};
    return SketchDrivenFeature::create(FeatureId(record.at("id").get<std::string>()),
                                       record.at("name").get<std::string>(),
                                       BodyId(record.at("target_body").get<std::string>()),
                                       SketchDrivenDefinition{std::move(intent)});
  } catch (const std::exception& e) {
    return Result<SketchDrivenFeature>::failure(Error::validation("emboss", e.what()));
  }
}
} // namespace blcad::detail
