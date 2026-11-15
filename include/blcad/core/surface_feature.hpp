#pragma once

#include "blcad/core/body.hpp"
#include "blcad/core/feature.hpp"
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
enum class SurfaceReferenceSourceKind { Body, SemanticFace, SelectedFaces };
enum class TrimmingReferenceSourceKind { BoundaryCurve, ProfileRegion };
enum class SurfaceFeatureKind {
  BoundarySurface,
  FillSurface,
  TrimSurface,
  ExtendSurface,
  SurfaceStitch,
  ClosedShellToSolid,
  OffsetSurface,
  Thicken,
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

struct SurfaceFaceSelection {
  BodyId body;
  std::vector<FaceReference> faces;
};
using SurfaceReferenceSource = std::variant<BodyId, SemanticFaceReference, SurfaceFaceSelection>;

class SurfaceReference {
public:
  [[nodiscard]] static Result<SurfaceReference> create_body(BodyId body);
  [[nodiscard]] static Result<SurfaceReference> create_semantic_face(SemanticFaceReference face);
  [[nodiscard]] static Result<SurfaceReference> create_faces(BodyId owner,
                                                             std::vector<FaceReference> faces);

  [[nodiscard]] SurfaceReferenceSourceKind source_kind() const noexcept;
  [[nodiscard]] const SurfaceReferenceSource& source() const noexcept;
  [[nodiscard]] std::string source_node_id() const;
  [[nodiscard]] std::vector<std::string> source_nodes() const;
  [[nodiscard]] std::string stable_key() const;

private:
  explicit SurfaceReference(SurfaceReferenceSource source);
  SurfaceReferenceSource source_;
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

enum class SurfaceTrimSide { KeepInside, RemoveInside };
enum class SurfaceTrimProjection { OnSurface, AlongDirection };
struct SurfaceTrimOptions {
  SurfaceTrimSide side = SurfaceTrimSide::KeepInside;
  SurfaceTrimProjection projection = SurfaceTrimProjection::OnSurface;
  std::optional<Vector3> direction;
  ParameterId tolerance;
};
enum class SurfaceExtendMode { Natural, Tangent };
struct SurfaceExtendOptions {
  SurfaceExtendMode mode = SurfaceExtendMode::Natural;
  ParameterId tolerance;
  std::optional<ParameterId> maximum_deviation;
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
  [[nodiscard]] Result<TrimSurfaceFeature> with_options(std::optional<SurfaceTrimOptions>) const;
  [[nodiscard]] const std::optional<SurfaceTrimOptions>& options() const noexcept {
    return options_;
  }
  [[nodiscard]] const BodyId& result_body() const noexcept;

private:
  TrimSurfaceFeature(FeatureId, std::string, SurfaceReference, TrimmingReference, BodyId);
  FeatureId id_;
  std::string name_;
  SurfaceReference target_;
  TrimmingReference trimming_;
  std::optional<SurfaceTrimOptions> options_;
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
  [[nodiscard]] Result<ExtendSurfaceFeature>
      with_options(std::optional<SurfaceExtendOptions>) const;
  [[nodiscard]] const std::optional<SurfaceExtendOptions>& options() const noexcept {
    return options_;
  }
  [[nodiscard]] const BodyId& result_body() const noexcept;

private:
  ExtendSurfaceFeature(FeatureId, std::string, SurfaceReference, BoundaryCurveReference,
                       ParameterId, BodyId);
  FeatureId id_;
  std::string name_;
  SurfaceReference target_;
  BoundaryCurveReference boundary_;
  ParameterId distance_parameter_;
  std::optional<SurfaceExtendOptions> options_;
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

enum class SurfaceOffsetSide { AlongNormal, OppositeNormal, Symmetric };
enum class SurfaceOffsetApproximation { ExactAnalytic, VerifiedApproximation };
struct SurfaceOffsetParameters {
  ParameterId distance;
  ParameterId tolerance;
  SurfaceOffsetSide side = SurfaceOffsetSide::AlongNormal;
  SurfaceOffsetApproximation approximation = SurfaceOffsetApproximation::ExactAnalytic;
  std::optional<ParameterId> maximum_deviation;
};
class SurfaceOffsetFeature {
public:
  [[nodiscard]] static Result<SurfaceOffsetFeature> create_offset(FeatureId, std::string,
                                                                  SurfaceReference,
                                                                  SurfaceOffsetParameters,
                                                                  FeatureBodyResultContext);
  [[nodiscard]] static Result<SurfaceOffsetFeature> create_thicken(FeatureId, std::string,
                                                                   SurfaceReference,
                                                                   SurfaceOffsetParameters,
                                                                   FeatureBodyResultContext);
  [[nodiscard]] const FeatureId& id() const noexcept {
    return id_;
  }
  [[nodiscard]] const std::string& name() const noexcept {
    return name_;
  }
  [[nodiscard]] SurfaceFeatureKind kind() const noexcept {
    return kind_;
  }
  [[nodiscard]] const SurfaceReference& target() const noexcept {
    return target_;
  }
  [[nodiscard]] const SurfaceOffsetParameters& parameters() const noexcept {
    return parameters_;
  }
  [[nodiscard]] const FeatureBodyResultContext& body_result_context() const noexcept {
    return context_;
  }
  [[nodiscard]] const BodyId& result_body() const noexcept {
    return context_.effective_produced_body();
  }

private:
  SurfaceOffsetFeature(FeatureId, std::string, SurfaceFeatureKind, SurfaceReference,
                       SurfaceOffsetParameters, FeatureBodyResultContext);
  [[nodiscard]] static Result<SurfaceOffsetFeature> create(FeatureId, std::string,
                                                           SurfaceFeatureKind, SurfaceReference,
                                                           SurfaceOffsetParameters,
                                                           FeatureBodyResultContext);
  FeatureId id_;
  std::string name_;
  SurfaceFeatureKind kind_;
  SurfaceReference target_;
  SurfaceOffsetParameters parameters_;
  FeatureBodyResultContext context_;
};

using SurfaceFeature = std::variant<BoundarySurfaceFeature, FillSurfaceFeature, TrimSurfaceFeature,
                                    ExtendSurfaceFeature, SurfaceStitchFeature,
                                    ClosedShellToSolidFeature, SurfaceOffsetFeature>;

[[nodiscard]] const FeatureId& surface_feature_id(const SurfaceFeature& feature) noexcept;
[[nodiscard]] const std::string& surface_feature_name(const SurfaceFeature& feature) noexcept;
[[nodiscard]] SurfaceFeatureKind surface_feature_kind(const SurfaceFeature& feature) noexcept;
[[nodiscard]] const BodyId& surface_feature_result_body(const SurfaceFeature& feature) noexcept;
[[nodiscard]] Result<std::size_t> validate_surface_feature_parameters(const PartDocument&,
                                                                      const SurfaceFeature&);
[[nodiscard]] BodyKind surface_feature_result_kind(const SurfaceFeature&) noexcept;
[[nodiscard]] bool surface_feature_requires_v13(const SurfaceFeature&) noexcept;
[[nodiscard]] const FeatureBodyResultContext*
surface_feature_body_context(const SurfaceFeature&) noexcept;
[[nodiscard]] std::vector<std::string> surface_feature_input_nodes(const SurfaceFeature& feature);

} // namespace blcad
