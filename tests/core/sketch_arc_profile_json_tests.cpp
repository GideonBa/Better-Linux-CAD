#include "blcad/core/part_document.hpp"
#include "blcad/core/part_document_json.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

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
  auto sketch = Sketch::create(SketchId("sketch.arc.json"), "ArcJson", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  add_line(sketch.value(), "line.left", Point2{-10.0, 0.0}, Point2{-10.0, -10.0});
  add_line(sketch.value(), "line.bottom", Point2{-10.0, -10.0}, Point2{10.0, -10.0});
  add_line(sketch.value(), "line.right", Point2{10.0, -10.0}, Point2{10.0, 0.0});
  add_arc(sketch.value(), "arc.top", Point2{10.0, 0.0}, Point2{0.0, 10.0}, Point2{-10.0, 0.0});
  auto trim = SketchTrimExtendOperation::create_trim(SketchTrimOperationId("trim.arc.top"),
                                                     SketchEntityId("arc.top"), Point2{8.0, 1.0});
  REQUIRE(trim);
  REQUIRE(sketch.value().add_trim_extend_operation(trim.value()));
  auto profile = ArcClosedProfile::create(
      ProfileId("profile.arc"), {SketchEntityId("line.left"), SketchEntityId("line.bottom"),
                                 SketchEntityId("line.right"), SketchEntityId("arc.top")});
  REQUIRE(profile);
  REQUIRE(sketch.value().add_profile(profile.value()));
  return sketch.value();
}

} // namespace

TEST_CASE("Arc sketch records roundtrip through JSON", "[core][json][arc]") {
  auto document = PartDocument::create(DocumentId("part.arc.json"), "ArcJson");
  REQUIRE(document);
  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));
  REQUIRE(document.value().add_sketch(make_arc_sketch()));

  auto serialized = serialize_part_document_to_json(document.value());
  REQUIRE(serialized);
  CHECK(serialized.value().find("arc_segments") != std::string::npos);
  CHECK(serialized.value().find("trim_extend_operations") != std::string::npos);
  CHECK(serialized.value().find("arc_closed_profiles") != std::string::npos);

  auto restored = deserialize_part_document_from_json(serialized.value());
  REQUIRE(restored);
  const Sketch* sketch = restored.value().find_sketch(SketchId("sketch.arc.json"));
  REQUIRE(sketch != nullptr);
  CHECK(sketch->arc_segments().size() == 1U);
  CHECK(sketch->trim_extend_operations().size() == 1U);
  CHECK(sketch->arc_closed_profiles().size() == 1U);
  REQUIRE(sketch->find_arc_segment(SketchEntityId("arc.top")) != nullptr);
  REQUIRE(sketch->find_trim_extend_operation(SketchTrimOperationId("trim.arc.top")) != nullptr);
  REQUIRE(sketch->find_arc_closed_profile(ProfileId("profile.arc")) != nullptr);
}
