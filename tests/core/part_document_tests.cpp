#include "blcad/core/part_document.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

using namespace blcad;

namespace {

std::size_t index_of(const std::vector<std::string>& values, const std::string& value) {
  const auto found = std::find(values.begin(), values.end(), value);
  REQUIRE(found != values.end());
  return static_cast<std::size_t>(std::distance(values.begin(), found));
}

InvalidationStatus status_of(const InvalidationState& state, const std::string& node_id) {
  const auto* entry = state.find(node_id);
  REQUIRE(entry != nullptr);
  return entry->status;
}

Parameter make_length_parameter(const char* id, const char* name, double value_mm) {
  auto quantity = Quantity::length_mm(value_mm, id);
  REQUIRE(quantity);

  auto parameter = Parameter::create_length(ParameterId(id), name, quantity.value());
  REQUIRE(parameter);

  return parameter.value();
}

Sketch make_rectangle_sketch() {
  auto sketch =
      Sketch::create(SketchId("sketch.base"), "Sketch_BaseRectangle", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);

  auto rectangle = RectangleProfile::create(ProfileId("profile.base_rectangle"),
                                            ParameterId("part.width"), ParameterId("part.height"));
  REQUIRE(rectangle);
  REQUIRE(sketch.value().add_profile(rectangle.value()));

  return sketch.value();
}

Sketch make_circle_sketch() {
  auto sketch =
      Sketch::create(SketchId("sketch.hole"), "Sketch_CenterHole", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);

  auto circle =
      CircleProfile::create(ProfileId("profile.center_hole"), ParameterId("part.hole_diameter"));
  REQUIRE(circle);
  REQUIRE(sketch.value().add_profile(circle.value()));

  return sketch.value();
}

Sketch make_sketch_with_rectangle_and_circle() {
  auto sketch =
      Sketch::create(SketchId("sketch.base"), "Sketch_BaseRectangle", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);

  auto rectangle = RectangleProfile::create(ProfileId("profile.base_rectangle"),
                                            ParameterId("part.width"), ParameterId("part.height"));
  REQUIRE(rectangle);
  REQUIRE(sketch.value().add_profile(rectangle.value()));

  auto circle =
      CircleProfile::create(ProfileId("profile.center_hole"), ParameterId("part.hole_diameter"));
  REQUIRE(circle);
  REQUIRE(sketch.value().add_profile(circle.value()));

  return sketch.value();
}

PartDocument make_document_with_reference_plate_parameters() {
  auto document = PartDocument::create(DocumentId("part.base_plate"), "BasePlate");
  REQUIRE(document);

  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 120.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 80.0)));
  REQUIRE(
      document.value().add_parameter(make_length_parameter("part.thickness", "thickness", 8.0)));
  REQUIRE(document.value().add_parameter(
      make_length_parameter("part.hole_diameter", "hole_diameter", 20.0)));

  return document.value();
}

PartDocument make_document_with_reference_plate_sketch() {
  auto document = make_document_with_reference_plate_parameters();

  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.add_datum_plane(xy.value()));
  REQUIRE(document.add_sketch(make_sketch_with_rectangle_and_circle()));

  return document;
}

} // namespace

TEST_CASE("PartDocument stores identity and starts empty", "[core][part_document]") {
  const auto document = PartDocument::create(DocumentId("part.base_plate"), "BasePlate");

  REQUIRE(document);
  CHECK(document.value().id().value() == "part.base_plate");
  CHECK(document.value().name() == "BasePlate");
  CHECK(document.value().parameter_count() == 0);
  CHECK(document.value().datum_plane_count() == 0);
  CHECK(document.value().sketch_count() == 0);
  CHECK(document.value().feature_count() == 0);
  CHECK(document.value().parameters().empty());
  CHECK(document.value().datum_planes().empty());
  CHECK(document.value().sketches().empty());
  CHECK(document.value().features().empty());
  CHECK(document.value().dependency_graph().node_count() == 0);
  CHECK(document.value().dependency_graph().dependency_count() == 0);
  CHECK(document.value().invalidation_state().node_count() == 0);
}

TEST_CASE("PartDocument rejects empty ids", "[core][part_document]") {
  const auto document = PartDocument::create(DocumentId(), "BasePlate");

  REQUIRE(document.has_error());
  CHECK(document.error().category() == ErrorCategory::Validation);
  CHECK(document.error().object_id() == "part_document");
  CHECK(document.error().message() == "part document id must not be empty");
}

