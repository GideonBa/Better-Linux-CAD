#include "blcad/core/surface_feature.hpp"

#include <algorithm>
#include <set>
#include <type_traits>
#include <unordered_set>
#include <utility>

namespace blcad {
namespace {
template <typename T> Result<T> invalid(std::string id, std::string message) {
  return Result<T>::failure(Error::validation(std::move(id), std::move(message)));
}

template <typename Reference> bool unique_references(const std::vector<Reference>& references) {
  std::unordered_set<std::string> keys;
  return std::all_of(references.begin(), references.end(), [&keys](const auto& reference) {
    return keys.insert(reference.stable_key()).second;
  });
}

template <typename T> const FeatureId& id_of(const T& value) {
  return value.id();
}
template <typename T> const std::string& name_of(const T& value) {
  return value.name();
}
template <typename T> const BodyId& result_of(const T& value) {
  return value.result_body();
}
} // namespace

std::string_view to_string(BoundaryCurveSourceKind kind) noexcept {
  return kind == BoundaryCurveSourceKind::PathCurve ? "path_curve" : "semantic_edge";
}
std::string_view to_string(SurfaceReferenceSourceKind kind) noexcept {
  if (kind == SurfaceReferenceSourceKind::Body)
    return "body";
  if (kind == SurfaceReferenceSourceKind::SelectedFaces)
    return "selected_faces";
  return "semantic_face";
}
std::string_view to_string(TrimmingReferenceSourceKind kind) noexcept {
  return kind == TrimmingReferenceSourceKind::BoundaryCurve ? "boundary_curve" : "profile_region";
}
std::string_view to_string(SurfaceFeatureKind kind) noexcept {
  switch (kind) {
  case SurfaceFeatureKind::BoundarySurface:
    return "boundary_surface";
  case SurfaceFeatureKind::FillSurface:
    return "fill_surface";
  case SurfaceFeatureKind::TrimSurface:
    return "trim_surface";
  case SurfaceFeatureKind::ExtendSurface:
    return "extend_surface";
  case SurfaceFeatureKind::SurfaceStitch:
    return "surface_stitch";
  case SurfaceFeatureKind::ClosedShellToSolid:
    return "closed_shell_to_solid";
  case SurfaceFeatureKind::OffsetSurface:
    return "offset_surface";
  case SurfaceFeatureKind::Thicken:
    return "thicken";
  }
  return "boundary_surface";
}

Result<BoundaryCurveReference> BoundaryCurveReference::create_path_curve(PathCurveId path) {
  if (path.empty())
    return invalid<BoundaryCurveReference>("boundary_curve", "path id must not be empty");
  return Result<BoundaryCurveReference>::success(BoundaryCurveReference(std::move(path)));
}
Result<BoundaryCurveReference>
BoundaryCurveReference::create_semantic_edge(SemanticEdgeReference edge) {
  return Result<BoundaryCurveReference>::success(BoundaryCurveReference(std::move(edge)));
}
BoundaryCurveReference::BoundaryCurveReference(
    std::variant<PathCurveId, SemanticEdgeReference> source)
    : source_(std::move(source)) {}
BoundaryCurveSourceKind BoundaryCurveReference::source_kind() const noexcept {
  return std::holds_alternative<PathCurveId>(source_) ? BoundaryCurveSourceKind::PathCurve
                                                      : BoundaryCurveSourceKind::SemanticEdge;
}
const std::variant<PathCurveId, SemanticEdgeReference>&
BoundaryCurveReference::source() const noexcept {
  return source_;
}
std::string BoundaryCurveReference::source_node_id() const {
  return source_kind() == BoundaryCurveSourceKind::PathCurve
             ? std::get<PathCurveId>(source_).value()
             : std::get<SemanticEdgeReference>(source_).source_feature().value();
}
std::string BoundaryCurveReference::stable_key() const {
  return source_kind() == BoundaryCurveSourceKind::PathCurve
             ? "path:" + std::get<PathCurveId>(source_).value()
             : "edge:" + std::get<SemanticEdgeReference>(source_).node_id();
}

Result<SurfaceReference> SurfaceReference::create_body(BodyId body) {
  if (body.empty())
    return invalid<SurfaceReference>("surface_reference", "body id must not be empty");
  return Result<SurfaceReference>::success(SurfaceReference(std::move(body)));
}
Result<SurfaceReference> SurfaceReference::create_semantic_face(SemanticFaceReference face) {
  return Result<SurfaceReference>::success(SurfaceReference(std::move(face)));
}
namespace {
std::string framed(const std::string& value) {
  return std::to_string(value.size()) + ":" + value;
}
std::string face_key(const FaceReference& face) {
  return std::visit(
      [](const auto& source) {
        using T = std::decay_t<decltype(source)>;
        if constexpr (std::is_same_v<T, ResultFaceReference>)
          return "result/" + source.identity();
        else if constexpr (std::is_same_v<T, ImportedTopologyReference>)
          return "imported/" + framed(source.catalog().value()) + framed(source.entry().value());
        else if constexpr (std::is_same_v<T, SemanticFaceReference>)
          return "plane/" + framed(source.source_feature().value()) +
                 framed(std::string(to_string(source.face())));
        else
          return "cylinder/" + framed(source.source_feature().value()) +
                 framed(source.source_profile().value());
      },
      face.source());
}
} // namespace
Result<SurfaceReference> SurfaceReference::create_faces(BodyId owner,
                                                        std::vector<FaceReference> faces) {
  if (owner.empty() || faces.empty() || faces.size() > 64)
    return invalid<SurfaceReference>(
        owner.value(), "Surface face selection requires an owner and one to sixty-four faces");
  std::set<std::string> keys;
  for (const auto& face : faces) {
    if (face.role() != PartFeatureInputRole::SurfaceSourceFace ||
        !keys.insert(face_key(face)).second)
      return invalid<SurfaceReference>(owner.value(),
                                       "Surface inputs require unique typed source faces");
    if (const auto* result = std::get_if<ResultFaceReference>(&face.source());
        result && result->body() != owner)
      return invalid<SurfaceReference>(
          owner.value(), "Selected result face must belong to its explicit Surface input owner");
  }
  return Result<SurfaceReference>::success(
      SurfaceReference(SurfaceFaceSelection{std::move(owner), std::move(faces)}));
}
SurfaceReference::SurfaceReference(SurfaceReferenceSource source) : source_(std::move(source)) {}
SurfaceReferenceSourceKind SurfaceReference::source_kind() const noexcept {
  if (std::holds_alternative<BodyId>(source_))
    return SurfaceReferenceSourceKind::Body;
  return std::holds_alternative<SemanticFaceReference>(source_)
             ? SurfaceReferenceSourceKind::SemanticFace
             : SurfaceReferenceSourceKind::SelectedFaces;
}
const SurfaceReferenceSource& SurfaceReference::source() const noexcept {
  return source_;
}
std::string SurfaceReference::source_node_id() const {
  if (const auto* body = std::get_if<BodyId>(&source_))
    return "body:" + body->value();
  if (const auto* face = std::get_if<SemanticFaceReference>(&source_))
    return face->source_feature().value();
  return "body:" + std::get<SurfaceFaceSelection>(source_).body.value();
}
std::vector<std::string> SurfaceReference::source_nodes() const {
  std::vector<std::string> nodes{source_node_id()};
  if (const auto* selected = std::get_if<SurfaceFaceSelection>(&source_))
    for (const auto& face : selected->faces)
      nodes.push_back(face.source_node_id());
  std::sort(nodes.begin(), nodes.end());
  nodes.erase(std::unique(nodes.begin(), nodes.end()), nodes.end());
  return nodes;
}
std::string SurfaceReference::stable_key() const {
  if (const auto* body = std::get_if<BodyId>(&source_))
    return "body:" + body->value();
  if (const auto* face = std::get_if<SemanticFaceReference>(&source_))
    return "face:" + face->source_feature().value() + ":" + std::string(to_string(face->face()));
  const auto& selected = std::get<SurfaceFaceSelection>(source_);
  std::set<std::string> keys;
  for (const auto& face : selected.faces)
    keys.insert(face_key(face));
  std::string key = "faces/" + framed(selected.body.value());
  for (const auto& value : keys)
    key += framed(value);
  return key;
}

Result<TrimmingReference>
TrimmingReference::create_boundary_curve(BoundaryCurveReference boundary) {
  return Result<TrimmingReference>::success(TrimmingReference(std::move(boundary)));
}
Result<TrimmingReference> TrimmingReference::create_profile_region(ProfileRegionReference profile) {
  return Result<TrimmingReference>::success(TrimmingReference(std::move(profile)));
}
TrimmingReference::TrimmingReference(
    std::variant<BoundaryCurveReference, ProfileRegionReference> source)
    : source_(std::move(source)) {}
TrimmingReferenceSourceKind TrimmingReference::source_kind() const noexcept {
  return std::holds_alternative<BoundaryCurveReference>(source_)
             ? TrimmingReferenceSourceKind::BoundaryCurve
             : TrimmingReferenceSourceKind::ProfileRegion;
}
const std::variant<BoundaryCurveReference, ProfileRegionReference>&
TrimmingReference::source() const noexcept {
  return source_;
}
std::string TrimmingReference::source_node_id() const {
  return source_kind() == TrimmingReferenceSourceKind::BoundaryCurve
             ? std::get<BoundaryCurveReference>(source_).source_node_id()
             : std::get<ProfileRegionReference>(source_).source_node_id();
}

#define BLCAD_SURFACE_ACCESSORS(Type)                                                              \
  const FeatureId& Type::id() const noexcept {                                                     \
    return id_;                                                                                    \
  }                                                                                                \
  const std::string& Type::name() const noexcept {                                                 \
    return name_;                                                                                  \
  }                                                                                                \
  const BodyId& Type::result_body() const noexcept {                                               \
    return result_body_;                                                                           \
  }

Result<BoundarySurfaceFeature>
BoundarySurfaceFeature::create(FeatureId id, std::string name,
                               std::vector<BoundaryCurveReference> boundaries, BodyId result) {
  const auto object = id.empty() ? "boundary_surface" : id.value();
  if (id.empty() || name.empty() || result.empty() || boundaries.size() < 2U ||
      boundaries.size() > 4U)
    return invalid<BoundarySurfaceFeature>(
        object, "boundary surface requires id, name, result body, and two to four boundaries");
  if (!unique_references(boundaries))
    return invalid<BoundarySurfaceFeature>(object, "boundary surface boundaries must be unique");
  return Result<BoundarySurfaceFeature>::success(BoundarySurfaceFeature(
      std::move(id), std::move(name), std::move(boundaries), std::move(result)));
}
BoundarySurfaceFeature::BoundarySurfaceFeature(FeatureId id, std::string name,
                                               std::vector<BoundaryCurveReference> boundaries,
                                               BodyId result)
    : id_(std::move(id)), name_(std::move(name)), boundaries_(std::move(boundaries)),
      result_body_(std::move(result)) {}
BLCAD_SURFACE_ACCESSORS(BoundarySurfaceFeature)
const std::vector<BoundaryCurveReference>& BoundarySurfaceFeature::boundaries() const noexcept {
  return boundaries_;
}

Result<FillSurfaceFeature>
FillSurfaceFeature::create(FeatureId id, std::string name,
                           std::vector<BoundaryCurveReference> boundaries, BodyId result) {
  const auto object = id.empty() ? "fill_surface" : id.value();
  if (id.empty() || name.empty() || result.empty() || boundaries.empty())
    return invalid<FillSurfaceFeature>(
        object, "fill surface requires id, name, result body, and at least one boundary");
  if (!unique_references(boundaries))
    return invalid<FillSurfaceFeature>(object, "fill surface boundaries must be unique");
  return Result<FillSurfaceFeature>::success(
      FillSurfaceFeature(std::move(id), std::move(name), std::move(boundaries), std::move(result)));
}
FillSurfaceFeature::FillSurfaceFeature(FeatureId id, std::string name,
                                       std::vector<BoundaryCurveReference> boundaries,
                                       BodyId result)
    : id_(std::move(id)), name_(std::move(name)), boundaries_(std::move(boundaries)),
      result_body_(std::move(result)) {}
BLCAD_SURFACE_ACCESSORS(FillSurfaceFeature)
const std::vector<BoundaryCurveReference>& FillSurfaceFeature::boundaries() const noexcept {
  return boundaries_;
}

Result<TrimSurfaceFeature> TrimSurfaceFeature::create(FeatureId id, std::string name,
                                                      SurfaceReference target,
                                                      TrimmingReference trimming, BodyId result) {
  const auto object = id.empty() ? "trim_surface" : id.value();
  if (id.empty() || name.empty() || result.empty())
    return invalid<TrimSurfaceFeature>(
        object, "trim surface requires id, name, target, trimming reference, and result body");
  return Result<TrimSurfaceFeature>::success(TrimSurfaceFeature(
      std::move(id), std::move(name), std::move(target), std::move(trimming), std::move(result)));
}
TrimSurfaceFeature::TrimSurfaceFeature(FeatureId id, std::string name, SurfaceReference target,
                                       TrimmingReference trimming, BodyId result)
    : id_(std::move(id)), name_(std::move(name)), target_(std::move(target)),
      trimming_(std::move(trimming)), result_body_(std::move(result)) {}
BLCAD_SURFACE_ACCESSORS(TrimSurfaceFeature)
const SurfaceReference& TrimSurfaceFeature::target() const noexcept {
  return target_;
}
const TrimmingReference& TrimSurfaceFeature::trimming() const noexcept {
  return trimming_;
}

Result<ExtendSurfaceFeature> ExtendSurfaceFeature::create(FeatureId id, std::string name,
                                                          SurfaceReference target,
                                                          BoundaryCurveReference boundary,
                                                          ParameterId distance, BodyId result) {
  const auto object = id.empty() ? "extend_surface" : id.value();
  if (id.empty() || name.empty() || distance.empty() || result.empty())
    return invalid<ExtendSurfaceFeature>(
        object, "extend surface requires id, name, target, boundary, distance, and result body");
  return Result<ExtendSurfaceFeature>::success(
      ExtendSurfaceFeature(std::move(id), std::move(name), std::move(target), std::move(boundary),
                           std::move(distance), std::move(result)));
}
ExtendSurfaceFeature::ExtendSurfaceFeature(FeatureId id, std::string name, SurfaceReference target,
                                           BoundaryCurveReference boundary, ParameterId distance,
                                           BodyId result)
    : id_(std::move(id)), name_(std::move(name)), target_(std::move(target)),
      boundary_(std::move(boundary)), distance_parameter_(std::move(distance)),
      result_body_(std::move(result)) {}
BLCAD_SURFACE_ACCESSORS(ExtendSurfaceFeature)
const SurfaceReference& ExtendSurfaceFeature::target() const noexcept {
  return target_;
}
const BoundaryCurveReference& ExtendSurfaceFeature::boundary() const noexcept {
  return boundary_;
}
const ParameterId& ExtendSurfaceFeature::distance_parameter() const noexcept {
  return distance_parameter_;
}

Result<SurfaceStitchFeature> SurfaceStitchFeature::create(FeatureId id, std::string name,
                                                          std::vector<SurfaceReference> surfaces,
                                                          ParameterId tolerance, BodyId result) {
  const auto object = id.empty() ? "surface_stitch" : id.value();
  if (id.empty() || name.empty() || tolerance.empty() || result.empty() || surfaces.size() < 2U)
    return invalid<SurfaceStitchFeature>(
        object,
        "surface stitch requires id, name, result body, tolerance, and at least two surfaces");
  if (!unique_references(surfaces))
    return invalid<SurfaceStitchFeature>(object, "surface stitch inputs must be unique");
  return Result<SurfaceStitchFeature>::success(
      SurfaceStitchFeature(std::move(id), std::move(name), std::move(surfaces),
                           std::move(tolerance), std::move(result)));
}
SurfaceStitchFeature::SurfaceStitchFeature(FeatureId id, std::string name,
                                           std::vector<SurfaceReference> surfaces,
                                           ParameterId tolerance, BodyId result)
    : id_(std::move(id)), name_(std::move(name)), surfaces_(std::move(surfaces)),
      tolerance_parameter_(std::move(tolerance)), result_body_(std::move(result)) {}
BLCAD_SURFACE_ACCESSORS(SurfaceStitchFeature)
const std::vector<SurfaceReference>& SurfaceStitchFeature::surfaces() const noexcept {
  return surfaces_;
}
const ParameterId& SurfaceStitchFeature::tolerance_parameter() const noexcept {
  return tolerance_parameter_;
}

Result<ClosedShellToSolidFeature> ClosedShellToSolidFeature::create(FeatureId id, std::string name,
                                                                    SurfaceReference shell,
                                                                    BodyId result) {
  const auto object = id.empty() ? "closed_shell_to_solid" : id.value();
  if (id.empty() || name.empty() || result.empty())
    return invalid<ClosedShellToSolidFeature>(
        object, "closed-shell conversion requires id, name, shell, and result body");
  return Result<ClosedShellToSolidFeature>::success(ClosedShellToSolidFeature(
      std::move(id), std::move(name), std::move(shell), std::move(result)));
}
ClosedShellToSolidFeature::ClosedShellToSolidFeature(FeatureId id, std::string name,
                                                     SurfaceReference shell, BodyId result)
    : id_(std::move(id)), name_(std::move(name)), shell_(std::move(shell)),
      result_body_(std::move(result)) {}
BLCAD_SURFACE_ACCESSORS(ClosedShellToSolidFeature)
const SurfaceReference& ClosedShellToSolidFeature::shell() const noexcept {
  return shell_;
}

#undef BLCAD_SURFACE_ACCESSORS

const FeatureId& surface_feature_id(const SurfaceFeature& feature) noexcept {
  return std::visit([](const auto& value) -> const FeatureId& { return id_of(value); }, feature);
}
const std::string& surface_feature_name(const SurfaceFeature& feature) noexcept {
  return std::visit([](const auto& value) -> const std::string& { return name_of(value); },
                    feature);
}
SurfaceFeatureKind surface_feature_kind(const SurfaceFeature& feature) noexcept {
  return std::visit(
      [](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, BoundarySurfaceFeature>)
          return SurfaceFeatureKind::BoundarySurface;
        if constexpr (std::is_same_v<T, FillSurfaceFeature>)
          return SurfaceFeatureKind::FillSurface;
        if constexpr (std::is_same_v<T, TrimSurfaceFeature>)
          return SurfaceFeatureKind::TrimSurface;
        if constexpr (std::is_same_v<T, ExtendSurfaceFeature>)
          return SurfaceFeatureKind::ExtendSurface;
        if constexpr (std::is_same_v<T, SurfaceStitchFeature>)
          return SurfaceFeatureKind::SurfaceStitch;
        if constexpr (std::is_same_v<T, SurfaceOffsetFeature>)
          return value.kind();
        return SurfaceFeatureKind::ClosedShellToSolid;
      },
      feature);
}
const BodyId& surface_feature_result_body(const SurfaceFeature& feature) noexcept {
  return std::visit([](const auto& value) -> const BodyId& { return result_of(value); }, feature);
}
std::vector<std::string> surface_feature_input_nodes(const SurfaceFeature& feature) {
  return std::visit(
      [](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        std::vector<std::string> nodes;
        if constexpr (std::is_same_v<T, BoundarySurfaceFeature> ||
                      std::is_same_v<T, FillSurfaceFeature>)
          for (const auto& boundary : value.boundaries())
            nodes.push_back(boundary.source_node_id());
        else if constexpr (std::is_same_v<T, TrimSurfaceFeature>) {
          nodes = value.target().source_nodes();
          nodes.push_back(value.trimming().source_node_id());
          if (value.options())
            nodes.push_back(value.options()->tolerance.value());
        } else if constexpr (std::is_same_v<T, ExtendSurfaceFeature>) {
          nodes = value.target().source_nodes();
          nodes.push_back(value.boundary().source_node_id());
          nodes.push_back(value.distance_parameter().value());
          if (value.options()) {
            nodes.push_back(value.options()->tolerance.value());
            if (value.options()->maximum_deviation)
              nodes.push_back(value.options()->maximum_deviation->value());
          }
        } else if constexpr (std::is_same_v<T, SurfaceStitchFeature>) {
          for (const auto& surface : value.surfaces()) {
            const auto sources = surface.source_nodes();
            nodes.insert(nodes.end(), sources.begin(), sources.end());
          }
          nodes.push_back(value.tolerance_parameter().value());
        } else if constexpr (std::is_same_v<T, SurfaceOffsetFeature>) {
          nodes = value.target().source_nodes();
          nodes.push_back(value.parameters().distance.value());
          nodes.push_back(value.parameters().tolerance.value());
          if (value.parameters().maximum_deviation)
            nodes.push_back(value.parameters().maximum_deviation->value());
        } else
          nodes = value.shell().source_nodes();
        return nodes;
      },
      feature);
}

} // namespace blcad
