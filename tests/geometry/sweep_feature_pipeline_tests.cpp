#include "blcad/geometry/recompute_executor.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <utility>

using namespace blcad;
using namespace blcad::geometry;

namespace {

Parameter length_parameter(const char* id, double value) {
  auto quantity = Quantity::length_mm(value, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), id, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

Parameter angle_parameter(const char* id, double value) {
  auto quantity = Quantity::angle_deg(value, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_angle(ParameterId(id), id, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

Body body(const char* id, BodyKind kind = BodyKind::Solid) {
  auto value = Body::create(BodyId(id), id, kind);
  REQUIRE(value);
  return value.value();
}

FeatureBodyResultContext context(FeatureBodyOperationMode mode, const char* id) {
  auto value = FeatureBodyResultContext::create(
      mode,
      mode == FeatureBodyOperationMode::NewBody ? std::optional<BodyId>{}
                                                : std::optional<BodyId>{BodyId(id)},
      mode == FeatureBodyOperationMode::NewBody ? std::optional<BodyId>{BodyId(id)}
                                                : std::optional<BodyId>{});
  REQUIRE(value);
  return value.value();
}

ProfileRegionReference sweep_profile(const char* sketch, const char* profile) {
  auto value = ProfileRegionReference::create(SketchId(sketch), ProfileId(profile),
                                              PartFeatureInputRole::SweepProfile);
  REQUIRE(value);
  return value.value();
}

SweepProfileReference closed_sweep_profile(const char* sketch, const char* profile) {
  auto value = SweepProfileReference::create_closed_region(sweep_profile(sketch, profile));
  REQUIRE(value);
  return value.value();
}

PathSegmentReference planar_segment(const char* sketch, const char* entity,
                                    PlanarPathCurveKind kind = PlanarPathCurveKind::Line) {
  auto value = PathSegmentReference::create_planar(SketchId(sketch), SketchEntityId(entity), kind);
  REQUIRE(value);
  return value.value();
}

PathCurve path_curve(const char* id, std::vector<PathSegmentReference> segments,
                     PathOrientationRule orientation = PathOrientationRule::MinimumTwist,
                     std::optional<Vector3> up = std::nullopt) {
  auto value = PathCurve::create(PathCurveId(id), id, std::move(segments), PathClosure::Open,
                                 orientation, up);
  REQUIRE(value);
  return value.value();
}

ShapeCache cache() {
  auto value = ShapeCache::create(ShapeCacheId("cache.sweep"));
  REQUIRE(value);
  return value.value();
}

double body_volume(const ShapeCache& shape_cache, const char* id) {
  const GeometryShape* shape = shape_cache.find_body_shape(BodyId(id));
  REQUIRE(shape != nullptr);
  return RectangleExtrusionAdapter{}.summarize(*shape).volume_mm3;
}

void add_profile(PartDocument& document, const char* sketch_id = "sketch.profile",
                 const char* profile_id = "profile.sweep", double width = 4.0, double height = 6.0,
                 Point2 center = {}) {
  REQUIRE(document.add_parameter(length_parameter("sweep.width", width)));
  REQUIRE(document.add_parameter(length_parameter("sweep.height", height)));
  auto sketch = Sketch::create(SketchId(sketch_id), sketch_id, DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto rectangle = RectangleProfile::create(ProfileId(profile_id), ParameterId("sweep.width"),
                                            ParameterId("sweep.height"), center);
  REQUIRE(rectangle);
  REQUIRE(sketch.value().add_profile(rectangle.value()));
  REQUIRE(document.add_sketch(sketch.value()));
}

void add_xz_path_plane(PartDocument& document) {
  auto plane =
      ConstructionPlane::create_explicit(ConstructionPlaneId("plane.path"), "Path", {},
                                         {1.0, 0.0, 0.0}, {0.0, 0.0, 1.0}, {0.0, -1.0, 0.0});
  REQUIRE(plane);
  REQUIRE(document.add_construction_plane(plane.value()));
}

PartDocument planar_sweep_document(PathOrientationRule orientation, bool arc = false,
                                   std::optional<Vector3> up = std::nullopt) {
  auto created = PartDocument::create(DocumentId("part.sweep.planar"), "PlanarSweep");
  REQUIRE(created);
  PartDocument document = created.value();
  REQUIRE(document.add_datum_plane(DatumPlane::xy().value()));
  add_xz_path_plane(document);
  add_profile(document);
  auto path = Sketch::create(SketchId("sketch.path"), "Path", DatumPlaneId("plane.path"));
  REQUIRE(path);
  std::vector<PathSegmentReference> segments;
  if (arc) {
    auto curve = ArcSegment::create_three_point(SketchEntityId("arc.path"), {0.0, 0.0}, {4.0, 6.0},
                                                {10.0, 10.0});
    REQUIRE(curve);
    REQUIRE(path.value().add_entity(curve.value()));
    segments.push_back(planar_segment("sketch.path", "arc.path", PlanarPathCurveKind::Arc));
  } else {
    auto first = LineSegment::create(SketchEntityId("line.first"), {0.0, 0.0}, {0.0, 10.0});
    auto second = LineSegment::create(SketchEntityId("line.second"), {0.0, 10.0}, {8.0, 18.0});
    REQUIRE(first);
    REQUIRE(second);
    REQUIRE(path.value().add_entity(first.value()));
    REQUIRE(path.value().add_entity(second.value()));
    segments.push_back(planar_segment("sketch.path", "line.first"));
    segments.push_back(planar_segment("sketch.path", "line.second"));
  }
  REQUIRE(document.add_sketch(path.value()));
  REQUIRE(
      document.add_path_curve(path_curve("path.trajectory", std::move(segments), orientation, up)));
  REQUIRE(document.add_body(body("body.sweep")));
  auto feature = SweepFeature::create_sweep(
      FeatureId("sweep.main"), "Sweep", closed_sweep_profile("sketch.profile", "profile.sweep"),
      PathCurveId("path.trajectory"), context(FeatureBodyOperationMode::NewBody, "body.sweep"));
  REQUIRE(feature);
  REQUIRE(document.add_sweep_feature(feature.value()));
  return document;
}

PartDocument construction_line_sweep(std::optional<ParameterId> twist = std::nullopt,
                                     bool bounded = true) {
  auto created = PartDocument::create(DocumentId("part.sweep.line"), "LineSweep");
  REQUIRE(created);
  PartDocument document = created.value();
  REQUIRE(document.add_datum_plane(DatumPlane::xy().value()));
  add_profile(document);
  if (bounded) {
    auto start = ConstructionPoint::create_explicit(ConstructionPointId("point.start"), "Start",
                                                    {0.0, 0.0, 0.0});
    auto end = ConstructionPoint::create_explicit(ConstructionPointId("point.end"), "End",
                                                  {0.0, 0.0, 20.0});
    REQUIRE(start);
    REQUIRE(end);
    REQUIRE(document.add_construction_point(start.value()));
    REQUIRE(document.add_construction_point(end.value()));
    auto relation = ConstructionRelation::create_line_through_two_points(
        ConstructionRelationId("relation.path"), ConstructionPointId("point.start"),
        ConstructionPointId("point.end"));
    REQUIRE(relation);
    auto line = ConstructionLine::create_through_two_points(ConstructionLineId("line.path"), "Path",
                                                            relation.value());
    REQUIRE(line);
    REQUIRE(document.add_construction_line(line.value()));
  } else {
    auto line = ConstructionLine::create_explicit(ConstructionLineId("line.path"), "Path", {},
                                                  {0.0, 0.0, 1.0});
    REQUIRE(line);
    REQUIRE(document.add_construction_line(line.value()));
  }
  auto source = PathSegmentReference::create_construction_line(ConstructionLineId("line.path"));
  REQUIRE(source);
  REQUIRE(document.add_path_curve(path_curve("path.trajectory", {source.value()})));
  if (twist.has_value())
    REQUIRE(document.add_parameter(angle_parameter(twist->value().c_str(), 30.0)));
  REQUIRE(document.add_body(body("body.sweep")));
  auto feature = SweepFeature::create_sweep(
      FeatureId("sweep.main"), "Sweep", closed_sweep_profile("sketch.profile", "profile.sweep"),
      PathCurveId("path.trajectory"), context(FeatureBodyOperationMode::NewBody, "body.sweep"),
      std::nullopt, std::nullopt, twist);
  REQUIRE(feature);
  REQUIRE(document.add_sweep_feature(feature.value()));
  return document;
}

SketchCoordinate3D coordinate(double value) {
  auto quantity = Quantity::linear_displacement_mm(value, "sweep coordinate");
  REQUIRE(quantity);
  auto result = SketchCoordinate3D::create_explicit(quantity.value());
  REQUIRE(result);
  return result.value();
}

SketchPoint3D point3d(const char* id, double x, double y, double z) {
  auto result =
      SketchPoint3D::create(SketchEntityId(id), coordinate(x), coordinate(y), coordinate(z));
  REQUIRE(result);
  return result.value();
}

SketchPointReference3D local_point(const char* id) {
  auto result = SketchPointReference3D::create_local_point(SketchEntityId(id));
  REQUIRE(result);
  return result.value();
}

Parameter count_parameter(const char* id, std::size_t value) {
  auto quantity = Quantity::count(static_cast<double>(value), id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_count(ParameterId(id), id, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

PathSegmentReference spatial_segment(const char* entity, Sketch3DPathCurveKind kind) {
  auto result = PathSegmentReference::create_sketch_3d(Sketch3DId("sketch3d.path"),
                                                       SketchEntityId(entity), kind);
  REQUIRE(result);
  return result.value();
}

PartDocument spatial_sweep_document(bool helix) {
  auto created = PartDocument::create(DocumentId("part.sweep.spatial"), "SpatialSweep");
  REQUIRE(created);
  PartDocument document = created.value();
  REQUIRE(document.add_datum_plane(DatumPlane::xy().value()));
  if (helix) {
    add_profile(document, "sketch.profile", "profile.sweep", 0.8, 0.8, {-4.0, 0.0});
    REQUIRE(document.add_parameter(length_parameter("helix.radius", 4.0)));
    REQUIRE(document.add_parameter(length_parameter("helix.pitch", 5.0)));
    REQUIRE(document.add_parameter(count_parameter("helix.turns", 2U)));
    auto axis =
        DatumAxis::create_explicit(DatumAxisId("axis.helix"), "Helix axis", {}, {0.0, 0.0, 1.0});
    REQUIRE(axis);
    REQUIRE(document.add_datum_axis(axis.value()));
  } else {
    add_profile(document);
  }
  auto sketch = Sketch3D::create(Sketch3DId("sketch3d.path"), "Spatial path");
  REQUIRE(sketch);
  PathSegmentReference reference = spatial_segment("spline.path", Sketch3DPathCurveKind::Spline);
  if (helix) {
    auto axis = SketchHelixAxis3D::create_datum_axis(DatumAxisId("axis.helix"));
    REQUIRE(axis);
    auto curve =
        SketchHelix3D::create(SketchEntityId("helix.path"), axis.value(),
                              ParameterId("helix.radius"), ParameterId("helix.pitch"),
                              ParameterId("helix.turns"), SketchHelix3DHandedness::RightHanded);
    REQUIRE(curve);
    REQUIRE(sketch.value().add_helix(curve.value()));
    reference = spatial_segment("helix.path", Sketch3DPathCurveKind::Helix);
  } else {
    REQUIRE(sketch.value().add_point(point3d("p0", 0.0, 0.0, 0.0)));
    REQUIRE(sketch.value().add_point(point3d("p1", 0.0, 4.0, 6.0)));
    REQUIRE(sketch.value().add_point(point3d("p2", 3.0, -2.0, 13.0)));
    REQUIRE(sketch.value().add_point(point3d("p3", 0.0, 0.0, 20.0)));
    auto curve = SketchSpline3D::create(
        SketchEntityId("spline.path"), SketchSpline3DRepresentation::ControlPoints,
        {local_point("p0"), local_point("p1"), local_point("p2"), local_point("p3")}, 3U,
        SketchSpline3DContinuity::Curvature);
    REQUIRE(curve);
    REQUIRE(sketch.value().add_spline(curve.value()));
  }
  REQUIRE(document.add_sketch_3d(sketch.value()));
  REQUIRE(document.add_path_curve(path_curve("path.trajectory", {reference})));
  REQUIRE(document.add_body(body("body.sweep")));
  auto feature = SweepFeature::create_sweep(
      FeatureId("sweep.spatial"), "Spatial sweep",
      closed_sweep_profile("sketch.profile", "profile.sweep"), PathCurveId("path.trajectory"),
      context(FeatureBodyOperationMode::NewBody, "body.sweep"));
  REQUIRE(feature);
  REQUIRE(document.add_sweep_feature(feature.value()));
  return document;
}

} // namespace

TEST_CASE("Block 81 sweeps a closed profile along a bounded construction line",
          "[geometry][sweep-feature]") {
  auto document = construction_line_sweep();
  ShapeCache shape_cache = cache();
  auto result = GeometryRecomputeExecutor{}.execute_document(document, shape_cache);
  REQUIRE(result);
  CHECK(result.value().executed_feature_count == 1U);
  CHECK(body_volume(shape_cache, "body.sweep") == Catch::Approx(480.0).epsilon(1.0e-7));
  CHECK(shape_cache.find_feature_shape(FeatureId("sweep.main")) != nullptr);
}

TEST_CASE("Block 81 executes planar polyline and arc paths with explicit orientation rules",
          "[geometry][sweep-feature]") {
  auto minimum = planar_sweep_document(PathOrientationRule::MinimumTwist);
  auto normal = planar_sweep_document(PathOrientationRule::ProfileNormal, true);
  auto fixed =
      planar_sweep_document(PathOrientationRule::FixedUpVector, false, Vector3{0.0, 1.0, 0.0});
  ShapeCache minimum_cache = cache();
  ShapeCache normal_cache = cache();
  ShapeCache fixed_cache = cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(minimum, minimum_cache));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(normal, normal_cache));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(fixed, fixed_cache));
  CHECK(body_volume(minimum_cache, "body.sweep") > 400.0);
  CHECK(body_volume(normal_cache, "body.sweep") > 300.0);
  CHECK(body_volume(fixed_cache, "body.sweep") > 400.0);
}

TEST_CASE("Block 81 applies SweepCut and creates SweepSurface",
          "[geometry][sweep-feature][integration][part-construction-mvp]") {
  auto created = PartDocument::create(DocumentId("part.sweep.operations"), "SweepOperations");
  REQUIRE(created);
  PartDocument document = created.value();
  REQUIRE(document.add_datum_plane(DatumPlane::xy().value()));
  add_xz_path_plane(document);
  for (const auto& [id, value] : {std::pair{"base.width", 20.0},
                                  {"base.height", 20.0},
                                  {"base.depth", 20.0},
                                  {"cut.diameter", 6.0}})
    REQUIRE(document.add_parameter(length_parameter(id, value)));
  auto base_sketch = Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId("datum.xy"));
  auto cut_sketch = Sketch::create(SketchId("sketch.cut"), "Cut", DatumPlaneId("datum.xy"));
  auto trajectory = Sketch::create(SketchId("sketch.path"), "Path", DatumPlaneId("plane.path"));
  auto surface_profile =
      Sketch::create(SketchId("sketch.surface"), "Surface", DatumPlaneId("datum.xy"));
  REQUIRE(base_sketch);
  REQUIRE(cut_sketch);
  REQUIRE(trajectory);
  REQUIRE(surface_profile);
  auto base_profile = RectangleProfile::create(ProfileId("profile.base"), ParameterId("base.width"),
                                               ParameterId("base.height"));
  auto cut_profile = CircleProfile::create(ProfileId("profile.cut"), ParameterId("cut.diameter"));
  auto path_line = LineSegment::create(SketchEntityId("line.path"), {0.0, 0.0}, {0.0, 20.0});
  auto open_line = LineSegment::create(SketchEntityId("line.profile"), {-4.0, 0.0}, {4.0, 0.0});
  REQUIRE(base_profile);
  REQUIRE(cut_profile);
  REQUIRE(path_line);
  REQUIRE(open_line);
  REQUIRE(base_sketch.value().add_profile(base_profile.value()));
  REQUIRE(cut_sketch.value().add_profile(cut_profile.value()));
  REQUIRE(trajectory.value().add_entity(path_line.value()));
  REQUIRE(surface_profile.value().add_entity(open_line.value()));
  REQUIRE(document.add_sketch(base_sketch.value()));
  REQUIRE(document.add_sketch(cut_sketch.value()));
  REQUIRE(document.add_sketch(trajectory.value()));
  REQUIRE(document.add_sketch(surface_profile.value()));
  REQUIRE(document.add_path_curve(
      path_curve("path.trajectory", {planar_segment("sketch.path", "line.path")})));
  REQUIRE(document.add_path_curve(
      path_curve("path.surface-profile", {planar_segment("sketch.surface", "line.profile")})));
  REQUIRE(document.add_body(body("body.base")));
  REQUIRE(document.add_body(body("body.surface", BodyKind::Surface)));
  auto base = Feature::create_additive_extrude(FeatureId("feature.base"), "Base",
                                               SketchId("sketch.base"), ParameterId("base.depth"));
  REQUIRE(base);
  auto based = base.value().with_body_result_context(
      context(FeatureBodyOperationMode::NewBody, "body.base"));
  REQUIRE(based);
  REQUIRE(document.add_feature(based.value()));
  auto cut = SweepFeature::create_sweep_cut(
      FeatureId("sweep.cut"), "Cut", closed_sweep_profile("sketch.cut", "profile.cut"),
      PathCurveId("path.trajectory"), context(FeatureBodyOperationMode::Cut, "body.base"));
  auto open = SweepProfileReference::create_open_path(PathCurveId("path.surface-profile"));
  REQUIRE(cut);
  REQUIRE(open);
  auto surface = SweepFeature::create_sweep_surface(
      FeatureId("sweep.surface"), "Surface", open.value(), PathCurveId("path.trajectory"),
      context(FeatureBodyOperationMode::NewBody, "body.surface"));
  REQUIRE(surface);
  REQUIRE(document.add_sweep_feature(cut.value()));
  REQUIRE(document.add_sweep_feature(surface.value()));
  ShapeCache shape_cache = cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(document, shape_cache));
  CHECK(body_volume(shape_cache, "body.base") < 8000.0);
  const GeometryShape* surface_shape = shape_cache.find_body_shape(BodyId("body.surface"));
  REQUIRE(surface_shape != nullptr);
  CHECK_FALSE(surface_shape->empty());
  CHECK(RectangleExtrusionAdapter{}.summarize(*surface_shape).solid_count == 0U);
}

TEST_CASE("Block 82 applies explicit twist and still rejects unbounded paths transactionally",
          "[geometry][sweep-3d]") {
  ShapeCache shape_cache = cache();
  auto valid = construction_line_sweep();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(valid, shape_cache));
  const double before = body_volume(shape_cache, "body.sweep");
  auto twisted = construction_line_sweep(ParameterId("sweep.twist"));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(twisted, shape_cache));
  CHECK(body_volume(shape_cache, "body.sweep") == Catch::Approx(before).epsilon(1.0e-5));
  auto unbounded = construction_line_sweep(std::nullopt, false);
  auto failed_line = GeometryRecomputeExecutor{}.execute_document(unbounded, shape_cache);
  REQUIRE(failed_line.has_error());
  CHECK(failed_line.error().message() ==
        "Block 81 requires a construction-line path bounded by two points");
  CHECK(body_volume(shape_cache, "body.sweep") == Catch::Approx(before));
}

TEST_CASE("Block 82 sweeps solids along a spatial spline and a helix",
          "[geometry][sweep-3d][integration][part-construction-mvp]") {
  auto spline = spatial_sweep_document(false);
  auto helix = spatial_sweep_document(true);
  ShapeCache spline_cache = cache();
  ShapeCache helix_cache = cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(spline, spline_cache));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(helix, helix_cache));
  CHECK(body_volume(spline_cache, "body.sweep") > 400.0);
  CHECK(body_volume(helix_cache, "body.sweep") > 5.0);
  const GeometryShape* helical_shape = helix_cache.find_body_shape(BodyId("body.sweep"));
  REQUIRE(helical_shape != nullptr);
  auto bounds = BodyTransformAdapter{}.bounds(*helical_shape);
  REQUIRE(bounds);
  CHECK(bounds.value().maximum.x - bounds.value().minimum.x > 7.0);
  CHECK(bounds.value().maximum.y - bounds.value().minimum.y > 7.0);
}

