#include "blcad/core/part_document.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace blcad;

namespace {

Parameter make_length_parameter(const char* id, const char* name, double value_mm) {
  auto quantity = Quantity::length_mm(value_mm, id);
  REQUIRE(quantity);

  auto parameter = Parameter::create_length(ParameterId(id), name, quantity.value());
  REQUIRE(parameter);

  return parameter.value();
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

TEST_CASE("ConstructionLine validates explicit direction", "[core][construction_geometry]") {
  const auto line = ConstructionLine::create_explicit(
      ConstructionLineId("construction_line.axis_z"), "AxisZ", Point3{0.0, 0.0, 0.0},
      Vector3{0.0, 0.0, 1.0});

  REQUIRE(line);
  CHECK(line.value().id().value() == "construction_line.axis_z");
  CHECK(line.value().point() == Point3{0.0, 0.0, 0.0});
  CHECK(line.value().direction() == Vector3{0.0, 0.0, 1.0});

  const auto zero = ConstructionLine::create_explicit(
      ConstructionLineId("construction_line.bad"), "Bad", Point3{0.0, 0.0, 0.0},
      Vector3{0.0, 0.0, 0.0});
  REQUIRE(zero.has_error());
  CHECK(zero.error().message() == "construction line direction must not be zero length");

  const auto non_unit = ConstructionLine::create_explicit(
      ConstructionLineId("construction_line.bad"), "Bad", Point3{0.0, 0.0, 0.0},
      Vector3{0.0, 0.0, 2.0});
  REQUIRE(non_unit.has_error());
  CHECK(non_unit.error().message() == "construction line direction must be unit length");
}

TEST_CASE("ConstructionPlane validates orthonormal frame", "[core][construction_geometry]") {
  const auto plane = make_xy_offset_plane();

  CHECK(plane.id().value() == "construction_plane.offset_xy");
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
  REQUIRE(document.value().add_construction_plane(make_xy_offset_plane({ParameterId("part.depth")})));

  auto sketch = Sketch::create(SketchId("sketch.on_construction_plane"), "OnConstructionPlane",
                               DatumPlaneId("construction_plane.offset_xy"));
  REQUIRE(sketch);
  auto rectangle = RectangleProfile::create(ProfileId("profile.rectangle"), ParameterId("part.width"),
                                            ParameterId("part.height"));
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

  REQUIRE(document.value().mark_all_clean, true);
  document.value().mark_all_clean();
  const auto affected = document.value().mark_parameter_changed(ParameterId("part.depth"));
  REQUIRE(affected);
  const auto plan = document.value().create_recompute_plan();
  REQUIRE(plan);
  CHECK(plan.value().contains("construction_plane.offset_xy"));
  CHECK(plan.value().contains("sketch.on_construction_plane"));
  CHECK(plan.value().contains("feature.construction_plane_prism"));
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
