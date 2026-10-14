#include "blcad/core/sketch_modify.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>

using namespace blcad;

namespace {

Sketch make_sketch() {
  auto sketch = Sketch::create(SketchId("sketch.modify"), "Modify", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  return std::move(sketch.value());
}

void add_line(Sketch& sketch, const char* id, Point2 start, Point2 end) {
  REQUIRE(sketch.add_entity(LineSegment::create(SketchEntityId(id), start, end).value()));
}

void add_arc(Sketch& sketch, const char* id, Point2 start, Point2 mid, Point2 end) {
  REQUIRE(sketch.add_entity(
      ArcSegment::create_three_point(SketchEntityId(id), start, mid, end).value()));
}

void add_spline(Sketch& sketch, const char* id, Point2 start, Point2 control1, Point2 control2,
                Point2 end) {
  REQUIRE(sketch.add_entity(
      SplineSegment::create_cubic_bezier(SketchEntityId(id), start, control1, control2, end)
          .value()));
}

const LineSegment* line(const Sketch& sketch, const char* id) {
  return sketch.find_line_segment(SketchEntityId(id));
}

bool same(Point2 first, Point2 second) {
  return std::hypot(first.x - second.x, first.y - second.y) <= 1.0e-6;
}

bool contains(const std::vector<std::string>& values, const char* expected) {
  return std::find(values.begin(), values.end(), expected) != values.end();
}

Parameter length_parameter(const char* id, double millimeters) {
  auto quantity = Quantity::length_mm(millimeters, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), id, quantity.value());
  REQUIRE(parameter);
  return std::move(parameter.value());
}

} // namespace

TEST_CASE("Block 116 trim shortens a line to the picked bounding intersection",
          "[core][sketch-modify]") {
  Sketch sketch = make_sketch();
  add_line(sketch, "line.h", {0.0, 0.0}, {10.0, 0.0});
  add_line(sketch, "line.v", {4.0, -5.0}, {4.0, 5.0});

  auto result = SketchModifyService::trim(sketch, SketchEntityId("line.h"), {8.0, 0.0});
  REQUIRE(result);
  CHECK(result.value().invalidated_ids.empty());
  const auto* trimmed = line(result.value().sketch, "line.h");
  REQUIRE(trimmed != nullptr);
  CHECK(same(trimmed->start(), {0.0, 0.0}));
  CHECK(same(trimmed->end(), {4.0, 0.0}));
  CHECK(result.value().sketch.line_segments().size() == 2U);
  REQUIRE(result.value().attachments.size() == 1U);
  CHECK(result.value().attachments[0].result_entity == SketchEntityId("line.h"));
  CHECK(result.value().attachments[0].result_endpoint == SketchModifyEndSelector::End);
  CHECK(result.value().attachments[0].cutter_entity == "entity/line.v");
  CHECK_FALSE(result.value().attachments[0].cutter_endpoint.has_value());
  CHECK(result.value().invalidated_endpoints ==
        std::vector<SketchModifyInvalidatedEndpoint>{
            {SketchEntityId("line.h"), SketchModifyEndSelector::End}});
}

TEST_CASE("Block 116 trim removes the middle span between two intersections as a split",
          "[core][sketch-modify]") {
  Sketch sketch = make_sketch();
  add_line(sketch, "line.h", {0.0, 0.0}, {12.0, 0.0});
  add_line(sketch, "line.a", {3.0, -2.0}, {3.0, 2.0});
  add_line(sketch, "line.b", {9.0, -2.0}, {9.0, 2.0});

  auto result = SketchModifyService::trim(sketch, SketchEntityId("line.h"), {6.0, 0.0});
  REQUIRE(result);
  CHECK(std::find(result.value().invalidated_ids.begin(), result.value().invalidated_ids.end(),
                  "line.h") != result.value().invalidated_ids.end());
  const auto* first = line(result.value().sketch, "line.h");
  const auto* second = line(result.value().sketch, "line.h.split.1");
  REQUIRE(first != nullptr);
  REQUIRE(second != nullptr);
  CHECK(same(first->end(), {3.0, 0.0}));
  CHECK(same(second->start(), {9.0, 0.0}));
  CHECK(result.value().attachments ==
        std::vector<SketchModifyTrimAttachment>{
            {SketchEntityId("line.h"), SketchModifyEndSelector::End, "entity/line.a",
             std::nullopt},
            {SketchEntityId("line.h.split.1"), SketchModifyEndSelector::Start,
             "entity/line.b", std::nullopt}});
  CHECK(result.value().invalidated_endpoints.empty());
}

