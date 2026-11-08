#include "../support/engineering_resource_fixture.hpp"
#include "../support/part_history_fixture.hpp"
#include "blcad/core/hole_feature.hpp"
#include "blcad/core/part_document_json.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <limits>

using namespace blcad;
namespace resources = blcad::test::engineering_fixture;
namespace history = blcad::test::history_fixture;
using history::accept;
using history::checked;
namespace {
PlaneReference support(PartFeatureInputRole role = PartFeatureInputRole::HoleSupportPlane) {
  return checked(PlaneReference::create_datum_plane(role, DatumPlaneId("datum.xy")));
}
std::vector<HolePlacement> points() {
  return {{HolePlacementId("point.a"), Point2{-3, 0}}, {HolePlacementId("point.b"), Point2{3, 0}}};
}
HoleFeature hole(HoleDiameter diameter = ParameterId("width"),
                 HoleExtent extent = HoleThroughAllExtent{}, HoleTip tip = HoleFlatTip{},
                 std::vector<HolePlacement> placements = points()) {
  return checked(HoleFeature::create(FeatureId("hole"), "Semantic hole", HoleKind::Plain,
                                     BodyId("body.base"), support(), std::move(placements),
                                     std::move(diameter), std::move(extent),
                                     HoleDirection::OppositeSupportNormal, std::move(tip)));
}
EngineeringStandardTable table(std::string revision, double diameter, bool cell_present = true) {
  std::vector<EngineeringTableColumn> columns{
      {"diameter", "Synthetic diameter", EngineeringTableValueKind::Length, false},
      {"label", "Designation", EngineeringTableValueKind::Text}};
  std::vector<EngineeringTableCell> cells{
      {"label", std::string("Synthetic M6 normal"), resources::provenance()}};
  if (cell_present)
    cells.push_back(
        {"diameter", checked(Quantity::length_mm(diameter)),
         checked(EngineeringProvenance::create(EngineeringDataOrigin::Manufacturer,
                                               "Synthetic diameter source " + revision))});
  return checked(EngineeringStandardTable::create(
      resources::key("synthetic.holes", revision), "Synthetic clearance table",
      resources::provenance(), std::move(columns),
      {{"m6.normal", "Synthetic row", std::move(cells)}}));
}
HoleStandardSelection selected(std::string revision = "r1", std::string row = "m6.normal",
                               std::string column = "diameter") {
  return {resources::key("synthetic.holes", std::move(revision)), std::move(row),
          std::move(column)};
}
} // namespace

TEST_CASE("Semantic Hole intent preserves stable independent placements and bounded typed modes",
          "[core][parity-b152][hole-intent]") {
  const auto f = hole();
  CHECK(f.id() == FeatureId("hole"));
  CHECK(f.name() == "Semantic hole");
  CHECK(f.kind() == HoleKind::Plain);
  CHECK(to_string(f.kind()) == "plain");
  CHECK(to_string(HoleKind::Clearance) == "clearance");
  CHECK(to_string(HoleDirection::SupportNormal) == "support_normal");
  CHECK(to_string(f.direction()) == "opposite_support_normal");
  CHECK(to_string(static_cast<HoleKind>(99)) == "unknown");
  CHECK(to_string(static_cast<HoleDirection>(99)) == "unknown");
  CHECK(f.target_body() == BodyId("body.base"));
  CHECK(f.support().role() == PartFeatureInputRole::HoleSupportPlane);
  CHECK(f.direction() == HoleDirection::OppositeSupportNormal);
  CHECK(std::holds_alternative<HoleThroughAllExtent>(f.extent()));
  CHECK(std::holds_alternative<HoleFlatTip>(f.tip()));
  REQUIRE(f.placements().size() == 2);
  CHECK(f.placements()[0].id == HolePlacementId("point.a"));
  CHECK(std::get<Point2>(f.placements()[0].source) == Point2{-3, 0});
  CHECK(f.parameter_dependencies() == std::vector{ParameterId("width")});
  const auto blind = hole(ParameterId("width"), HoleBlindExtent{ParameterId("depth")},
                          HoleConicalTip{ParameterId("angle")});
  CHECK(blind.parameter_dependencies() ==
        std::vector{ParameterId("width"), ParameterId("depth"), ParameterId("angle")});
  CHECK(hole(ParameterId("width"), HoleSymmetricExtent{ParameterId("width")})
            .parameter_dependencies() == std::vector{ParameterId("width")});
  const auto clearance = checked(HoleFeature::create(
      FeatureId("clearance"), "Clearance", HoleKind::Clearance, BodyId("body.base"), support(),
      points(), selected(), HoleToNextExtent{}, HoleDirection::SupportNormal));
  CHECK(clearance.kind() == HoleKind::Clearance);
  CHECK(clearance.direction() == HoleDirection::SupportNormal);
  CHECK(clearance.parameter_dependencies().empty());
}

