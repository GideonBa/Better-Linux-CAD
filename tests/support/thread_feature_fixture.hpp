#pragma once

#include "blcad/core/part_document_json.hpp"
#include "blcad/core/thread_feature.hpp"
#include "engineering_resource_fixture.hpp"
#include "part_history_fixture.hpp"
#include <nlohmann/json.hpp>

namespace blcad::test::thread_fixture {
using history_fixture::accept;
using history_fixture::checked;
inline ThreadParameterProfile custom(bool tapered = false) {
  return {"Synthetic system",
          "Synthetic 10",
          "Authored tolerance",
          ParameterId("thread.major"),
          ParameterId("thread.minor"),
          ParameterId("thread.pitch"),
          ParameterId("thread.angle"),
          tapered ? std::optional(ParameterId("thread.taper")) : std::nullopt};
}
inline ThreadStandardProfile selected(std::string revision = "r1", bool tapered = false) {
  return {engineering_fixture::key("synthetic.threads", std::move(revision)),
          "synthetic.row",
          {"system", "designation", "tolerance", "major", "minor", "pitch", "angle",
           tapered ? std::optional(std::string("taper")) : std::nullopt}};
}
inline ThreadDefinition definition(ThreadProfile profile = custom(), bool offset = false,
                                   ThreadHandedness handedness = ThreadHandedness::Right,
                                   ThreadRepresentation mode = ThreadRepresentation::Cosmetic) {
  return checked(ThreadDefinition::create(
      std::move(profile), ParameterId("thread.length"),
      offset ? std::optional(ParameterId("thread.start")) : std::nullopt, handedness, mode));
}
inline EngineeringStandardTable table(std::string revision = "r1", double pitch = 1.7,
                                      std::string omitted = {}) {
  using K = EngineeringTableValueKind;
  const auto provenance = [&](const std::string& column) {
    return checked(EngineeringProvenance::create(EngineeringDataOrigin::UserDefined,
                                                 "Synthetic " + revision + "/" + column));
  };
  std::vector<EngineeringTableColumn> columns;
  std::vector<EngineeringTableCell> cells;
  for (const auto& [column, value] : {std::pair{"system", "Synthetic table system"},
                                      {"designation", "Synthetic table size"},
                                      {"tolerance", "Synthetic table tolerance"}}) {
    columns.push_back({column, column, K::Text, omitted != column});
    if (omitted != column)
      cells.push_back({column, std::string(value), provenance(column)});
  }
  for (const auto& [column, value] :
       {std::pair{"major", 10.}, {"minor", 8.}, {"pitch", pitch}, {"angle", 55.}, {"taper", -2.}}) {
    const bool angle = std::string_view(column) == "angle" || std::string_view(column) == "taper";
    columns.push_back({column, column, angle ? K::Angle : K::Length, omitted != column});
    if (omitted != column)
      cells.push_back({column,
                       checked(angle ? Quantity::angle_deg(value) : Quantity::length_mm(value)),
                       provenance(column)});
  }
  return checked(EngineeringStandardTable::create(
      engineering_fixture::key("synthetic.threads", std::move(revision)), "Synthetic threads",
      engineering_fixture::provenance(), std::move(columns),
      {{"synthetic.row", "Synthetic row", std::move(cells)}}));
}
inline PartDocument part() {
  auto part = history_fixture::native_base();
  for (const auto& [id, value] :
       {std::pair{"major", 10.}, {"minor", 8.}, {"pitch", 1.7}, {"start", 1.}, {"length", 4.}})
    accept(part.add_parameter(checked(Parameter::create_length(
        ParameterId(std::string("thread.") + id), id, checked(Quantity::length_mm(value))))));
  for (const auto& [id, value] : {std::pair{"angle", 55.}, {"taper", -2.}})
    accept(part.add_parameter(checked(Parameter::create_angle(
        ParameterId(std::string("thread.") + id), id, checked(Quantity::angle_deg(value))))));
  accept(part.add_engineering_resource(table()));
  accept(part.add_engineering_resource(table("r2", 2.2)));
  return part;
}
inline PartDocument pair_part(std::string id = "thread.part", bool resources = true) {
  auto value = part();
  if (!resources) {
    accept(value.remove_engineering_resource(selected().table));
    accept(value.remove_engineering_resource(selected("r2").table));
  }
  auto data = nlohmann::json::parse(checked(serialize_part_document_to_json(value)));
  data["document"]["id"] = id;
  value = checked(deserialize_part_document_from_json(data.dump()));
  checked(value.set_parameter_value(ParameterId("height"), checked(Quantity::length_mm(16))));
  accept(value.add_parameter(checked(Parameter::create_length(
      ParameterId("drill.depth"), "Drill depth", checked(Quantity::length_mm(6))))));
  auto sketch =
      checked(Sketch::create(SketchId("shaft.profile"), "Shaft profile", DatumPlaneId("datum.xy")));
  accept(sketch.add_profile(checked(
      CircleProfile::create(ProfileId("shaft.circle"), ParameterId("thread.major"), {30, 0}))));
  accept(value.add_sketch(std::move(sketch)));
  const auto context = history_fixture::new_body(value, "shaft");
  accept(value.add_feature(checked(
      checked(Feature::create_additive_extrude(FeatureId("shaft"), "Shaft",
                                               SketchId("shaft.profile"), ParameterId("depth")))
          .with_body_result_context(context))));
  return value;
}
inline ThreadFeature external(ThreadDefinition thread = definition(), std::string id = "external") {
  auto support = checked(FaceReference::create_result(
      PartFeatureInputRole::ThreadSupportFace,
      checked(ResultFaceReference::create(
          FeatureId("shaft"), BodyId("body.shaft"),
          {{FeatureId("shaft"), "extrude/circle/shaft.circle/wall"}}))));
  return checked(ThreadFeature::create(FeatureId(std::move(id)), "External thread",
                                       BodyId("body.shaft"), std::move(support),
                                       std::move(thread)));
}
inline HoleFeature internal(ThreadDefinition thread = definition(), std::string id = "internal") {
  auto support = checked(PlaneReference::create_semantic_face(
      PartFeatureInputRole::HoleSupportPlane,
      checked(SemanticFaceReference::create(FeatureId("base"), SemanticFace::Top))));
  return checked(HoleFeature::create(
      FeatureId(std::move(id)), "Threaded hole", HoleKind::Plain, BodyId("body.base"),
      std::move(support), {{HolePlacementId("center"), Point2{}}}, ParameterId("thread.minor"),
      HoleBlindExtent{ParameterId("drill.depth")}, HoleDirection::OppositeSupportNormal,
      HoleFlatTip{}, {}, std::move(thread)));
}
} // namespace blcad::test::thread_fixture
