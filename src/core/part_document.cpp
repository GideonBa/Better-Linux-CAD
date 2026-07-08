#include "blcad/core/part_document.hpp"

#include <string>
#include <utility>

namespace blcad {

namespace {

Result<std::size_t> add_dependency_if_missing(DependencyGraph& graph, std::string dependency,
                                               std::string dependent) {
  if (graph.has_dependency(dependency, dependent)) {
    return Result<std::size_t>::success(graph.dependency_count());
  }

  return graph.add_dependency(std::move(dependency), std::move(dependent));
}

} // namespace

Result<PartDocument> PartDocument::create(DocumentId id, std::string name) {
  const auto object_id = id.empty() ? std::string("part_document") : id.value();

  if (id.empty()) {
    return Result<PartDocument>::failure(
        Error::validation(object_id, "part document id must not be empty"));
  }

  if (name.empty()) {
    return Result<PartDocument>::failure(
        Error::validation(object_id, "part document name must not be empty"));
  }

  return Result<PartDocument>::success(PartDocument(std::move(id), std::move(name)));
}

Result<std::size_t> PartDocument::add_parameter(Parameter parameter) {
  if (has_parameter_id(parameter.id())) {
    return Result<std::size_t>::failure(Error::validation(
        parameter.id().value(), "parameter id must be unique within part document"));
  }

  if (has_parameter_name(parameter.name())) {
    return Result<std::size_t>::failure(Error::validation(
        parameter.id().value(), "parameter name must be unique within part document"));
  }

  auto graph = dependency_graph_;
  const auto added_node = graph.add_node(parameter.id().value());
  if (added_node.has_error()) {
    return Result<std::size_t>::failure(added_node.error());
  }

  auto invalidation_state = invalidation_state_;
  const auto synced_state = invalidation_state.sync_from_graph(graph);
  if (synced_state.has_error()) {
    return Result<std::size_t>::failure(synced_state.error());
  }

  dependency_graph_ = std::move(graph);
  invalidation_state_ = std::move(invalidation_state);
  parameters_.push_back(std::move(parameter));
  return Result<std::size_t>::success(parameters_.size() - 1);
}

Result<std::size_t> PartDocument::add_datum_plane(DatumPlane datum_plane) {
  if (has_workplane_id(datum_plane.id())) {
    return Result<std::size_t>::failure(Error::validation(
        datum_plane.id().value(), "workplane id must be unique within part document"));
  }

  datum_planes_.push_back(std::move(datum_plane));
  return Result<std::size_t>::success(datum_planes_.size() - 1);
}

Result<std::size_t> PartDocument::add_derived_workplane(DerivedWorkplane workplane) {
  if (has_workplane_id(workplane.id())) {
    return Result<std::size_t>::failure(Error::validation(
        workplane.id().value(), "workplane id must be unique within part document"));
  }

  const Feature* source_feature = find_feature(workplane.face_reference().source_feature());
  if (source_feature == nullptr) {
    return Result<std::size_t>::failure(Error::validation(
        workplane.id().value(), "derived workplane source feature must exist in part document"));
  }

  if (source_feature->type() != FeatureType::AdditiveExtrude) {
    return Result<std::size_t>::failure(Error::validation(
        workplane.id().value(), "derived workplane source feature must be an additive extrude"));
  }

  if (workplane.face_reference().face() != SemanticFace::Top) {
    return Result<std::size_t>::failure(
        Error::validation(workplane.id().value(), "only top semantic face is supported"));
  }

  auto graph = dependency_graph_;
  const auto added_node = graph.add_node(workplane.id().value());
  if (added_node.has_error()) {
    return Result<std::size_t>::failure(added_node.error());
  }

  auto source_dependency = add_dependency_if_missing(
      graph, workplane.face_reference().source_feature().value(), workplane.id().value());
  if (source_dependency.has_error()) {
    return Result<std::size_t>::failure(source_dependency.error());
  }

  auto invalidation_state = invalidation_state_;
  const auto synced_state = invalidation_state.sync_from_graph(graph);
  if (synced_state.has_error()) {
    return Result<std::size_t>::failure(synced_state.error());
  }

  dependency_graph_ = std::move(graph);
  invalidation_state_ = std::move(invalidation_state);
  derived_workplanes_.push_back(std::move(workplane));
  return Result<std::size_t>::success(derived_workplanes_.size() - 1);
}

Result<std::size_t> PartDocument::add_sketch(Sketch sketch) {
  if (has_sketch_id(sketch.id())) {
    return Result<std::size_t>::failure(
        Error::validation(sketch.id().value(), "sketch id must be unique within part document"));
  }

  if (!has_workplane_id(sketch.workplane())) {
    return Result<std::size_t>::failure(
        Error::validation(sketch.id().value(), "sketch workplane must exist in part document"));
  }

  for (const auto& profile : sketch.rectangle_profiles()) {
    if (!has_parameter_id(profile.width_parameter())) {
      return Result<std::size_t>::failure(Error::validation(
          profile.id().value(), "rectangle width parameter must exist in part document"));
    }

    if (!has_parameter_id(profile.height_parameter())) {
      return Result<std::size_t>::failure(Error::validation(
          profile.id().value(), "rectangle height parameter must exist in part document"));
    }
  }

  for (const auto& profile : sketch.circle_profiles()) {
    if (!has_parameter_id(profile.diameter_parameter())) {
      return Result<std::size_t>::failure(Error::validation(
          profile.id().value(), "circle diameter parameter must exist in part document"));
    }
  }

  auto graph = dependency_graph_;
  const auto added_node = graph.add_node(sketch.id().value());
  if (added_node.has_error()) {
    return Result<std::size_t>::failure(added_node.error());
  }

  if (has_derived_workplane_id(sketch.workplane())) {
    auto workplane_dependency =
        add_dependency_if_missing(graph, sketch.workplane().value(), sketch.id().value());
    if (workplane_dependency.has_error()) {
      return Result<std::size_t>::failure(workplane_dependency.error());
    }
  }

  for (const auto& profile : sketch.rectangle_profiles()) {
    auto width_dependency =
        add_dependency_if_missing(graph, profile.width_parameter().value(), sketch.id().value());
    if (width_dependency.has_error()) {
      return Result<std::size_t>::failure(width_dependency.error());
    }

    auto height_dependency =
        add_dependency_if_missing(graph, profile.height_parameter().value(), sketch.id().value());
    if (height_dependency.has_error()) {
      return Result<std::size_t>::failure(height_dependency.error());
    }
  }

  for (const auto& profile : sketch.circle_profiles()) {
    auto diameter_dependency =
        add_dependency_if_missing(graph, profile.diameter_parameter().value(), sketch.id().value());
    if (diameter_dependency.has_error()) {
      return Result<std::size_t>::failure(diameter_dependency.error());
    }
  }

  auto invalidation_state = invalidation_state_;
  const auto synced_state = invalidation_state.sync_from_graph(graph);
  if (synced_state.has_error()) {
    return Result<std::size_t>::failure(synced_state.error());
  }

  dependency_graph_ = std::move(graph);
  invalidation_state_ = std::move(invalidation_state);
  sketches_.push_back(std::move(sketch));
  return Result<std::size_t>::success(sketches_.size() - 1);
}

Result<std::size_t> PartDocument::add_feature(Feature feature) {
  if (has_feature_id(feature.id())) {
    return Result<std::size_t>::failure(
        Error::validation(feature.id().value(), "feature id must be unique within part document"));
  }

  if (!has_sketch_id(feature.input_sketch())) {
    return Result<std::size_t>::failure(Error::validation(
        feature.id().value(), "feature input sketch must exist in part document"));
  }

  if (feature.type() == FeatureType::AdditiveExtrude &&
      !has_parameter_id(feature.length_parameter())) {
    return Result<std::size_t>::failure(Error::validation(
        feature.id().value(), "additive extrude length parameter must exist in part document"));
  }

  if (feature.type() == FeatureType::SubtractiveExtrude && !has_feature_id(feature.target_feature())) {
    return Result<std::size_t>::failure(Error::validation(
        feature.id().value(), "subtractive extrude target feature must exist in part document"));
  }

  auto graph = dependency_graph_;
  const auto added_node = graph.add_node(feature.id().value());
  if (added_node.has_error()) {
    return Result<std::size_t>::failure(added_node.error());
  }

  auto sketch_dependency =
      add_dependency_if_missing(graph, feature.input_sketch().value(), feature.id().value());
  if (sketch_dependency.has_error()) {
    return Result<std::size_t>::failure(sketch_dependency.error());
  }

  if (feature.type() == FeatureType::AdditiveExtrude) {
    auto length_dependency =
        add_dependency_if_missing(graph, feature.length_parameter().value(), feature.id().value());
    if (length_dependency.has_error()) {
      return Result<std::size_t>::failure(length_dependency.error());
    }
  }

  if (feature.type() == FeatureType::SubtractiveExtrude) {
    auto target_dependency =
        add_dependency_if_missing(graph, feature.target_feature().value(), feature.id().value());
    if (target_dependency.has_error()) {
      return Result<std::size_t>::failure(target_dependency.error());
    }
  }

  auto invalidation_state = invalidation_state_;
  const auto synced_state = invalidation_state.sync_from_graph(graph);
  if (synced_state.has_error()) {
    return Result<std::size_t>::failure(synced_state.error());
  }

  dependency_graph_ = std::move(graph);
  invalidation_state_ = std::move(invalidation_state);
  features_.push_back(std::move(feature));
  return Result<std::size_t>::success(features_.size() - 1);
}

Result<std::vector<std::string>> PartDocument::mark_parameter_changed(ParameterId id) {
  if (id.empty()) {
    return Result<std::vector<std::string>>::failure(
        Error::validation("parameter", "parameter id must not be empty"));
  }

  if (!has_parameter_id(id)) {
    return Result<std::vector<std::string>>::failure(
        Error::validation(id.value(), "parameter must exist in part document"));
  }

  return invalidation_state_.mark_changed(dependency_graph_, id.value());
}

Result<std::vector<std::string>> PartDocument::set_parameter_value(ParameterId id, Quantity value) {
  if (id.empty()) {
    return Result<std::vector<std::string>>::failure(
        Error::validation("parameter", "parameter id must not be empty"));
  }

  const Parameter* existing = find_parameter(id);
  if (existing == nullptr) {
    return Result<std::vector<std::string>>::failure(
        Error::validation(id.value(), "parameter must exist in part document"));
  }

  auto updated = existing->with_value(value);
  if (updated.has_error()) {
    return Result<std::vector<std::string>>::failure(updated.error());
  }

  for (auto& parameter : parameters_) {
    if (parameter.id() == id) {
      parameter = std::move(updated.value());
      break;
    }
  }

  return invalidation_state_.mark_changed(dependency_graph_, id.value());
}

Result<RecomputePlan> PartDocument::create_recompute_plan() const {
  return RecomputePlan::from_graph_and_invalidation_state(dependency_graph_, invalidation_state_);
}

void PartDocument::mark_all_clean() noexcept {
  invalidation_state_.mark_all_clean();
}

const DocumentId& PartDocument::id() const noexcept {
  return id_;
}

const std::string& PartDocument::name() const noexcept {
  return name_;
}

const std::vector<Parameter>& PartDocument::parameters() const noexcept {
  return parameters_;
}

const std::vector<DatumPlane>& PartDocument::datum_planes() const noexcept {
  return datum_planes_;
}

const std::vector<DerivedWorkplane>& PartDocument::derived_workplanes() const noexcept {
  return derived_workplanes_;
}

const std::vector<Sketch>& PartDocument::sketches() const noexcept {
  return sketches_;
}

const std::vector<Feature>& PartDocument::features() const noexcept {
  return features_;
}

const DependencyGraph& PartDocument::dependency_graph() const noexcept {
  return dependency_graph_;
}

const InvalidationState& PartDocument::invalidation_state() const noexcept {
  return invalidation_state_;
}

std::size_t PartDocument::parameter_count() const noexcept {
  return parameters_.size();
}

std::size_t PartDocument::datum_plane_count() const noexcept {
  return datum_planes_.size();
}

std::size_t PartDocument::derived_workplane_count() const noexcept {
  return derived_workplanes_.size();
}

std::size_t PartDocument::sketch_count() const noexcept {
  return sketches_.size();
}

std::size_t PartDocument::feature_count() const noexcept {
  return features_.size();
}

const Parameter* PartDocument::find_parameter(ParameterId id) const noexcept {
  for (const auto& parameter : parameters_) {
    if (parameter.id() == id) {
      return &parameter;
    }
  }

  return nullptr;
}

const Parameter* PartDocument::find_parameter(std::string_view name) const noexcept {
  for (const auto& parameter : parameters_) {
    if (parameter.name() == name) {
      return &parameter;
    }
  }

  return nullptr;
}

const DatumPlane* PartDocument::find_datum_plane(DatumPlaneId id) const noexcept {
  for (const auto& datum_plane : datum_planes_) {
    if (datum_plane.id() == id) {
      return &datum_plane;
    }
  }

  return nullptr;
}

const DerivedWorkplane* PartDocument::find_derived_workplane(DatumPlaneId id) const noexcept {
  for (const auto& workplane : derived_workplanes_) {
    if (workplane.id() == id) {
      return &workplane;
    }
  }

  return nullptr;
}

const Sketch* PartDocument::find_sketch(SketchId id) const noexcept {
  for (const auto& sketch : sketches_) {
    if (sketch.id() == id) {
      return &sketch;
    }
  }

  return nullptr;
}

const Feature* PartDocument::find_feature(FeatureId id) const noexcept {
  for (const auto& feature : features_) {
    if (feature.id() == id) {
      return &feature;
    }
  }

  return nullptr;
}

bool PartDocument::has_workplane_id(const DatumPlaneId& id) const noexcept {
  return has_datum_plane_id(id) || has_derived_workplane_id(id);
}

PartDocument::PartDocument(DocumentId id, std::string name)
    : id_(std::move(id)), name_(std::move(name)) {}

bool PartDocument::has_parameter_id(const ParameterId& id) const noexcept {
  return find_parameter(id) != nullptr;
}

bool PartDocument::has_parameter_name(std::string_view name) const noexcept {
  return find_parameter(name) != nullptr;
}

bool PartDocument::has_datum_plane_id(const DatumPlaneId& id) const noexcept {
  return find_datum_plane(id) != nullptr;
}

bool PartDocument::has_derived_workplane_id(const DatumPlaneId& id) const noexcept {
  return find_derived_workplane(id) != nullptr;
}

bool PartDocument::has_sketch_id(const SketchId& id) const noexcept {
  return find_sketch(id) != nullptr;
}

bool PartDocument::has_feature_id(const FeatureId& id) const noexcept {
  return find_feature(id) != nullptr;
}

} // namespace blcad