TEST_CASE("Block 116 trim removes a line with no bounding intersection", "[core][sketch-modify]") {
  Sketch sketch = make_sketch();
  add_line(sketch, "line.h", {0.0, 0.0}, {10.0, 0.0});
  auto result = SketchModifyService::trim(sketch, SketchEntityId("line.h"), {5.0, 0.0});
  REQUIRE(result);
  CHECK(result.value().sketch.line_segments().empty());
  CHECK(result.value().attachments.empty());
  CHECK(result.value().invalidated_endpoints.empty());
}

TEST_CASE("Block 116 trim metadata prefers a cutter endpoint for true coincidence",
          "[core][sketch-modify][trim-coincident]") {
  Sketch sketch = make_sketch();
  add_line(sketch, "line.target", {0.0, 0.0}, {10.0, 0.0});
  add_line(sketch, "line.cutter", {4.0, 0.0}, {4.0, 5.0});

  auto result =
      SketchModifyService::trim(sketch, SketchEntityId("line.target"), {8.0, 0.0});
  REQUIRE(result);
  REQUIRE(result.value().attachments.size() == 1U);
  CHECK(result.value().attachments[0].cutter_entity == "entity/line.cutter");
  REQUIRE(result.value().attachments[0].cutter_endpoint.has_value());
  CHECK(*result.value().attachments[0].cutter_endpoint == SketchModifyEndSelector::Start);
}

TEST_CASE("Block 116 extend lengthens the picked end to the next intersection",
          "[core][sketch-modify]") {
  Sketch sketch = make_sketch();
  add_line(sketch, "line.h", {0.0, 0.0}, {4.0, 0.0});
  add_line(sketch, "line.v", {8.0, -3.0}, {8.0, 3.0});

  auto result = SketchModifyService::extend(sketch, SketchEntityId("line.h"), {4.0, 0.0});
  REQUIRE(result);
  const auto* extended = line(result.value().sketch, "line.h");
  REQUIRE(extended != nullptr);
  CHECK(same(extended->start(), {0.0, 0.0}));
  CHECK(same(extended->end(), {8.0, 0.0}));
}

TEST_CASE("Block 116 split divides a line into two entities at the projected point",
          "[core][sketch-modify]") {
  Sketch sketch = make_sketch();
  add_line(sketch, "line.h", {0.0, 0.0}, {10.0, 0.0});
  auto result = SketchModifyService::split(sketch, SketchEntityId("line.h"), {6.0, 1.0});
  REQUIRE(result);
  const auto* first = line(result.value().sketch, "line.h");
  const auto* second = line(result.value().sketch, "line.h.split.1");
  REQUIRE(first != nullptr);
  REQUIRE(second != nullptr);
  CHECK(same(first->end(), {6.0, 0.0}));
  CHECK(same(second->start(), {6.0, 0.0}));
  CHECK(same(second->end(), {10.0, 0.0}));
}

TEST_CASE("Block 116 fillet trims two lines and inserts a tangent arc", "[core][sketch-modify]") {
  Sketch sketch = make_sketch();
  add_line(sketch, "line.h", {0.0, 0.0}, {10.0, 0.0});
  add_line(sketch, "line.v", {0.0, 0.0}, {0.0, 10.0});

  auto result =
      SketchModifyService::fillet(sketch, SketchEntityId("line.h"), SketchEntityId("line.v"), 2.0);
  REQUIRE(result);
  const Sketch& modified = result.value().sketch;
  CHECK(modified.arc_segments().size() == 1U);
  const auto* trimmed_h = line(modified, "line.h");
  const auto* trimmed_v = line(modified, "line.v");
  REQUIRE(trimmed_h != nullptr);
  REQUIRE(trimmed_v != nullptr);
  CHECK(same(trimmed_h->start(), {2.0, 0.0}));
  CHECK(same(trimmed_v->end(), {0.0, 10.0}));
  CHECK(same(trimmed_v->start(), {0.0, 2.0}));
  const auto& arc = modified.arc_segments().front();
  CHECK((same(arc.start(), {2.0, 0.0}) || same(arc.start(), {0.0, 2.0})));
}

