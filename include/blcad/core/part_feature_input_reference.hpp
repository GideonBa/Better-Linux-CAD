#pragma once

#include "blcad/core/datum_plane.hpp"
#include "blcad/core/generated_topology_reference.hpp"
#include "blcad/core/id.hpp"
#include "blcad/core/result.hpp"
#include "blcad/core/sketch_line_reference.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <variant>

namespace blcad {

class PartDocument;

enum class PartFeatureInputCapability { ProfileRegion, Axis, Line, Plane, Face, Edge, Body };

enum class PartFeatureInputRole {
  ProfileRegion,
  RevolveProfile,
  SweepProfile,
  LoftSection,
  RevolveAxis,
  PatternAxis,
  DraftPullDirection,
  MirrorPlane,
  DraftNeutralPlane,
  ExtrudeLimitFace,
  FilletEdge,
  ChamferEdge,
  ShellRemovalFace,
  DraftFace,
  TargetBody,
  ToolBody,
  SourceBody,
};

enum class PartFeatureInputSourceKind {
  SketchProfileRegion,
  SketchLine,
  DatumAxis,
  ConstructionLine,
  SemanticAxis,
  SemanticLinearEdge,
  DatumPlane,
  ConstructionPlane,
  SemanticPlanarFace,
  SemanticCylindricalFace,
  SemanticCircularEdge,
  ResultEdge,
  Body,
};

[[nodiscard]] std::string_view to_string(PartFeatureInputCapability capability) noexcept;
[[nodiscard]] std::string_view to_string(PartFeatureInputRole role) noexcept;
[[nodiscard]] std::string_view to_string(PartFeatureInputSourceKind source_kind) noexcept;
[[nodiscard]] bool part_feature_input_role_accepts(PartFeatureInputRole role,
                                                   PartFeatureInputCapability capability) noexcept;

class ProfileRegionReference {
public:
  [[nodiscard]] static Result<ProfileRegionReference>
  create(SketchId sketch, ProfileId profile,
         PartFeatureInputRole role = PartFeatureInputRole::ProfileRegion);

  [[nodiscard]] const SketchId& sketch() const noexcept;
  [[nodiscard]] const ProfileId& profile() const noexcept;
  [[nodiscard]] PartFeatureInputRole role() const noexcept;
  [[nodiscard]] PartFeatureInputCapability expected_capability() const noexcept;
  [[nodiscard]] PartFeatureInputSourceKind source_kind() const noexcept;
  [[nodiscard]] std::string source_node_id() const;

private:
  ProfileRegionReference(SketchId sketch, ProfileId profile, PartFeatureInputRole role);
  SketchId sketch_;
  ProfileId profile_;
  PartFeatureInputRole role_;
};

using AxisReferenceSource = std::variant<DatumAxisId, ConstructionLineId, SemanticAxisReference,
                                         SemanticEdgeReference, SketchLineReference>;

class AxisReference {
public:
  [[nodiscard]] static Result<AxisReference> create_sketch_line(PartFeatureInputRole role,
                                                                SketchLineReference line);
  [[nodiscard]] static Result<AxisReference> create_datum_axis(PartFeatureInputRole role,
                                                               DatumAxisId axis);
  [[nodiscard]] static Result<AxisReference>
  create_construction_line(PartFeatureInputRole role, ConstructionLineId line,
                           PartFeatureInputCapability expected_capability);
  [[nodiscard]] static Result<AxisReference> create_semantic_axis(PartFeatureInputRole role,
                                                                  SemanticAxisReference axis);
  [[nodiscard]] static Result<AxisReference>
  create_semantic_edge(PartFeatureInputRole role, SemanticEdgeReference edge,
                       PartFeatureInputCapability expected_capability);

  [[nodiscard]] PartFeatureInputRole role() const noexcept;
  [[nodiscard]] PartFeatureInputCapability expected_capability() const noexcept;
  [[nodiscard]] PartFeatureInputSourceKind source_kind() const noexcept;
  [[nodiscard]] const AxisReferenceSource& source() const noexcept;
  [[nodiscard]] std::string source_node_id() const;

private:
  AxisReference(PartFeatureInputRole role, PartFeatureInputCapability expected_capability,
                AxisReferenceSource source);
  PartFeatureInputRole role_;
  PartFeatureInputCapability expected_capability_;
  AxisReferenceSource source_;
};

using PlaneReferenceSource = std::variant<DatumPlaneId, ConstructionPlaneId, SemanticFaceReference>;

class PlaneReference {
public:
  [[nodiscard]] static Result<PlaneReference> create_datum_plane(PartFeatureInputRole role,
                                                                 DatumPlaneId plane);
  [[nodiscard]] static Result<PlaneReference> create_construction_plane(PartFeatureInputRole role,
                                                                        ConstructionPlaneId plane);
  [[nodiscard]] static Result<PlaneReference> create_semantic_face(PartFeatureInputRole role,
                                                                   SemanticFaceReference face);

