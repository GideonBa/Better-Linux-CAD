#pragma once

#include "blcad/core/datum_plane.hpp"
#include "blcad/core/dependency_graph.hpp"
#include "blcad/core/feature.hpp"
#include "blcad/core/id.hpp"
#include "blcad/core/invalidation_state.hpp"
#include "blcad/core/parameter.hpp"
#include "blcad/core/quantity.hpp"
#include "blcad/core/recompute_plan.hpp"
#include "blcad/core/result.hpp"
#include "blcad/core/sketch.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace blcad {

class PartDocument {
public:
  [[nodiscard]] static Result<PartDocument> create(DocumentId id, std::string name);

  [[nodiscard]] Result<std::size_t> add_parameter(Parameter parameter);
  [[nodiscard]] Result<std::size_t> add_datum_plane(DatumPlane datum_plane);
  [[nodiscard]] Result<std::size_t> add_sketch(Sketch sketch);
  [[nodiscard]] Result<std::size_t> add_feature(Feature feature);
  [[nodiscard]] Result<std::vector<std::string>> mark_parameter_changed(ParameterId id);
  // Sets a parameter value, validates it, and marks the parameter and its
  // dependents as changed. Returns the affected graph nodes.
  [[nodiscard]] Result<std::vector<std::string>> set_parameter_value(ParameterId id,
                                                                     Quantity value);
  [[nodiscard]] Result<RecomputePlan> create_recompute_plan() const;
  void mark_all_clean() noexcept;

  [[nodiscard]] const DocumentId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] const std::vector<Parameter>& parameters() const noexcept;
  [[nodiscard]] const std::vector<DatumPlane>& datum_planes() const noexcept;
  [[nodiscard]] const std::vector<Sketch>& sketches() const noexcept;
  [[nodiscard]] const std::vector<Feature>& features() const noexcept;
  [[nodiscard]] const DependencyGraph& dependency_graph() const noexcept;
  [[nodiscard]] const InvalidationState& invalidation_state() const noexcept;
  [[nodiscard]] std::size_t parameter_count() const noexcept;
  [[nodiscard]] std::size_t datum_plane_count() const noexcept;
  [[nodiscard]] std::size_t sketch_count() const noexcept;
  [[nodiscard]] std::size_t feature_count() const noexcept;

  [[nodiscard]] const Parameter* find_parameter(ParameterId id) const noexcept;
  [[nodiscard]] const Parameter* find_parameter(std::string_view name) const noexcept;
  [[nodiscard]] const DatumPlane* find_datum_plane(DatumPlaneId id) const noexcept;
  [[nodiscard]] const Sketch* find_sketch(SketchId id) const noexcept;
  [[nodiscard]] const Feature* find_feature(FeatureId id) const noexcept;

private:
  PartDocument(DocumentId id, std::string name);

  [[nodiscard]] bool has_parameter_id(const ParameterId& id) const noexcept;
  [[nodiscard]] bool has_parameter_name(std::string_view name) const noexcept;
  [[nodiscard]] bool has_datum_plane_id(const DatumPlaneId& id) const noexcept;
  [[nodiscard]] bool has_sketch_id(const SketchId& id) const noexcept;
  [[nodiscard]] bool has_feature_id(const FeatureId& id) const noexcept;

  DocumentId id_;
  std::string name_;
  std::vector<Parameter> parameters_;
  std::vector<DatumPlane> datum_planes_;
  std::vector<Sketch> sketches_;
  std::vector<Feature> features_;
  DependencyGraph dependency_graph_;
  InvalidationState invalidation_state_;
};

} // namespace blcad
