#include "blcad/core/part_document_json.hpp"

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

PartDocument make_projected_reference_document() {
  auto document =
      PartDocument::create(DocumentId("part.projected_refs"), "Projected reference part");
  REQUIRE(document);

  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 100.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 60.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.depth", "depth", 10.0)));

  auto datum = DatumPlane::xy();
  REQUIRE(datum);
  REQUIRE(document.value().add_datum_plane(datum.value()));

  auto base_sketch =
      Sketch::create(SketchId("sketch.base"), "Sketch_Base", DatumPlaneId("datum.xy"));
  REQUIRE(base_sketch);
  auto rectangle = RectangleProfile::create(ProfileId("profile.base"), ParameterId("part.width"),
                                            ParameterId("part.height"));
  REQUIRE(rectangle);
  REQUIRE(base_sketch.value().add_profile(rectangle.value()));
  REQUIRE(document.value().add_sketch(base_sketch.value()));

  auto feature = Feature::create_additive_extrude(
      FeatureId("feature.base"), "BaseExtrude", SketchId("sketch.base"), ParameterId("part.depth"));
  REQUIRE(feature);
  REQUIRE(document.value().add_feature(feature.value()));

  auto construction_point = ConstructionPoint::create_explicit(ConstructionPointId("point.mid"),
                                                               "Mid", Point3{0.0, 0.0, 0.0});
  REQUIRE(construction_point);
  REQUIRE(document.value().add_construction_point(construction_point.value()));

  auto construction_line = ConstructionLine::create_explicit(
      ConstructionLineId("line.axis"), "Axis", Point3{0.0, 0.0, 0.0}, Vector3{1.0, 0.0, 0.0});
  REQUIRE(construction_line);
  REQUIRE(document.value().add_construction_line(construction_line.value()));

  return document.value();
}

} // namespace

TEST_CASE("Projected sketch references and constraints roundtrip through part document JSON",
          "[core][json][sketch]") {
  PartDocument document = make_projected_reference_document();

  auto sketch = Sketch::create(SketchId("sketch.top"), "Sketch_Top", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.helper"), Point2{-1.0, 0.0}, Point2{1.0, 0.0})
          .value()));

  auto vertex =
      SemanticVertexReference::create(FeatureId("feature.base"), SemanticVertex::TopFrontRight);
  REQUIRE(vertex);
  auto edge = SemanticEdgeReference::create(FeatureId("feature.base"), SemanticEdge::TopFront);
  REQUIRE(edge);

  auto projected_point = ProjectedSketchPoint::create_from_construction_point(
      SketchEntityId("ref.point.mid"), ConstructionPointId("point.mid"));
  REQUIRE(projected_point);
  REQUIRE(sketch.value().add_reference(projected_point.value()));

  auto projected_vertex = ProjectedSketchPoint::create_from_semantic_vertex(
      SketchEntityId("ref.vertex.top_front_right"), vertex.value());
  REQUIRE(projected_vertex);
  REQUIRE(sketch.value().add_reference(projected_vertex.value()));

  auto projected_line = ProjectedSketchLine::create_from_construction_line(
      SketchEntityId("ref.line.axis"), ConstructionLineId("line.axis"));
  REQUIRE(projected_line);
  REQUIRE(sketch.value().add_reference(projected_line.value()));

  auto projected_edge = ProjectedSketchLine::create_from_semantic_edge(
      SketchEntityId("ref.edge.top_front"), edge.value());
  REQUIRE(projected_edge);
  REQUIRE(sketch.value().add_reference(projected_edge.value()));

  auto start_target =
      SketchReferenceTarget::create_line_segment_start(SketchEntityId("line.helper"));
  REQUIRE(start_target);
  auto point_target =
      SketchReferenceTarget::create_projected_point(SketchEntityId("ref.point.mid"));
  REQUIRE(point_target);
  auto coincident = SketchConstraint::create_coincident_to_projected_point(
      SketchConstraintId("constraint.start_on_mid"), start_target.value(), point_target.value());
  REQUIRE(coincident);
  REQUIRE(sketch.value().add_constraint(coincident.value()));

  auto line_target = SketchReferenceTarget::create_line_segment(SketchEntityId("line.helper"));
  REQUIRE(line_target);
  auto projected_line_target =
      SketchReferenceTarget::create_projected_line(SketchEntityId("ref.line.axis"));
  REQUIRE(projected_line_target);
  auto parallel = SketchConstraint::create_parallel_to_projected_line(
      SketchConstraintId("constraint.parallel_axis"), line_target.value(),
      projected_line_target.value());
  REQUIRE(parallel);
  REQUIRE(sketch.value().add_constraint(parallel.value()));

  REQUIRE(document.add_sketch(sketch.value()));

  auto serialized = serialize_part_document_to_json(document);
  REQUIRE(serialized);
  CHECK(serialized.value().find("projected_points") != std::string::npos);
  CHECK(serialized.value().find("semantic_vertex") != std::string::npos);
  CHECK(serialized.value().find("projected_lines") != std::string::npos);
  CHECK(serialized.value().find("semantic_edge") != std::string::npos);
  CHECK(serialized.value().find("constraints") != std::string::npos);
  CHECK(serialized.value().find("coincident_to_projected_point") != std::string::npos);
  CHECK(serialized.value().find("parallel_to_projected_line") != std::string::npos);

  auto restored = deserialize_part_document_from_json(serialized.value());
  REQUIRE(restored);
  const Sketch* restored_sketch = restored.value().find_sketch(SketchId("sketch.top"));
  REQUIRE(restored_sketch != nullptr);
  REQUIRE(restored_sketch->projected_points().size() == 2);
  REQUIRE(restored_sketch->projected_lines().size() == 2);
  REQUIRE(restored_sketch->constraints().size() == 2);
  CHECK(restored_sketch->projected_points()[0].construction_point().value() == "point.mid");
  REQUIRE(restored_sketch->projected_points()[1].semantic_vertex().has_value());
  CHECK(restored_sketch->projected_points()[1].referenced_node_id() ==
        "feature.base.vertex.top_front_right");
  CHECK(restored_sketch->projected_lines()[0].construction_line().value() == "line.axis");
  REQUIRE(restored_sketch->projected_lines()[1].semantic_edge().has_value());
  CHECK(restored_sketch->projected_lines()[1].referenced_node_id() ==
        "feature.base.edge.top_front");
  CHECK(restored_sketch->constraints()[0].id().value() == "constraint.start_on_mid");
  CHECK(restored_sketch->constraints()[1].id().value() == "constraint.parallel_axis");
}
