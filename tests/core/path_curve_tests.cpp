#include "blcad/core/part_document_json.hpp"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

using namespace blcad;
using json = nlohmann::json;

namespace {
SketchCoordinate3D coordinate(double value) {
  auto quantity = Quantity::linear_displacement_mm(value, "path coordinate");
  REQUIRE(quantity);
  auto result = SketchCoordinate3D::create_explicit(quantity.value());
  REQUIRE(result);
  return result.value();
}

SketchPoint3D point(const char* id, double x, double y, double z) {
  auto result =
      SketchPoint3D::create(SketchEntityId(id), coordinate(x), coordinate(y), coordinate(z));
  REQUIRE(result);
  return result.value();
}

Parameter length_parameter(const char* id, double value) {
  auto quantity = Quantity::length_mm(value, id);
  REQUIRE(quantity);
  auto result = Parameter::create_length(ParameterId(id), id, quantity.value());
  REQUIRE(result);
  return result.value();
}

PartDocument path_document() {
  auto document = PartDocument::create(DocumentId("part.path"), "Path document");
  REQUIRE(document);
  REQUIRE(document.value().add_datum_plane(DatumPlane::xy().value()));

  auto planar = Sketch::create(SketchId("sketch.planar"), "Planar", DatumPlaneId("datum.xy"));
  REQUIRE(planar);
  auto first = LineSegment::create(SketchEntityId("line.a"), {0.0, 0.0}, {1.0, 0.0});
  auto second = LineSegment::create(SketchEntityId("line.b"), {1.0 + 5.0e-8, 0.0}, {2.0, 0.0});
  auto disconnected = LineSegment::create(SketchEntityId("line.far"), {4.0, 0.0}, {5.0, 0.0});
  auto closing = LineSegment::create(SketchEntityId("line.closing"), {2.0, 0.0}, {0.0, 0.0});
  REQUIRE(first);
  REQUIRE(second);
  REQUIRE(disconnected);
  REQUIRE(closing);
  REQUIRE(planar.value().add_entity(first.value()));
  REQUIRE(planar.value().add_entity(second.value()));
  REQUIRE(planar.value().add_entity(disconnected.value()));
  REQUIRE(planar.value().add_entity(closing.value()));
  REQUIRE(document.value().add_sketch(planar.value()));

  auto spatial = Sketch3D::create(Sketch3DId("sketch3d.path"), "Spatial");
  REQUIRE(spatial);
  REQUIRE(spatial.value().add_point(point("p0", 0.0, 0.0, 0.0)));
  REQUIRE(spatial.value().add_point(point("p1", 1.0, 0.0, 0.0)));
  REQUIRE(spatial.value().add_point(point("p2", 2.0, 1.0, 0.0)));
  auto line_a =
      SketchLine3D::create(SketchEntityId("line3d.a"), SketchEntityId("p0"), SketchEntityId("p1"));
  auto line_b =
      SketchLine3D::create(SketchEntityId("line3d.b"), SketchEntityId("p2"), SketchEntityId("p1"));
  REQUIRE(line_a);
  REQUIRE(line_b);
  REQUIRE(spatial.value().add_line(line_a.value()));
  REQUIRE(spatial.value().add_line(line_b.value()));
  REQUIRE(document.value().add_sketch_3d(spatial.value()));

  auto construction = ConstructionLine::create_explicit(ConstructionLineId("line.construction"),
                                                        "Construction", {}, {1.0, 0.0, 0.0});
  REQUIRE(construction);
  REQUIRE(document.value().add_construction_line(construction.value()));

  REQUIRE(document.value().add_parameter(length_parameter("profile.width", 10.0)));
  REQUIRE(document.value().add_parameter(length_parameter("profile.height", 8.0)));
  REQUIRE(document.value().add_parameter(length_parameter("feature.depth", 2.0)));
  auto profile_sketch =
      Sketch::create(SketchId("sketch.profile"), "Profile", DatumPlaneId("datum.xy"));
  REQUIRE(profile_sketch);
  auto rectangle = RectangleProfile::create(
      ProfileId("profile.rectangle"), ParameterId("profile.width"), ParameterId("profile.height"));
  REQUIRE(rectangle);
  REQUIRE(profile_sketch.value().add_profile(rectangle.value()));
  REQUIRE(document.value().add_sketch(profile_sketch.value()));
  auto feature = Feature::create_additive_extrude(
      FeatureId("feature.base"), "Base", SketchId("sketch.profile"), ParameterId("feature.depth"));
  REQUIRE(feature);
  REQUIRE(document.value().add_feature(feature.value()));
  return document.value();
}

PathSegmentReference planar_segment(const char* entity, bool reversed = false) {
  auto result = PathSegmentReference::create_planar(
      SketchId("sketch.planar"), SketchEntityId(entity), PlanarPathCurveKind::Line, reversed);
  REQUIRE(result);
  return result.value();
}

PathSegmentReference spatial_segment(const char* entity, bool reversed = false) {
  auto result = PathSegmentReference::create_sketch_3d(
      Sketch3DId("sketch3d.path"), SketchEntityId(entity), Sketch3DPathCurveKind::Line, reversed);
  REQUIRE(result);
  return result.value();
}

PathCurve path(const char* id, std::vector<PathSegmentReference> segments,
               PathOrientationRule orientation = PathOrientationRule::MinimumTwist,
               std::optional<Vector3> up = std::nullopt, double tolerance = 1.0e-7) {
  auto result = PathCurve::create(PathCurveId(id), id, std::move(segments), PathClosure::Open,
                                  orientation, up, PathContinuityHint::G1, tolerance);
  REQUIRE(result);
  return result.value();
}
} // namespace

