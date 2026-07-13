#include "blcad/core/part_feature_input_reference.hpp"

#include "blcad/core/error.hpp"
#include "blcad/core/part_document.hpp"

#include <type_traits>
#include <utility>

namespace blcad {
namespace {

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
}

[[nodiscard]] Result<std::size_t> validate_role(PartFeatureInputRole role,
                                                PartFeatureInputCapability capability,
                                                std::string_view object_id) {
  if (!part_feature_input_role_accepts(role, capability))
    return Result<std::size_t>::failure(validation_error(
        std::string(object_id), "feature input role does not accept the expected capability"));
  return Result<std::size_t>::success(1U);
}

[[nodiscard]] Result<std::size_t> validate_feature_source(const PartDocument& document,
                                                          const FeatureId& feature_id,
                                                          std::string_view context) {
  if (document.find_feature(feature_id) == nullptr &&
      document.find_revolve_feature(feature_id) == nullptr)
    return Result<std::size_t>::failure(
        validation_error(feature_id.value(), std::string(context) + " source feature must exist"));
  return Result<std::size_t>::success(1U);
}

} // namespace

std::string_view to_string(PartFeatureInputCapability capability) noexcept {
  switch (capability) {
  case PartFeatureInputCapability::ProfileRegion:
    return "profile_region";
  case PartFeatureInputCapability::Axis:
    return "axis";
  case PartFeatureInputCapability::Line:
    return "line";
  case PartFeatureInputCapability::Plane:
    return "plane";
  case PartFeatureInputCapability::Face:
    return "face";
  case PartFeatureInputCapability::Edge:
    return "edge";
  case PartFeatureInputCapability::Body:
    return "body";
  }
  return "profile_region";
}

std::string_view to_string(PartFeatureInputRole role) noexcept {
  switch (role) {
  case PartFeatureInputRole::ProfileRegion:
    return "profile_region";
  case PartFeatureInputRole::RevolveProfile:
    return "revolve_profile";
  case PartFeatureInputRole::SweepProfile:
    return "sweep_profile";
  case PartFeatureInputRole::LoftSection:
    return "loft_section";
  case PartFeatureInputRole::RevolveAxis:
    return "revolve_axis";
  case PartFeatureInputRole::PatternAxis:
    return "pattern_axis";
  case PartFeatureInputRole::DraftPullDirection:
    return "draft_pull_direction";
  case PartFeatureInputRole::MirrorPlane:
    return "mirror_plane";
  case PartFeatureInputRole::DraftNeutralPlane:
    return "draft_neutral_plane";
  case PartFeatureInputRole::ExtrudeLimitFace:
    return "extrude_limit_face";
  case PartFeatureInputRole::FilletEdge:
    return "fillet_edge";
  case PartFeatureInputRole::ChamferEdge:
    return "chamfer_edge";
  case PartFeatureInputRole::ShellRemovalFace:
    return "shell_removal_face";
  case PartFeatureInputRole::DraftFace:
    return "draft_face";
  case PartFeatureInputRole::TargetBody:
    return "target_body";
  case PartFeatureInputRole::ToolBody:
    return "tool_body";
  case PartFeatureInputRole::SourceBody:
    return "source_body";
  }
  return "profile_region";
}

std::string_view to_string(PartFeatureInputSourceKind source_kind) noexcept {
  switch (source_kind) {
  case PartFeatureInputSourceKind::SketchProfileRegion:
    return "sketch_profile_region";
  case PartFeatureInputSourceKind::DatumAxis:
    return "datum_axis";
  case PartFeatureInputSourceKind::ConstructionLine:
    return "construction_line";
  case PartFeatureInputSourceKind::SemanticAxis:
    return "semantic_axis";
  case PartFeatureInputSourceKind::SemanticLinearEdge:
    return "semantic_linear_edge";
  case PartFeatureInputSourceKind::DatumPlane:
    return "datum_plane";
  case PartFeatureInputSourceKind::ConstructionPlane:
    return "construction_plane";
  case PartFeatureInputSourceKind::SemanticPlanarFace:
    return "semantic_planar_face";
  case PartFeatureInputSourceKind::SemanticCylindricalFace:
    return "semantic_cylindrical_face";
  case PartFeatureInputSourceKind::SemanticCircularEdge:
    return "semantic_circular_edge";
  case PartFeatureInputSourceKind::Body:
    return "body";
  }
  return "sketch_profile_region";
}

