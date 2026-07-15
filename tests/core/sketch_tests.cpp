#include "blcad/core/part_document.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/sketch.hpp"
#include "blcad/core/sketch_edit_commands.hpp"
#include "blcad/core/sketch_topology.hpp"
#include "blcad/core/sketch_topology_json.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

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

TEST_CASE("Sketch topology migration shares coincident points deterministically",
          "[core][sketch-topology]") {
  const auto make_triangle = [](const std::vector<std::string>& insertion_order) {
    auto sketch =
        Sketch::create(SketchId("sketch.topology"), "Sketch_Topology", DatumPlaneId("datum.xy"));
    REQUIRE(sketch);
    for (const auto& id : insertion_order) {
      if (id == "line.a")
        REQUIRE(sketch.value().add_entity(LineSegment::create(
            SketchEntityId(id), Point2{0.0, 0.0}, Point2{2.0, 0.0}).value()));
      else if (id == "line.b")
        REQUIRE(sketch.value().add_entity(LineSegment::create(
            SketchEntityId(id), Point2{2.0, 0.0}, Point2{0.0, 2.0}).value()));
      else
        REQUIRE(sketch.value().add_entity(LineSegment::create(
            SketchEntityId(id), Point2{0.0, 2.0}, Point2{0.0, 0.0}).value()));
    }
    auto profile = ClosedProfile::create(
        ProfileId("profile.triangle"),
        {SketchEntityId("line.a"), SketchEntityId("line.b"), SketchEntityId("line.c")});
    REQUIRE(profile);
    REQUIRE(sketch.value().add_profile(profile.value()));
    return sketch.value();
  };

  SketchTopologyMigrationReport first_report;
  auto first = SketchTopology::migrate_legacy(
      make_triangle({"line.c", "line.a", "line.b"}), &first_report);
  REQUIRE(first);
  CHECK(first_report.migrated());
  CHECK(first.value().points().size() == 3U);
  CHECK(first_report.identity_changes().size() == 3U);

  const auto* line_a = first.value().find_entity("entity/line.a");
  const auto* line_b = first.value().find_entity("entity/line.b");
  const auto* profile = first.value().find_entity("profile/profile.triangle");
  REQUIRE(line_a != nullptr);
  REQUIRE(line_b != nullptr);
  REQUIRE(profile != nullptr);
  CHECK(line_a->points()[1] == line_b->points()[0]);
  CHECK(profile->entity_dependencies() ==
        std::vector<std::string>{"entity/line.a", "entity/line.b", "entity/line.c"});

  SketchTopologyMigrationReport second_report;
  auto second = SketchTopology::migrate_legacy(
      make_triangle({"line.b", "line.c", "line.a"}), &second_report);
  REQUIRE(second);
  CHECK(second.value() == first.value());
  CHECK(second_report == first_report);
}

