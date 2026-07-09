#include "blcad/core/part_document.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/reference_recovery.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace blcad;

namespace {

Parameter make_length_parameter(const char* id, const char* name, double value_mm) {
  auto quantity = Quantity::length_mm(value_mm, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), name, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

PartDocument make_reference_recovery_document() {
  auto document = PartDocument::create(DocumentId("part.reference_recovery"), "Reference Recovery");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 100.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 60.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.depth", "depth", 10.0)));
  auto datum = DatumPlane::xy();
  REQUIRE(datum);
  REQUIRE(document.value().add_datum_plane(datum.value()));
  auto sketch = Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto rectangle = RectangleProfile::create(ProfileId("profile.base"), ParameterId("part.width"),
                                            ParameterId("part.height"));
  REQUIRE(rectangle);
  REQUIRE(sketch.value().add_profile(rectangle.value()));
  REQUIRE(document.value().add_sketch(sketch.value()));
  auto feature = Feature::create_additive_extrude(FeatureId("feature.base"), "BaseExtrude",
                                                  SketchId("sketch.base"),
                                                  ParameterId("part.depth"));
  REQUIRE(feature);
  REQUIRE(document.value().add_feature(feature.value()));
  return document.value();
}

} // namespace

TEST_CASE("ReferenceRecoveryEvaluator reports resolved and lost semantic references", "[core][recovery]") {
  PartDocument document = make_reference_recovery_document();
  const ReferenceRecoveryEvaluator evaluator;

  auto resolved_target = SemanticReferenceTarget::create_face(FeatureId("feature.base"),
                                                              SemanticFace::Top);
  REQUIRE(resolved_target);
  auto resolved = evaluator.evaluate(ReferenceStatusId("status.base_top"), document,
                                     resolved_target.value());
  REQUIRE(resolved);
  CHECK(resolved.value().status() == ReferenceStatusKind::Resolved);
  CHECK(resolved.value().target().node_id() == "feature.base.face.top");

  auto missing_target = SemanticReferenceTarget::create_edge(FeatureId("feature.deleted"),
                                                             SemanticEdge::TopFront);
  REQUIRE(missing_target);
  auto lost = evaluator.evaluate(ReferenceStatusId("status.deleted_edge"), document,
                                 missing_target.value());
  REQUIRE(lost);
  CHECK(lost.value().status() == ReferenceStatusKind::Lost);
  CHECK(lost.value().message() ==
        "semantic reference source is not available in the current generated topology");
}

TEST_CASE("PartDocument stores explicit reference status, remap, and sketch origin override records",
          "[core][recovery]") {
  PartDocument document = make_reference_recovery_document();

  auto lost_target = SemanticReferenceTarget::create_face(FeatureId("feature.old"), SemanticFace::Top);
  REQUIRE(lost_target);
  auto lost = ReferenceStatusRecord::create_lost(ReferenceStatusId("status.old_top"),
                                                 lost_target.value(), "source feature was removed");
  REQUIRE(lost);
  REQUIRE(document.add_reference_status(lost.value()));

  auto replacement = SemanticReferenceTarget::create_face(FeatureId("feature.base"), SemanticFace::Top);
  REQUIRE(replacement);
  auto remap = ReferenceRemapRecord::create(ReferenceRemapId("remap.old_top_to_base_top"),
                                            lost_target.value(), replacement.value(),
                                            "user selected replacement top face");
  REQUIRE(remap);
  REQUIRE(document.add_reference_remap(remap.value()));

  auto origin = SketchOriginOverrideRecord::create(SketchId("sketch.base"), Point2{5.0, 7.0});
  REQUIRE(origin);
  REQUIRE(document.add_sketch_origin_override(origin.value()));

  CHECK(document.reference_status_count() == 1);
  CHECK(document.reference_remap_count() == 1);
  CHECK(document.sketch_origin_override_count() == 1);
  REQUIRE(document.find_reference_status(ReferenceStatusId("status.old_top")) != nullptr);
  REQUIRE(document.find_reference_remap(ReferenceRemapId("remap.old_top_to_base_top")) != nullptr);
  const SketchOriginOverrideRecord* restored_origin =
      document.find_sketch_origin_override(SketchId("sketch.base"));
  REQUIRE(restored_origin != nullptr);
  CHECK(restored_origin->local_origin().x == 5.0);
  CHECK(restored_origin->local_origin().y == 7.0);
}

TEST_CASE("Reference recovery metadata roundtrips through JSON", "[core][json][recovery]") {
  PartDocument document = make_reference_recovery_document();
  auto lost_target = SemanticReferenceTarget::create_vertex(FeatureId("feature.old"),
                                                            SemanticVertex::TopFrontRight);
  REQUIRE(lost_target);
  auto lost = ReferenceStatusRecord::create_lost(ReferenceStatusId("status.old_vertex"),
                                                 lost_target.value(), "old vertex is unavailable");
  REQUIRE(lost);
  REQUIRE(document.add_reference_status(lost.value()));

  auto replacement = SemanticReferenceTarget::create_vertex(FeatureId("feature.base"),
                                                            SemanticVertex::TopFrontRight);
  REQUIRE(replacement);
  auto remap = ReferenceRemapRecord::create(ReferenceRemapId("remap.vertex"), lost_target.value(),
                                            replacement.value(), "manual replacement");
  REQUIRE(remap);
  REQUIRE(document.add_reference_remap(remap.value()));

  auto origin = SketchOriginOverrideRecord::create(SketchId("sketch.base"), Point2{2.0, 3.0});
  REQUIRE(origin);
  REQUIRE(document.add_sketch_origin_override(origin.value()));

  auto serialized = serialize_part_document_to_json(document);
  REQUIRE(serialized);
  CHECK(serialized.value().find("reference_statuses") != std::string::npos);
  CHECK(serialized.value().find("reference_remaps") != std::string::npos);
  CHECK(serialized.value().find("sketch_origin_overrides") != std::string::npos);

  auto restored = deserialize_part_document_from_json(serialized.value());
  REQUIRE(restored);
  CHECK(restored.value().reference_status_count() == 1);
  CHECK(restored.value().reference_remap_count() == 1);
  CHECK(restored.value().sketch_origin_override_count() == 1);
  const ReferenceStatusRecord* restored_status =
      restored.value().find_reference_status(ReferenceStatusId("status.old_vertex"));
  REQUIRE(restored_status != nullptr);
  CHECK(restored_status->status() == ReferenceStatusKind::Lost);
  CHECK(restored_status->target().node_id() == "feature.old.vertex.top_front_right");
}
