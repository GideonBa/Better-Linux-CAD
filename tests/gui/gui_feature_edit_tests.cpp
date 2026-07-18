#include "blcad/gui/gui_feature_edit.hpp"

#include "blcad/core/body.hpp"
#include "blcad/core/datum_plane.hpp"
#include "blcad/core/edge_treatment_feature.hpp"
#include "blcad/core/feature.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/sketch.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

using namespace blcad;
using namespace blcad::gui;

namespace {

Parameter length(const char* id, double value) {
  return Parameter::create_length(ParameterId(id), id, Quantity::length_mm(value, id).value()).value();
}

Feature extrude(const char* depth_parameter, const char* input_sketch = "sketch.base") {
  return Feature::create_additive_extrude(FeatureId("feature.base"), "Base", SketchId(input_sketch),
                                          ParameterId(depth_parameter))
      .value()
      .with_body_result_context(
          FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt,
                                           BodyId("body.base")).value())
      .value();
}

// A Part with a base extrude (feature.base -> body.base) plus a fillet on that
// body (feature.fillet), built through the session and recomputed.
void build_part(GuiDocumentSession& session) {
  REQUIRE(session.create_part(DocumentId("part.edit"), "Edit"));
  REQUIRE(session.commit_part_transaction("Setup", [](PartDocument& part) -> Result<std::size_t> {
    for (auto parameter : {length("width", 20.0), length("height", 10.0), length("depth", 5.0),
                           length("depth2", 8.0), length("radius", 1.0)})
      if (auto added = part.add_parameter(std::move(parameter)); added.has_error()) return added;
    if (auto added = part.add_datum_plane(DatumPlane::xy().value()); added.has_error()) return added;
    auto sketch = Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId("datum.xy")).value();
    if (auto added = sketch.add_profile(RectangleProfile::create(ProfileId("profile.base"),
                                                                 ParameterId("width"),
                                                                 ParameterId("height")).value());
        added.has_error())
      return added;
    if (auto added = part.add_sketch(std::move(sketch)); added.has_error()) return added;
    if (auto added = part.add_body(Body::create(BodyId("body.base"), "Base", BodyKind::Solid).value());
        added.has_error())
      return added;
    if (auto added = part.add_feature(extrude("depth")); added.has_error()) return added;
    auto edge = EdgeReference::create_linear(
                    PartFeatureInputRole::FilletEdge,
                    SemanticEdgeReference::create(FeatureId("feature.base"), SemanticEdge::TopFront)
                        .value())
                    .value();
    return part.add_fillet_feature(
        FilletFeature::create(FeatureId("feature.fillet"), "Fillet", BodyId("body.base"),
                              std::vector<EdgeReference>{edge}, ParameterId("radius")).value());
  }));
  REQUIRE(session.recompute());
}

} // namespace

TEST_CASE("Block 129 feature edit identifies the family of a double-clicked feature",
          "[gui][feature-edit]") {
  GuiDocumentSession session;
  build_part(session);

  GuiFeatureEditController edit;
  REQUIRE(edit.begin(session, FeatureId("feature.base")));
  CHECK(edit.active());
  CHECK(edit.kind() == GuiFeatureEditKind::Extrude);
  CHECK(edit.feature().value() == "feature.base");
  edit.cancel();
  CHECK_FALSE(edit.active());

  REQUIRE(edit.begin(session, FeatureId("feature.fillet")));
  CHECK(edit.kind() == GuiFeatureEditKind::Fillet);

  CHECK(edit.begin(session, FeatureId("feature.ghost")).has_error());
}

TEST_CASE("Block 129 feature edit commits one update transaction and fails closed",
          "[gui][feature-edit]") {
  GuiDocumentSession session;
  build_part(session);

  GuiFeatureEditController edit;
  REQUIRE(edit.begin(session, FeatureId("feature.base")));

  // A replacement that references a missing sketch fails closed with no mutation.
  const std::string before = serialize_part_document_to_json(*session.part_document()).value();
  CHECK(edit.commit(session, extrude("depth", "sketch.missing"),
                    &PartDocument::update_feature).has_error());
  CHECK(serialize_part_document_to_json(*session.part_document()).value() == before);
  CHECK(edit.active());  // still editing after a rejected commit

  // A valid rebind commits one transaction with a stable id and exact undo.
  REQUIRE(edit.preview(session, extrude("depth2"), &PartDocument::update_feature));
  REQUIRE(edit.commit(session, extrude("depth2"), &PartDocument::update_feature));
  CHECK_FALSE(edit.active());
  CHECK(session.part_document()->find_feature(FeatureId("feature.base"))
            ->length_parameter().value() == "depth2");
  CHECK(session.part_document()->features().size() == 1);

  REQUIRE(session.undo());
  CHECK(session.part_document()->find_feature(FeatureId("feature.base"))
            ->length_parameter().value() == "depth");
}

TEST_CASE("Block 129 editing an upstream feature recomputes the downstream feature",
          "[integration][feature-edit-recompute]") {
  GuiDocumentSession session;
  build_part(session);
  CHECK(session.part_document()->fillet_features().size() == 1);

  GuiFeatureEditController edit;
  REQUIRE(edit.begin(session, FeatureId("feature.base")));
  REQUIRE(edit.commit(session, extrude("depth2"), &PartDocument::update_feature));

  // The downstream fillet survives the edit and recomputes on the new body.
  CHECK(session.part_document()->fillet_features().size() == 1);
  REQUIRE(session.recompute());
  REQUIRE(session.part_shape_cache() != nullptr);
  CHECK(session.part_shape_cache()->find_body_result(BodyId("body.base")) != nullptr);
  CHECK(session.derived_results_fresh());
}