TEST_CASE("PartDocument rejects empty names", "[core][part_document]") {
  const auto document = PartDocument::create(DocumentId("part.base_plate"), "");

  REQUIRE(document.has_error());
  CHECK(document.error().category() == ErrorCategory::Validation);
  CHECK(document.error().object_id() == "part.base_plate");
  CHECK(document.error().message() == "part document name must not be empty");
}

TEST_CASE("PartDocument adds and finds parameters by id and name", "[core][part_document]") {
  auto document = PartDocument::create(DocumentId("part.base_plate"), "BasePlate");
  REQUIRE(document);

  const auto added =
      document.value().add_parameter(make_length_parameter("part.width", "width", 120.0));

  REQUIRE(added);
  CHECK(added.value() == 0);
  CHECK(document.value().parameter_count() == 1);

  const auto* by_id = document.value().find_parameter(ParameterId("part.width"));
  REQUIRE(by_id != nullptr);
  CHECK(by_id->name() == "width");
  CHECK(by_id->value().millimeters() == Catch::Approx(120.0));

  const auto* by_name = document.value().find_parameter("width");
  REQUIRE(by_name != nullptr);
  CHECK(by_name->id().value() == "part.width");

  CHECK(document.value().dependency_graph().node_count() == 1);
  CHECK(document.value().dependency_graph().dependency_count() == 0);
  CHECK(document.value().dependency_graph().has_node("part.width"));
  CHECK(document.value().invalidation_state().node_count() == 1);
  CHECK(status_of(document.value().invalidation_state(), "part.width") ==
        InvalidationStatus::Clean);
}

TEST_CASE("PartDocument preserves parameter insertion order", "[core][part_document]") {
  auto document = PartDocument::create(DocumentId("part.base_plate"), "BasePlate");
  REQUIRE(document);

  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 120.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 80.0)));

  REQUIRE(document.value().parameters().size() == 2);
  CHECK(document.value().parameters()[0].id().value() == "part.width");
  CHECK(document.value().parameters()[1].id().value() == "part.height");
}

TEST_CASE("PartDocument rejects duplicate parameter ids", "[core][part_document]") {
  auto document = PartDocument::create(DocumentId("part.base_plate"), "BasePlate");
  REQUIRE(document);

  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 120.0)));

  const auto duplicate =
      document.value().add_parameter(make_length_parameter("part.width", "plate_width", 100.0));

  REQUIRE(duplicate.has_error());
  CHECK(duplicate.error().category() == ErrorCategory::Validation);
  CHECK(duplicate.error().object_id() == "part.width");
  CHECK(duplicate.error().message() == "parameter id must be unique within part document");
  CHECK(document.value().parameter_count() == 1);
}

TEST_CASE("PartDocument rejects duplicate parameter names", "[core][part_document]") {
  auto document = PartDocument::create(DocumentId("part.base_plate"), "BasePlate");
  REQUIRE(document);

  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 120.0)));

  const auto duplicate =
      document.value().add_parameter(make_length_parameter("part.plate_width", "width", 100.0));

  REQUIRE(duplicate.has_error());
  CHECK(duplicate.error().category() == ErrorCategory::Validation);
  CHECK(duplicate.error().object_id() == "part.plate_width");
  CHECK(duplicate.error().message() == "parameter name must be unique within part document");
  CHECK(document.value().parameter_count() == 1);
}

TEST_CASE("PartDocument returns null for missing parameters", "[core][part_document]") {
  const auto document = PartDocument::create(DocumentId("part.base_plate"), "BasePlate");
  REQUIRE(document);

  CHECK(document.value().find_parameter(ParameterId("part.missing")) == nullptr);
  CHECK(document.value().find_parameter("missing") == nullptr);
}

TEST_CASE("PartDocument adds and finds datum planes", "[core][part_document]") {
  auto document = PartDocument::create(DocumentId("part.base_plate"), "BasePlate");
  REQUIRE(document);

  auto xy = DatumPlane::xy();
  REQUIRE(xy);

  const auto added = document.value().add_datum_plane(xy.value());

  REQUIRE(added);
  CHECK(added.value() == 0);
  CHECK(document.value().datum_plane_count() == 1);

  const auto* found = document.value().find_datum_plane(DatumPlaneId("datum.xy"));
  REQUIRE(found != nullptr);
  CHECK(found->name() == "XY");
}

