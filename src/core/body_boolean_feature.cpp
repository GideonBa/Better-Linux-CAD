#include "blcad/core/body_boolean_feature.hpp"

#include <algorithm>
#include <utility>

namespace blcad {

std::string_view to_string(BodyBooleanOperation operation) noexcept {
  switch (operation) {
  case BodyBooleanOperation::Add:
    return "add";
  case BodyBooleanOperation::Subtract:
    return "subtract";
  case BodyBooleanOperation::Intersect:
    return "intersect";
  }
  return "add";
}

std::string_view to_string(BodyBooleanResultMode mode) noexcept {
  switch (mode) {
  case BodyBooleanResultMode::ModifyTarget:
    return "modify_target";
  case BodyBooleanResultMode::NewBody:
    return "new_body";
  }
  return "modify_target";
}

Result<BodyBooleanFeature>
BodyBooleanFeature::create(FeatureId id, BodyBooleanOperation operation, BodyId target_body,
                           std::vector<BodyId> tool_bodies, BodyBooleanResultMode result_mode,
                           std::optional<BodyId> produced_body, bool keep_tool_bodies) {
  const std::string object_id = id.empty() ? "body_boolean" : id.value();
  if (id.empty())
    return Result<BodyBooleanFeature>::failure(
        Error::validation(object_id, "body boolean feature id must not be empty"));
  if (target_body.empty())
    return Result<BodyBooleanFeature>::failure(
        Error::validation(object_id, "body boolean target body must not be empty"));
  if (tool_bodies.empty())
    return Result<BodyBooleanFeature>::failure(
        Error::validation(object_id, "body boolean requires at least one tool body"));

  for (const BodyId& tool : tool_bodies) {
    if (tool.empty())
      return Result<BodyBooleanFeature>::failure(
          Error::validation(object_id, "body boolean tool body ids must not be empty"));
    if (tool == target_body)
      return Result<BodyBooleanFeature>::failure(
          Error::validation(object_id, "body boolean target body must not also be a tool body"));
  }
  std::sort(tool_bodies.begin(), tool_bodies.end(),
            [](const BodyId& left, const BodyId& right) { return left.value() < right.value(); });
  if (std::adjacent_find(tool_bodies.begin(), tool_bodies.end()) != tool_bodies.end())
    return Result<BodyBooleanFeature>::failure(
        Error::validation(object_id, "body boolean tool body ids must be unique"));

  switch (result_mode) {
  case BodyBooleanResultMode::ModifyTarget:
    if (produced_body.has_value())
      return Result<BodyBooleanFeature>::failure(
          Error::validation(object_id, "modify-target body boolean must not name a produced body"));
    break;
  case BodyBooleanResultMode::NewBody:
    if (!produced_body.has_value() || produced_body->empty())
      return Result<BodyBooleanFeature>::failure(
          Error::validation(object_id, "new-body body boolean requires produced body"));
    if (*produced_body == target_body ||
        std::binary_search(
            tool_bodies.begin(), tool_bodies.end(), *produced_body,
            [](const BodyId& left, const BodyId& right) { return left.value() < right.value(); }))
      return Result<BodyBooleanFeature>::failure(Error::validation(
          object_id, "new-body body boolean result must differ from target and tool bodies"));
    break;
  default:
    return Result<BodyBooleanFeature>::failure(
        Error::validation(object_id, "unsupported body boolean result mode"));
  }

  switch (operation) {
  case BodyBooleanOperation::Add:
  case BodyBooleanOperation::Subtract:
  case BodyBooleanOperation::Intersect:
    break;
  default:
    return Result<BodyBooleanFeature>::failure(
        Error::validation(object_id, "unsupported body boolean operation"));
  }

  return Result<BodyBooleanFeature>::success(
      BodyBooleanFeature(std::move(id), operation, std::move(target_body), std::move(tool_bodies),
                         result_mode, std::move(produced_body), keep_tool_bodies));
}

const FeatureId& BodyBooleanFeature::id() const noexcept {
  return id_;
}
BodyBooleanOperation BodyBooleanFeature::operation() const noexcept {
  return operation_;
}
const BodyId& BodyBooleanFeature::target_body() const noexcept {
  return target_body_;
}
const std::vector<BodyId>& BodyBooleanFeature::tool_bodies() const noexcept {
  return tool_bodies_;
}
BodyBooleanResultMode BodyBooleanFeature::result_mode() const noexcept {
  return result_mode_;
}
const std::optional<BodyId>& BodyBooleanFeature::produced_body() const noexcept {
  return produced_body_;
}
const BodyId& BodyBooleanFeature::effective_result_body() const noexcept {
  return produced_body_.has_value() ? *produced_body_ : target_body_;
}
bool BodyBooleanFeature::keep_tool_bodies() const noexcept {
  return keep_tool_bodies_;
}

BodyBooleanFeature::BodyBooleanFeature(FeatureId id, BodyBooleanOperation operation,
                                       BodyId target_body, std::vector<BodyId> tool_bodies,
                                       BodyBooleanResultMode result_mode,
                                       std::optional<BodyId> produced_body, bool keep_tool_bodies)
    : id_(std::move(id)), operation_(operation), target_body_(std::move(target_body)),
      tool_bodies_(std::move(tool_bodies)), result_mode_(result_mode),
      produced_body_(std::move(produced_body)), keep_tool_bodies_(keep_tool_bodies) {}

} // namespace blcad