TEST_CASE("Block 82 adapter accepts an auxiliary guide and rejects guide plus twist",
          "[geometry][sweep-3d]") {
  const std::vector<ClosedProfileCurveSegment> profile{
      {ClosedProfileCurveKind::Line, {-2.0, -2.0, 0.0}, {}, {2.0, -2.0, 0.0}},
      {ClosedProfileCurveKind::Line, {2.0, -2.0, 0.0}, {}, {2.0, 2.0, 0.0}},
      {ClosedProfileCurveKind::Line, {2.0, 2.0, 0.0}, {}, {-2.0, 2.0, 0.0}},
      {ClosedProfileCurveKind::Line, {-2.0, 2.0, 0.0}, {}, {-2.0, -2.0, 0.0}}};
  const std::vector<SweepPathSegment> path{
      {SweepPathSegmentKind::Line, {0.0, 0.0, 0.0}, {}, {0.0, 0.0, 20.0}}};
  const std::vector<SweepPathSegment> guide{
      {SweepPathSegmentKind::Line, {-4.0, 0.0, 0.0}, {}, {0.0, -4.0, 20.0}}};
  auto guided = SweepAdapter{}.sweep_closed_profile(FeatureId("sweep.guided"), profile, path,
                                                    PathOrientationRule::MinimumTwist, std::nullopt,
                                                    0.0, &guide);
  REQUIRE(guided);
  CHECK(RectangleExtrusionAdapter{}.summarize(guided.value()).volume_mm3 > 300.0);
  auto ambiguous = SweepAdapter{}.sweep_closed_profile(FeatureId("sweep.ambiguous"), profile, path,
                                                       PathOrientationRule::MinimumTwist,
                                                       std::nullopt, 30.0, &guide);
  REQUIRE(ambiguous.has_error());
  CHECK(ambiguous.error().message() == "combined guide and twist are not supported");
}

