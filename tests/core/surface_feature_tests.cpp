#include "blcad/core/part_document_json.hpp"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

using namespace blcad;
using json = nlohmann::json;

namespace {

Body make_body(const char* id, BodyKind kind) {
  auto body = Body::create(BodyId(id), id, kind);
  REQUIRE(body);
  return body.value();
}

BoundaryCurveReference boundary(const char* id) {
  auto reference = BoundaryCurveReference::create_path_curve(PathCurveId(id));
  REQUIRE(reference);
  return reference.value();
}

SurfaceReference surface(const char* id) {
  auto reference = SurfaceReference::create_body(BodyId(id));
  REQUIRE(reference);
  return reference.value();
}

void add_open_path(PartDocument& document, const char* sketch_id, const char* entity_id,
                   const char* path_id, double y) {
  auto sketch = Sketch::create(SketchId(sketch_id), sketch_id, DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto line = LineSegment::create(SketchEntityId(entity_id), {0.0, y}, {10.0, y});
  REQUIRE(line);
  REQUIRE(sketch.value().add_entity(line.value()));
  REQUIRE(document.add_sketch(sketch.value()));
  auto segment = PathSegmentReference::create_planar(SketchId(sketch_id), SketchEntityId(entity_id),
                                                     PlanarPathCurveKind::Line);
  REQUIRE(segment);
  auto path = PathCurve::create(PathCurveId(path_id), path_id, {segment.value()}, PathClosure::Open,
                                PathOrientationRule::MinimumTwist);
  REQUIRE(path);
  REQUIRE(document.add_path_curve(path.value()));
}

PartDocument surface_document() {
  auto created = PartDocument::create(DocumentId("part.surface"), "Surface intent");
  REQUIRE(created);
  PartDocument document = created.value();
  REQUIRE(document.add_datum_plane(DatumPlane::xy().value()));
  add_open_path(document, "sketch.a", "line.a", "path.a", 0.0);
  add_open_path(document, "sketch.b", "line.b", "path.b", 10.0);
  for (const char* id :
       {"surface.boundary", "surface.fill", "surface.trim", "surface.extend", "surface.stitch"})
    REQUIRE(document.add_body(make_body(id, BodyKind::Surface)));
  REQUIRE(document.add_body(make_body("solid.closed", BodyKind::Solid)));
  auto distance = Quantity::length_mm(5.0, "distance");
  auto tolerance = Quantity::length_mm(0.01, "tolerance");
  REQUIRE(distance);
  REQUIRE(tolerance);
  REQUIRE(document.add_parameter(
      Parameter::create_length(ParameterId("distance"), "distance", distance.value()).value()));
  REQUIRE(document.add_parameter(
      Parameter::create_length(ParameterId("tolerance"), "tolerance", tolerance.value()).value()));
  return document;
}

void add_surface_chain(PartDocument& document) {
  auto boundary_feature = BoundarySurfaceFeature::create(
      FeatureId("surface.boundary.feature"), "Boundary", {boundary("path.a"), boundary("path.b")},
      BodyId("surface.boundary"));
  auto fill =
      FillSurfaceFeature::create(FeatureId("surface.fill.feature"), "Fill",
                                 {boundary("path.a"), boundary("path.b")}, BodyId("surface.fill"));
  REQUIRE(boundary_feature);
  REQUIRE(fill);
  REQUIRE(document.add_surface_feature(boundary_feature.value()));
  REQUIRE(document.add_surface_feature(fill.value()));

  auto trimming = TrimmingReference::create_boundary_curve(boundary("path.a"));
  REQUIRE(trimming);
  auto trim = TrimSurfaceFeature::create(FeatureId("surface.trim.feature"), "Trim",
                                         surface("surface.boundary"), trimming.value(),
                                         BodyId("surface.trim"));
  auto extend = ExtendSurfaceFeature::create(FeatureId("surface.extend.feature"), "Extend",
                                             surface("surface.trim"), boundary("path.b"),
                                             ParameterId("distance"), BodyId("surface.extend"));
  REQUIRE(trim);
  REQUIRE(extend);
  REQUIRE(document.add_surface_feature(trim.value()));
  REQUIRE(document.add_surface_feature(extend.value()));

  auto stitch = SurfaceStitchFeature::create(FeatureId("surface.stitch.feature"), "Stitch",
                                             {surface("surface.fill"), surface("surface.extend")},
                                             ParameterId("tolerance"), BodyId("surface.stitch"));
  REQUIRE(stitch);
  REQUIRE(document.add_surface_feature(stitch.value()));
  auto solid = ClosedShellToSolidFeature::create(FeatureId("surface.solid.feature"), "Solid",
                                                 surface("surface.stitch"), BodyId("solid.closed"));
  REQUIRE(solid);
  REQUIRE(document.add_surface_feature(solid.value()));
}

} // namespace

TEST_CASE("Block 88 freezes the first six Surface feature intents", "[core][surface-feature]") {
  CHECK(to_string(SurfaceFeatureKind::BoundarySurface) == "boundary_surface");
  CHECK(to_string(SurfaceFeatureKind::ClosedShellToSolid) == "closed_shell_to_solid");
  CHECK(BoundarySurfaceFeature::create(FeatureId("bad"), "Bad", {boundary("path.a")},
                                       BodyId("surface"))
            .has_error());
  CHECK(SurfaceStitchFeature::create(FeatureId("bad.stitch"), "Bad", {surface("surface.a")},
                                     ParameterId("tolerance"), BodyId("surface.result"))
            .has_error());
  auto trim = TrimmingReference::create_boundary_curve(boundary("path.a"));
  REQUIRE(trim);
  CHECK(trim.value().source_kind() == TrimmingReferenceSourceKind::BoundaryCurve);
  auto profile = ProfileRegionReference::create(SketchId("sketch"), ProfileId("profile"));
  REQUIRE(profile);
  auto profile_trim = TrimmingReference::create_profile_region(profile.value());
  REQUIRE(profile_trim);
  CHECK(profile_trim.value().source_kind() == TrimmingReferenceSourceKind::ProfileRegion);
  auto edge = SemanticEdgeReference::create(FeatureId("feature"), SemanticEdge::TopFront);
  auto face = SemanticFaceReference::create(FeatureId("feature"), SemanticFace::Top);
  REQUIRE(edge);
  REQUIRE(face);
  auto edge_boundary = BoundaryCurveReference::create_semantic_edge(edge.value());
  auto face_surface = SurfaceReference::create_semantic_face(face.value());
  REQUIRE(edge_boundary);
  REQUIRE(face_surface);
  CHECK(edge_boundary.value().source_kind() == BoundaryCurveSourceKind::SemanticEdge);
  CHECK(face_surface.value().source_kind() == SurfaceReferenceSourceKind::SemanticFace);
}

TEST_CASE("Block 88 integrates Surface dependencies, invalidation, and removal",
          "[core][surface-feature]") {
  auto document = surface_document();
  add_surface_chain(document);
  CHECK(document.surface_feature_count() == 6U);
  CHECK(document.dependency_graph().has_dependency("path.a", "surface.boundary.feature"));
  CHECK(
      document.dependency_graph().has_dependency("body:surface.boundary", "surface.trim.feature"));
  CHECK(document.dependency_graph().has_dependency("distance", "surface.extend.feature"));
  CHECK(document.dependency_graph().has_dependency("surface.solid.feature", "body:solid.closed"));
  CHECK(document.remove_path_curve(PathCurveId("path.a")).has_error());
  CHECK(document.remove_body(BodyId("surface.stitch")).has_error());
  CHECK(document.remove_body(BodyId("solid.closed")).has_error());
  CHECK(document.remove_surface_feature(FeatureId("surface.boundary.feature")).has_error());
  document.mark_all_clean();
  auto affected = document.mark_parameter_changed(ParameterId("distance"));
  REQUIRE(affected);
  REQUIRE(document.invalidation_state().find("surface.extend.feature") != nullptr);
  CHECK(document.invalidation_state().find("surface.extend.feature")->status ==
        InvalidationStatus::Dirty);
  REQUIRE(document.remove_surface_feature(FeatureId("surface.solid.feature")));
  REQUIRE(document.remove_body(BodyId("solid.closed")));
}

TEST_CASE("Block 88 roundtrips strict and backward-compatible Surface JSON",
          "[core][surface-feature]") {
  auto document = surface_document();
  add_surface_chain(document);
  auto serialized = serialize_part_document_to_json(document);
  REQUIRE(serialized);
  json value = json::parse(serialized.value());
  REQUIRE(value["surface_features"].size() == 6U);
  CHECK(value["surface_features"][0]["type"] == "boundary_surface");
  CHECK(value["surface_features"][3]["distance_parameter"] == "distance");
  CHECK(value["surface_features"][5]["result_body"] == "solid.closed");

  auto restored = deserialize_part_document_from_json(serialized.value());
  INFO((restored.has_error() ? restored.error().object_id() + ": " + restored.error().message()
                             : std::string("round-trip succeeded")));
  REQUIRE(restored);
  CHECK(restored.value().surface_feature_count() == 6U);
  REQUIRE(restored.value().find_surface_feature(FeatureId("surface.stitch.feature")) != nullptr);
  CHECK(surface_feature_kind(*restored.value().find_surface_feature(
            FeatureId("surface.stitch.feature"))) == SurfaceFeatureKind::SurfaceStitch);

  json malformed = value;
  malformed["surface_features"][0]["unexpected"] = true;
  CHECK(deserialize_part_document_from_json(malformed.dump()).has_error());
  malformed = value;
  malformed["surface_features"][0]["boundaries"] = json::array();
  CHECK(deserialize_part_document_from_json(malformed.dump()).has_error());
  malformed = value;
  malformed["surface_features"][0]["target"] = value["surface_features"][2]["target"];
  CHECK(deserialize_part_document_from_json(malformed.dump()).has_error());
  value.erase("surface_features");
  auto historical = deserialize_part_document_from_json(value.dump());
  REQUIRE(historical);
  CHECK(historical.value().surface_feature_count() == 0U);
}