TEST_CASE("Block 116 chamfer trims two lines and inserts a connecting line",
          "[core][sketch-modify]") {
  Sketch sketch = make_sketch();
  add_line(sketch, "line.h", {0.0, 0.0}, {10.0, 0.0});
  add_line(sketch, "line.v", {0.0, 0.0}, {0.0, 10.0});

  auto result =
      SketchModifyService::chamfer(sketch, SketchEntityId("line.h"), SketchEntityId("line.v"), 3.0);
  REQUIRE(result);
  const Sketch& modified = result.value().sketch;
  CHECK(modified.line_segments().size() == 3U);
  const auto* chamfer = line(modified, "line.chamfer.1");
  REQUIRE(chamfer != nullptr);
  CHECK((same(chamfer->start(), {3.0, 0.0}) || same(chamfer->start(), {0.0, 3.0})));
  CHECK((same(chamfer->end(), {3.0, 0.0}) || same(chamfer->end(), {0.0, 3.0})));
}

TEST_CASE("Block 116 preserves an orientation constraint on an in-place trimmed line",
          "[core][sketch-modify]") {
  Sketch sketch = make_sketch();
  add_line(sketch, "line.h", {0.0, 0.0}, {10.0, 0.0});
  add_line(sketch, "line.v", {4.0, -5.0}, {4.0, 5.0});
  REQUIRE(sketch.add_constraint(
      SketchGeometricConstraint::create_horizontal(
          SketchConstraintId("horizontal.h"),
          SketchReferenceTarget::create_line_segment(SketchEntityId("line.h")).value())
          .value()));

  auto result = SketchModifyService::trim(sketch, SketchEntityId("line.h"), {8.0, 0.0});
  REQUIRE(result);
  CHECK(result.value().sketch.geometric_constraints().size() == 1U);
}

TEST_CASE("Block 116 rejects splitting a line referenced by a dimension", "[core][sketch-modify]") {
  Sketch sketch = make_sketch();
  add_line(sketch, "line.h", {0.0, 0.0}, {10.0, 0.0});
  REQUIRE(sketch.add_dimension(
      SketchDrivingDimension::create_point_to_point_distance(
          SketchDimensionId("length.h"),
          SketchReferenceTarget::create_line_segment_start(SketchEntityId("line.h")).value(),
          SketchReferenceTarget::create_line_segment_end(SketchEntityId("line.h")).value(),
          ParameterId("length"))
          .value()));

  auto result = SketchModifyService::split(sketch, SketchEntityId("line.h"), {5.0, 0.0});
  REQUIRE_FALSE(result);
  CHECK(result.error().message().find("length.h") != std::string::npos);
}