TEST_CASE("Block 79 validates ordered planar connectivity with an explicit tolerance",
          "[core][path-curve]") {
  auto document = path_document();
  REQUIRE(document.add_path_curve(
      path("path.planar", {planar_segment("line.a"), planar_segment("line.b")})));
  CHECK(document.path_curve_count() == 1U);
  CHECK(document.find_path_curve(PathCurveId("path.planar"))->segments().size() == 2U);

  auto too_strict = PathCurve::create(
      PathCurveId("path.strict"), "Strict", {planar_segment("line.a"), planar_segment("line.b")},
      PathClosure::Open, PathOrientationRule::ProfileNormal, std::nullopt, std::nullopt, 1.0e-9);
  REQUIRE(too_strict);
  CHECK(document.add_path_curve(too_strict.value()).has_error());
  CHECK(document
            .add_path_curve(
                path("path.disconnected", {planar_segment("line.a"), planar_segment("line.far")}))
            .has_error());
  auto open_loop = PathCurve::create(
      PathCurveId("path.open-loop"), "Open loop",
      {planar_segment("line.a"), planar_segment("line.b"), planar_segment("line.closing")},
      PathClosure::Open, PathOrientationRule::MinimumTwist);
  REQUIRE(open_loop);
  CHECK(document.add_path_curve(open_loop.value()).has_error());
  auto closed = PathCurve::create(
      PathCurveId("path.closed"), "Closed",
      {planar_segment("line.a"), planar_segment("line.b"), planar_segment("line.closing")},
      PathClosure::Closed, PathOrientationRule::MinimumTwist);
  REQUIRE(closed);
  CHECK(document.add_path_curve(closed.value()));
}

TEST_CASE("Block 79 preserves authored segment direction and source dependencies",
          "[core][path-curve]") {
  auto document = path_document();
  REQUIRE(document.add_path_curve(
      path("path.spatial", {spatial_segment("line3d.a"), spatial_segment("line3d.b", true)})));
  const PathCurve* stored = document.find_path_curve(PathCurveId("path.spatial"));
  REQUIRE(stored != nullptr);
  CHECK_FALSE(stored->segments().front().reversed());
  CHECK(stored->segments().back().reversed());
  CHECK(document.dependency_graph().has_dependency("sketch3d.path", "path.spatial"));
  CHECK(document.remove_sketch_3d(Sketch3DId("sketch3d.path")).has_error());
  CHECK(document.remove_path_curve(PathCurveId("path.spatial")));
  CHECK(document.remove_sketch_3d(Sketch3DId("sketch3d.path")));
}

TEST_CASE("Block 79 roundtrips every path source family and orientation metadata",
          "[core][path-curve]") {
  auto document = path_document();
  REQUIRE(document.add_path_curve(path("path.planar", {planar_segment("line.a")})));
  REQUIRE(
      document.add_path_curve(path("path.spatial", {spatial_segment("line3d.a")},
                                   PathOrientationRule::FixedUpVector, Vector3{0.0, 0.0, 1.0})));
  auto construction =
      PathSegmentReference::create_construction_line(ConstructionLineId("line.construction"));
  REQUIRE(construction);
  REQUIRE(document.add_path_curve(path("path.construction", {construction.value()})));
  auto edge = SemanticEdgeReference::create(FeatureId("feature.base"), SemanticEdge::TopFront);
  REQUIRE(edge);
  auto semantic = PathSegmentReference::create_semantic_edge(edge.value(), true);
  REQUIRE(semantic);
  REQUIRE(document.add_path_curve(path("path.semantic", {semantic.value()})));

  auto serialized = serialize_part_document_to_json(document);
  REQUIRE(serialized);
  auto restored = deserialize_part_document_from_json(serialized.value());
  REQUIRE(restored);
  CHECK(restored.value().path_curve_count() == 4U);
  const auto* spatial = restored.value().find_path_curve(PathCurveId("path.spatial"));
  REQUIRE(spatial != nullptr);
  CHECK(spatial->orientation_rule() == PathOrientationRule::FixedUpVector);
  REQUIRE(spatial->fixed_up_vector().has_value());
  CHECK(spatial->fixed_up_vector()->z == 1.0);
  CHECK(spatial->continuity_hint() == PathContinuityHint::G1);
  CHECK(
      restored.value().find_path_curve(PathCurveId("path.semantic"))->segments()[0].source_kind() ==
      PathSegmentSourceKind::SemanticGeneratedEdge);
}

TEST_CASE("Block 79 JSON is additive and rejects malformed path intent", "[core][path-curve]") {
  auto document = path_document();
  auto serialized = serialize_part_document_to_json(document);
  REQUIRE(serialized);
  json historical = json::parse(serialized.value());
  historical.erase("path_curves");
  auto restored = deserialize_part_document_from_json(historical.dump());
  REQUIRE(restored);
  CHECK(restored.value().path_curve_count() == 0U);

  REQUIRE(document.add_path_curve(path("path.planar", {planar_segment("line.a")})));
  auto with_path = serialize_part_document_to_json(document);
  REQUIRE(with_path);
  json malformed = json::parse(with_path.value());
  malformed["path_curves"][0]["unexpected"] = true;
  CHECK(deserialize_part_document_from_json(malformed.dump()).has_error());

  CHECK(PathCurve::create(PathCurveId("path.bad-up"), "Bad", {planar_segment("line.a")},
                          PathClosure::Open, PathOrientationRule::FixedUpVector)
            .has_error());
  CHECK(PathCurve::create(PathCurveId("path.duplicate"), "Duplicate",
                          {planar_segment("line.a"), planar_segment("line.a")}, PathClosure::Open,
                          PathOrientationRule::MinimumTwist)
            .has_error());
}
