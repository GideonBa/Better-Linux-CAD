#include "blcad/core/assembly_constraint.hpp"
#include "blcad/core/assembly_document.hpp"
#include "blcad/core/assembly_document_json.hpp"
#include "blcad/core/generated_topology_reference.hpp"
#include "blcad/core/part_document.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/reference_recovery.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <utility>
#include <variant>

using namespace blcad;

namespace {

Parameter make_length_parameter(const char* id, const char* name, double value_mm) {
  auto quantity = Quantity::length_mm(value_mm, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), name, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

Parameter make_count_parameter(const char* id, const char* name, double value) {
  auto quantity = Quantity::count(value, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_count(ParameterId(id), name, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

void add_common_parameters_and_datum(PartDocument& document) {
  REQUIRE(document.add_parameter(make_length_parameter("part.width", "width", 100.0)));
  REQUIRE(document.add_parameter(make_length_parameter("part.height", "height", 60.0)));
  REQUIRE(document.add_parameter(make_length_parameter("part.depth", "depth", 10.0)));
  REQUIRE(document.add_parameter(make_length_parameter("part.hole_diameter", "hole_diameter", 12.0)));
  auto datum = DatumPlane::xy();
  REQUIRE(datum);
  REQUIRE(document.add_datum_plane(datum.value()));
}

Sketch rectangle_sketch(const char* sketch_id, const char* profile_id) {
  auto sketch = Sketch::create(SketchId(sketch_id), sketch_id, DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto rectangle = RectangleProfile::create(ProfileId(profile_id), ParameterId("part.width"),
                                            ParameterId("part.height"));
  REQUIRE(rectangle);
  REQUIRE(sketch.value().add_profile(rectangle.value()));
  return sketch.value();
}

Sketch circle_sketch(const char* sketch_id, const char* profile_id) {
  auto sketch = Sketch::create(SketchId(sketch_id), sketch_id, DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto circle = CircleProfile::create(ProfileId(profile_id), ParameterId("part.hole_diameter"));
  REQUIRE(circle);
  REQUIRE(sketch.value().add_profile(circle.value()));
  return sketch.value();
}

PartDocument make_supported_document(bool auxiliary_feature_first = false) {
  auto document = PartDocument::create(DocumentId("part.generated_topology"), "GeneratedTopology");
  REQUIRE(document);
  add_common_parameters_and_datum(document.value());

  REQUIRE(document.value().add_sketch(rectangle_sketch("sketch.base", "profile.base")));
  REQUIRE(document.value().add_sketch(rectangle_sketch("sketch.aux", "profile.aux")));
  REQUIRE(document.value().add_sketch(circle_sketch("sketch.hole", "profile.hole")));

  auto base = Feature::create_additive_extrude(FeatureId("feature.base"), "Base",
                                               SketchId("sketch.base"), ParameterId("part.depth"));
  REQUIRE(base);
  auto auxiliary = Feature::create_additive_extrude(
      FeatureId("feature.aux"), "Aux", SketchId("sketch.aux"), ParameterId("part.depth"));
  REQUIRE(auxiliary);

  if (auxiliary_feature_first) {
    REQUIRE(document.value().add_feature(auxiliary.value()));
    REQUIRE(document.value().add_feature(base.value()));
  } else {
    REQUIRE(document.value().add_feature(base.value()));
    REQUIRE(document.value().add_feature(auxiliary.value()));
  }

  auto hole = Feature::create_subtractive_extrude(FeatureId("feature.hole"), "Hole",
                                                  SketchId("sketch.hole"),
                                                  FeatureId("feature.base"));
  REQUIRE(hole);
  REQUIRE(document.value().add_feature(hole.value()));
  return document.value();
}

PartDocument make_pattern_document() {
  auto document = PartDocument::create(DocumentId("part.pattern_topology"), "PatternTopology");
  REQUIRE(document);
  add_common_parameters_and_datum(document.value());
  REQUIRE(document.value().add_parameter(
      make_length_parameter("part.pattern_radius", "pattern_radius", 25.0)));
  REQUIRE(document.value().add_parameter(make_count_parameter("part.pattern_count", "pattern_count", 6.0)));

  REQUIRE(document.value().add_sketch(rectangle_sketch("sketch.base", "profile.base")));
  auto base = Feature::create_additive_extrude(FeatureId("feature.base"), "Base",
                                               SketchId("sketch.base"), ParameterId("part.depth"));
  REQUIRE(base);
  REQUIRE(document.value().add_feature(base.value()));

  auto sketch = Sketch::create(SketchId("sketch.pattern"), "Pattern", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto pattern = CircularHolePattern::create(ProfileId("pattern.holes"),
                                             ParameterId("part.pattern_radius"),
                                             ParameterId("part.pattern_count"),
                                             ParameterId("part.hole_diameter"));
  REQUIRE(pattern);
  REQUIRE(sketch.value().add_profile(pattern.value()));
  REQUIRE(document.value().add_sketch(sketch.value()));

  auto cut = Feature::create_subtractive_extrude(FeatureId("feature.pattern_cut"), "PatternCut",
                                                 SketchId("sketch.pattern"),
                                                 FeatureId("feature.base"));
  REQUIRE(cut);
  REQUIRE(document.value().add_feature(cut.value()));
  return document.value();
}

PartDocument make_ambiguous_circle_document() {
  auto document = PartDocument::create(DocumentId("part.ambiguous_topology"), "AmbiguousTopology");
  REQUIRE(document);
  add_common_parameters_and_datum(document.value());
  REQUIRE(document.value().add_sketch(rectangle_sketch("sketch.base", "profile.base")));
  auto base = Feature::create_additive_extrude(FeatureId("feature.base"), "Base",
                                               SketchId("sketch.base"), ParameterId("part.depth"));
  REQUIRE(base);
  REQUIRE(document.value().add_feature(base.value()));

  auto sketch = Sketch::create(SketchId("sketch.holes"), "Holes", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto first = CircleProfile::create(ProfileId("profile.hole.a"), ParameterId("part.hole_diameter"),
                                     Point2{-10.0, 0.0});
  auto second = CircleProfile::create(ProfileId("profile.hole.b"), ParameterId("part.hole_diameter"),
                                      Point2{10.0, 0.0});
  REQUIRE(first);
  REQUIRE(second);
  REQUIRE(sketch.value().add_profile(first.value()));
  REQUIRE(sketch.value().add_profile(second.value()));
  REQUIRE(document.value().add_sketch(sketch.value()));

  auto cut = Feature::create_subtractive_extrude(FeatureId("feature.holes"), "Holes",
                                                 SketchId("sketch.holes"),
                                                 FeatureId("feature.base"));
  REQUIRE(cut);
  REQUIRE(document.value().add_feature(cut.value()));
  return document.value();
}

PartDocument make_circle_additive_document() {
  auto document = PartDocument::create(DocumentId("part.circle_additive"), "CircleAdditive");
  REQUIRE(document);
  add_common_parameters_and_datum(document.value());
  REQUIRE(document.value().add_sketch(circle_sketch("sketch.round", "profile.round")));
  auto feature = Feature::create_additive_extrude(FeatureId("feature.round"), "Round",
                                                  SketchId("sketch.round"),
                                                  ParameterId("part.depth"));
  REQUIRE(feature);
  REQUIRE(document.value().add_feature(feature.value()));
  return document.value();
}

GeneratedTopologyReferenceIdentity cylinder_reference(const char* feature = "feature.hole",
                                                       const char* profile = "profile.hole") {
  auto reference = SemanticCylindricalFaceReference::create(
      FeatureId(feature), ProfileId(profile), SemanticCylindricalFace::Wall);
  REQUIRE(reference);
  return reference.value();
}

GeneratedTopologyReferenceIdentity circular_edge_reference(
    SemanticCircularEdge role, const char* feature = "feature.hole",
    const char* profile = "profile.hole") {
  auto reference =
      SemanticCircularEdgeReference::create(FeatureId(feature), ProfileId(profile), role);
  REQUIRE(reference);
  return reference.value();
}

GeneratedTopologyReferenceIdentity linear_edge_reference(
    SemanticEdge role = SemanticEdge::TopFront, const char* feature = "feature.base") {
  auto reference = SemanticEdgeReference::create(FeatureId(feature), role);
  REQUIRE(reference);
  return reference.value();
}

GeneratedTopologyReferenceIdentity vertex_reference(
    SemanticVertex role = SemanticVertex::TopFrontRight, const char* feature = "feature.base") {
  auto reference = SemanticVertexReference::create(FeatureId(feature), role);
  REQUIRE(reference);
  return reference.value();
}

std::string topology_spelling(const GeneratedTopologyReferenceIdentity& identity) {
  auto spelling = make_generated_topology_target_spelling(identity);
  REQUIRE(spelling);
  return spelling.value();
}

} // namespace

TEST_CASE("Generated topology producer matrices publish finite unique cardinality-one roles",
          "[core][semantic-generated-topology-reference]") {
  const auto rectangular = generated_topology_producer_role_matrix(
      GeneratedTopologyProducerKind::RectangularAdditiveExtrude);
  REQUIRE(rectangular.size() == 20U);
  CHECK(std::count_if(rectangular.begin(), rectangular.end(), [](const auto& rule) {
          return rule.family == GeneratedTopologyReferenceFamily::LinearEdge;
        }) == 12);
  CHECK(std::count_if(rectangular.begin(), rectangular.end(), [](const auto& rule) {
          return rule.family == GeneratedTopologyReferenceFamily::Vertex;
        }) == 8);
  CHECK(std::all_of(rectangular.begin(), rectangular.end(),
                    [](const auto& rule) { return rule.expected_cardinality == 1U; }));

  const auto circular = generated_topology_producer_role_matrix(
      GeneratedTopologyProducerKind::SingleCircleSubtractiveExtrude);
  REQUIRE(circular.size() == 3U);
  CHECK(circular[0] == GeneratedTopologyRoleRule{GeneratedTopologyReferenceFamily::CylindricalFace,
                                                "wall", 1U});
  CHECK(circular[1] == GeneratedTopologyRoleRule{GeneratedTopologyReferenceFamily::CircularEdge,
                                                "source_rim", 1U});
  CHECK(circular[2] == GeneratedTopologyRoleRule{GeneratedTopologyReferenceFamily::CircularEdge,
                                                "opposite_rim", 1U});
}

TEST_CASE("Generated topology spellings roundtrip exact producer-role identity",
          "[core][semantic-generated-topology-reference]") {
  auto cylindrical = SemanticCylindricalFaceReference::create(
      FeatureId("feature.hole/a%b:c"), ProfileId("profile.hole/50%"),
      SemanticCylindricalFace::Wall);
  REQUIRE(cylindrical);
  const GeneratedTopologyReferenceIdentity identity = cylindrical.value();

  const std::string spelling = topology_spelling(identity);
  CHECK(spelling ==
        "topo:cylindrical_face:feature%2Ehole%2Fa%25b%3Ac:profile%2Ehole%2F50%25:wall");
  CHECK(spelling.find('.') == std::string::npos);
  CHECK(is_generated_topology_target_spelling(spelling));

  auto restored = parse_generated_topology_target_spelling(spelling);
  REQUIRE(restored);
  REQUIRE(std::holds_alternative<SemanticCylindricalFaceReference>(restored.value()));
  const auto& restored_face = std::get<SemanticCylindricalFaceReference>(restored.value());
  CHECK(restored_face.source_feature().value() == "feature.hole/a%b:c");
  CHECK(restored_face.source_profile().value() == "profile.hole/50%");
  CHECK(restored_face.face() == SemanticCylindricalFace::Wall);
  CHECK(topology_spelling(restored.value()) == spelling);

  CHECK_FALSE(is_generated_topology_target_spelling("ref:datum_plane:datum%2Exy"));
  CHECK_FALSE(is_generated_topology_target_spelling("feature.base.top"));
  CHECK(parse_generated_topology_target_spelling(
            "topo:linear_edge:feature%2ebase:top_front")
            .has_error());
  CHECK(parse_generated_topology_target_spelling("topo:linear_edge:feature%2Ebase:unknown")
            .has_error());
  CHECK(parse_generated_topology_target_spelling(
            "topo:circular_edge:feature:profile:source_rim:extra")
            .has_error());
}

TEST_CASE("Supported generated topology references validate from producer intent",
          "[core][semantic-generated-topology-reference]") {
  const PartDocument document = make_supported_document();

  auto base_producer =
      classify_generated_topology_producer(document, FeatureId("feature.base"));
  REQUIRE(base_producer);
  CHECK(base_producer.value() == GeneratedTopologyProducerKind::RectangularAdditiveExtrude);
  auto hole_producer =
      classify_generated_topology_producer(document, FeatureId("feature.hole"));
  REQUIRE(hole_producer);
  CHECK(hole_producer.value() ==
        GeneratedTopologyProducerKind::SingleCircleSubtractiveExtrude);

  REQUIRE(validate_generated_topology_reference(document, linear_edge_reference()));
  CHECK(validate_generated_topology_reference(document, linear_edge_reference()).value() == 1U);
  REQUIRE(validate_generated_topology_reference(document, vertex_reference()));
  REQUIRE(validate_generated_topology_reference(document, cylinder_reference()));
  REQUIRE(validate_generated_topology_reference(
      document, circular_edge_reference(SemanticCircularEdge::SourceRim)));
  REQUIRE(validate_generated_topology_reference(
      document, circular_edge_reference(SemanticCircularEdge::OppositeRim)));

  CHECK(validate_generated_topology_reference(document,
                                               cylinder_reference("feature.hole", "profile.wrong"))
            .has_error());
  CHECK(validate_generated_topology_reference(
            document, linear_edge_reference(SemanticEdge::TopFront, "feature.hole"))
            .has_error());
}

TEST_CASE("Generated topology identity is independent of feature insertion order",
          "[core][semantic-generated-topology-reference]") {
  const PartDocument base_first = make_supported_document(false);
  const PartDocument auxiliary_first = make_supported_document(true);
  const auto cylinder = cylinder_reference();
  const auto edge = linear_edge_reference(SemanticEdge::BackLeft);

  REQUIRE(validate_generated_topology_reference(base_first, cylinder));
  REQUIRE(validate_generated_topology_reference(auxiliary_first, cylinder));
  REQUIRE(validate_generated_topology_reference(base_first, edge));
  REQUIRE(validate_generated_topology_reference(auxiliary_first, edge));
  CHECK(topology_spelling(cylinder) ==
        "topo:cylindrical_face:feature%2Ehole:profile%2Ehole:wall");
  CHECK(topology_spelling(edge) == "topo:linear_edge:feature%2Ebase:back_left");

  auto first_json = serialize_part_document_to_json(base_first);
  auto second_json = serialize_part_document_to_json(auxiliary_first);
  REQUIRE(first_json);
  REQUIRE(second_json);
  CHECK(first_json.value() != second_json.value());
  CHECK(topology_spelling(cylinder) == topology_spelling(cylinder));
}

TEST_CASE("Unsupported ambiguous and patterned producers fail closed",
          "[core][semantic-generated-topology-reference]") {
  const PartDocument unsupported = make_circle_additive_document();
  auto unsupported_producer =
      classify_generated_topology_producer(unsupported, FeatureId("feature.round"));
  REQUIRE(unsupported_producer.has_error());
  CHECK(unsupported_producer.error().message().find("RectangleProfile") != std::string::npos);

  const PartDocument ambiguous = make_ambiguous_circle_document();
  auto ambiguous_producer =
      classify_generated_topology_producer(ambiguous, FeatureId("feature.holes"));
  REQUIRE(ambiguous_producer.has_error());
  CHECK(ambiguous_producer.error().message().find("ambiguous") != std::string::npos);

  const PartDocument patterned = make_pattern_document();
  auto patterned_producer =
      classify_generated_topology_producer(patterned, FeatureId("feature.pattern_cut"));
  REQUIRE(patterned_producer.has_error());
  CHECK(patterned_producer.error().message() ==
        "patterned generated topology is unavailable until stable per-instance semantic identity exists");
}

TEST_CASE("Reference recovery resolves supported generated topology and reports lost pattern identity",
          "[core][semantic-generated-topology-recovery]") {
  const ReferenceRecoveryEvaluator evaluator;
  PartDocument supported = make_supported_document();
  auto before_supported = serialize_part_document_to_json(supported);
  REQUIRE(before_supported);

  auto resolved = evaluator.evaluate(supported, cylinder_reference());
  REQUIRE(resolved);
  CHECK(resolved.value().status == ReferenceStatusKind::Resolved);
  CHECK(resolved.value().message.empty());

  auto after_supported = serialize_part_document_to_json(supported);
  REQUIRE(after_supported);
  CHECK(after_supported.value() == before_supported.value());

  PartDocument patterned = make_pattern_document();
  auto before_patterned = serialize_part_document_to_json(patterned);
  REQUIRE(before_patterned);
  auto lost = evaluator.evaluate(
      patterned, cylinder_reference("feature.pattern_cut", "pattern.holes"));
  REQUIRE(lost);
  CHECK(lost.value().status == ReferenceStatusKind::Lost);
  CHECK(lost.value().message ==
        "patterned generated topology is unavailable until stable per-instance semantic identity exists");
  auto after_patterned = serialize_part_document_to_json(patterned);
  REQUIRE(after_patterned);
  CHECK(after_patterned.value() == before_patterned.value());
}

TEST_CASE("Assembly JSON preserves generated topology semantic spellings byte-for-byte",
          "[core][semantic-generated-topology-reference]") {
  auto assembly = AssemblyDocument::create(DocumentId("assembly.topology"), "TopologyAssembly");
  REQUIRE(assembly);
  REQUIRE(assembly.value().add_member_part(DocumentId("part.generated_topology")));
  auto component = ComponentInstance::create(ComponentInstanceId("component.part"), "Part",
                                             DocumentId("part.generated_topology"));
  REQUIRE(component);
  REQUIRE(assembly.value().add_component_instance(component.value()));

  const std::string cylindrical = topology_spelling(cylinder_reference());
  const std::string vertex = topology_spelling(vertex_reference(SemanticVertex::BottomBackLeft));
  auto target_a = AssemblyConstraintTarget::create(ComponentInstanceId("component.part"), cylindrical);
  auto target_b = AssemblyConstraintTarget::create(ComponentInstanceId("component.part"), vertex);
  REQUIRE(target_a);
  REQUIRE(target_b);
  auto constraint = AssemblyConstraint::create(
      AssemblyConstraintId("constraint.topology"), "Topology", AssemblyConstraintType::Mate,
      std::move(target_a.value()), std::move(target_b.value()));
  REQUIRE(constraint);
  REQUIRE(assembly.value().add_constraint(std::move(constraint.value())));

  auto serialized = serialize_assembly_document_to_json(assembly.value());
  REQUIRE(serialized);
  auto restored = deserialize_assembly_document_from_json(serialized.value());
  REQUIRE(restored);
  const AssemblyConstraint* roundtripped =
      restored.value().find_constraint(AssemblyConstraintId("constraint.topology"));
  REQUIRE(roundtripped != nullptr);
  CHECK(roundtripped->target_a().semantic_reference() == cylindrical);
  CHECK(roundtripped->target_b().semantic_reference() == vertex);
  REQUIRE(parse_generated_topology_target_spelling(roundtripped->target_a().semantic_reference()));
  REQUIRE(parse_generated_topology_target_spelling(roundtripped->target_b().semantic_reference()));
}
