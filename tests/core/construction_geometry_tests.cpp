#include "blcad/core/part_document.hpp"

#include <catch2/catch_test_macros.hpp>

#include <utility>
#include <vector>

using namespace blcad;

namespace {

Parameter make_length_parameter(const char* id, const char* name, double value_mm) {
  auto quantity = Quantity::length_mm(value_mm, id);
  REQUIRE(quantity);

  auto parameter = Parameter::create_length(ParameterId(id), name, quantity.value());
  REQUIRE(parameter);

  return parameter.value();
}

ConstructionPoint make_point(const char* id, Point3 position) {
  auto point = ConstructionPoint::create_explicit(ConstructionPointId(id), id, position);
  REQUIRE(point);
  return point.value();
}

ConstructionPlane make_xy_offset_plane(std::vector<ParameterId> dependencies = {}) {
  auto plane = ConstructionPlane::create_explicit(
      ConstructionPlaneId("construction_plane.offset_xy"), "OffsetXY", Point3{0.0, 0.0, 25.0},
      Vector3{1.0, 0.0, 0.0}, Vector3{0.0, 1.0, 0.0}, Vector3{0.0, 0.0, 1.0},
      std::move(dependencies));
  REQUIRE(plane);
  return plane.value();
}

} // namespace

TEST_CASE("ConstructionPoint stores explicit placement", "[core][construction_geometry]") {
  const auto point = ConstructionPoint::create_explicit(
      ConstructionPointId("construction_point.anchor"), "Anchor", Point3{1.0, 2.0, 3.0});

  REQUIRE(point);
  CHECK(point.value().id().value() == "construction_point.anchor");
  CHECK(point.value().name() == "Anchor");
  CHECK(point.value().position() == Point3{1.0, 2.0, 3.0});
  CHECK(point.value().parameter_dependencies().empty());
}

TEST_CASE("ConstructionRelation stores relation-driven definitions",
          "[core][construction_geometry]") {
  const auto offset = ConstructionRelation::create_plane_offset_from_plane(
      ConstructionRelationId("relation.offset_xy"), DatumPlaneId("datum.xy"),
      ParameterId("part.offset"));
  REQUIRE(offset);
  CHECK(offset.value().type() == ConstructionRelationType::PlaneOffsetFromPlane);
  CHECK(offset.value().source_plane().value() == "datum.xy");
  CHECK(offset.value().offset_parameter().value() == "part.offset");
  REQUIRE(offset.value().parameter_dependencies().size() == 1);
  CHECK(offset.value().parameter_dependencies().front().value() == "part.offset");

  const auto line = ConstructionRelation::create_line_through_two_points(
      ConstructionRelationId("relation.axis"), ConstructionPointId("construction_point.a"),
      ConstructionPointId("construction_point.b"));
  REQUIRE(line);
  CHECK(line.value().type() == ConstructionRelationType::LineThroughTwoPoints);
  CHECK(line.value().first_point().value() == "construction_point.a");
  CHECK(line.value().second_point().value() == "construction_point.b");

  const auto duplicate_points = ConstructionRelation::create_line_through_two_points(
      ConstructionRelationId("relation.bad"), ConstructionPointId("construction_point.a"),
      ConstructionPointId("construction_point.a"));
  REQUIRE(duplicate_points.has_error());
  CHECK(duplicate_points.error().message() ==
        "line through two points requires two distinct points");
}

TEST_CASE("ConstructionLine validates explicit direction", "[core][construction_geometry]") {
  const auto line =
      ConstructionLine::create_explicit(ConstructionLineId("construction_line.axis_z"), "AxisZ",
                                        Point3{0.0, 0.0, 0.0}, Vector3{0.0, 0.0, 1.0});

  REQUIRE(line);
  CHECK(line.value().id().value() == "construction_line.axis_z");
  CHECK(line.value().kind() == ConstructionLineKind::Explicit);
  CHECK(line.value().point() == Point3{0.0, 0.0, 0.0});
  CHECK(line.value().direction() == Vector3{0.0, 0.0, 1.0});

  const auto zero =
      ConstructionLine::create_explicit(ConstructionLineId("construction_line.bad"), "Bad",
                                        Point3{0.0, 0.0, 0.0}, Vector3{0.0, 0.0, 0.0});
  REQUIRE(zero.has_error());
  CHECK(zero.error().message() == "construction line direction must not be zero length");

  const auto non_unit =
      ConstructionLine::create_explicit(ConstructionLineId("construction_line.bad"), "Bad",
                                        Point3{0.0, 0.0, 0.0}, Vector3{0.0, 0.0, 2.0});
  REQUIRE(non_unit.has_error());
  CHECK(non_unit.error().message() == "construction line direction must be unit length");
}

