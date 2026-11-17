#pragma once
#include "blcad/core/id.hpp"
#include "blcad/core/result.hpp"
#include <memory>
#include <string>
#include <string_view>
#include <vector>
namespace blcad {
struct RibWebIntent;
struct SketchDrivenDefinition;
class SketchDrivenFeature {
public:
  [[nodiscard]] static Result<SketchDrivenFeature>
  create(FeatureId id, std::string name, BodyId target, SketchDrivenDefinition definition);
  [[nodiscard]] static Result<SketchDrivenFeature>
  create_rib_web(FeatureId id, std::string name, BodyId target, RibWebIntent intent);
  [[nodiscard]] const FeatureId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] std::string_view kind_name() const noexcept;
  [[nodiscard]] const BodyId& target_body() const noexcept;
  [[nodiscard]] const SketchDrivenDefinition& definition() const noexcept;
  [[nodiscard]] std::vector<ParameterId> parameter_dependencies() const;
  [[nodiscard]] std::vector<std::string> input_nodes() const;

private:
  SketchDrivenFeature(FeatureId, std::string, BodyId,
                      std::shared_ptr<const SketchDrivenDefinition>);
  FeatureId id_;
  std::string name_;
  BodyId target_;
  std::shared_ptr<const SketchDrivenDefinition> definition_;
};
} // namespace blcad