  [[nodiscard]] PartFeatureInputRole role() const noexcept;
  [[nodiscard]] PartFeatureInputCapability expected_capability() const noexcept;
  [[nodiscard]] PartFeatureInputSourceKind source_kind() const noexcept;
  [[nodiscard]] const PlaneReferenceSource& source() const noexcept;
  [[nodiscard]] std::string source_node_id() const;

private:
  PlaneReference(PartFeatureInputRole role, PlaneReferenceSource source);
  PartFeatureInputRole role_;
  PlaneReferenceSource source_;
};

using FaceReferenceSource = std::variant<SemanticFaceReference, SemanticCylindricalFaceReference>;

class FaceReference {
public:
  [[nodiscard]] static Result<FaceReference> create_planar(PartFeatureInputRole role,
                                                           SemanticFaceReference face);
  [[nodiscard]] static Result<FaceReference>
  create_cylindrical(PartFeatureInputRole role, SemanticCylindricalFaceReference face);

  [[nodiscard]] PartFeatureInputRole role() const noexcept;
  [[nodiscard]] PartFeatureInputCapability expected_capability() const noexcept;
  [[nodiscard]] PartFeatureInputSourceKind source_kind() const noexcept;
  [[nodiscard]] const FaceReferenceSource& source() const noexcept;
  [[nodiscard]] std::string source_node_id() const;

private:
  FaceReference(PartFeatureInputRole role, FaceReferenceSource source);
  PartFeatureInputRole role_;
  FaceReferenceSource source_;
};

// A geometric selection witness anchored to the producing feature, never a
// kernel traversal index. Recovery requires a unique complete matching witness
// on the current Body and fails closed after incompatible upstream edits.
class ResultEdgeReference {
public:
  static constexpr std::size_t sample_count = 17U;
  [[nodiscard]] static Result<ResultEdgeReference> create(FeatureId source_feature, int curve_type,
                                                          double length_mm,
                                                          std::array<Point3, sample_count> samples);
  [[nodiscard]] const FeatureId& source_feature() const noexcept {
    return source_feature_;
  }
  [[nodiscard]] int curve_type() const noexcept {
    return curve_type_;
  }
  [[nodiscard]] double length_mm() const noexcept {
    return length_mm_;
  }
  [[nodiscard]] const std::array<Point3, sample_count>& samples() const noexcept {
    return samples_;
  }
  [[nodiscard]] std::string identity() const;

private:
  ResultEdgeReference(FeatureId source_feature, int curve_type, double length_mm,
                      std::array<Point3, sample_count> samples);
  FeatureId source_feature_;
  int curve_type_;
  double length_mm_;
  std::array<Point3, sample_count> samples_;
};

using EdgeReferenceSource =
    std::variant<SemanticEdgeReference, SemanticCircularEdgeReference, ResultEdgeReference>;

class EdgeReference {
public:
  [[nodiscard]] static Result<EdgeReference> create_linear(PartFeatureInputRole role,
                                                           SemanticEdgeReference edge);
  [[nodiscard]] static Result<EdgeReference> create_circular(PartFeatureInputRole role,
                                                             SemanticCircularEdgeReference edge);

  [[nodiscard]] static Result<EdgeReference> create_result(PartFeatureInputRole role,
                                                           ResultEdgeReference edge);

  [[nodiscard]] PartFeatureInputRole role() const noexcept;
  [[nodiscard]] PartFeatureInputCapability expected_capability() const noexcept;
  [[nodiscard]] PartFeatureInputSourceKind source_kind() const noexcept;
  [[nodiscard]] const EdgeReferenceSource& source() const noexcept;
  [[nodiscard]] std::string source_node_id() const;

private:
  EdgeReference(PartFeatureInputRole role, EdgeReferenceSource source);
  PartFeatureInputRole role_;
  EdgeReferenceSource source_;
};

class BodyReference {
public:
  [[nodiscard]] static Result<BodyReference> create(PartFeatureInputRole role, BodyId body);

  [[nodiscard]] PartFeatureInputRole role() const noexcept;
  [[nodiscard]] PartFeatureInputCapability expected_capability() const noexcept;
  [[nodiscard]] PartFeatureInputSourceKind source_kind() const noexcept;
  [[nodiscard]] const BodyId& body() const noexcept;
  [[nodiscard]] std::string source_node_id() const;

private:
  BodyReference(PartFeatureInputRole role, BodyId body);
  PartFeatureInputRole role_;
  BodyId body_;
};

[[nodiscard]] Result<std::size_t>
validate_part_feature_input_reference(const PartDocument& document,
                                      const ProfileRegionReference& reference);
[[nodiscard]] Result<std::size_t>
validate_part_feature_input_reference(const PartDocument& document, const AxisReference& reference);
[[nodiscard]] Result<std::size_t>
validate_part_feature_input_reference(const PartDocument& document,
                                      const PlaneReference& reference);
[[nodiscard]] Result<std::size_t>
validate_part_feature_input_reference(const PartDocument& document, const FaceReference& reference);
[[nodiscard]] Result<std::size_t>
validate_part_feature_input_reference(const PartDocument& document, const EdgeReference& reference);
[[nodiscard]] Result<std::size_t>
validate_part_feature_input_reference(const PartDocument& document, const BodyReference& reference);

} // namespace blcad
