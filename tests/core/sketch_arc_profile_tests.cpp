#include "blcad/core/sketch.hpp"
#include "blcad/core/sketch_contour.hpp"
#include "blcad/core/sketch_transform_pattern.hpp"
#include "blcad/core/sketch_spline_edit.hpp"
#include "blcad/core/sketch_modify.hpp"
#include "blcad/core/sketch_topology_part_document.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace blcad;

namespace {

void add_line(Sketch& sketch, const char* id, Point2 start, Point2 end) {
  auto line = LineSegment::create(SketchEntityId(id), start, end);
  REQUIRE(line);
  REQUIRE(sketch.add_entity(line.value()));
}

void add_arc(Sketch& sketch, const char* id, Point2 start, Point2 mid, Point2 end) {
  auto arc = ArcSegment::create_three_point(SketchEntityId(id), start, mid, end);
  REQUIRE(arc);
  REQUIRE(sketch.add_entity(arc.value()));
}

Sketch make_arc_sketch() {
  auto sketch = Sketch::create(SketchId("sketch.arc"), "Arc", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  add_line(sketch.value(), "line.left", Point2{-10.0, 0.0}, Point2{-10.0, -10.0});
  add_line(sketch.value(), "line.bottom", Point2{-10.0, -10.0}, Point2{10.0, -10.0});
  add_line(sketch.value(), "line.right", Point2{10.0, -10.0}, Point2{10.0, 0.0});
  add_arc(sketch.value(), "arc.top", Point2{10.0, 0.0}, Point2{0.0, 10.0}, Point2{-10.0, 0.0});
  return sketch.value();
}

} // namespace

TEST_CASE("ArcSegment rejects collinear three-point arcs", "[core][sketch][arc]") {
  auto arc = ArcSegment::create_three_point(SketchEntityId("arc.bad"), Point2{0.0, 0.0},
                                            Point2{1.0, 0.0}, Point2{2.0, 0.0});
  REQUIRE_FALSE(arc);
  CHECK(arc.error().message() == "arc segment three-point definition must not be collinear");
}

TEST_CASE("Sketch stores trim and extend metadata for explicit curve entities",
          "[core][sketch][arc]") {
  Sketch sketch = make_arc_sketch();
  auto trim = SketchTrimExtendOperation::create_trim(SketchTrimOperationId("trim.arc.top"),
                                                     SketchEntityId("arc.top"), Point2{8.0, 1.0});
  REQUIRE(trim);
  REQUIRE(sketch.add_trim_extend_operation(trim.value()));
  CHECK(sketch.trim_extend_operations().size() == 1U);
  REQUIRE(sketch.find_trim_extend_operation(SketchTrimOperationId("trim.arc.top")) != nullptr);
}

TEST_CASE("Sketch validates ordered line and arc closed profiles", "[core][sketch][arc]") {
  Sketch sketch = make_arc_sketch();
  auto profile = ArcClosedProfile::create(
      ProfileId("profile.arc"), {SketchEntityId("line.left"), SketchEntityId("line.bottom"),
                                 SketchEntityId("line.right"), SketchEntityId("arc.top")});
  REQUIRE(profile);
  REQUIRE(sketch.add_profile(profile.value()));
  CHECK(sketch.arc_closed_profiles().size() == 1U);
  auto vertices = sketch.arc_closed_profile_vertices(sketch.arc_closed_profiles().front());
  REQUIRE(vertices);
  CHECK(vertices.value().size() == 4U);
}

TEST_CASE("Sketch rejects self-intersecting line and arc closed profiles", "[core][sketch][arc]") {
  auto sketch = Sketch::create(SketchId("sketch.arc.cross"), "ArcCross", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  add_line(sketch.value(), "line.bottom", Point2{0.0, 0.0}, Point2{10.0, 0.0});
  add_line(sketch.value(), "line.right", Point2{10.0, 0.0}, Point2{10.0, 10.0});
  add_arc(sketch.value(), "arc.crossing", Point2{10.0, 10.0}, Point2{5.0, -1.0}, Point2{0.0, 10.0});
  add_line(sketch.value(), "line.left", Point2{0.0, 10.0}, Point2{0.0, 0.0});

  auto profile = ArcClosedProfile::create(
      ProfileId("profile.crossing"), {SketchEntityId("line.bottom"), SketchEntityId("line.right"),
                                      SketchEntityId("arc.crossing"), SketchEntityId("line.left")});
  REQUIRE(profile);
  auto added = sketch.value().add_profile(profile.value());
  REQUIRE_FALSE(added);
  CHECK(added.error().message() == "arc closed profile curve segments must not self-intersect");
}

TEST_CASE("Curved profiles traverse reversed source curves without changing endpoint identity",
          "[core][sketch][arc][sketch-curved-regions]") {
  auto sketch = Sketch::create(SketchId("semicircle"), "Semicircle", DatumPlaneId("datum.xy")).value();
  // Both authored edges have the same endpoint direction. The profile needs
  // the arc in reverse; reversing the authored arc would invalidate references.
  add_line(sketch, "base", {-10, 0}, {10, 0});
  add_arc(sketch, "top", {-10, 0}, {0, 10}, {10, 0});
  const auto original = *sketch.find_arc_segment(SketchEntityId("top"));
  auto profile = ArcClosedProfile::create(ProfileId("profile"), {SketchEntityId("base"), SketchEntityId("top")});
  REQUIRE(profile);
  REQUIRE(sketch.add_profile(profile.value()));
  auto traversal = resolve_sketch_contour(sketch, profile.value().curve_segments());
  REQUIRE(traversal);
  CHECK_FALSE(traversal.value()[0].reversed);
  CHECK(traversal.value()[1].reversed);
  auto topology = SketchTopology::migrate_legacy(sketch);
  REQUIRE(topology);
  const auto* base = topology.value().find_entity("entity/base");
  const auto* top = topology.value().find_entity("entity/top");
  REQUIRE(base);
  REQUIRE(top);
  CHECK(base->points().front() == top->points().front());
  CHECK(base->points().back() == top->points().back());
  CHECK(topology.value().points().size() == 8U);
  auto restored = SketchTopologyLegacyMaterializer{}.materialize(sketch, topology.value());
  REQUIRE(restored);
  CHECK(restored.value().find_arc_segment(SketchEntityId("top"))->start().x == original.start().x);
  CHECK(restored.value().find_arc_segment(SketchEntityId("top"))->end().x == original.end().x);
  CHECK(SketchTopology::migrate_legacy(restored.value()).value() == topology.value());
}

TEST_CASE("Two circular arcs can bound a full circle and open contours still fail",
          "[core][sketch][arc][sketch-curved-regions]") {
  auto sketch = Sketch::create(SketchId("circle"), "Circle", DatumPlaneId("datum.xy")).value();
  add_arc(sketch, "top", {-10, 0}, {0, 10}, {10, 0});
  add_arc(sketch, "bottom", {10, 0}, {0, -10}, {-10, 0});
  auto profile = ArcClosedProfile::create(ProfileId("profile"), {SketchEntityId("top"), SketchEntityId("bottom")});
  REQUIRE(profile);
  REQUIRE(sketch.add_profile(profile.value()));
  CHECK(sketch.arc_closed_profile_vertices(profile.value()).value().size() == 2U);
  auto open = Sketch::create(SketchId("open"), "Open", DatumPlaneId("datum.xy")).value();
  add_arc(open, "top", {-10, 0}, {0, 10}, {10, 0});
  add_line(open, "base", {-10, 0}, {9, 0});
  auto gap = ArcClosedProfile::create(ProfileId("gap"), {SketchEntityId("top"), SketchEntityId("base")});
  REQUIRE(gap);
  CHECK_FALSE(open.add_profile(gap.value()));
  add_line(open, "back", {9, 0}, {-10, 0});
  auto flat = ArcClosedProfile::create(ProfileId("flat"), {SketchEntityId("base"), SketchEntityId("back")});
  REQUIRE(flat);
  CHECK_FALSE(open.add_profile(flat.value()));
  auto overlap = Sketch::create(SketchId("overlap"), "Overlap", DatumPlaneId("datum.xy")).value();
  add_arc(overlap, "a", {-10, 0}, {0, 10}, {10, 0});
  add_arc(overlap, "b", {10, 0}, {0, 10}, {-10, 0});
  auto doubled = ArcClosedProfile::create(ProfileId("overlap"), {SketchEntityId("a"), SketchEntityId("b")});
  REQUIRE(doubled);
  CHECK_FALSE(overlap.add_profile(doubled.value()));
}

TEST_CASE("Curved construction flags survive the PartDocument topology bridge",
          "[core][sketch][construction-conversion]") {
  auto sketch = make_arc_sketch();
  REQUIRE(sketch.add_entity(SplineSegment::create_cubic_bezier(SketchEntityId("spline"), {20, 0},
                                                               {22, 5}, {28, 5}, {30, 0},
                                                               {1, .8, .8, 1})
                                .value()));
  const auto topology = SketchTopology::migrate_legacy(sketch).value();
  std::vector<SketchTopologyEntity> entities;
  for (const auto& entity : topology.entities())
    entities.push_back(entity.reference() ? entity
                                          : SketchTopologyEntity::create(
                                                entity.id(), entity.kind(), entity.points(),
                                                entity.entity_dependencies(),
                                                {.construction = true}, entity.weights())
                                                .value());
  auto changed = SketchTopology::create(sketch.id(), topology.points(), std::move(entities),
                                        topology.dependencies());
  REQUIRE(changed);
  auto restored = SketchTopologyLegacyMaterializer{}.materialize(sketch, changed.value());
  REQUIRE(restored);
  const auto migrated = SketchTopology::migrate_legacy(restored.value()).value();
  CHECK(migrated.find_entity("entity/arc.top")->construction());
  CHECK(migrated.find_entity("entity/spline")->construction());
  CHECK(migrated.find_entity("entity/spline")->weights() ==
        topology.find_entity("entity/spline")->weights());
  CHECK(migrated.find_entity("entity/arc.top")->points() ==
        topology.find_entity("entity/arc.top")->points());
}

TEST_CASE("Circle construction state is not silently flattened by legacy materialization",
          "[core][sketch][construction-conversion]") {
  auto sketch = Sketch::create(SketchId("circle"), "Circle", DatumPlaneId("datum.xy")).value();
  REQUIRE(sketch.add_profile(
      CircleProfile::create(ProfileId("circle"), ParameterId("diameter"), {30, 40}).value()));
  const auto topology = SketchTopology::migrate_legacy(sketch).value();
  const auto* circle = topology.find_entity("profile/circle");
  auto entities = topology.entities();
  for (auto& entity : entities)
    if (entity.id() == circle->id())
      entity = SketchTopologyEntity::create(entity.id(), entity.kind(), entity.points(), {},
                                            {.construction = true})
                   .value();
  const auto changed = SketchTopology::create(sketch.id(), topology.points(), std::move(entities),
                                              topology.dependencies())
                           .value();
  auto restored = SketchTopologyLegacyMaterializer{}.materialize(sketch, changed);
  REQUIRE(restored);
  const auto migrated = SketchTopology::migrate_legacy(restored.value()).value();
  CHECK(migrated.find_entity("profile/circle")->construction());
  CHECK(migrated.find_entity("profile/circle")->points() == circle->points());
}

TEST_CASE("Construction conversion preserves mixed curve and generated-profile identity",
          "[core][sketch][construction-conversion]") {
  auto sketch = make_arc_sketch();
  REQUIRE(sketch.add_profile(
      ArcClosedProfile::create(ProfileId("group"),
                               {SketchEntityId("line.left"), SketchEntityId("line.bottom"),
                                SketchEntityId("line.right"), SketchEntityId("arc.top")})
          .value()));
  REQUIRE(sketch.add_entity(SplineSegment::create_cubic_bezier(SketchEntityId("spline"), {20, 0},
                                                               {22, 5}, {28, 5}, {30, 0},
                                                               {1, .8, .8, 1})
                                .value()));
  REQUIRE(sketch.add_profile(CircleProfile::create(ProfileId("circle"), ParameterId("d")).value()));
  REQUIRE(sketch.add_profile(
      RectangleProfile::create(ProfileId("rectangle"), ParameterId("w"), ParameterId("h"))
          .value()));
  const auto original = SketchTopology::migrate_legacy(sketch).value();
  SECTION("Mixed selection and generated group convert together and return exactly") {
    auto changed = sketch.set_construction(
        {SketchEntityId("spline"), SketchEntityId("spline")},
        {ProfileId("group"), ProfileId("circle"), ProfileId("rectangle")}, true);
    REQUIRE(changed);
    CHECK(changed.value() == 7);
    CHECK(sketch.profile_has_construction(ProfileId("group")));
    const auto converted = SketchTopology::migrate_legacy(sketch);
    REQUIRE(converted);
    for (const auto& entity : original.entities()) {
      const auto* current = converted.value().find_entity(entity.id());
      REQUIRE(current);
      CHECK(current->points() == entity.points());
      CHECK(current->weights() == entity.weights());
      for (const auto& point : entity.points())
        CHECK(converted.value().find_point(point)->position() ==
              original.find_point(point)->position());
    }
    REQUIRE(sketch.set_construction(
        {SketchEntityId("spline")},
        {ProfileId("group"), ProfileId("circle"), ProfileId("rectangle")}, false));
    CHECK(SketchTopology::migrate_legacy(sketch).value() == original);
  }
  SECTION("Composite inner contours and overlapping group selections expand exactly once") {
    auto composite =
        Sketch::create(SketchId("composite"), "Composite", DatumPlaneId("datum.xy")).value();
    add_line(composite, "outer.a", {-10, -10}, {10, -10});
    add_line(composite, "outer.b", {10, -10}, {10, 10});
    add_line(composite, "outer.c", {10, 10}, {-10, 10});
    add_line(composite, "outer.d", {-10, 10}, {-10, -10});
    add_line(composite, "inner.a", {0, -5}, {1, -5});
    add_line(composite, "inner.b", {1, -5}, {0, -4});
    add_line(composite, "inner.c", {0, -4}, {0, -5});
    const std::vector<SketchEntityId> outer{SketchEntityId("outer.a"), SketchEntityId("outer.b"),
                                            SketchEntityId("outer.c"), SketchEntityId("outer.d")};
    const std::vector<std::vector<SketchEntityId>> inner{
        {SketchEntityId("inner.a"), SketchEntityId("inner.b"), SketchEntityId("inner.c")}};
    REQUIRE(composite.add_profile(ClosedProfile::create(ProfileId("group"), outer).value()));
    auto added = composite.add_profile(
        CompositeClosedProfile::create(ProfileId("composite"), outer, inner).value());
    INFO((added ? std::string("Composite fixture accepted") : added.error().message()));
    REQUIRE(added);
    const auto before = SketchTopology::migrate_legacy(composite).value();
    auto converted =
        composite.set_construction({}, {ProfileId("group"), ProfileId("composite")}, true);
    REQUIRE(converted);
    CHECK(converted.value() == 7);
    CHECK(composite.profile_has_construction(ProfileId("composite")));
    CHECK(composite.find_composite_closed_profile(ProfileId("composite"))->outer_contour() ==
          outer);
    CHECK(composite.find_composite_closed_profile(ProfileId("composite"))->inner_contours() ==
          inner);
    for (const auto& id : inner.front())
      CHECK(composite.curve_is_construction(id));
    REQUIRE(composite.set_construction({}, {ProfileId("composite")}, false));
    CHECK(SketchTopology::migrate_legacy(composite).value() == before);
  }
  SECTION("One construction edge keeps the existing shared point ids") {
    REQUIRE(sketch.set_construction({SketchEntityId("arc.top")}, {}, true));
    auto mixed = SketchTopology::migrate_legacy(sketch);
    REQUIRE(mixed);
    CHECK(mixed.value().find_entity("entity/arc.top")->points() ==
          original.find_entity("entity/arc.top")->points());
    CHECK(sketch.profile_has_construction(ProfileId("group")));
    REQUIRE(sketch.set_construction({SketchEntityId("arc.top")}, {}, false));
    CHECK(SketchTopology::migrate_legacy(sketch).value() == original);
  }
  SECTION("An invalid mixed target leaves the whole selection unchanged") {
    REQUIRE_FALSE(
        sketch.set_construction({SketchEntityId("arc.top"), SketchEntityId("missing")}, {}, true));
    CHECK(SketchTopology::migrate_legacy(sketch).value() == original);
  }
}

TEST_CASE("Projected curves cannot change role even in a mixed selection",
          "[core][sketch][construction-conversion]") {
  auto sketch = make_arc_sketch();
  REQUIRE(sketch.add_reference(ProjectedSketchLine::create_from_construction_line(
                                   SketchEntityId("reference"), ConstructionLineId("source"))
                                   .value()));
  const auto original = SketchTopology::migrate_legacy(sketch).value();
  auto converted =
      sketch.set_construction({SketchEntityId("arc.top"), SketchEntityId("reference")}, {}, true);
  REQUIRE_FALSE(converted);
  CHECK(converted.error().message().find("read-only") != std::string::npos);
  CHECK(converted.error().message().find("detach") != std::string::npos);
  CHECK(SketchTopology::migrate_legacy(sketch).value() == original);
}

TEST_CASE("Construction roles survive curved transforms copies splits and control edits",
          "[core][sketch][construction-conversion]") {
  auto sketch = Sketch::create(SketchId("aux"), "Auxiliary", DatumPlaneId("datum.xy")).value();
  REQUIRE(sketch.add_entity(
      ArcSegment::create_three_point(SketchEntityId("arc"), {0, 0}, {5, 5}, {10, 0})
          .value()
          .with_construction(true)));
  const auto spline = SplineSegment::create_cubic_bezier(SketchEntityId("spline"), {20, 0}, {22, 5},
                                                         {28, 5}, {30, 0}, {1, .8, .8, 1})
                          .value()
                          .with_construction(true);
  REQUIRE(sketch.add_entity(spline));
  SECTION("Move and copy retain roles and rational weights") {
    auto moved =
        SketchTransformPatternService::move(sketch, {SketchEntityId("arc"), spline.id()}, {5, 8});
    REQUIRE(moved);
    CHECK(moved.value().sketch.find_arc_segment(SketchEntityId("arc"))->construction());
    CHECK(moved.value().sketch.find_spline_segment(spline.id())->construction());
    CHECK(moved.value().sketch.find_spline_segment(spline.id())->weights() == spline.weights());
    auto copied = SketchTransformPatternService::copy(
        moved.value().sketch, {SketchEntityId("arc"), spline.id()}, {40, 0});
    REQUIRE(copied);
    REQUIRE(copied.value().created_entities.size() == 2);
    for (const auto& id : copied.value().created_entities)
      CHECK(copied.value().sketch.curve_is_construction(id));
  }
  SECTION("Spline split keeps both exact auxiliary subcurves") {
    auto split = SketchModifyService::split(sketch, spline.id(), spline.curve().point(.4));
    REQUIRE(split);
    REQUIRE(split.value().sketch.spline_segments().size() == 2);
    for (const auto& segment : split.value().sketch.spline_segments())
      CHECK(segment.construction());
  }
  SECTION("A rational control edit does not reset the role") {
    auto edit = SketchSplineEditModel::from_segments(sketch, {spline.id()});
    REQUIRE(edit);
    REQUIRE(edit.value().move_control_point(0, 1, {22, 8}));
    const auto preview = edit.value().preview();
    REQUIRE(preview.segments.size() == 1);
    CHECK(preview.segments.front().construction());
    CHECK(preview.segments.front().weights() == spline.weights());
    CHECK(preview.segments.front().control1() == Point2{22, 8});
  }
}