TEST_CASE("Hole factories reject malformed identities coordinates roles extents and tips",
          "[core][parity-b152][hole-intent]") {
  const auto create = [](std::vector<HolePlacement> placements,
                         HoleDiameter diameter = ParameterId("width"),
                         HoleExtent extent = HoleThroughAllExtent{}, HoleTip tip = HoleFlatTip{}) {
    return HoleFeature::create(FeatureId("hole"), "Hole", HoleKind::Plain, BodyId("body.base"),
                               support(), std::move(placements), std::move(diameter),
                               std::move(extent), HoleDirection::SupportNormal, std::move(tip));
  };
  CHECK_FALSE(create({}));
  auto duplicate = points();
  duplicate[1].id = duplicate[0].id;
  CHECK_FALSE(create(duplicate));
  CHECK_FALSE(create({{HolePlacementId{}, Point2{}}}));
  for (double v :
       {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
    CHECK_FALSE(create({{HolePlacementId("p"), Point2{v, 0}}}));
    CHECK_FALSE(create({{HolePlacementId("p"), Point2{0, v}}}));
  }
  CHECK_FALSE(
      create({{HolePlacementId("p"), HoleSketchPointReference{SketchId{}, SketchEntityId("p")}}}));
  CHECK_FALSE(
      create({{HolePlacementId("p"), HoleSketchPointReference{SketchId("s"), SketchEntityId{}}}}));
  CHECK_FALSE(create(points(), ParameterId{}));
  CHECK_FALSE(create(points(), selected("r1", " ")));
  CHECK_FALSE(create(points(), selected("r1", "m6.normal", "")));
  CHECK_FALSE(create(points(), ParameterId("width"), HoleBlindExtent{ParameterId{}}));
  CHECK_FALSE(create(points(), ParameterId("width"), HoleSymmetricExtent{ParameterId{}}));
  CHECK_FALSE(create(points(), ParameterId("width"), HoleUpToBodyExtent{BodyId{}}));
  CHECK_FALSE(create(points(), ParameterId("width"), HoleThroughAllExtent{},
                     HoleConicalTip{ParameterId("angle")}));
  CHECK_FALSE(create(points(), ParameterId("width"), HoleBlindExtent{ParameterId("depth")},
                     HoleConicalTip{ParameterId{}}));
  for (const auto kind : {HoleKind::Plain, static_cast<HoleKind>(99)})
    CHECK_FALSE(HoleFeature::create(FeatureId{}, "Hole", kind, BodyId("body.base"), support(),
                                    points(), ParameterId("width")));
  CHECK_FALSE(HoleFeature::create(FeatureId("h"), " ", HoleKind::Plain, BodyId("body.base"),
                                  support(), points(), ParameterId("width")));
  CHECK_FALSE(HoleFeature::create(FeatureId("h"), "Hole", HoleKind::Plain, BodyId{}, support(),
                                  points(), ParameterId("width")));
  CHECK_FALSE(HoleFeature::create(FeatureId("h"), "Hole", static_cast<HoleKind>(99),
                                  BodyId("body.base"), support(), points(), ParameterId("width")));
  CHECK_FALSE(HoleFeature::create(FeatureId("h"), "Hole", HoleKind::Plain, BodyId("body.base"),
                                  support(), points(), ParameterId("width"), HoleThroughAllExtent{},
                                  static_cast<HoleDirection>(99)));
  CHECK_FALSE(HoleFeature::create(FeatureId("h"), "Hole", HoleKind::Plain, BodyId("body.base"),
                                  support(PartFeatureInputRole::MirrorPlane), points(),
                                  ParameterId("width")));
  auto face = checked(FaceReference::create_planar(
      PartFeatureInputRole::ExtrudeLimitFace,
      checked(SemanticFaceReference::create(FeatureId("base"), SemanticFace::Top))));
  CHECK_FALSE(create(points(), ParameterId("width"), HoleToFaceExtent{face}));
}

TEST_CASE("Pinned Hole standards resolve exact typed cells and their provenance without mutation",
          "[core][parity-b152][hole-standards]") {
  auto part = history::native_base();
  accept(part.add_engineering_resource(table("r1", 6.6)));
  accept(part.add_engineering_resource(table("r2", 7.1)));
  accept(part.add_engineering_resource(table("empty", 1, false)));
  accept(part.add_engineering_resource(resources::material()));
  const auto mixed = resources::table();
  accept(part.add_engineering_resource(mixed));
  const auto before = checked(serialize_part_document_to_json(part));
  const auto generation = part.executable_generation();
  const auto a = checked(resolve_hole_diameter(part, selected()));
  const auto b = checked(resolve_hole_diameter(part, selected("r2")));
  CHECK(a.diameter.millimeters() == Catch::Approx(6.6));
  CHECK(b.diameter.millimeters() == Catch::Approx(7.1));
  REQUIRE(a.provenance);
  CHECK(a.provenance->origin() == EngineeringDataOrigin::Manufacturer);
  CHECK(a.provenance->source() == "Synthetic diameter source r1");
  CHECK_FALSE(resolve_hole_diameter(part, selected("latest")));
  CHECK_FALSE(resolve_hole_diameter(part, selected("r1", "missing")));
  CHECK_FALSE(resolve_hole_diameter(part, selected("r1", "m6.normal", "missing")));
  CHECK_FALSE(resolve_hole_diameter(part, selected("r1", "m6.normal", "label")));
  CHECK_FALSE(resolve_hole_diameter(part, selected("empty")));
  CHECK_FALSE(resolve_hole_diameter(part, selected("r1", "")));
  CHECK_FALSE(resolve_hole_diameter(part, ParameterId{}));
  CHECK_FALSE(resolve_hole_diameter(part, ParameterId("missing")));
  CHECK_FALSE(resolve_hole_diameter(part, ParameterId("count")));
  CHECK_FALSE(resolve_hole_diameter(part, ParameterId("angle")));
  CHECK_FALSE(resolve_hole_diameter(
      part, HoleStandardSelection{resources::key(), "m6.normal", "diameter"}));
  for (const auto& column : mixed.columns())
    if (column.kind != EngineeringTableValueKind::Length)
      CHECK_FALSE(resolve_hole_diameter(
          part, HoleStandardSelection{mixed.key(), mixed.rows().front().key, column.key}));
  const auto custom = checked(resolve_hole_diameter(part, ParameterId("width")));
  CHECK(custom.diameter.millimeters() == Catch::Approx(20));
  CHECK_FALSE(custom.provenance);
  CHECK(checked(serialize_part_document_to_json(part)) == before);
  CHECK(part.executable_generation() == generation);
  auto roundtrip = checked(deserialize_part_document_from_json(before));
  CHECK(checked(resolve_hole_diameter(roundtrip, selected())).diameter == a.diameter);
  CHECK(checked(resolve_hole_diameter(roundtrip, selected())).provenance == a.provenance);
}

TEST_CASE(
    "Hole document input validation shares native semantic support and typed parameter authorities",
    "[core][parity-b152][hole-inputs]") {
  auto part = history::native_base();
  const auto before = checked(serialize_part_document_to_json(part));
  REQUIRE(validate_hole_feature_inputs(part, hole()));
  REQUIRE(validate_hole_feature_inputs(
      part, hole(ParameterId("width"), HoleBlindExtent{ParameterId("depth")})));
  REQUIRE(validate_hole_feature_inputs(
      part, hole(ParameterId("width"), HoleSymmetricExtent{ParameterId("depth")})));
  CHECK_FALSE(validate_hole_feature_inputs(part, hole(ParameterId("count"))));
  CHECK_FALSE(validate_hole_feature_inputs(
      part, hole(ParameterId("width"), HoleBlindExtent{ParameterId("count")})));
  CHECK_FALSE(validate_hole_feature_inputs(
      part, hole(ParameterId("width"), HoleUpToBodyExtent{BodyId("missing")})));
  CHECK_FALSE(validate_hole_feature_inputs(
      part, hole(ParameterId("width"), HoleUpToBodyExtent{BodyId("body.base")})));
  auto on_top = checked(PlaneReference::create_semantic_face(
      PartFeatureInputRole::HoleSupportPlane,
      checked(SemanticFaceReference::create(FeatureId("base"), SemanticFace::Top))));
  REQUIRE(validate_hole_feature_inputs(
      part,
      checked(HoleFeature::create(FeatureId("top.hole"), "Top hole", HoleKind::Plain,
                                  BodyId("body.base"), on_top, points(), ParameterId("width")))));
  CHECK_FALSE(validate_hole_feature_inputs(
      part, hole(ParameterId("width"), HoleThroughAllExtent{}, HoleFlatTip{},
                 {{HolePlacementId("p"),
                   HoleSketchPointReference{SketchId("profile"), SketchEntityId("missing")}}})));
  CHECK(checked(serialize_part_document_to_json(part)) == before);
  accept(part.add_parameter(checked(Parameter::create_angle(ParameterId("tip.angle"), "Tip angle",
                                                            checked(Quantity::angle_deg(118))))));
  REQUIRE(validate_hole_feature_inputs(part, hole(ParameterId("width"),
                                                  HoleBlindExtent{ParameterId("depth")},
                                                  HoleConicalTip{ParameterId("tip.angle")})));
  for (double angle : {0., -1., 180., 181.}) {
    auto candidate = part;
    REQUIRE(candidate.set_parameter_value(ParameterId("tip.angle"),
                                          checked(Quantity::angle_deg(angle))));
    CHECK_FALSE(validate_hole_feature_inputs(
        candidate, hole(ParameterId("width"), HoleBlindExtent{ParameterId("depth")},
                        HoleConicalTip{ParameterId("tip.angle")})));
  }
  CHECK_FALSE(validate_hole_feature_inputs(part, hole(ParameterId("width"),
                                                      HoleBlindExtent{ParameterId("depth")},
                                                      HoleConicalTip{ParameterId("width")})));
  CHECK(part.features().size() == 1); // Validation never publishes another executable record.
}

TEST_CASE("Hole point and limit inputs retain existing document identities without adding geometry",
          "[core][parity-b152][hole-inputs]") {
  auto part = history::native_base();
  auto sketch =
      checked(Sketch::create(SketchId("placement"), "Placement", DatumPlaneId("datum.xy")));
  accept(sketch.add_entity(checked(SketchPoint::create(SketchEntityId("center"), {-2, 3}))));
  accept(part.add_sketch(std::move(sketch)));
  accept(part.add_body(checked(Body::create(BodyId("limit"), "Limit", BodyKind::Solid))));
  accept(part.add_body(checked(Body::create(BodyId("sheet"), "Sheet", BodyKind::Surface))));
  const auto before = checked(serialize_part_document_to_json(part));
  const auto generation = part.executable_generation();
  const auto placement = std::vector<HolePlacement>{
      {HolePlacementId("p"),
       HoleSketchPointReference{SketchId("placement"), SketchEntityId("center")}}};
  REQUIRE(validate_hole_feature_inputs(
      part, hole(ParameterId("width"), HoleThroughAllExtent{}, HoleFlatTip{}, placement)));
  REQUIRE(validate_hole_feature_inputs(part, hole(ParameterId("width"), HoleToNextExtent{})));
  REQUIRE(validate_hole_feature_inputs(
      part, hole(ParameterId("width"), HoleUpToBodyExtent{BodyId("limit")})));
  CHECK_FALSE(validate_hole_feature_inputs(
      part, hole(ParameterId("width"), HoleUpToBodyExtent{BodyId("sheet")})));
  const auto face = checked(FaceReference::create_planar(
      PartFeatureInputRole::HoleLimitFace,
      checked(SemanticFaceReference::create(FeatureId("base"), SemanticFace::Top))));
  REQUIRE(validate_hole_feature_inputs(part, hole(ParameterId("width"), HoleToFaceExtent{face})));
  const auto missing_face = checked(FaceReference::create_planar(
      PartFeatureInputRole::HoleLimitFace,
      checked(SemanticFaceReference::create(FeatureId("missing"), SemanticFace::Top))));
  CHECK_FALSE(validate_hole_feature_inputs(
      part, hole(ParameterId("width"), HoleToFaceExtent{missing_face})));
  for (const auto& body : {BodyId("missing"), BodyId("sheet")})
    CHECK_FALSE(validate_hole_feature_inputs(
        part, checked(HoleFeature::create(FeatureId("hole"), "Hole", HoleKind::Plain, body,
                                          support(), points(), ParameterId("width")))));
  const auto missing_support = checked(PlaneReference::create_datum_plane(
      PartFeatureInputRole::HoleSupportPlane, DatumPlaneId("missing")));
  CHECK_FALSE(validate_hole_feature_inputs(
      part,
      checked(HoleFeature::create(FeatureId("hole"), "Hole", HoleKind::Plain, BodyId("body.base"),
                                  missing_support, points(), ParameterId("width")))));
  auto reference = history::imported(true);
  const auto refused = validate_hole_feature_inputs(reference, hole());
  REQUIRE_FALSE(refused);
  CHECK(refused.error().message().find("Reference Part") != std::string::npos);
  CHECK(checked(serialize_part_document_to_json(part)) == before);
  CHECK(part.executable_generation() == generation);
  CHECK(part.features().size() == 1);
}

TEST_CASE("Hole reference roles accept only plane support and face limits",
          "[core][parity-b152][hole-inputs]") {
  CHECK(to_string(PartFeatureInputRole::HoleSupportPlane) == "hole_support_plane");
  CHECK(to_string(PartFeatureInputRole::HoleLimitFace) == "hole_limit_face");
  CHECK(part_feature_input_role_accepts(PartFeatureInputRole::HoleSupportPlane,
                                        PartFeatureInputCapability::Plane));
  CHECK_FALSE(part_feature_input_role_accepts(PartFeatureInputRole::HoleSupportPlane,
                                              PartFeatureInputCapability::Face));
  CHECK(part_feature_input_role_accepts(PartFeatureInputRole::HoleLimitFace,
                                        PartFeatureInputCapability::Face));
  CHECK_FALSE(part_feature_input_role_accepts(PartFeatureInputRole::HoleLimitFace,
                                              PartFeatureInputCapability::Body));
  auto imported_plane = checked(ImportedTopologyReference::create(
      ImportedTopologyCatalogId("catalog"), ImportedTopologyId("face"),
      ImportedTopologyCapability::Plane));
  auto imported_face = checked(ImportedTopologyReference::create(
      ImportedTopologyCatalogId("catalog"), ImportedTopologyId("face"),
      ImportedTopologyCapability::Face));
  CHECK(PlaneReference::create_imported(PartFeatureInputRole::HoleSupportPlane, imported_plane));
  CHECK_FALSE(
      PlaneReference::create_imported(PartFeatureInputRole::HoleSupportPlane, imported_face));
  CHECK(FaceReference::create_imported(PartFeatureInputRole::HoleLimitFace, imported_face));
}