TEST_CASE("Block 116 trim invalidates only the changed closed profile and preserves free points",
          "[core][sketch-modify]") {
  Sketch sketch = make_sketch();
  add_line(sketch, "line.a", {0.0, 0.0}, {10.0, 0.0});
  add_line(sketch, "line.b", {10.0, 0.0}, {10.0, 10.0});
  add_line(sketch, "line.c", {10.0, 10.0}, {0.0, 10.0});
  add_line(sketch, "line.d", {0.0, 10.0}, {0.0, 0.0});
  add_line(sketch, "line.cut", {4.0, -2.0}, {4.0, 2.0});
  add_line(sketch, "other.a", {20.0, 20.0}, {24.0, 20.0});
  add_line(sketch, "other.b", {24.0, 20.0}, {24.0, 24.0});
  add_line(sketch, "other.c", {24.0, 24.0}, {20.0, 24.0});
  add_line(sketch, "other.d", {20.0, 24.0}, {20.0, 20.0});
  REQUIRE(sketch.add_entity(
      SketchPoint::create(SketchEntityId("point.free"), Point2{30.0, 31.0}).value()));
  REQUIRE(
      sketch.add_profile(ClosedProfile::create(ProfileId("profile.rect"),
                                               {SketchEntityId("line.a"), SketchEntityId("line.b"),
                                                SketchEntityId("line.c"), SketchEntityId("line.d")})
                             .value()));
  REQUIRE(sketch.add_profile(
      ClosedProfile::create(ProfileId("profile.unchanged"),
                            {SketchEntityId("other.a"), SketchEntityId("other.b"),
                             SketchEntityId("other.c"), SketchEntityId("other.d")})
          .value()));

  auto result = SketchModifyService::trim(sketch, SketchEntityId("line.a"), {8.0, 0.0});
  REQUIRE(result);
  CHECK(result.value().invalidated_ids.empty());
  CHECK(contains(result.value().invalidated_profile_ids, "profile.rect"));
  CHECK_FALSE(contains(result.value().invalidated_profile_ids, "profile.unchanged"));
  CHECK(result.value().sketch.find_closed_profile(ProfileId("profile.rect")) == nullptr);
  CHECK(result.value().sketch.find_closed_profile(ProfileId("profile.unchanged")) != nullptr);
  const auto* trimmed = line(result.value().sketch, "line.a");
  REQUIRE(trimmed != nullptr);
  CHECK(same(trimmed->start(), {0.0, 0.0}));
  CHECK(same(trimmed->end(), {4.0, 0.0}));
  CHECK(line(result.value().sketch, "line.b") != nullptr);
  CHECK(line(result.value().sketch, "line.c") != nullptr);
  CHECK(line(result.value().sketch, "line.d") != nullptr);
  const SketchPoint* free_point = result.value().sketch.find_point(SketchEntityId("point.free"));
  REQUIRE(free_point != nullptr);
  CHECK(same(free_point->position(), {30.0, 31.0}));
}

TEST_CASE("Block 116 trim supports line and arc members of an arc closed slot profile",
          "[core][sketch-modify]") {
  Sketch sketch = make_sketch();
  add_line(sketch, "slot.top", {-5.0, 2.0}, {5.0, 2.0});
  add_arc(sketch, "slot.right", {5.0, 2.0}, {7.0, 0.0}, {5.0, -2.0});
  add_line(sketch, "slot.bottom", {5.0, -2.0}, {-5.0, -2.0});
  add_arc(sketch, "slot.left", {-5.0, -2.0}, {-7.0, 0.0}, {-5.0, 2.0});
  REQUIRE(sketch.add_profile(
      ArcClosedProfile::create(ProfileId("profile.slot"),
                               {SketchEntityId("slot.top"), SketchEntityId("slot.right"),
                                SketchEntityId("slot.bottom"), SketchEntityId("slot.left")})
          .value()));

  SECTION("line member") {
    add_line(sketch, "line.cut", {0.0, 1.0}, {0.0, 3.0});
    auto result = SketchModifyService::trim(sketch, SketchEntityId("slot.top"), {4.0, 2.0});
    REQUIRE(result);
    CHECK(contains(result.value().invalidated_profile_ids, "profile.slot"));
    CHECK(result.value().sketch.arc_closed_profiles().empty());
    const auto* trimmed = line(result.value().sketch, "slot.top");
    REQUIRE(trimmed != nullptr);
    CHECK(same(trimmed->start(), {-5.0, 2.0}));
    CHECK(same(trimmed->end(), {0.0, 2.0}));
    CHECK(result.value().sketch.find_arc_segment(SketchEntityId("slot.right")) != nullptr);
    CHECK(result.value().sketch.find_arc_segment(SketchEntityId("slot.left")) != nullptr);
  }

  SECTION("arc member") {
    add_line(sketch, "line.cut", {6.0, 0.0}, {8.0, 0.0});
    auto result = SketchModifyService::trim(sketch, SketchEntityId("slot.right"), {6.5, 1.5});
    REQUIRE(result);
    CHECK(contains(result.value().invalidated_profile_ids, "profile.slot"));
    CHECK(result.value().sketch.arc_closed_profiles().empty());
    const auto* trimmed = result.value().sketch.find_arc_segment(SketchEntityId("slot.right"));
    REQUIRE(trimmed != nullptr);
    CHECK(same(trimmed->start(), {7.0, 0.0}));
    CHECK(same(trimmed->end(), {5.0, -2.0}));
    CHECK(line(result.value().sketch, "slot.top") != nullptr);
    CHECK(line(result.value().sketch, "slot.bottom") != nullptr);
    CHECK(result.value().sketch.find_arc_segment(SketchEntityId("slot.left")) != nullptr);
  }
}

