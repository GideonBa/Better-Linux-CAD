#pragma once

#include "blcad/core/generated_topology_reference.hpp"
#include "blcad/core/part_feature_input_reference.hpp"
#include "blcad/core/path_curve.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace blcad {

enum class BoundaryCurveSourceKind { PathCurve, SemanticEdge };
enum class SurfaceReferenceSourceKind { Body, SemanticFace };
enum class TrimmingReferenceSourceKind { BoundaryCurve, ProfileRegion };
enum class SurfaceFeatureKind {
  BoundarySurface,
  FillSurface,
  TrimSurface,
  ExtendSurface,
  SurfaceStitch,
  ClosedShellToSolid,
};

[[nodiscard]] std::string_view to_string(BoundaryCurveSourceKind kind) noexcept;
[[nodiscard]] std::string_view to_string(SurfaceReferenceSourceKind kind) noexcept;
[[nodiscard]] std::string_view to_string(TrimmingReferenceSourceKind kind) noexcept;
[[nodiscard]] std::string_view to_string(SurfaceFeatureKind kind) noexcept;

class BoundaryCurveReference {
public:
  [[nodiscard]] static Result<BoundaryCurveReference> create_path_curve(PathCurveId path);
  [[nodiscard]] static Result<BoundaryCurveReference>
  create_semantic_edge(SemanticEdgeReference edge);

  [[nodiscard]] BoundaryCurveSourceKind source_kind() const noexcept;
  [[nodiscard]] const std::variant<PathCurveId, SemanticEdgeReference>& source() const noexcept;
  [[nodiscard]] std::string source_node_id() const;
  [[nodiscard]] std::string stable_key() const;

private:
  explicit BoundaryCurveReference(std::variant<PathCurveId, SemanticEdgeReference> source);
  std::variant<PathCurveId, SemanticEdgeReference> source_;
};

class SurfaceReference {
public:
  [[nodiscard]] static Result<SurfaceReference> create_body(BodyId body);
  [[nodiscard]] static Result<SurfaceReference> create_semantic_face(SemanticFaceReference face);

  [[nodiscard]] SurfaceReferenceSourceKind source_kind() const noexcept;
  [[nodiscard]] const std::variant<BodyId, SemanticFaceReference>& source() const noexcept;
  [[nodiscard]] std::string source_node_id() const;
  [[nodiscard]] std::string stable_key() const;

private:
  explicit SurfaceReference(std::variant<BodyId, SemanticFaceReference> source);
  std::variant<BodyId, SemanticFaceReference> source_;
};

class TrimmingReference {
public:
  [[nodiscard]] static Result<TrimmingReference>
  create_boundary_curve(BoundaryCurveReference boundary);
  [[nodiscard]] static Result<TrimmingReference>
  create_profile_region(ProfileRegionReference profile);

  [[nodiscard]] TrimmingReferenceSourceKind source_kind() const noexcept;
  [[nodiscard]] const std::variant<BoundaryCurveReference, ProfileRegionReference>&
  source() const noexcept;
  [[nodiscard]] std::string source_node_id() const;

private:
  explicit TrimmingReference(std::variant<BoundaryCurveReference, ProfileRegionReference> source);
  std::variant<BoundaryCurveReference, ProfileRegionReference> source_;
};

class BoundarySurfaceFeature {
public:
  [[nodiscard]] static Result<BoundarySurfaceFeature>
  create(FeatureId id, std::string name, std::vector<BoundaryCurveReference> boundaries,
         BodyId result_body);
  [[nodiscard]] const FeatureId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] const std::vector<BoundaryCurveReference>& boundaries() const noexcept;
  [[nodiscard]] const BodyId& result_body() const noexcept;

private:
  BoundarySurfaceFeature(FeatureId, std::string, std::vector<BoundaryCurveReference>, BodyId);
  FeatureId id_;
  std::string name_;
  std::vector<BoundaryCurveReference> boundaries_;
  BodyId result_body_;
};

class FillSurfaceFeature {
public:
  [[nodiscard]] static Result<FillSurfaceFeature>
  create(FeatureId id, std::string name, std::vector<BoundaryCurveReference> boundaries,
         BodyId result_body);
  [[nodiscard]] const FeatureId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] const std::vector<BoundaryCurveReference>& boundaries() const noexcept;
  [[nodiscard]] const BodyId& result_body() const noexcept;