TEST_CASE("PartDocument rejects duplicate datum plane ids", "[core][part_document]") {
  auto document = PartDocument::create(DocumentId("part.base_plate"), "BasePlate");
  REQUIRE(document);

  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));

  auto duplicate_xy = DatumPlane::xy();
  REQUIRE(duplicate_xy);

  const auto duplicate = document.value().add_datum_plane(duplicate_xy.value());

  REQUIRE(duplicate.has_error());
  CHECK(duplicate.error().category() == ErrorCategory::Validation);
  CHECK(duplicate.error().object_id() == "datum.xy");
  CHECK(duplicate.error().message() == "datum plane id must be unique within part document");
  CHECK(document.value().datum_plane_count() == 1);
}

TEST_CASE("PartDocument adds a sketch with valid workplane and parameter references",
          "[core][part_document]") {
  auto document = make_document_with_reference_plate_parameters();

  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.add_datum_plane(xy.value()));

  const auto added = document.add_sketch(make_sketch_with_rectangle_and_circle());

  REQUIRE(added);
  CHECK(added.value() == 0);
  CHECK(document.sketch_count() == 1);

  const auto* sketch = document.find_sketch(SketchId("sketch.base"));
  REQUIRE(sketch != nullptr);
  CHECK(sketch->name() == "Sketch_BaseRectangle");
  CHECK(sketch->workplane().value() == "datum.xy");
  CHECK(sketch->profile_count() == 2);

  CHECK(document.dependency_graph().has_node("sketch.base"));
  CHECK(document.dependency_graph().has_dependency("part.width", "sketch.base"));
  CHECK(document.dependency_graph().has_dependency("part.height", "sketch.base"));
  CHECK(document.dependency_graph().has_dependency("part.hole_diameter", "sketch.base"));
}

TEST_CASE("PartDocument rejects duplicate sketch ids", "[core][part_document]") {
  auto document = make_document_with_reference_plate_parameters();

  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.add_datum_plane(xy.value()));
  REQUIRE(document.add_sketch(make_sketch_with_rectangle_and_circle()));

  const auto duplicate = document.add_sketch(make_sketch_with_rectangle_and_circle());

  REQUIRE(duplicate.has_error());
  CHECK(duplicate.error().category() == ErrorCategory::Validation);
  CHECK(duplicate.error().object_id() == "sketch.base");
  CHECK(duplicate.error().message() == "sketch id must be unique within part document");
  CHECK(document.sketch_count() == 1);
}

TEST_CASE("PartDocument rejects sketches with missing workplanes", "[core][part_document]") {
  auto document = make_document_with_reference_plate_parameters();

  const auto added = document.add_sketch(make_sketch_with_rectangle_and_circle());

  REQUIRE(added.has_error());
  CHECK(added.error().category() == ErrorCategory::Validation);
  CHECK(added.error().object_id() == "sketch.base");
  CHECK(added.error().message() == "sketch workplane must exist in part document");
  CHECK(document.sketch_count() == 0);
}

TEST_CASE("PartDocument rejects rectangle profiles with missing width parameter",
          "[core][part_document]") {
  auto document = PartDocument::create(DocumentId("part.base_plate"), "BasePlate");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 80.0)));
  REQUIRE(document.value().add_parameter(
      make_length_parameter("part.hole_diameter", "hole_diameter", 20.0)));

  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));

  auto sketch =
      Sketch::create(SketchId("sketch.base"), "Sketch_BaseRectangle", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto rectangle = RectangleProfile::create(ProfileId("profile.base_rectangle"),
                                            ParameterId("part.width"), ParameterId("part.height"));
  REQUIRE(rectangle);
  REQUIRE(sketch.value().add_profile(rectangle.value()));

  const auto added = document.value().add_sketch(sketch.value());

  REQUIRE(added.has_error());
  CHECK(added.error().category() == ErrorCategory::Validation);
  CHECK(added.error().object_id() == "profile.base_rectangle");
  CHECK(added.error().message() == "rectangle width parameter must exist in part document");
}

