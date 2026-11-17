#pragma once

#include "blcad/core/part_feature_input_reference.hpp"

#include <string>
#include <variant>
#include <vector>

namespace blcad {
class PartDocument;

enum class DirectEditFaceCapability { Planar, Cylindrical, AnySurface };
enum class DirectEditSizeMode { Radius, Diameter };
enum class DirectEditBlendPolicy { RejectAffectedBlends };
enum class DirectEditHealPolicy { ExtendAdjacent };

// Explicit model-space frame. Values are authored, never inferred again from
// a transient viewport triad. z_axis is the right-handed cross product x × y.
struct DirectEditFrame {
  Point3 origin_mm{0, 0, 0};
  Vector3 x_axis{1, 0, 0};
  Vector3 y_axis{0, 1, 0};
};
struct DirectEditPolicy {
  double tolerance_mm = 1e-6;
  DirectEditBlendPolicy blends = DirectEditBlendPolicy::RejectAffectedBlends;
  DirectEditHealPolicy healing = DirectEditHealPolicy::ExtendAdjacent;
};
struct DirectEditFace {
  FaceReference reference;
  DirectEditFaceCapability expected;
};
struct DirectFaceMove {
  ParameterId x, y, z;
  Vector3 signs{1, 1, 1};
};
struct DirectFaceRotate {
  Vector3 axis;
  ParameterId angle;
};
struct DirectFaceOffset {
  ParameterId distance;
  bool reverse = false;
};
struct DirectFaceSize {
  ParameterId size;
  DirectEditSizeMode mode;
};
struct DirectFaceDeleteHeal {};
using DirectEditOperation = std::variant<DirectFaceMove, DirectFaceRotate, DirectFaceOffset,
                                         DirectFaceSize, DirectFaceDeleteHeal>;

class DirectEditFeature {
public:
  [[nodiscard]] static Result<DirectEditFeature>
  create(FeatureId id, std::string name, BodyId target, FeatureId predecessor,
         std::vector<DirectEditFace> selections, DirectEditOperation operation,
         DirectEditFrame frame = {}, DirectEditPolicy policy = {});
  [[nodiscard]] const FeatureId& id() const noexcept {
    return id_;
  }
  [[nodiscard]] const std::string& name() const noexcept {
    return name_;
  }
  [[nodiscard]] const BodyId& target_body() const noexcept {
    return target_;
  }
  [[nodiscard]] const FeatureId& predecessor() const noexcept {
    return predecessor_;
  }
  [[nodiscard]] const std::vector<DirectEditFace>& selections() const noexcept {
    return selections_;
  }
  [[nodiscard]] std::vector<FaceReference> faces() const;
  [[nodiscard]] const DirectEditOperation& operation() const noexcept {
    return operation_;
  }
  [[nodiscard]] const DirectEditFrame& frame() const noexcept {
    return frame_;
  }
  [[nodiscard]] const DirectEditPolicy& policy() const noexcept {
    return policy_;
  }
  [[nodiscard]] std::vector<ParameterId> parameter_dependencies() const;
  [[nodiscard]] std::vector<std::string> input_nodes() const;

private:
  DirectEditFeature(FeatureId, std::string, BodyId, FeatureId, std::vector<DirectEditFace>,
                    DirectEditOperation, DirectEditFrame, DirectEditPolicy);
  FeatureId id_;
  std::string name_;
  BodyId target_;
  FeatureId predecessor_;
  std::vector<DirectEditFace> selections_;
  DirectEditOperation operation_;
  DirectEditFrame frame_;
  DirectEditPolicy policy_;
};

[[nodiscard]] Result<std::size_t> validate_direct_edit_parameters(const PartDocument&,
                                                                  const DirectEditFeature&);
[[nodiscard]] Result<std::size_t> validate_direct_edit_inputs(const PartDocument&,
                                                              const DirectEditFeature&);
} // namespace blcad