bool part_feature_input_role_accepts(PartFeatureInputRole role,
                                     PartFeatureInputCapability capability) noexcept {
  switch (role) {
  case PartFeatureInputRole::ProfileRegion:
  case PartFeatureInputRole::RevolveProfile:
  case PartFeatureInputRole::SweepProfile:
  case PartFeatureInputRole::LoftSection:
    return capability == PartFeatureInputCapability::ProfileRegion;
  case PartFeatureInputRole::RevolveAxis:
  case PartFeatureInputRole::PatternAxis:
    return capability == PartFeatureInputCapability::Axis;
  case PartFeatureInputRole::DraftPullDirection:
    return capability == PartFeatureInputCapability::Axis ||
           capability == PartFeatureInputCapability::Line;
  case PartFeatureInputRole::MirrorPlane:
  case PartFeatureInputRole::DraftNeutralPlane:
    return capability == PartFeatureInputCapability::Plane;
  case PartFeatureInputRole::ExtrudeLimitFace:
  case PartFeatureInputRole::ShellRemovalFace:
  case PartFeatureInputRole::DraftFace:
    return capability == PartFeatureInputCapability::Face;
  case PartFeatureInputRole::FilletEdge:
  case PartFeatureInputRole::ChamferEdge:
    return capability == PartFeatureInputCapability::Edge;
  case PartFeatureInputRole::TargetBody:
  case PartFeatureInputRole::ToolBody:
  case PartFeatureInputRole::SourceBody:
    return capability == PartFeatureInputCapability::Body;
  }
  return false;
}

Result<ProfileRegionReference> ProfileRegionReference::create(SketchId sketch, ProfileId profile,
                                                              PartFeatureInputRole role) {
  const std::string object_id = profile.empty() ? "profile_region" : profile.value();
  if (sketch.empty() || profile.empty())
    return Result<ProfileRegionReference>::failure(
        validation_error(object_id, "profile region sketch and profile ids must not be empty"));
  auto valid = validate_role(role, PartFeatureInputCapability::ProfileRegion, object_id);
  if (valid.has_error())
    return Result<ProfileRegionReference>::failure(valid.error());
  return Result<ProfileRegionReference>::success(
      ProfileRegionReference(std::move(sketch), std::move(profile), role));
}

ProfileRegionReference::ProfileRegionReference(SketchId sketch, ProfileId profile,
                                               PartFeatureInputRole role)
    : sketch_(std::move(sketch)), profile_(std::move(profile)), role_(role) {}
const SketchId& ProfileRegionReference::sketch() const noexcept {
  return sketch_;
}
const ProfileId& ProfileRegionReference::profile() const noexcept {
  return profile_;
}
PartFeatureInputRole ProfileRegionReference::role() const noexcept {
  return role_;
}
PartFeatureInputCapability ProfileRegionReference::expected_capability() const noexcept {
  return PartFeatureInputCapability::ProfileRegion;
}
PartFeatureInputSourceKind ProfileRegionReference::source_kind() const noexcept {
  return PartFeatureInputSourceKind::SketchProfileRegion;
}
std::string ProfileRegionReference::source_node_id() const {
  return sketch_.value();
}

AxisReference::AxisReference(PartFeatureInputRole role,
                             PartFeatureInputCapability expected_capability,
                             AxisReferenceSource source)
    : role_(role), expected_capability_(expected_capability), source_(std::move(source)) {}

Result<AxisReference> AxisReference::create_datum_axis(PartFeatureInputRole role,
                                                       DatumAxisId axis) {
  if (axis.empty())
    return Result<AxisReference>::failure(
        validation_error("axis_reference", "datum axis id must not be empty"));
  auto valid = validate_role(role, PartFeatureInputCapability::Axis, axis.value());
  if (valid.has_error())
    return Result<AxisReference>::failure(valid.error());
  return Result<AxisReference>::success(
      AxisReference(role, PartFeatureInputCapability::Axis, std::move(axis)));
}

Result<AxisReference>
AxisReference::create_construction_line(PartFeatureInputRole role, ConstructionLineId line,
                                        PartFeatureInputCapability expected_capability) {
  if (line.empty())
    return Result<AxisReference>::failure(
        validation_error("axis_reference", "construction line id must not be empty"));
  if (expected_capability != PartFeatureInputCapability::Axis &&
      expected_capability != PartFeatureInputCapability::Line)
    return Result<AxisReference>::failure(validation_error(
        line.value(), "construction line may provide only Axis or Line capability"));
  auto valid = validate_role(role, expected_capability, line.value());
  if (valid.has_error())
    return Result<AxisReference>::failure(valid.error());
  return Result<AxisReference>::success(AxisReference(role, expected_capability, std::move(line)));
}

