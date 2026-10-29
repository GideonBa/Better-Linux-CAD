#pragma once

#include "blcad/core/id.hpp"
#include "blcad/core/result.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace blcad {
class PartDocument;

// A producer-defined semantic face origin. Roles name authored generation
// (caps, Sketch sides and explicit copy instances), never OCCT traversal order.
struct ResultFaceOrigin {
  FeatureId feature;
  std::string role;
  friend bool operator==(const ResultFaceOrigin&, const ResultFaceOrigin&) = default;
};

class ResultFaceReference {
public:
  [[nodiscard]] static Result<ResultFaceReference> create(FeatureId producer, BodyId body,
                                                          std::vector<ResultFaceOrigin> origins) {
    if (producer.empty() || body.empty() || origins.empty() ||
        std::any_of(origins.begin(), origins.end(), [](const auto& origin) {
          return origin.feature.empty() || origin.role.empty();
        }))
      return Result<ResultFaceReference>::failure(Error::validation(
          "result_face_reference",
          "Result-face support requires a producer, Body and nonempty semantic face origins"));
    std::sort(origins.begin(), origins.end(), [](const auto& a, const auto& b) {
      return std::pair{a.feature.value(), a.role} < std::pair{b.feature.value(), b.role};
    });
    if (std::adjacent_find(origins.begin(), origins.end()) != origins.end())
      return Result<ResultFaceReference>::failure(
          Error::validation(producer.value(), "Result-face origins must be unique"));
    return Result<ResultFaceReference>::success(
        ResultFaceReference(std::move(producer), std::move(body), std::move(origins)));
  }
  [[nodiscard]] const FeatureId& producer() const noexcept {
    return producer_;
  }
  [[nodiscard]] const BodyId& body() const noexcept {
    return body_;
  }
  [[nodiscard]] const std::vector<ResultFaceOrigin>& origins() const noexcept {
    return origins_;
  }
  [[nodiscard]] std::string identity() const {
    std::string result = "result-face/";
    const auto append = [&](const std::string& value) {
      result += std::to_string(value.size()) + ":" + value + "/";
    };
    append(producer_.value());
    append(body_.value());
    for (const auto& origin : origins_) {
      append(origin.feature.value());
      append(origin.role);
    }
    return result;
  }
  friend bool operator==(const ResultFaceReference&, const ResultFaceReference&) = default;

private:
  ResultFaceReference(FeatureId producer, BodyId body, std::vector<ResultFaceOrigin> origins)
      : producer_(std::move(producer)), body_(std::move(body)), origins_(std::move(origins)) {}
  FeatureId producer_;
  BodyId body_;
  std::vector<ResultFaceOrigin> origins_;
};

class ResultFaceWorkplane {
public:
  [[nodiscard]] static Result<ResultFaceWorkplane> create(DatumPlaneId id, std::string name,
                                                          ResultFaceReference reference) {
    if (id.empty() || name.empty())
      return Result<ResultFaceWorkplane>::failure(Error::validation(
          "result_face_workplane", "Result-face workplane requires identity and name"));
    return Result<ResultFaceWorkplane>::success(
        ResultFaceWorkplane(std::move(id), std::move(name), std::move(reference)));
  }
  [[nodiscard]] const DatumPlaneId& id() const noexcept {
    return id_;
  }
  [[nodiscard]] const std::string& name() const noexcept {
    return name_;
  }
  [[nodiscard]] const ResultFaceReference& reference() const noexcept {
    return reference_;
  }

private:
  ResultFaceWorkplane(DatumPlaneId id, std::string name, ResultFaceReference reference)
      : id_(std::move(id)), name_(std::move(name)), reference_(std::move(reference)) {}
  DatumPlaneId id_;
  std::string name_;
  ResultFaceReference reference_;
};
[[nodiscard]] Result<std::size_t>
validate_result_face_workplane_reference(const PartDocument& document,
                                         const ResultFaceWorkplane& plane);
} // namespace blcad
