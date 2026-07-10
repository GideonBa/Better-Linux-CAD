#include "blcad/core/sketch.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace blcad;

TEST_CASE("LineSegment stores endpoints", "[core][sketch]") {
  const auto line =
      LineSegment::create(SketchEntityId("line.a"), Point2{0.0, 0.0}, Point2{1.0, 0.0});

  REQUIRE(line);
  CHECK(line.value().id().value() == "line.a");
  CHECK(line.value().start() == Point2{0.0, 0.0});
  CHECK(line.value().end() == Point2{1.0, 0.0});
}

TEST_CASE("LineSegment rejects empty ids and zero length", "[core][sketch]") {
  const auto missing_id = LineSegment::create(SketchEntityId(), Point2{0.0, 0.0}, Point2{1.0, 0.0});
  REQUIRE(missing_id.has_error());
  CHECK(missing_id.error().object_id() == "line_segment");
  CHECK(missing_id.error().message() == "line segment id must not be empty");

  const auto zero_length =
      LineSegment::create(SketchEntityId("line.zero"), Point2{0.0, 0.0}, Point2{0.0, 0.0});
  REQUIRE(zero_length.has_error());
  CHECK(zero_length.error().object_id() == "line.zero");
  CHECK(zero_length.error().message() == "line segment endpoints must not be identical");
}

TEST_CASE("RectangleProfile stores center and parameter references", "[core][sketch]") {
  const auto profile = RectangleProfile::create(
      ProfileId("profile.base_rectangle"), ParameterId("part.width"), ParameterId("part.height"));

  REQUIRE(profile);
  const Point2 expected_center{0.0, 0.0};

  CHECK(profile.value().id().value() == "profile.base_rectangle");
  CHECK(profile.value().center() == expected_center);
  CHECK(profile.value().width_parameter().value() == "part.width");
  CHECK(profile.value().height_parameter().value() == "part.height");
}

TEST_CASE("RectangleProfile rejects missing ids and parameter references", "[core][sketch]") {
  const auto missing_id =
      RectangleProfile::create(ProfileId(), ParameterId("part.width"), ParameterId("part.height"));
  REQUIRE(missing_id.has_error());
  CHECK(missing_id.error().object_id() == "rectangle_profile");
  CHECK(missing_id.error().message() == "rectangle profile id must not be empty");

  const auto missing_width = RectangleProfile::create(ProfileId("profile.base_rectangle"),
                                                      ParameterId(), ParameterId("part.height"));
  REQUIRE(missing_width.has_error());
  CHECK(missing_width.error().object_id() == "profile.base_rectangle");
  CHECK(missing_width.error().message() == "rectangle width parameter id must not be empty");

  const auto missing_height = RectangleProfile::create(ProfileId("profile.base_rectangle"),
                                                       ParameterId("part.width"), ParameterId());
  REQUIRE(missing_height.has_error());
  CHECK(missing_height.error().object_id() == "profile.base_rectangle");
  CHECK(missing_height.error().message() == "rectangle height parameter id must not be empty");
}

TEST_CASE("CircleProfile stores center and diameter parameter reference", "[core][sketch]") {
  const auto profile =
      CircleProfile::create(ProfileId("profile.center_hole"), ParameterId("part.hole_diameter"));

  REQUIRE(profile);
  const Point2 expected_center{0.0, 0.0};

  CHECK(profile.value().id().value() == "profile.center_hole");
  CHECK(profile.value().center() == expected_center);
  CHECK(profile.value().diameter_parameter().value() == "part.hole_diameter");
}

TEST_CASE("CircleProfile rejects missing ids and diameter references", "[core][sketch]") {
  const auto missing_id = CircleProfile::create(ProfileId(), ParameterId("part.hole_diameter"));
  REQUIRE(missing_id.has_error());
  CHECK(missing_id.error().object_id() == "circle_profile");
  CHECK(missing_id.error().message() == "circle profile id must not be empty");

  const auto missing_diameter =
      CircleProfile::create(ProfileId("profile.center_hole"), ParameterId());
  REQUIRE(missing_diameter.has_error());
  CHECK(missing_diameter.error().object_id() == "profile.center_hole");
  CHECK(missing_diameter.error().message() == "circle diameter parameter id must not be empty");
}

TEST_CASE("ClosedProfile stores ordered line segment references", "[core][sketch]") {
  auto profile = ClosedProfile::create(
      ProfileId("profile.triangle"),
      {SketchEntityId("line.a"), SketchEntityId("line.b"), SketchEntityId("line.c")});

  REQUIRE(profile);
  CHECK(profile.value().id().value() == "profile.triangle");
  REQUIRE(profile.value().line_segments().size() == 3);
  CHECK(profile.value().line_segments()[0].value() == "line.a");
}

