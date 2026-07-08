#pragma once

#include <string>
#include <utility>

namespace blcad {

template <typename Tag> class TypedId {
public:
  TypedId() = default;

  explicit TypedId(std::string value) : value_(std::move(value)) {}

  [[nodiscard]] const std::string& value() const noexcept {
    return value_;
  }
  [[nodiscard]] bool empty() const noexcept {
    return value_.empty();
  }

  [[nodiscard]] explicit operator bool() const noexcept {
    return !empty();
  }

  friend bool operator==(const TypedId&, const TypedId&) = default;

private:
  std::string value_;
};

struct DocumentIdTag;
struct ParameterIdTag;
struct DatumPlaneIdTag;
struct ProfileIdTag;
struct SketchIdTag;
struct FeatureIdTag;
struct ShapeCacheIdTag;

using DocumentId = TypedId<DocumentIdTag>;
using ParameterId = TypedId<ParameterIdTag>;
using DatumPlaneId = TypedId<DatumPlaneIdTag>;
using ProfileId = TypedId<ProfileIdTag>;
using SketchId = TypedId<SketchIdTag>;
using FeatureId = TypedId<FeatureIdTag>;
using ShapeCacheId = TypedId<ShapeCacheIdTag>;

} // namespace blcad