TEST_CASE("ConstructionLine stores line-through-two-points relation",
          "[core][construction_geometry]") {
  auto relation = ConstructionRelation::create_line_through_two_points(
      ConstructionRelationId("relation.axis_ab"), ConstructionPointId("construction_point.a"),
      ConstructionPointId("construction_point.b"));
  REQUIRE(relation);

  auto line = ConstructionLine::create_through_two_points(
      ConstructionLineId("construction_line.axis_ab"), "AxisAB", relation.value());

  REQUIRE(line);
  CHECK(line.value().kind() == ConstructionLineKind::ThroughTwoPoints);
  REQUIRE(line.value().relation().has_value());
  CHECK(line.value().relation().value().id().value() == "relation.axis_ab");
}

TEST_CASE("ConstructionPlane validates orthonormal frame", "[core][construction_geometry]") {
  const auto plane = make_xy_offset_plane();

  CHECK(plane.id().value() == "construction_plane.offset_xy");
  CHECK(plane.kind() == ConstructionPlaneKind::Explicit);
  CHECK(plane.workplane_id().value() == "construction_plane.offset_xy");
  CHECK(plane.origin() == Point3{0.0, 0.0, 25.0});
  CHECK(plane.x_axis() == Vector3{1.0, 0.0, 0.0});
  CHECK(plane.y_axis() == Vector3{0.0, 1.0, 0.0});
  CHECK(plane.normal() == Vector3{0.0, 0.0, 1.0});

  const auto zero_axis = ConstructionPlane::create_explicit(
      ConstructionPlaneId("construction_plane.bad"), "Bad", Point3{0.0, 0.0, 0.0},
      Vector3{0.0, 0.0, 0.0}, Vector3{0.0, 1.0, 0.0}, Vector3{0.0, 0.0, 1.0});
  REQUIRE(zero_axis.has_error());
  CHECK(zero_axis.error().message() ==
        "construction plane axes and normal must not be zero length");

  const auto non_orthogonal = ConstructionPlane::create_explicit(
      ConstructionPlaneId("construction_plane.bad"), "Bad", Point3{0.0, 0.0, 0.0},
      Vector3{1.0, 0.0, 0.0}, Vector3{1.0, 0.0, 0.0}, Vector3{0.0, 0.0, 1.0});
  REQUIRE(non_orthogonal.has_error());
  CHECK(non_orthogonal.error().message() ==
        "construction plane axes and normal must be orthogonal");

  const auto wrong_normal = ConstructionPlane::create_explicit(
      ConstructionPlaneId("construction_plane.bad"), "Bad", Point3{0.0, 0.0, 0.0},
      Vector3{1.0, 0.0, 0.0}, Vector3{0.0, 1.0, 0.0}, Vector3{0.0, 0.0, -1.0});
  REQUIRE(wrong_normal.has_error());
  CHECK(wrong_normal.error().message() ==
        "construction plane normal must match x_axis cross y_axis");
}

TEST_CASE("ConstructionPlane stores relation-driven definitions", "[core][construction_geometry]") {
  auto offset_relation = ConstructionRelation::create_plane_offset_from_plane(
      ConstructionRelationId("relation.offset_xy"), DatumPlaneId("datum.xy"),
      ParameterId("part.offset"));
  REQUIRE(offset_relation);

  auto offset_plane = ConstructionPlane::create_offset_from_plane(
      ConstructionPlaneId("construction_plane.offset_xy"), "OffsetXY", offset_relation.value());
  REQUIRE(offset_plane);
  CHECK(offset_plane.value().kind() == ConstructionPlaneKind::OffsetFromPlane);
  REQUIRE(offset_plane.value().relation().has_value());
  CHECK(offset_plane.value().parameter_dependencies().front().value() == "part.offset");

  auto points_relation = ConstructionRelation::create_plane_through_three_points(
      ConstructionRelationId("relation.plane_abc"), ConstructionPointId("construction_point.a"),
      ConstructionPointId("construction_point.b"), ConstructionPointId("construction_point.c"));
  REQUIRE(points_relation);

  auto points_plane = ConstructionPlane::create_through_three_points(
      ConstructionPlaneId("construction_plane.abc"), "PlaneABC", points_relation.value());
  REQUIRE(points_plane);
  CHECK(points_plane.value().kind() == ConstructionPlaneKind::ThroughThreePoints);
  REQUIRE(points_plane.value().relation().has_value());
  CHECK(points_plane.value().relation().value().third_point().value() == "construction_point.c");
}