TEST_CASE("PartDocument rejects rectangle profiles with missing height parameter",
          "[core][part_document]") {
  auto document = PartDocument::create(DocumentId("part.base_plate"), "BasePlate");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 120.0)));
  REQUIRE(document.value().add_parameter(
      make_length_parameter("part.hole_diameter", "hole_diameter", 20.0)));

  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));

  auto sketch =
      Sketch::create(SketchId("sketch.base"), "Sketch_BaseRectangle", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto rectangle = RectangleProfile::create(ProfileId("profile.base_rectangle"),
                                            ParameterId("part.width"), ParameterId("part.height"));
  REQUIRE(rectangle);
  REQUIRE(sketch.value().add_profile(rectangle.value()));

  const auto added = document.value().add_sketch(sketch.value());

  REQUIRE(added.has_error());
  CHECK(added.error().category() == ErrorCategory::Validation);
  CHECK(added.error().object_id() == "profile.base_rectangle");
  CHECK(added.error().message() == "rectangle height parameter must exist in part document");
}

TEST_CASE("PartDocument rejects circle profiles with missing diameter parameter",
          "[core][part_document]") {
  auto document = PartDocument::create(DocumentId("part.base_plate"), "BasePlate");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 120.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 80.0)));

  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));

  auto sketch =
      Sketch::create(SketchId("sketch.hole"), "Sketch_CenterHole", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto circle =
      CircleProfile::create(ProfileId("profile.center_hole"), ParameterId("part.hole_diameter"));
  REQUIRE(circle);
  REQUIRE(sketch.value().add_profile(circle.value()));

  const auto added = document.value().add_sketch(sketch.value());

  REQUIRE(added.has_error());
  CHECK(added.error().category() == ErrorCategory::Validation);
  CHECK(added.error().object_id() == "profile.center_hole");
  CHECK(added.error().message() == "circle diameter parameter must exist in part document");
}

TEST_CASE("PartDocument returns null for missing datum planes and sketches",
          "[core][part_document]") {
  const auto document = PartDocument::create(DocumentId("part.base_plate"), "BasePlate");
  REQUIRE(document);

  CHECK(document.value().find_datum_plane(DatumPlaneId("datum.missing")) == nullptr);
  CHECK(document.value().find_sketch(SketchId("sketch.missing")) == nullptr);
}

TEST_CASE("PartDocument adds and finds additive extrude features", "[core][part_document]") {
  auto document = make_document_with_reference_plate_sketch();

  auto feature =
      Feature::create_additive_extrude(FeatureId("feature.base_extrude"), "BaseExtrude",
                                       SketchId("sketch.base"), ParameterId("part.thickness"));
  REQUIRE(feature);

  const auto added = document.add_feature(feature.value());

  REQUIRE(added);
  CHECK(added.value() == 0);
  CHECK(document.feature_count() == 1);

  const auto* found = document.find_feature(FeatureId("feature.base_extrude"));
  REQUIRE(found != nullptr);
  CHECK(found->name() == "BaseExtrude");
  CHECK(found->type() == FeatureType::AdditiveExtrude);

  CHECK(document.dependency_graph().has_node("feature.base_extrude"));
  CHECK(document.dependency_graph().has_dependency("sketch.base", "feature.base_extrude"));
  CHECK(document.dependency_graph().has_dependency("part.thickness", "feature.base_extrude"));
}

TEST_CASE("PartDocument adds subtractive extrude features after their target feature",
          "[core][part_document]") {
  auto document = make_document_with_reference_plate_sketch();

  auto base =
      Feature::create_additive_extrude(FeatureId("feature.base_extrude"), "BaseExtrude",
                                       SketchId("sketch.base"), ParameterId("part.thickness"));
  REQUIRE(base);
  REQUIRE(document.add_feature(base.value()));

  auto cut = Feature::create_subtractive_extrude(FeatureId("feature.center_hole_cut"),
                                                 "CenterHoleCut", SketchId("sketch.base"),
                                                 FeatureId("feature.base_extrude"));
  REQUIRE(cut);

  const auto added = document.add_feature(cut.value());

  REQUIRE(added);
  CHECK(added.value() == 1);
  CHECK(document.feature_count() == 2);

  const auto* found = document.find_feature(FeatureId("feature.center_hole_cut"));
  REQUIRE(found != nullptr);
  CHECK(found->type() == FeatureType::SubtractiveExtrude);
  CHECK(found->target_feature().value() == "feature.base_extrude");

  CHECK(document.dependency_graph().has_node("feature.center_hole_cut"));
  CHECK(document.dependency_graph().has_dependency("sketch.base", "feature.center_hole_cut"));
  CHECK(document.dependency_graph().has_dependency("feature.base_extrude",
                                                   "feature.center_hole_cut"));
}

