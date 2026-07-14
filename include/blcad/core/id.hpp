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
struct DatumAxisIdTag;
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
struct Sketch3DIdTag;
struct FeatureIdTag;
struct ShapeCacheIdTag;
struct ParameterBindingIdTag;
struct ComponentInstanceIdTag;
struct SubassemblyInstanceIdTag;
struct AssemblyConstraintIdTag;
struct AssemblyJointIdTag;
struct BodyIdTag;
struct BodyTransformIdTag;

using DocumentId = TypedId<DocumentIdTag>;
using ParameterId = TypedId<ParameterIdTag>;
using DatumPlaneId = TypedId<DatumPlaneIdTag>;
using DatumAxisId = TypedId<DatumAxisIdTag>;
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
using Sketch3DId = TypedId<Sketch3DIdTag>;
using FeatureId = TypedId<FeatureIdTag>;
using ShapeCacheId = TypedId<ShapeCacheIdTag>;
using ParameterBindingId = TypedId<ParameterBindingIdTag>;
using ComponentInstanceId = TypedId<ComponentInstanceIdTag>;
using SubassemblyInstanceId = TypedId<SubassemblyInstanceIdTag>;
using AssemblyConstraintId = TypedId<AssemblyConstraintIdTag>;
using AssemblyJointId = TypedId<AssemblyJointIdTag>;
using BodyId = TypedId<BodyIdTag>;
using BodyTransformId = TypedId<BodyTransformIdTag>;

} // namespace blcad