TEST_CASE("PartDocument stores construction geometry and exposes construction planes as workplanes",
          "[core][construction_geometry]") {
  auto document = PartDocument::create(DocumentId("part.construction"), "ConstructionPart");
  REQUIRE(document);

  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 20.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 10.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.depth", "depth", 5.0)));

  REQUIRE(document.value().add_construction_point(
      ConstructionPoint::create_explicit(ConstructionPointId("construction_point.anchor"), "Anchor",
                                         Point3{0.0, 0.0, 0.0})
          .value()));
  REQUIRE(document.value().add_construction_line(
      ConstructionLine::create_explicit(ConstructionLineId("construction_line.axis_z"), "AxisZ",
                                        Point3{0.0, 0.0, 0.0}, Vector3{0.0, 0.0, 1.0})
          .value()));
  REQUIRE(
      document.value().add_construction_plane(make_xy_offset_plane({ParameterId("part.depth")})));

  auto sketch = Sketch::create(SketchId("sketch.on_construction_plane"), "OnConstructionPlane",
                               DatumPlaneId("construction_plane.offset_xy"));
  REQUIRE(sketch);
  auto rectangle = RectangleProfile::create(ProfileId("profile.rectangle"),
                                            ParameterId("part.width"), ParameterId("part.height"));
  REQUIRE(rectangle);
  REQUIRE(sketch.value().add_profile(rectangle.value()));
  REQUIRE(document.value().add_sketch(sketch.value()));

  auto feature = Feature::create_additive_extrude(
      FeatureId("feature.construction_plane_prism"), "ConstructionPlanePrism",
      SketchId("sketch.on_construction_plane"), ParameterId("part.depth"));
  REQUIRE(feature);
  REQUIRE(document.value().add_feature(feature.value()));

  CHECK(document.value().construction_point_count() == 1);
  CHECK(document.value().construction_line_count() == 1);
  CHECK(document.value().construction_plane_count() == 1);
  CHECK(document.value().has_workplane_id(DatumPlaneId("construction_plane.offset_xy")));
  CHECK(document.value().dependency_graph().has_dependency("part.depth",
                                                           "construction_plane.offset_xy"));
  CHECK(document.value().dependency_graph().has_dependency("construction_plane.offset_xy",
                                                           "sketch.on_construction_plane"));
  CHECK(document.value().dependency_graph().has_dependency("sketch.on_construction_plane",
                                                           "feature.construction_plane_prism"));

  document.value().mark_all_clean();
  const auto affected = document.value().mark_parameter_changed(ParameterId("part.depth"));
  REQUIRE(affected);
  const auto plan = document.value().create_recompute_plan();
  REQUIRE(plan);
  CHECK(plan.value().contains("construction_plane.offset_xy"));
  CHECK(plan.value().contains("sketch.on_construction_plane"));
  CHECK(plan.value().contains("feature.construction_plane_prism"));
}

