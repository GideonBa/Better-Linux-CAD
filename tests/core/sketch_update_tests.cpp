#include "blcad/core/part_document.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace blcad;

TEST_CASE("PartDocument updates a planar sketch atomically and preserves authored order",
          "[core][sketch-update]") {
  auto part = PartDocument::create(DocumentId("part.sketch_update"), "Sketch Update");
  REQUIRE(part);
  REQUIRE(part.value().add_datum_plane(DatumPlane::xy().value()));
  REQUIRE(part.value().add_sketch(
      Sketch::create(SketchId("sketch.first"), "First", DatumPlaneId("datum.xy")).value()));
  REQUIRE(part.value().add_sketch(
      Sketch::create(SketchId("sketch.second"), "Second", DatumPlaneId("datum.xy")).value()));

  Sketch replacement = *part.value().find_sketch(SketchId("sketch.first"));
  REQUIRE(replacement.add_entity(
      LineSegment::create(SketchEntityId("line.updated"), {0.0, 0.0}, {10.0, 0.0}).value()));
  REQUIRE(part.value().update_sketch(std::move(replacement)));

  REQUIRE(part.value().sketches().size() == 2);
  CHECK(part.value().sketches()[0].id().value() == "sketch.first");
  CHECK(part.value().sketches()[1].id().value() == "sketch.second");
  CHECK(part.value().find_sketch(SketchId("sketch.first"))
            ->find_line_segment(SketchEntityId("line.updated")) != nullptr);
  CHECK(part.value().invalidation_state().find("sketch.first")->status ==
        InvalidationStatus::Changed);

  auto missing = Sketch::create(SketchId("sketch.missing"), "Missing", DatumPlaneId("datum.xy"));
  REQUIRE(missing);
  CHECK(part.value().update_sketch(std::move(missing.value())).has_error());
  CHECK(part.value().sketch_count() == 2);
}