TEST_CASE("PartDocument dependency graph represents the MVP reference plate order",
          "[core][part_document]") {
  auto document = make_document_with_reference_plate_parameters();

  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.add_datum_plane(xy.value()));
  REQUIRE(document.add_sketch(make_rectangle_sketch()));
  REQUIRE(document.add_sketch(make_circle_sketch()));

  auto base =
      Feature::create_additive_extrude(FeatureId("feature.base_extrude"), "BaseExtrude",
                                       SketchId("sketch.base"), ParameterId("part.thickness"));
  REQUIRE(base);
  REQUIRE(document.add_feature(base.value()));

  auto cut = Feature::create_subtractive_extrude(FeatureId("feature.center_hole_cut"),
                                                 "CenterHoleCut", SketchId("sketch.hole"),
                                                 FeatureId("feature.base_extrude"));
  REQUIRE(cut);
  REQUIRE(document.add_feature(cut.value()));

  const auto& graph = document.dependency_graph();
  CHECK(graph.node_count() == 8);
  CHECK(graph.dependency_count() == 7);
  CHECK(graph.has_dependency("part.width", "sketch.base"));
  CHECK(graph.has_dependency("part.height", "sketch.base"));
  CHECK(graph.has_dependency("part.hole_diameter", "sketch.hole"));
  CHECK(graph.has_dependency("sketch.base", "feature.base_extrude"));
  CHECK(graph.has_dependency("part.thickness", "feature.base_extrude"));
  CHECK(graph.has_dependency("sketch.hole", "feature.center_hole_cut"));
  CHECK(graph.has_dependency("feature.base_extrude", "feature.center_hole_cut"));
  CHECK(document.invalidation_state().node_count() == graph.node_count());
  CHECK(document.invalidation_state().nodes_with_status(InvalidationStatus::Clean).size() ==
        graph.node_count());

  const auto ordered = graph.topological_order();
  REQUIRE(ordered);
  CHECK(index_of(ordered.value(), "part.width") < index_of(ordered.value(), "sketch.base"));
  CHECK(index_of(ordered.value(), "part.height") < index_of(ordered.value(), "sketch.base"));
  CHECK(index_of(ordered.value(), "part.hole_diameter") < index_of(ordered.value(), "sketch.hole"));
  CHECK(index_of(ordered.value(), "sketch.base") <
        index_of(ordered.value(), "feature.base_extrude"));
  CHECK(index_of(ordered.value(), "part.thickness") <
        index_of(ordered.value(), "feature.base_extrude"));
  CHECK(index_of(ordered.value(), "sketch.hole") <
        index_of(ordered.value(), "feature.center_hole_cut"));
  CHECK(index_of(ordered.value(), "feature.base_extrude") <
        index_of(ordered.value(), "feature.center_hole_cut"));
}

TEST_CASE("PartDocument marks affected graph nodes after a parameter change",
          "[core][part_document]") {
  auto document = make_document_with_reference_plate_parameters();

  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.add_datum_plane(xy.value()));
  REQUIRE(document.add_sketch(make_rectangle_sketch()));
  REQUIRE(document.add_sketch(make_circle_sketch()));

  auto base =
      Feature::create_additive_extrude(FeatureId("feature.base_extrude"), "BaseExtrude",
                                       SketchId("sketch.base"), ParameterId("part.thickness"));
  REQUIRE(base);
  REQUIRE(document.add_feature(base.value()));

  auto cut = Feature::create_subtractive_extrude(FeatureId("feature.center_hole_cut"),
                                                 "CenterHoleCut", SketchId("sketch.hole"),
                                                 FeatureId("feature.base_extrude"));
  REQUIRE(cut);
  REQUIRE(document.add_feature(cut.value()));

  const auto affected = document.mark_parameter_changed(ParameterId("part.width"));

  REQUIRE(affected);
  CHECK(affected.value().size() == 4);
  CHECK(affected.value()[0] == "part.width");
  CHECK(status_of(document.invalidation_state(), "part.width") == InvalidationStatus::Changed);
  CHECK(status_of(document.invalidation_state(), "sketch.base") == InvalidationStatus::Dirty);
  CHECK(status_of(document.invalidation_state(), "feature.base_extrude") ==
        InvalidationStatus::Dirty);
  CHECK(status_of(document.invalidation_state(), "feature.center_hole_cut") ==
        InvalidationStatus::Dirty);
  CHECK(status_of(document.invalidation_state(), "part.height") == InvalidationStatus::Clean);
  CHECK(status_of(document.invalidation_state(), "part.hole_diameter") ==
        InvalidationStatus::Clean);
  CHECK(status_of(document.invalidation_state(), "sketch.hole") == InvalidationStatus::Clean);

  document.mark_all_clean();
  CHECK(document.invalidation_state().nodes_with_status(InvalidationStatus::Clean).size() ==
        document.dependency_graph().node_count());
}