Result<AxisReference> AxisReference::create_semantic_axis(PartFeatureInputRole role,
                                                          SemanticAxisReference axis) {
  auto valid = validate_role(role, PartFeatureInputCapability::Axis, axis.source_feature().value());
  if (valid.has_error())
    return Result<AxisReference>::failure(valid.error());
  return Result<AxisReference>::success(
      AxisReference(role, PartFeatureInputCapability::Axis, std::move(axis)));
}

Result<AxisReference>
AxisReference::create_semantic_edge(PartFeatureInputRole role, SemanticEdgeReference edge,
                                    PartFeatureInputCapability expected_capability) {
  if (expected_capability != PartFeatureInputCapability::Axis &&
      expected_capability != PartFeatureInputCapability::Line)
    return Result<AxisReference>::failure(
        validation_error(edge.source_feature().value(),
                         "semantic linear edge may provide only Axis or Line capability"));
  auto valid = validate_role(role, expected_capability, edge.source_feature().value());
  if (valid.has_error())
    return Result<AxisReference>::failure(valid.error());
  return Result<AxisReference>::success(AxisReference(role, expected_capability, std::move(edge)));
}

PartFeatureInputRole AxisReference::role() const noexcept {
  return role_;
}
PartFeatureInputCapability AxisReference::expected_capability() const noexcept {
  return expected_capability_;
}
const AxisReferenceSource& AxisReference::source() const noexcept {
  return source_;
}
PartFeatureInputSourceKind AxisReference::source_kind() const noexcept {
  if (std::holds_alternative<DatumAxisId>(source_))
    return PartFeatureInputSourceKind::DatumAxis;
  if (std::holds_alternative<ConstructionLineId>(source_))
    return PartFeatureInputSourceKind::ConstructionLine;
  if (std::holds_alternative<SemanticAxisReference>(source_))
    return PartFeatureInputSourceKind::SemanticAxis;
  return PartFeatureInputSourceKind::SemanticLinearEdge;
}
std::string AxisReference::source_node_id() const {
  return std::visit(
      [](const auto& source) -> std::string {
        using T = std::decay_t<decltype(source)>;
        if constexpr (std::is_same_v<T, DatumAxisId> || std::is_same_v<T, ConstructionLineId>)
          return source.value();
        else
          return source.source_feature().value();
      },
      source_);
}

PlaneReference::PlaneReference(PartFeatureInputRole role, PlaneReferenceSource source)
    : role_(role), source_(std::move(source)) {}
Result<PlaneReference> PlaneReference::create_datum_plane(PartFeatureInputRole role,
                                                          DatumPlaneId plane) {
  if (plane.empty())
    return Result<PlaneReference>::failure(
        validation_error("plane_reference", "datum plane id must not be empty"));
  auto valid = validate_role(role, PartFeatureInputCapability::Plane, plane.value());
  if (valid.has_error())
    return Result<PlaneReference>::failure(valid.error());
  return Result<PlaneReference>::success(PlaneReference(role, std::move(plane)));
}
Result<PlaneReference> PlaneReference::create_construction_plane(PartFeatureInputRole role,
                                                                 ConstructionPlaneId plane) {
  if (plane.empty())
    return Result<PlaneReference>::failure(
        validation_error("plane_reference", "construction plane id must not be empty"));
  auto valid = validate_role(role, PartFeatureInputCapability::Plane, plane.value());
  if (valid.has_error())
    return Result<PlaneReference>::failure(valid.error());
  return Result<PlaneReference>::success(PlaneReference(role, std::move(plane)));
}
Result<PlaneReference> PlaneReference::create_semantic_face(PartFeatureInputRole role,
                                                            SemanticFaceReference face) {
  auto valid =
      validate_role(role, PartFeatureInputCapability::Plane, face.source_feature().value());
  if (valid.has_error())
    return Result<PlaneReference>::failure(valid.error());
  return Result<PlaneReference>::success(PlaneReference(role, std::move(face)));
}
PartFeatureInputRole PlaneReference::role() const noexcept {
  return role_;
}
PartFeatureInputCapability PlaneReference::expected_capability() const noexcept {
  return PartFeatureInputCapability::Plane;
}
const PlaneReferenceSource& PlaneReference::source() const noexcept {
  return source_;
}
PartFeatureInputSourceKind PlaneReference::source_kind() const noexcept {
  if (std::holds_alternative<DatumPlaneId>(source_))
    return PartFeatureInputSourceKind::DatumPlane;
  if (std::holds_alternative<ConstructionPlaneId>(source_))
    return PartFeatureInputSourceKind::ConstructionPlane;
  return PartFeatureInputSourceKind::SemanticPlanarFace;
}
std::string PlaneReference::source_node_id() const {
  return std::visit(
      [](const auto& source) -> std::string {
        using T = std::decay_t<decltype(source)>;
        if constexpr (std::is_same_v<T, SemanticFaceReference>)
          return source.source_feature().value();
        else
          return source.value();
      },
      source_);
}