TEST_CASE("Block 116 trim invalidates a composite profile without deleting its other curves",
          "[core][sketch-modify]") {
  Sketch sketch = make_sketch();
  add_line(sketch, "outer.bottom", {-10.0, -8.0}, {10.0, -8.0});
  add_line(sketch, "outer.right", {10.0, -8.0}, {10.0, 8.0});
  add_line(sketch, "outer.top", {10.0, 8.0}, {-10.0, 8.0});
  add_line(sketch, "outer.left", {-10.0, 8.0}, {-10.0, -8.0});
  add_line(sketch, "inner.bottom", {-5.0, -3.0}, {5.0, -3.0});
  add_line(sketch, "inner.right", {5.0, -3.0}, {5.0, 3.0});
  add_line(sketch, "inner.top", {5.0, 3.0}, {-5.0, 3.0});
  add_line(sketch, "inner.left", {-5.0, 3.0}, {-5.0, -3.0});
  add_line(sketch, "line.cut", {0.0, 2.0}, {0.0, 4.0});
  REQUIRE(sketch.add_profile(CompositeClosedProfile::create(
                                 ProfileId("profile.with_hole"),
                                 {SketchEntityId("outer.bottom"), SketchEntityId("outer.right"),
                                  SketchEntityId("outer.top"), SketchEntityId("outer.left")},
                                 {{SketchEntityId("inner.bottom"), SketchEntityId("inner.right"),
                                   SketchEntityId("inner.top"), SketchEntityId("inner.left")}})
                                 .value()));

  auto result = SketchModifyService::trim(sketch, SketchEntityId("inner.top"), {4.0, 3.0});
  REQUIRE(result);
  CHECK(contains(result.value().invalidated_profile_ids, "profile.with_hole"));
  CHECK(result.value().sketch.composite_closed_profiles().empty());
  CHECK(result.value().sketch.line_segments().size() == 9U);
  const auto* trimmed = line(result.value().sketch, "inner.top");
  REQUIRE(trimmed != nullptr);
  CHECK(same(trimmed->start(), {0.0, 3.0}));
  CHECK(same(trimmed->end(), {-5.0, 3.0}));
  CHECK(line(result.value().sketch, "outer.bottom") != nullptr);
  CHECK(line(result.value().sketch, "inner.bottom") != nullptr);
}