TEST_CASE("PartDocument creates a recompute plan from dirty graph nodes", "[core][part_document]") {
  auto document = make_document_with_reference_plate_parameters();

  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.add_datum_plane(xy.value()));
  REQUIRE(document.add_sketch(make_rectangle_sketch()));
  REQUIRE(document.add_sketch(make_circle_sketch()));

  auto base =
      Feature::create_additive_extrude(FeatureId("feature.base_extrude"), "BaseExtrude",
                                       SketchId("sketch.base"), ParameterId("part.thickness"));
  REQUIRE(base);
  REQUIRE(document.add_feature(base.value()));

  auto cut = Feature::create_subtractive_extrude(FeatureId("feature.center_hole_cut"),
                                                 "CenterHoleCut", SketchId("sketch.hole"),
                                                 FeatureId("feature.base_extrude"));
  REQUIRE(cut);
  REQUIRE(document.add_feature(cut.value()));

  REQUIRE(document.mark_parameter_changed(ParameterId("part.width")));

  const auto plan = document.create_recompute_plan();

  REQUIRE(plan);
  CHECK(plan.value().step_count() == 3);
  CHECK(!plan.value().contains("part.width"));
  CHECK(plan.value().contains("sketch.base"));
  CHECK(plan.value().contains("feature.base_extrude"));
  CHECK(plan.value().contains("feature.center_hole_cut"));

  const auto nodes = plan.value().node_ids();
  CHECK(index_of(nodes, "sketch.base") < index_of(nodes, "feature.base_extrude"));
  CHECK(index_of(nodes, "feature.base_extrude") < index_of(nodes, "feature.center_hole_cut"));

  document.mark_all_clean();
  const auto clean_plan = document.create_recompute_plan();
  REQUIRE(clean_plan);
  CHECK(clean_plan.value().empty());
}

TEST_CASE("PartDocument rejects invalid parameter change markers", "[core][part_document]") {
  auto document = make_document_with_reference_plate_parameters();

  const auto missing = document.mark_parameter_changed(ParameterId("part.missing"));
  REQUIRE(missing.has_error());
  CHECK(missing.error().category() == ErrorCategory::Validation);
  CHECK(missing.error().object_id() == "part.missing");
  CHECK(missing.error().message() == "parameter must exist in part document");

  const auto empty = document.mark_parameter_changed(ParameterId());
  REQUIRE(empty.has_error());
  CHECK(empty.error().category() == ErrorCategory::Validation);
  CHECK(empty.error().object_id() == "parameter");
  CHECK(empty.error().message() == "parameter id must not be empty");
}

TEST_CASE("PartDocument updates a parameter value and marks dependents changed",
          "[core][part_document]") {
  auto document = make_document_with_reference_plate_parameters();

  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.add_datum_plane(xy.value()));
  REQUIRE(document.add_sketch(make_rectangle_sketch()));
  REQUIRE(document.add_sketch(make_circle_sketch()));

  auto cut_sketch_feature =
      Feature::create_additive_extrude(FeatureId("feature.base_extrude"), "BaseExtrude",
                                       SketchId("sketch.base"), ParameterId("part.thickness"));
  REQUIRE(cut_sketch_feature);
  REQUIRE(document.add_feature(cut_sketch_feature.value()));

  const auto new_value = Quantity::length_mm(150.0, "part.width");
  REQUIRE(new_value);

  const auto affected = document.set_parameter_value(ParameterId("part.width"), new_value.value());

  REQUIRE(affected);
  CHECK(affected.value()[0] == "part.width");

  const Parameter* updated = document.find_parameter(ParameterId("part.width"));
  REQUIRE(updated != nullptr);
  CHECK(updated->value().millimeters() == Catch::Approx(150.0));

  CHECK(status_of(document.invalidation_state(), "part.width") == InvalidationStatus::Changed);
  CHECK(status_of(document.invalidation_state(), "sketch.base") == InvalidationStatus::Dirty);
  CHECK(status_of(document.invalidation_state(), "feature.base_extrude") ==
        InvalidationStatus::Dirty);
  CHECK(status_of(document.invalidation_state(), "part.height") == InvalidationStatus::Clean);
}

