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
struct ConstructionPointIdTag;
struct ConstructionLineIdTag;
struct ConstructionPlaneIdTag;
struct ConstructionRelationIdTag;
struct ReferenceStatusIdTag;
struct ReferenceRemapIdTag;
struct SketchEntityIdTag;
struct SketchConstraintIdTag;
struct SketchDimensionIdTag;
struct SketchTrimOperationIdTag;
struct ProfileIdTag;
struct SketchIdTag;
struct FeatureIdTag;
struct ShapeCacheIdTag;
struct ParameterBindingIdTag;
struct ComponentInstanceIdTag;

using DocumentId = TypedId<DocumentIdTag>;
using ParameterId = TypedId<ParameterIdTag>;
using DatumPlaneId = TypedId<DatumPlaneIdTag>;
using ConstructionPointId = TypedId<ConstructionPointIdTag>;
using ConstructionLineId = TypedId<ConstructionLineIdTag>;
using ConstructionPlaneId = TypedId<ConstructionPlaneIdTag>;
using ConstructionRelationId = TypedId<ConstructionRelationIdTag>;
using ReferenceStatusId = TypedId<ReferenceStatusIdTag>;
using ReferenceRemapId = TypedId<ReferenceRemapIdTag>;
using SketchEntityId = TypedId<SketchEntityIdTag>;
using SketchConstraintId = TypedId<SketchConstraintIdTag>;
using SketchDimensionId = TypedId<SketchDimensionIdTag>;
using SketchTrimOperationId = TypedId<SketchTrimOperationIdTag>;
using ProfileId = TypedId<ProfileIdTag>;
using SketchId = TypedId<SketchIdTag>;
using FeatureId = TypedId<FeatureIdTag>;
using ShapeCacheId = TypedId<ShapeCacheIdTag>;
using ParameterBindingId = TypedId<ParameterBindingIdTag>;
using ComponentInstanceId = TypedId<ComponentInstanceIdTag>;

} // namespace blcad