TEST_CASE("Block 116 trim splits a cubic spline with exact De Casteljau subcurves",
          "[core][sketch-modify]") {
  Sketch sketch = make_sketch();
  add_spline(sketch, "spline.top", {0.0, 0.0}, {3.0, 6.0}, {7.0, 6.0}, {10.0, 0.0});
  add_line(sketch, "profile.right", {10.0, 0.0}, {10.0, -2.0});
  add_line(sketch, "profile.bottom", {10.0, -2.0}, {0.0, -2.0});
  add_line(sketch, "profile.left", {0.0, -2.0}, {0.0, 0.0});
  add_line(sketch, "cut.left", {2.40625, -4.0}, {2.40625, 8.0});
  add_line(sketch, "cut.right", {7.59375, -4.0}, {7.59375, 8.0});
  REQUIRE(sketch.add_profile(
      ArcClosedProfile::create(ProfileId("profile.spline"),
                               {SketchEntityId("spline.top"), SketchEntityId("profile.right"),
                                SketchEntityId("profile.bottom"), SketchEntityId("profile.left")})
          .value()));

  auto result = SketchModifyService::trim(sketch, SketchEntityId("spline.top"), {5.0, 4.5});
  REQUIRE(result);
  CHECK(contains(result.value().invalidated_ids, "spline.top"));
  CHECK(contains(result.value().invalidated_profile_ids, "profile.spline"));
  CHECK(result.value().sketch.arc_closed_profiles().empty());
  const auto* first = result.value().sketch.find_spline_segment(SketchEntityId("spline.top"));
  const auto* second =
      result.value().sketch.find_spline_segment(SketchEntityId("spline.top.split.1"));
  REQUIRE(first != nullptr);
  REQUIRE(second != nullptr);
  CHECK(same(first->start(), {0.0, 0.0}));
  CHECK(same(first->control1(), {0.75, 1.5}));
  CHECK(same(first->control2(), {1.5625, 2.625}));
  CHECK(same(first->end(), {2.40625, 3.375}));
  CHECK(same(second->start(), {7.59375, 3.375}));
  CHECK(same(second->control1(), {8.4375, 2.625}));
  CHECK(same(second->control2(), {9.25, 1.5}));
  CHECK(same(second->end(), {10.0, 0.0}));
}

