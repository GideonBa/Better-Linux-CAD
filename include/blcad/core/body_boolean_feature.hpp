#pragma once

#include "blcad/core/id.hpp"
#include "blcad/core/result.hpp"

#include <optional>
#include <string_view>
#include <vector>

namespace blcad {

enum class BodyBooleanOperation { Add, Subtract, Intersect };
enum class BodyBooleanResultMode { ModifyTarget, NewBody };

[[nodiscard]] std::string_view to_string(BodyBooleanOperation operation) noexcept;
[[nodiscard]] std::string_view to_string(BodyBooleanResultMode mode) noexcept;

class BodyBooleanFeature {
public:
  [[nodiscard]] static Result<BodyBooleanFeature>
  create(FeatureId id, BodyBooleanOperation operation, BodyId target_body,
         std::vector<BodyId> tool_bodies, BodyBooleanResultMode result_mode,
         std::optional<BodyId> produced_body, bool keep_tool_bodies);

  [[nodiscard]] const FeatureId& id() const noexcept;
  [[nodiscard]] BodyBooleanOperation operation() const noexcept;
  [[nodiscard]] const BodyId& target_body() const noexcept;
  [[nodiscard]] const std::vector<BodyId>& tool_bodies() const noexcept;
  [[nodiscard]] BodyBooleanResultMode result_mode() const noexcept;
  [[nodiscard]] const std::optional<BodyId>& produced_body() const noexcept;
  [[nodiscard]] const BodyId& effective_result_body() const noexcept;
  [[nodiscard]] bool keep_tool_bodies() const noexcept;

  friend bool operator==(const BodyBooleanFeature&, const BodyBooleanFeature&) = default;

private:
  BodyBooleanFeature(FeatureId id, BodyBooleanOperation operation, BodyId target_body,
                     std::vector<BodyId> tool_bodies, BodyBooleanResultMode result_mode,
                     std::optional<BodyId> produced_body, bool keep_tool_bodies);

  FeatureId id_;
  BodyBooleanOperation operation_ = BodyBooleanOperation::Add;
  BodyId target_body_;
  std::vector<BodyId> tool_bodies_;
  BodyBooleanResultMode result_mode_ = BodyBooleanResultMode::ModifyTarget;
  std::optional<BodyId> produced_body_;
  bool keep_tool_bodies_ = false;
};

} // namespace blcad