TEST_CASE("ClosedProfile rejects invalid references", "[core][sketch]") {
  const auto missing_id = ClosedProfile::create(
      ProfileId(), {SketchEntityId("line.a"), SketchEntityId("line.b"), SketchEntityId("line.c")});
  REQUIRE(missing_id.has_error());
  CHECK(missing_id.error().object_id() == "closed_profile");
  CHECK(missing_id.error().message() == "closed profile id must not be empty");

  const auto too_few = ClosedProfile::create(ProfileId("profile.bad"),
                                             {SketchEntityId("line.a"), SketchEntityId("line.b")});
  REQUIRE(too_few.has_error());
  CHECK(too_few.error().message() == "closed profile requires at least three line segments");

  const auto duplicate = ClosedProfile::create(
      ProfileId("profile.bad"),
      {SketchEntityId("line.a"), SketchEntityId("line.b"), SketchEntityId("line.a")});
  REQUIRE(duplicate.has_error());
  CHECK(duplicate.error().message() ==
        "closed profile line segment ids must be unique within profile");
}

TEST_CASE("Sketch stores identity and workplane reference", "[core][sketch]") {
  const auto sketch =
      Sketch::create(SketchId("sketch.base"), "Sketch_BaseRectangle", DatumPlaneId("datum.xy"));

  REQUIRE(sketch);
  CHECK(sketch.value().id().value() == "sketch.base");
  CHECK(sketch.value().name() == "Sketch_BaseRectangle");
  CHECK(sketch.value().workplane().value() == "datum.xy");
  CHECK(sketch.value().profile_count() == 0);
}

TEST_CASE("Sketch rejects empty id, name and workplane", "[core][sketch]") {
  const auto missing_id =
      Sketch::create(SketchId(), "Sketch_BaseRectangle", DatumPlaneId("datum.xy"));
  REQUIRE(missing_id.has_error());
  CHECK(missing_id.error().object_id() == "sketch");
  CHECK(missing_id.error().message() == "sketch id must not be empty");

  const auto missing_name = Sketch::create(SketchId("sketch.base"), "", DatumPlaneId("datum.xy"));
  REQUIRE(missing_name.has_error());
  CHECK(missing_name.error().object_id() == "sketch.base");
  CHECK(missing_name.error().message() == "sketch name must not be empty");

  const auto missing_workplane =
      Sketch::create(SketchId("sketch.base"), "Sketch_BaseRectangle", DatumPlaneId());
  REQUIRE(missing_workplane.has_error());
  CHECK(missing_workplane.error().object_id() == "sketch.base");
  CHECK(missing_workplane.error().message() == "sketch workplane id must not be empty");
}

TEST_CASE("Sketch adds rectangle circle and closed profiles", "[core][sketch]") {
  auto sketch =
      Sketch::create(SketchId("sketch.base"), "Sketch_BaseRectangle", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);

  auto rectangle = RectangleProfile::create(ProfileId("profile.base_rectangle"),
                                            ParameterId("part.width"), ParameterId("part.height"));
  REQUIRE(rectangle);

  auto circle =
      CircleProfile::create(ProfileId("profile.center_hole"), ParameterId("part.hole_diameter"));
  REQUIRE(circle);

  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.a"), Point2{0.0, 0.0}, Point2{1.0, 0.0}).value()));
  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.b"), Point2{1.0, 0.0}, Point2{0.0, 1.0}).value()));
  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.c"), Point2{0.0, 1.0}, Point2{0.0, 0.0}).value()));

  auto closed_profile = ClosedProfile::create(
      ProfileId("profile.triangle"),
      {SketchEntityId("line.a"), SketchEntityId("line.b"), SketchEntityId("line.c")});
  REQUIRE(closed_profile);

  const auto rectangle_index = sketch.value().add_profile(rectangle.value());
  REQUIRE(rectangle_index);
  CHECK(rectangle_index.value() == 0);

  const auto circle_index = sketch.value().add_profile(circle.value());
  REQUIRE(circle_index);
  CHECK(circle_index.value() == 1);

  const auto closed_index = sketch.value().add_profile(closed_profile.value());
  REQUIRE(closed_index);
  CHECK(closed_index.value() == 2);

  CHECK(sketch.value().profile_count() == 3);
  CHECK(sketch.value().line_segments().size() == 3);
  CHECK(sketch.value().rectangle_profiles().size() == 1);
  CHECK(sketch.value().circle_profiles().size() == 1);
  CHECK(sketch.value().closed_profiles().size() == 1);
  CHECK(sketch.value().find_line_segment(SketchEntityId("line.a")) != nullptr);
  CHECK(sketch.value().find_rectangle_profile(ProfileId("profile.base_rectangle")) != nullptr);
  CHECK(sketch.value().find_circle_profile(ProfileId("profile.center_hole")) != nullptr);
  CHECK(sketch.value().find_closed_profile(ProfileId("profile.triangle")) != nullptr);
}