TEST_CASE("PartDocument rejects setting values on invalid parameters", "[core][part_document]") {
  auto document = make_document_with_reference_plate_parameters();

  const auto value = Quantity::length_mm(10.0, "quantity");
  REQUIRE(value);

  const auto missing = document.set_parameter_value(ParameterId("part.missing"), value.value());
  REQUIRE(missing.has_error());
  CHECK(missing.error().object_id() == "part.missing");
  CHECK(missing.error().message() == "parameter must exist in part document");

  const auto empty = document.set_parameter_value(ParameterId(), value.value());
  REQUIRE(empty.has_error());
  CHECK(empty.error().object_id() == "parameter");
  CHECK(empty.error().message() == "parameter id must not be empty");
}

TEST_CASE("PartDocument rejects duplicate feature ids", "[core][part_document]") {
  auto document = make_document_with_reference_plate_sketch();

  auto feature =
      Feature::create_additive_extrude(FeatureId("feature.base_extrude"), "BaseExtrude",
                                       SketchId("sketch.base"), ParameterId("part.thickness"));
  REQUIRE(feature);
  REQUIRE(document.add_feature(feature.value()));

  auto duplicate =
      Feature::create_additive_extrude(FeatureId("feature.base_extrude"), "BaseExtrudeCopy",
                                       SketchId("sketch.base"), ParameterId("part.thickness"));
  REQUIRE(duplicate);

  const auto added = document.add_feature(duplicate.value());

  REQUIRE(added.has_error());
  CHECK(added.error().category() == ErrorCategory::Validation);
  CHECK(added.error().object_id() == "feature.base_extrude");
  CHECK(added.error().message() == "feature id must be unique within part document");
  CHECK(document.feature_count() == 1);
}

TEST_CASE("PartDocument rejects features with missing input sketches", "[core][part_document]") {
  auto document = make_document_with_reference_plate_parameters();

  auto feature =
      Feature::create_additive_extrude(FeatureId("feature.base_extrude"), "BaseExtrude",
                                       SketchId("sketch.missing"), ParameterId("part.thickness"));
  REQUIRE(feature);

  const auto added = document.add_feature(feature.value());

  REQUIRE(added.has_error());
  CHECK(added.error().category() == ErrorCategory::Validation);
  CHECK(added.error().object_id() == "feature.base_extrude");
  CHECK(added.error().message() == "feature input sketch must exist in part document");
}

TEST_CASE("PartDocument rejects additive extrudes with missing length parameters",
          "[core][part_document]") {
  auto document = make_document_with_reference_plate_sketch();

  auto feature = Feature::create_additive_extrude(FeatureId("feature.base_extrude"), "BaseExtrude",
                                                  SketchId("sketch.base"),
                                                  ParameterId("part.missing_thickness"));
  REQUIRE(feature);

  const auto added = document.add_feature(feature.value());

  REQUIRE(added.has_error());
  CHECK(added.error().category() == ErrorCategory::Validation);
  CHECK(added.error().object_id() == "feature.base_extrude");
  CHECK(added.error().message() == "additive extrude length parameter must exist in part document");
}

TEST_CASE("PartDocument rejects subtractive extrudes with missing target features",
          "[core][part_document]") {
  auto document = make_document_with_reference_plate_sketch();

  auto feature = Feature::create_subtractive_extrude(FeatureId("feature.center_hole_cut"),
                                                     "CenterHoleCut", SketchId("sketch.base"),
                                                     FeatureId("feature.base_extrude"));
  REQUIRE(feature);

  const auto added = document.add_feature(feature.value());

  REQUIRE(added.has_error());
  CHECK(added.error().category() == ErrorCategory::Validation);
  CHECK(added.error().object_id() == "feature.center_hole_cut");
  CHECK(added.error().message() ==
        "subtractive extrude target feature must exist in part document");
}

TEST_CASE("PartDocument returns null for missing features", "[core][part_document]") {
  const auto document = PartDocument::create(DocumentId("part.base_plate"), "BasePlate");
  REQUIRE(document);

  CHECK(document.value().find_feature(FeatureId("feature.missing")) == nullptr);
}
