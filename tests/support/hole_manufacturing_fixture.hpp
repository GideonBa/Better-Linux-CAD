#pragma once
#include "hole_feature_fixture.hpp"

namespace blcad::test::manufacturing_fixture {
using history_fixture::accept;
using history_fixture::checked;
inline void dimension(PartDocument& part, const char* id, double value) {
  accept(part.add_parameter(
      checked(Parameter::create_length(ParameterId(id), id, checked(Quantity::length_mm(value))))));
}
inline HoleManufacturing variant(int mode, bool standard = false) {
  const auto d = [&](const char* parameter, const char* column) -> HoleDimension {
    return standard ? HoleDimension{HoleStandardSelection{hole_fixture::key(), "small", column}}
                    : HoleDimension{ParameterId(parameter)};
  };
  switch (mode) {
  case 1:
    return HoleCounterbore{d("seat.diameter", "seat.diameter"), d("seat.depth", "seat.depth")};
  case 2:
    return HoleCountersink{d("seat.diameter", "seat.diameter"), d("angle", "angle")};
  case 3:
    return HoleSpotface{d("seat.diameter", "seat.diameter"), d("seat.depth", "seat.depth")};
  case 4:
    return HoleStepped{
        {{"outer", d("outer.diameter", "outer.diameter"), d("outer.depth", "outer.depth")},
         {"inner", d("seat.diameter", "seat.diameter"), d("seat.depth", "seat.depth")}}};
  case 5:
  case 6:
    return HoleFitted{"Synthetic fit; no normative designation", d("minimum", "minimum"),
                      d("maximum", "maximum"),
                      mode == 6 ? HoleFitProcess::Reamed : HoleFitProcess::Fitted};
  case 7:
    return HoleCenter{d("seat.diameter", "seat.diameter"), d("angle", "angle")};
  case 8:
    return HoleSlot{checked(ProfileRegionReference::create(SketchId("slot"), ProfileId("slot")))};
  default:
    return std::monostate{};
  }
}
inline HoleFeature feature(int mode, HoleExtent extent = HoleThroughAllExtent{},
                           bool standard = false) {
  const auto old = hole_fixture::hole();
  if (mode == 7)
    extent = HoleBlindExtent{ParameterId("blind")};
  return checked(HoleFeature::create(
      old.id(), "Manufacturing Hole", old.kind(), old.target_body(), old.support(),
      old.placements(), ParameterId("diameter"), std::move(extent), old.direction(),
      mode == 7 ? HoleTip{HoleConicalTip{ParameterId("angle")}} : HoleTip{HoleFlatTip{}},
      variant(mode, standard)));
}
inline PartDocument seed(bool authored = false) {
  auto p = history_fixture::native_base();
  for (const auto& [id, value] : std::vector<std::pair<const char*, double>>{{"diameter", 2},
                                                                             {"seat.diameter", 4},
                                                                             {"seat.depth", 2},
                                                                             {"blind", 5},
                                                                             {"outer.diameter", 6},
                                                                             {"outer.depth", 1},
                                                                             {"minimum", 2},
                                                                             {"maximum", 2.1}})
    dimension(p, id, value);
  (void)checked(p.set_parameter_value(ParameterId("angle"), checked(Quantity::angle_deg(90))));
  accept(p.add_construction_plane(
      checked(ConstructionPlane::create_explicit(ConstructionPlaneId("slot.plane"), "Slot plane",
                                                 {0, 0, 8}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}))));
  auto sketch =
      checked(Sketch::create(SketchId("slot"), "Existing slot", DatumPlaneId("slot.plane")));
  accept(
      sketch.add_entity(checked(LineSegment::create(SketchEntityId("slot.top"), {-2, 1}, {2, 1}))));
  accept(sketch.add_entity(checked(
      ArcSegment::create_three_point(SketchEntityId("slot.right"), {2, 1}, {3, 0}, {2, -1}))));
  accept(sketch.add_entity(
      checked(LineSegment::create(SketchEntityId("slot.bottom"), {2, -1}, {-2, -1}))));
  accept(sketch.add_entity(checked(
      ArcSegment::create_three_point(SketchEntityId("slot.left"), {-2, -1}, {-3, 0}, {-2, 1}))));
  accept(sketch.add_profile(checked(ArcClosedProfile::create(
      ProfileId("slot"), {SketchEntityId("slot.top"), SketchEntityId("slot.right"),
                          SketchEntityId("slot.bottom"), SketchEntityId("slot.left")}))));
  accept(p.add_sketch(sketch));
  if (authored)
    accept(p.apply_history_intent({{"base"}, {}}));
  return p;
}
inline PartDocument part(int mode, HoleExtent extent = HoleThroughAllExtent{},
                         bool authored = false) {
  auto p = seed(authored);
  accept(p.add_hole_feature(feature(mode, std::move(extent))));
  return p;
}
inline EngineeringStandardTable table(std::string revision = "r1", bool missing_depth = false,
                                      double seat = 4) {
  std::vector<EngineeringTableColumn> columns;
  std::vector<EngineeringTableCell> cells;
  const auto source = engineering_fixture::provenance();
  for (const auto& [id, value] :
       std::vector<std::pair<const char*, double>>{{"seat.diameter", seat},
                                                   {"seat.depth", 2},
                                                   {"outer.diameter", 6},
                                                   {"outer.depth", 1},
                                                   {"minimum", 2},
                                                   {"maximum", 2.1}}) {
    columns.push_back({id, id, EngineeringTableValueKind::Length, false});
    if (!missing_depth || std::string(id) != "seat.depth")
      cells.push_back({id, checked(Quantity::length_mm(value)), source});
  }
  columns.push_back({"angle", "Angle", EngineeringTableValueKind::Angle});
  cells.push_back({"angle", checked(Quantity::angle_deg(90)), source});
  return checked(EngineeringStandardTable::create(
      hole_fixture::key(std::move(revision)), "Synthetic manufacturing table", source,
      std::move(columns), {{"small", "Synthetic", std::move(cells)}}));
}
} // namespace blcad::test::manufacturing_fixture