TEST_CASE("Sketch rejects duplicate profile ids across profile types", "[core][sketch]") {
  auto sketch =
      Sketch::create(SketchId("sketch.base"), "Sketch_BaseRectangle", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);

  auto rectangle = RectangleProfile::create(ProfileId("profile.shared"), ParameterId("part.width"),
                                            ParameterId("part.height"));
  REQUIRE(rectangle);
  REQUIRE(sketch.value().add_profile(rectangle.value()));

  auto circle =
      CircleProfile::create(ProfileId("profile.shared"), ParameterId("part.hole_diameter"));
  REQUIRE(circle);

  const auto duplicate = sketch.value().add_profile(circle.value());
  REQUIRE(duplicate.has_error());
  CHECK(duplicate.error().category() == ErrorCategory::Validation);
  CHECK(duplicate.error().object_id() == "profile.shared");
  CHECK(duplicate.error().message() == "profile id must be unique within sketch");
}

TEST_CASE("Sketch validates ordered closed profile loops", "[core][sketch]") {
  auto sketch =
      Sketch::create(SketchId("sketch.triangle"), "Sketch_Triangle", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);

  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.a"), Point2{0.0, 0.0}, Point2{2.0, 0.0}).value()));
  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.b"), Point2{2.0, 0.0}, Point2{0.0, 2.0}).value()));
  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.c"), Point2{0.0, 2.0}, Point2{0.0, 0.0}).value()));

  auto profile = ClosedProfile::create(
      ProfileId("profile.triangle"),
      {SketchEntityId("line.a"), SketchEntityId("line.b"), SketchEntityId("line.c")});
  REQUIRE(profile);

  REQUIRE(sketch.value().add_profile(profile.value()));
  const auto vertices = sketch.value().closed_profile_vertices(profile.value());
  REQUIRE(vertices);
  CHECK(vertices.value().size() == 3);
}

TEST_CASE("Sketch rejects disconnected and self-intersecting closed profiles", "[core][sketch]") {
  auto disconnected = Sketch::create(SketchId("sketch.disconnected"), "Sketch_Disconnected",
                                     DatumPlaneId("datum.xy"));
  REQUIRE(disconnected);
  REQUIRE(disconnected.value().add_entity(
      LineSegment::create(SketchEntityId("line.a"), Point2{0.0, 0.0}, Point2{1.0, 0.0}).value()));
  REQUIRE(disconnected.value().add_entity(
      LineSegment::create(SketchEntityId("line.b"), Point2{2.0, 0.0}, Point2{0.0, 1.0}).value()));
  REQUIRE(disconnected.value().add_entity(
      LineSegment::create(SketchEntityId("line.c"), Point2{0.0, 1.0}, Point2{0.0, 0.0}).value()));
  auto disconnected_profile = ClosedProfile::create(
      ProfileId("profile.disconnected"),
      {SketchEntityId("line.a"), SketchEntityId("line.b"), SketchEntityId("line.c")});
  REQUIRE(disconnected_profile);
  const auto disconnected_added = disconnected.value().add_profile(disconnected_profile.value());
  REQUIRE(disconnected_added.has_error());
  CHECK(disconnected_added.error().message() ==
        "closed profile line segments must be ordered and connected");

  auto bowtie =
      Sketch::create(SketchId("sketch.bowtie"), "Sketch_Bowtie", DatumPlaneId("datum.xy"));
  REQUIRE(bowtie);
  REQUIRE(bowtie.value().add_entity(
      LineSegment::create(SketchEntityId("line.a"), Point2{0.0, 0.0}, Point2{2.0, 2.0}).value()));
  REQUIRE(bowtie.value().add_entity(
      LineSegment::create(SketchEntityId("line.b"), Point2{2.0, 2.0}, Point2{0.0, 2.0}).value()));
  REQUIRE(bowtie.value().add_entity(
      LineSegment::create(SketchEntityId("line.c"), Point2{0.0, 2.0}, Point2{2.0, 0.0}).value()));
  REQUIRE(bowtie.value().add_entity(
      LineSegment::create(SketchEntityId("line.d"), Point2{2.0, 0.0}, Point2{0.0, 0.0}).value()));
  auto bowtie_profile = ClosedProfile::create(ProfileId("profile.bowtie"),
                                              {SketchEntityId("line.a"), SketchEntityId("line.b"),
                                               SketchEntityId("line.c"), SketchEntityId("line.d")});
  REQUIRE(bowtie_profile);
  const auto bowtie_added = bowtie.value().add_profile(bowtie_profile.value());
  REQUIRE(bowtie_added.has_error());
  CHECK(bowtie_added.error().message() == "closed profile line segments must not self-intersect");
}