TEST_CASE("Block 81 rejects a fixed-up vector parallel to the path tangent",
          "[geometry][sweep-feature]") {
  auto document =
      planar_sweep_document(PathOrientationRule::FixedUpVector, false, Vector3{0.0, 0.0, 1.0});
  ShapeCache shape_cache = cache();
  auto result = GeometryRecomputeExecutor{}.execute_document(document, shape_cache);
  REQUIRE(result.has_error());
  CHECK(result.error().message() ==
        "fixed-up vector must not be parallel to the path start tangent");
  CHECK(shape_cache.body_shape_count() == 0U);
}

TEST_CASE("Block 81 adapter rejects non-adjacent path self-intersection",
          "[geometry][sweep-feature]") {
  const std::vector<ClosedProfileCurveSegment> profile{
      {ClosedProfileCurveKind::Line, {-6.0, -1.0, 0.0}, {}, {-4.0, -1.0, 0.0}},
      {ClosedProfileCurveKind::Line, {-4.0, -1.0, 0.0}, {}, {-4.0, 1.0, 0.0}},
      {ClosedProfileCurveKind::Line, {-4.0, 1.0, 0.0}, {}, {-6.0, 1.0, 0.0}},
      {ClosedProfileCurveKind::Line, {-6.0, 1.0, 0.0}, {}, {-6.0, -1.0, 0.0}}};
  const std::vector<SweepPathSegment> crossing{
      {SweepPathSegmentKind::Line, {-5.0, 0.0, 0.0}, {}, {5.0, 0.0, 10.0}},
      {SweepPathSegmentKind::Line, {5.0, 0.0, 10.0}, {}, {5.0, 0.0, 0.0}},
      {SweepPathSegmentKind::Line, {5.0, 0.0, 0.0}, {}, {-5.0, 0.0, 10.0}}};
  auto result = SweepAdapter{}.sweep_closed_profile(FeatureId("sweep.crossing"), profile, crossing,
                                                    PathOrientationRule::MinimumTwist);
  REQUIRE(result.has_error());
  CHECK(result.error().message() ==
        "sweep path must not self-intersect or repeat branch junctions");
}