FaceReference::FaceReference(PartFeatureInputRole role, FaceReferenceSource source)
    : role_(role), source_(std::move(source)) {}
Result<FaceReference> FaceReference::create_planar(PartFeatureInputRole role,
                                                   SemanticFaceReference face) {
  auto valid = validate_role(role, PartFeatureInputCapability::Face, face.source_feature().value());
  if (valid.has_error())
    return Result<FaceReference>::failure(valid.error());
  return Result<FaceReference>::success(FaceReference(role, std::move(face)));
}
Result<FaceReference> FaceReference::create_cylindrical(PartFeatureInputRole role,
                                                        SemanticCylindricalFaceReference face) {
  auto valid = validate_role(role, PartFeatureInputCapability::Face, face.source_feature().value());
  if (valid.has_error())
    return Result<FaceReference>::failure(valid.error());
  return Result<FaceReference>::success(FaceReference(role, std::move(face)));
}
PartFeatureInputRole FaceReference::role() const noexcept {
  return role_;
}
PartFeatureInputCapability FaceReference::expected_capability() const noexcept {
  return PartFeatureInputCapability::Face;
}
const FaceReferenceSource& FaceReference::source() const noexcept {
  return source_;
}
PartFeatureInputSourceKind FaceReference::source_kind() const noexcept {
  return std::holds_alternative<SemanticFaceReference>(source_)
             ? PartFeatureInputSourceKind::SemanticPlanarFace
             : PartFeatureInputSourceKind::SemanticCylindricalFace;
}
std::string FaceReference::source_node_id() const {
  return std::visit([](const auto& source) { return source.source_feature().value(); }, source_);
}

EdgeReference::EdgeReference(PartFeatureInputRole role, EdgeReferenceSource source)
    : role_(role), source_(std::move(source)) {}
Result<EdgeReference> EdgeReference::create_linear(PartFeatureInputRole role,
                                                   SemanticEdgeReference edge) {
  auto valid = validate_role(role, PartFeatureInputCapability::Edge, edge.source_feature().value());
  if (valid.has_error())
    return Result<EdgeReference>::failure(valid.error());
  return Result<EdgeReference>::success(EdgeReference(role, std::move(edge)));
}
Result<EdgeReference> EdgeReference::create_circular(PartFeatureInputRole role,
                                                     SemanticCircularEdgeReference edge) {
  auto valid = validate_role(role, PartFeatureInputCapability::Edge, edge.source_feature().value());
  if (valid.has_error())
    return Result<EdgeReference>::failure(valid.error());
  return Result<EdgeReference>::success(EdgeReference(role, std::move(edge)));
}
PartFeatureInputRole EdgeReference::role() const noexcept {
  return role_;
}
PartFeatureInputCapability EdgeReference::expected_capability() const noexcept {
  return PartFeatureInputCapability::Edge;
}
const EdgeReferenceSource& EdgeReference::source() const noexcept {
  return source_;
}
PartFeatureInputSourceKind EdgeReference::source_kind() const noexcept {
  return std::holds_alternative<SemanticEdgeReference>(source_)
             ? PartFeatureInputSourceKind::SemanticLinearEdge
             : PartFeatureInputSourceKind::SemanticCircularEdge;
}
std::string EdgeReference::source_node_id() const {
  return std::visit([](const auto& source) { return source.source_feature().value(); }, source_);
}

BodyReference::BodyReference(PartFeatureInputRole role, BodyId body)
    : role_(role), body_(std::move(body)) {}
Result<BodyReference> BodyReference::create(PartFeatureInputRole role, BodyId body) {
  if (body.empty())
    return Result<BodyReference>::failure(
        validation_error("body_reference", "body id must not be empty"));
  auto valid = validate_role(role, PartFeatureInputCapability::Body, body.value());
  if (valid.has_error())
    return Result<BodyReference>::failure(valid.error());
  return Result<BodyReference>::success(BodyReference(role, std::move(body)));
}
PartFeatureInputRole BodyReference::role() const noexcept {
  return role_;
}
PartFeatureInputCapability BodyReference::expected_capability() const noexcept {
  return PartFeatureInputCapability::Body;
}
PartFeatureInputSourceKind BodyReference::source_kind() const noexcept {
  return PartFeatureInputSourceKind::Body;
}
const BodyId& BodyReference::body() const noexcept {
  return body_;
}
std::string BodyReference::source_node_id() const {
  return "body:" + body_.value();
}