private:
  FillSurfaceFeature(FeatureId, std::string, std::vector<BoundaryCurveReference>, BodyId);
  FeatureId id_;
  std::string name_;
  std::vector<BoundaryCurveReference> boundaries_;
  BodyId result_body_;
};

class TrimSurfaceFeature {
public:
  [[nodiscard]] static Result<TrimSurfaceFeature> create(FeatureId id, std::string name,
                                                         SurfaceReference target,
                                                         TrimmingReference trimming,
                                                         BodyId result_body);
  [[nodiscard]] const FeatureId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] const SurfaceReference& target() const noexcept;
  [[nodiscard]] const TrimmingReference& trimming() const noexcept;
  [[nodiscard]] const BodyId& result_body() const noexcept;

private:
  TrimSurfaceFeature(FeatureId, std::string, SurfaceReference, TrimmingReference, BodyId);
  FeatureId id_;
  std::string name_;
  SurfaceReference target_;
  TrimmingReference trimming_;
  BodyId result_body_;
};

class ExtendSurfaceFeature {
public:
  [[nodiscard]] static Result<ExtendSurfaceFeature>
  create(FeatureId id, std::string name, SurfaceReference target, BoundaryCurveReference boundary,
         ParameterId distance_parameter, BodyId result_body);
  [[nodiscard]] const FeatureId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] const SurfaceReference& target() const noexcept;
  [[nodiscard]] const BoundaryCurveReference& boundary() const noexcept;
  [[nodiscard]] const ParameterId& distance_parameter() const noexcept;
  [[nodiscard]] const BodyId& result_body() const noexcept;

private:
  ExtendSurfaceFeature(FeatureId, std::string, SurfaceReference, BoundaryCurveReference,
                       ParameterId, BodyId);
  FeatureId id_;
  std::string name_;
  SurfaceReference target_;
  BoundaryCurveReference boundary_;
  ParameterId distance_parameter_;
  BodyId result_body_;
};

class SurfaceStitchFeature {
public:
  [[nodiscard]] static Result<SurfaceStitchFeature> create(FeatureId id, std::string name,
                                                           std::vector<SurfaceReference> surfaces,
                                                           ParameterId tolerance_parameter,
                                                           BodyId result_body);
  [[nodiscard]] const FeatureId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] const std::vector<SurfaceReference>& surfaces() const noexcept;
  [[nodiscard]] const ParameterId& tolerance_parameter() const noexcept;
  [[nodiscard]] const BodyId& result_body() const noexcept;

private:
  SurfaceStitchFeature(FeatureId, std::string, std::vector<SurfaceReference>, ParameterId, BodyId);
  FeatureId id_;
  std::string name_;
  std::vector<SurfaceReference> surfaces_;
  ParameterId tolerance_parameter_;
  BodyId result_body_;
};

class ClosedShellToSolidFeature {
public:
  [[nodiscard]] static Result<ClosedShellToSolidFeature>
  create(FeatureId id, std::string name, SurfaceReference shell, BodyId result_body);
  [[nodiscard]] const FeatureId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] const SurfaceReference& shell() const noexcept;
  [[nodiscard]] const BodyId& result_body() const noexcept;

private:
  ClosedShellToSolidFeature(FeatureId, std::string, SurfaceReference, BodyId);
  FeatureId id_;
  std::string name_;
  SurfaceReference shell_;
  BodyId result_body_;
};

using SurfaceFeature =
    std::variant<BoundarySurfaceFeature, FillSurfaceFeature, TrimSurfaceFeature,
                 ExtendSurfaceFeature, SurfaceStitchFeature, ClosedShellToSolidFeature>;

[[nodiscard]] const FeatureId& surface_feature_id(const SurfaceFeature& feature) noexcept;
[[nodiscard]] const std::string& surface_feature_name(const SurfaceFeature& feature) noexcept;
[[nodiscard]] SurfaceFeatureKind surface_feature_kind(const SurfaceFeature& feature) noexcept;
[[nodiscard]] const BodyId& surface_feature_result_body(const SurfaceFeature& feature) noexcept;
[[nodiscard]] std::vector<std::string> surface_feature_input_nodes(const SurfaceFeature& feature);

} // namespace blcad