TEST_CASE("Sketch topology edit commands are dependency-safe and exactly undoable",
          "[core][sketch-edit-command]") {
  auto sketch =
      Sketch::create(SketchId("sketch.edit"), "Sketch_Edit", DatumPlaneId("datum.xy"));
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

  auto topology = SketchTopology::migrate_legacy(sketch.value());
  REQUIRE(topology);
  const SketchTopology initial = topology.value();
  const auto* line_a = initial.find_entity("entity/line.a");
  REQUIRE(line_a != nullptr);
  const SketchPointId shared = line_a->points()[1];

  auto move = SketchEditCommand::move_point(shared, Point2{3.0, 0.0});
  REQUIRE(move);
  SketchTopologyUndoStack stack(initial);
  REQUIRE(stack.apply(move.value()));
  const SketchTopology moved = stack.current();
  const auto* moved_point = moved.find_point(shared);
  const auto* moved_line_b = moved.find_entity("entity/line.b");
  REQUIRE(moved_point != nullptr);
  REQUIRE(moved_line_b != nullptr);
  CHECK(moved_point->position() == Point2{3.0, 0.0});
  CHECK(moved_line_b->points()[0] == shared);

  auto remove_shared = SketchEditCommand::remove_point(shared);
  REQUIRE(remove_shared);
  const auto blocked_point = SketchEditCommandExecutor{}.apply(moved, remove_shared.value());
  REQUIRE(blocked_point.has_error());
  CHECK(blocked_point.error().category() == ErrorCategory::Dependency);

  auto remove_line = SketchEditCommand::remove_entity("entity/line.a");
  REQUIRE(remove_line);
  const auto blocked_line = SketchEditCommandExecutor{}.apply(moved, remove_line.value());
  REQUIRE(blocked_line.has_error());
  CHECK(blocked_line.error().category() == ErrorCategory::Dependency);
  CHECK(blocked_line.error().message().find("profile/profile.triangle") != std::string::npos);

  REQUIRE(stack.undo());
  CHECK(stack.current() == initial);
  REQUIRE(stack.redo());
  CHECK(stack.current() == moved);

  auto new_point = SketchTopologyPoint::create(SketchPointId("point.extra"), Point2{5.0, 5.0});
  REQUIRE(new_point);
  auto added_point = SketchEditCommandExecutor{}.apply(
      initial, SketchEditCommand::add_point(new_point.value()));
  REQUIRE(added_point);
  auto extra_line = SketchTopologyEntity::create(
      "entity/line.extra", SketchTopologyEntityKind::Line,
      {SketchPointId("point.extra"), line_a->points()[0]});
  REQUIRE(extra_line);
  auto added_line = SketchEditCommandExecutor{}.apply(
      added_point.value().after(), SketchEditCommand::add_entity(extra_line.value()));
  REQUIRE(added_line);

  auto replacement = SketchTopologyEntity::create(
      "entity/line.extra", SketchTopologyEntityKind::Line,
      {SketchPointId("point.extra"), shared}, {}, {.construction = true, .reference = false});
  REQUIRE(replacement);
  auto replaced = SketchEditCommandExecutor{}.apply(
      added_line.value().after(), SketchEditCommand::replace_entity(replacement.value()));
  REQUIRE(replaced);
  REQUIRE(replaced.value().after().find_entity("entity/line.extra") != nullptr);
  CHECK(replaced.value().after().find_entity("entity/line.extra")->construction());

  auto remove_extra_line = SketchEditCommand::remove_entity("entity/line.extra");
  REQUIRE(remove_extra_line);
  auto removed_line = SketchEditCommandExecutor{}.apply(
      replaced.value().after(), remove_extra_line.value());
  REQUIRE(removed_line);
  auto remove_extra_point = SketchEditCommand::remove_point(SketchPointId("point.extra"));
  REQUIRE(remove_extra_point);
  auto removed_point = SketchEditCommandExecutor{}.apply(
      removed_line.value().after(), remove_extra_point.value());
  REQUIRE(removed_point);
  CHECK(removed_point.value().after().find_point(SketchPointId("point.extra")) == nullptr);
}

TEST_CASE("Legacy Part Sketch JSON migrates to canonical topology JSON",
          "[core][sketch-json-migration]") {
  auto document = PartDocument::create(DocumentId("part.topology"), "TopologyPart");
  REQUIRE(document);
  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));

  auto sketch =
      Sketch::create(SketchId("sketch.legacy"), "Sketch_Legacy", DatumPlaneId("datum.xy"));
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
  REQUIRE(document.value().add_sketch(sketch.value()));

  auto legacy_json = serialize_part_document_to_json(document.value());
  REQUIRE(legacy_json);
  REQUIRE(legacy_json.value().find("\"start\"") != std::string::npos);

  auto migrated = migrate_legacy_part_document_sketch_json(
      legacy_json.value(), SketchId("sketch.legacy"));
  REQUIRE(migrated);
  CHECK(migrated.value().report.migrated());
  CHECK(migrated.value().topology.points().size() == 3U);
  CHECK(migrated.value().topology.find_entity("entity/line.a") != nullptr);
  CHECK(migrated.value().topology.find_entity("profile/profile.triangle") != nullptr);

  auto topology_json = serialize_sketch_topology_to_json(migrated.value().topology);
  REQUIRE(topology_json);
  CHECK(topology_json.value().find("blcad.sketch_topology.mvp8") != std::string::npos);
  CHECK(topology_json.value().find("\"construction\"") != std::string::npos);
  CHECK(topology_json.value().find("\"reference\"") != std::string::npos);

  auto restored = deserialize_sketch_topology_from_json(topology_json.value());
  REQUIRE(restored);
  CHECK(restored.value() == migrated.value().topology);

  auto duplicate_point = topology_json.value();
  const std::string needle = "\"id\": \"entity/line.a/start\"";
  const auto position = duplicate_point.find(needle);
  REQUIRE(position != std::string::npos);
  duplicate_point.replace(position, needle.size(), "\"id\": \"entity/line.a/end\"");
  const auto rejected = deserialize_sketch_topology_from_json(duplicate_point);
  REQUIRE(rejected.has_error());
  CHECK(rejected.error().category() == ErrorCategory::Validation);
}