TEST_CASE("Block 116 document-aware trim rewrites circle profiles and uses circles as cutters",
          "[core][sketch-modify]") {
  SECTION("trimmed circle becomes the complementary arc") {
    Sketch sketch = make_sketch();
    add_line(sketch, "line.cut", {0.0, -20.0}, {0.0, 20.0});
    REQUIRE(sketch.add_profile(
        CircleProfile::create(ProfileId("profile.circle"), ParameterId("circle.diameter"))
            .value()));
    auto document = PartDocument::create(DocumentId("part.circle.trim"), "Circle trim");
    REQUIRE(document);
    REQUIRE(document.value().add_parameter(length_parameter("circle.diameter", 20.0)));
    REQUIRE(document.value().add_datum_plane(DatumPlane::xy().value()));
    REQUIRE(document.value().add_sketch(std::move(sketch)));
    const Sketch* source = document.value().find_sketch(SketchId("sketch.modify"));
    REQUIRE(source != nullptr);

    CHECK_FALSE(SketchModifyService::trim(document.value(), *source, ProfileId("profile.circle"),
                                          {0.0, 0.0}));
    CHECK_FALSE(SketchModifyService::trim(document.value(), *source, ProfileId("profile.circle"),
                                          {20.0, 0.0}));
    constexpr double display_step = 5.0 * 3.14159265358979323846 / 180.0;
    const Point2 display_chord_midpoint{5.0 * (1.0 + std::cos(display_step)),
                                        5.0 * std::sin(display_step)};
    CHECK(SketchModifyService::trim(document.value(), *source, ProfileId("profile.circle"),
                                    display_chord_midpoint));
    auto result = SketchModifyService::trim(document.value(), *source, ProfileId("profile.circle"),
                                            {10.0, 0.0});
    REQUIRE(result);
    CHECK(result.value().sketch.circle_profiles().empty());
    CHECK(contains(result.value().invalidated_profile_ids, "profile.circle"));
    REQUIRE(result.value().sketch.arc_segments().size() == 1U);
    const ArcSegment& remaining = result.value().sketch.arc_segments().front();
    CHECK(same(remaining.start(), {0.0, 10.0}));
    CHECK(same(remaining.mid(), {-10.0, 0.0}));
    CHECK(same(remaining.end(), {0.0, -10.0}));
    CHECK(line(result.value().sketch, "line.cut") != nullptr);
    CHECK(result.value().attachments ==
          std::vector<SketchModifyTrimAttachment>{
              {remaining.id(), SketchModifyEndSelector::Start, "entity/line.cut", std::nullopt},
              {remaining.id(), SketchModifyEndSelector::End, "entity/line.cut", std::nullopt}});
  }

  SECTION("circle with no two-sided boundary is removed") {
    Sketch sketch = make_sketch();
    REQUIRE(sketch.add_profile(
        CircleProfile::create(ProfileId("profile.circle"), ParameterId("circle.diameter"))
            .value()));
    auto document = PartDocument::create(DocumentId("part.circle.delete"), "Circle delete");
    REQUIRE(document);
    REQUIRE(document.value().add_parameter(length_parameter("circle.diameter", 20.0)));
    REQUIRE(document.value().add_datum_plane(DatumPlane::xy().value()));
    REQUIRE(document.value().add_sketch(std::move(sketch)));
    const Sketch* source = document.value().find_sketch(SketchId("sketch.modify"));
    REQUIRE(source != nullptr);

    auto result = SketchModifyService::trim(document.value(), *source, ProfileId("profile.circle"),
                                            {10.0, 0.0});
    REQUIRE(result);
    CHECK(result.value().sketch.circle_profiles().empty());
    CHECK(result.value().sketch.arc_segments().empty());
    CHECK(contains(result.value().invalidated_profile_ids, "profile.circle"));
    CHECK(result.value().attachments.empty());
  }

  SECTION("circle profile cuts an explicit line") {
    Sketch sketch = make_sketch();
    add_line(sketch, "line.target", {-15.0, 0.0}, {15.0, 0.0});
    REQUIRE(sketch.add_profile(
        CircleProfile::create(ProfileId("profile.circle"), ParameterId("circle.diameter"))
            .value()));
    auto document = PartDocument::create(DocumentId("part.circle.cutter"), "Circle cutter");
    REQUIRE(document);
    REQUIRE(document.value().add_parameter(length_parameter("circle.diameter", 20.0)));
    REQUIRE(document.value().add_datum_plane(DatumPlane::xy().value()));
    REQUIRE(document.value().add_sketch(std::move(sketch)));
    const Sketch* source = document.value().find_sketch(SketchId("sketch.modify"));
    REQUIRE(source != nullptr);

    auto result = SketchModifyService::trim(document.value(), *source,
                                            SketchEntityId("line.target"), {0.0, 0.0});
    REQUIRE(result);
    const auto* first = line(result.value().sketch, "line.target");
    const auto* second = line(result.value().sketch, "line.target.split.1");
    REQUIRE(first != nullptr);
    REQUIRE(second != nullptr);
    CHECK(same(first->start(), {-15.0, 0.0}));
    CHECK(same(first->end(), {-10.0, 0.0}));
    CHECK(same(second->start(), {10.0, 0.0}));
    CHECK(same(second->end(), {15.0, 0.0}));
    CHECK(result.value().sketch.find_circle_profile(ProfileId("profile.circle")) != nullptr);
    CHECK(result.value().invalidated_profile_ids.empty());
    CHECK(result.value().attachments ==
          std::vector<SketchModifyTrimAttachment>{
              {SketchEntityId("line.target"), SketchModifyEndSelector::End,
               "profile/profile.circle", std::nullopt},
              {SketchEntityId("line.target.split.1"), SketchModifyEndSelector::Start,
               "profile/profile.circle", std::nullopt}});
  }
}

TEST_CASE("Block 116 rejects degenerate and unknown modify requests", "[core][sketch-modify]") {
  Sketch sketch = make_sketch();
  add_line(sketch, "line.h", {0.0, 0.0}, {10.0, 0.0});
  add_line(sketch, "line.p", {0.0, 2.0}, {10.0, 2.0});

  CHECK_FALSE(SketchModifyService::trim(sketch, SketchEntityId("missing"), {1.0, 0.0}));
  CHECK_FALSE(
      SketchModifyService::fillet(sketch, SketchEntityId("line.h"), SketchEntityId("line.p"), 1.0));
  CHECK_FALSE(
      SketchModifyService::fillet(sketch, SketchEntityId("line.h"), SketchEntityId("line.h"), 1.0));
  CHECK_FALSE(SketchModifyService::chamfer(sketch, SketchEntityId("line.h"),
                                           SketchEntityId("line.p"), -1.0));
}