Result<std::size_t> validate_part_feature_input_reference(const PartDocument& document,
                                                          const ProfileRegionReference& reference) {
  const Sketch* sketch = document.find_sketch(reference.sketch());
  if (sketch == nullptr)
    return Result<std::size_t>::failure(
        validation_error(reference.sketch().value(), "profile region sketch must exist"));
  const bool has_profile = sketch->find_rectangle_profile(reference.profile()) != nullptr ||
                           sketch->find_circle_profile(reference.profile()) != nullptr ||
                           sketch->find_closed_profile(reference.profile()) != nullptr ||
                           sketch->find_arc_closed_profile(reference.profile()) != nullptr ||
                           sketch->find_composite_closed_profile(reference.profile()) != nullptr ||
                           sketch->find_circular_hole_pattern(reference.profile()) != nullptr;
  if (!has_profile)
    return Result<std::size_t>::failure(validation_error(
        reference.profile().value(), "profile region profile must belong to referenced sketch"));
  return Result<std::size_t>::success(1U);
}

Result<std::size_t> validate_part_feature_input_reference(const PartDocument& document,
                                                          const AxisReference& reference) {
  return std::visit(
      [&](const auto& source) -> Result<std::size_t> {
        using T = std::decay_t<decltype(source)>;
        if constexpr (std::is_same_v<T, DatumAxisId>) {
          return document.find_datum_axis(source) != nullptr
                     ? Result<std::size_t>::success(1U)
                     : Result<std::size_t>::failure(
                           validation_error(source.value(), "datum axis source must exist"));
        } else if constexpr (std::is_same_v<T, ConstructionLineId>) {
          return document.find_construction_line(source) != nullptr
                     ? Result<std::size_t>::success(1U)
                     : Result<std::size_t>::failure(
                           validation_error(source.value(), "construction line source must exist"));
        } else if constexpr (std::is_same_v<T, SemanticAxisReference>) {
          return validate_feature_source(document, source.source_feature(), "semantic axis");
        } else {
          return validate_generated_topology_reference(document,
                                                       GeneratedTopologyReferenceIdentity(source));
        }
      },
      reference.source());
}

Result<std::size_t> validate_part_feature_input_reference(const PartDocument& document,
                                                          const PlaneReference& reference) {
  return std::visit(
      [&](const auto& source) -> Result<std::size_t> {
        using T = std::decay_t<decltype(source)>;
        if constexpr (std::is_same_v<T, DatumPlaneId>)
          return document.find_datum_plane(source) != nullptr
                     ? Result<std::size_t>::success(1U)
                     : Result<std::size_t>::failure(
                           validation_error(source.value(), "datum plane source must exist"));
        else if constexpr (std::is_same_v<T, ConstructionPlaneId>)
          return document.find_construction_plane(source) != nullptr
                     ? Result<std::size_t>::success(1U)
                     : Result<std::size_t>::failure(validation_error(
                           source.value(), "construction plane source must exist"));
        else
          return validate_feature_source(document, source.source_feature(), "semantic planar face");
      },
      reference.source());
}

Result<std::size_t> validate_part_feature_input_reference(const PartDocument& document,
                                                          const FaceReference& reference) {
  return std::visit(
      [&](const auto& source) -> Result<std::size_t> {
        using T = std::decay_t<decltype(source)>;
        if constexpr (std::is_same_v<T, SemanticFaceReference>)
          return validate_feature_source(document, source.source_feature(), "semantic planar face");
        else
          return validate_generated_topology_reference(document,
                                                       GeneratedTopologyReferenceIdentity(source));
      },
      reference.source());
}

Result<std::size_t> validate_part_feature_input_reference(const PartDocument& document,
                                                          const EdgeReference& reference) {
  return std::visit(
      [&](const auto& source) {
        return validate_generated_topology_reference(document,
                                                     GeneratedTopologyReferenceIdentity(source));
      },
      reference.source());
}

Result<std::size_t> validate_part_feature_input_reference(const PartDocument& document,
                                                          const BodyReference& reference) {
  return document.find_body(reference.body()) != nullptr
             ? Result<std::size_t>::success(1U)
             : Result<std::size_t>::failure(
                   validation_error(reference.body().value(), "body source must exist"));
}

} // namespace blcad