TEST_CASE("PartDocument stores relation-driven construction geometry dependencies",
          "[core][construction_geometry]") {
  auto document =
      PartDocument::create(DocumentId("part.relation_construction"), "RelationConstruction");
  REQUIRE(document);

  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 20.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 10.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.offset", "offset", 15.0)));
  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));

  REQUIRE(document.value().add_construction_point(
      make_point("construction_point.a", Point3{0.0, 0.0, 0.0})));
  REQUIRE(document.value().add_construction_point(
      make_point("construction_point.b", Point3{10.0, 0.0, 0.0})));
  REQUIRE(document.value().add_construction_point(
      make_point("construction_point.c", Point3{0.0, 10.0, 0.0})));

  auto line_relation = ConstructionRelation::create_line_through_two_points(
      ConstructionRelationId("relation.axis_ab"), ConstructionPointId("construction_point.a"),
      ConstructionPointId("construction_point.b"));
  REQUIRE(line_relation);
  auto line = ConstructionLine::create_through_two_points(
      ConstructionLineId("construction_line.axis_ab"), "AxisAB", line_relation.value());
  REQUIRE(line);
  REQUIRE(document.value().add_construction_line(line.value()));

  auto offset_relation = ConstructionRelation::create_plane_offset_from_plane(
      ConstructionRelationId("relation.offset_xy"), DatumPlaneId("datum.xy"),
      ParameterId("part.offset"));
  REQUIRE(offset_relation);
  auto offset_plane = ConstructionPlane::create_offset_from_plane(
      ConstructionPlaneId("construction_plane.offset_xy"), "OffsetXY", offset_relation.value());
  REQUIRE(offset_plane);
  REQUIRE(document.value().add_construction_plane(offset_plane.value()));

  auto points_relation = ConstructionRelation::create_plane_through_three_points(
      ConstructionRelationId("relation.plane_abc"), ConstructionPointId("construction_point.a"),
      ConstructionPointId("construction_point.b"), ConstructionPointId("construction_point.c"));
  REQUIRE(points_relation);
  auto points_plane = ConstructionPlane::create_through_three_points(
      ConstructionPlaneId("construction_plane.abc"), "PlaneABC", points_relation.value());
  REQUIRE(points_plane);
  REQUIRE(document.value().add_construction_plane(points_plane.value()));

  CHECK(document.value().dependency_graph().has_dependency("construction_point.a",
                                                           "construction_line.axis_ab"));
  CHECK(document.value().dependency_graph().has_dependency("construction_point.b",
                                                           "construction_line.axis_ab"));
  CHECK(document.value().dependency_graph().has_dependency("datum.xy",
                                                           "construction_plane.offset_xy"));
  CHECK(document.value().dependency_graph().has_dependency("part.offset",
                                                           "construction_plane.offset_xy"));
  CHECK(document.value().dependency_graph().has_dependency("construction_point.a",
                                                           "construction_plane.abc"));
  CHECK(document.value().dependency_graph().has_dependency("construction_point.b",
                                                           "construction_plane.abc"));
  CHECK(document.value().dependency_graph().has_dependency("construction_point.c",
                                                           "construction_plane.abc"));

  auto sketch = Sketch::create(SketchId("sketch.offset"), "OffsetSketch",
                               DatumPlaneId("construction_plane.offset_xy"));
  REQUIRE(sketch);
  auto rectangle = RectangleProfile::create(ProfileId("profile.offset_rectangle"),
                                            ParameterId("part.width"), ParameterId("part.height"));
  REQUIRE(rectangle);
  REQUIRE(sketch.value().add_profile(rectangle.value()));
  REQUIRE(document.value().add_sketch(sketch.value()));

  document.value().mark_all_clean();
  const auto affected = document.value().mark_parameter_changed(ParameterId("part.offset"));
  REQUIRE(affected);
  const auto plan = document.value().create_recompute_plan();
  REQUIRE(plan);
  CHECK(plan.value().contains("construction_plane.offset_xy"));
  CHECK(plan.value().contains("sketch.offset"));
}

TEST_CASE("PartDocument rejects invalid relation-driven construction geometry references",
          "[core][construction_geometry]") {
  auto document = PartDocument::create(DocumentId("part.invalid_relations"), "InvalidRelations");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(make_length_parameter("part.offset", "offset", 15.0)));

  auto missing_line_relation = ConstructionRelation::create_line_through_two_points(
      ConstructionRelationId("relation.missing_line"), ConstructionPointId("construction_point.a"),
      ConstructionPointId("construction_point.b"));
  REQUIRE(missing_line_relation);
  auto missing_line = ConstructionLine::create_through_two_points(
      ConstructionLineId("construction_line.missing"), "Missing", missing_line_relation.value());
  REQUIRE(missing_line);
  const auto added_line = document.value().add_construction_line(missing_line.value());
  REQUIRE(added_line.has_error());
  CHECK(added_line.error().message() ==
        "line through two points references must exist in part document");

  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));
  REQUIRE(document.value().add_construction_point(
      make_point("construction_point.a", Point3{0.0, 0.0, 0.0})));
  REQUIRE(document.value().add_construction_point(
      make_point("construction_point.b", Point3{1.0, 1.0, 1.0})));
  REQUIRE(document.value().add_construction_point(
      make_point("construction_point.c", Point3{2.0, 2.0, 2.0})));

  auto collinear_relation = ConstructionRelation::create_plane_through_three_points(
      ConstructionRelationId("relation.collinear"), ConstructionPointId("construction_point.a"),
      ConstructionPointId("construction_point.b"), ConstructionPointId("construction_point.c"));
  REQUIRE(collinear_relation);
  auto collinear_plane = ConstructionPlane::create_through_three_points(
      ConstructionPlaneId("construction_plane.collinear"), "Collinear", collinear_relation.value());
  REQUIRE(collinear_plane);
  const auto added_plane = document.value().add_construction_plane(collinear_plane.value());
  REQUIRE(added_plane.has_error());
  CHECK(added_plane.error().message() ==
        "plane through three points requires non-collinear points");
}

TEST_CASE("PartDocument rejects missing construction-geometry parameter dependencies",
          "[core][construction_geometry]") {
  auto document = PartDocument::create(DocumentId("part.construction"), "ConstructionPart");
  REQUIRE(document);

  auto plane = make_xy_offset_plane({ParameterId("part.missing")});
  const auto added = document.value().add_construction_plane(plane);

  REQUIRE(added.has_error());
  CHECK(added.error().object_id() == "construction_plane.offset_xy");
  CHECK(added.error().message() ==
        "construction plane parameter dependency must exist in part document");
}
