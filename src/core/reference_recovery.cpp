#include "blcad/core/reference_recovery.hpp"

#include "blcad/core/feature.hpp"
#include "blcad/core/part_document.hpp"

#include <string>
#include <utility>

namespace blcad {
namespace {

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
}

[[nodiscard]] Result<SemanticReferenceTarget> create_target(FeatureId source_feature,
                                                            SemanticReferenceKind kind,
                                                            std::optional<SemanticFace> face,
                                                            std::optional<SemanticEdge> edge,
                                                            std::optional<SemanticVertex> vertex) {
  const auto object_id = source_feature.empty() ? std::string("semantic_reference")
                                                : source_feature.value();
  if (source_feature.empty()) {
    return Result<SemanticReferenceTarget>::failure(
        validation_error(object_id, "semantic reference source feature id must not be empty"));
  }

  return Result<SemanticReferenceTarget>::success(
      SemanticReferenceTarget(std::move(source_feature), kind, face, edge, vertex));
}

[[nodiscard]] bool target_exists_in_current_seed(const PartDocument& document,
                                                 const SemanticReferenceTarget& target) noexcept {
  const Feature* feature = document.find_feature(target.source_feature());
  return feature != nullptr && feature->type() == FeatureType::AdditiveExtrude;
}

} // namespace

std::string_view to_string(SemanticReferenceKind kind) noexcept {
  switch (kind) {
  case SemanticReferenceKind::Face: return "face";
  case SemanticReferenceKind::Edge: return "edge";
  case SemanticReferenceKind::Vertex: return "vertex";
  }
  return "unknown";
}

std::string_view to_string(ReferenceStatusKind status) noexcept {
  switch (status) {
  case ReferenceStatusKind::Resolved: return "resolved";
  case ReferenceStatusKind::Lost: return "lost";
  }
  return "unknown";
}

Result<SemanticReferenceTarget> SemanticReferenceTarget::create_face(FeatureId source_feature,
                                                                     SemanticFace face) {
  return create_target(std::move(source_feature), SemanticReferenceKind::Face, face, std::nullopt,
                       std::nullopt);
}

Result<SemanticReferenceTarget> SemanticReferenceTarget::create_edge(FeatureId source_feature,
                                                                     SemanticEdge edge) {
  return create_target(std::move(source_feature), SemanticReferenceKind::Edge, std::nullopt, edge,
                       std::nullopt);
}

Result<SemanticReferenceTarget> SemanticReferenceTarget::create_vertex(FeatureId source_feature,
                                                                       SemanticVertex vertex) {
  return create_target(std::move(source_feature), SemanticReferenceKind::Vertex, std::nullopt,
                       std::nullopt, vertex);
}

SemanticReferenceKind SemanticReferenceTarget::kind() const noexcept { return kind_; }
const FeatureId& SemanticReferenceTarget::source_feature() const noexcept { return source_feature_; }
const std::optional<SemanticFace>& SemanticReferenceTarget::face() const noexcept { return face_; }
const std::optional<SemanticEdge>& SemanticReferenceTarget::edge() const noexcept { return edge_; }
const std::optional<SemanticVertex>& SemanticReferenceTarget::vertex() const noexcept { return vertex_; }

std::string SemanticReferenceTarget::node_id() const {
  switch (kind_) {
  case SemanticReferenceKind::Face:
    return source_feature_.value() + ".face." + std::string(to_string(face_.value()));
  case SemanticReferenceKind::Edge:
    return source_feature_.value() + ".edge." + std::string(to_string(edge_.value()));
  case SemanticReferenceKind::Vertex:
    return source_feature_.value() + ".vertex." + std::string(to_string(vertex_.value()));
  }
  return source_feature_.value() + ".unknown";
}

SemanticReferenceTarget::SemanticReferenceTarget(FeatureId source_feature, SemanticReferenceKind kind,
                                                 std::optional<SemanticFace> face,
                                                 std::optional<SemanticEdge> edge,
                                                 std::optional<SemanticVertex> vertex)
    : source_feature_(std::move(source_feature)), kind_(kind), face_(face), edge_(edge),
      vertex_(vertex) {}

Result<ReferenceStatusRecord> ReferenceStatusRecord::create_resolved(ReferenceStatusId id,
                                                                      SemanticReferenceTarget target) {
  const auto object_id = id.empty() ? std::string("reference_status") : id.value();
  if (id.empty()) {
    return Result<ReferenceStatusRecord>::failure(
        validation_error(object_id, "reference status id must not be empty"));
  }
  return Result<ReferenceStatusRecord>::success(ReferenceStatusRecord(
      std::move(id), std::move(target), ReferenceStatusKind::Resolved, {}));
}

Result<ReferenceStatusRecord> ReferenceStatusRecord::create_lost(ReferenceStatusId id,
                                                                  SemanticReferenceTarget target,
                                                                  std::string message) {
  const auto object_id = id.empty() ? std::string("reference_status") : id.value();
  if (id.empty()) {
    return Result<ReferenceStatusRecord>::failure(
        validation_error(object_id, "reference status id must not be empty"));
  }
  if (message.empty()) {
    return Result<ReferenceStatusRecord>::failure(
        validation_error(object_id, "lost reference status message must not be empty"));
  }
  return Result<ReferenceStatusRecord>::success(ReferenceStatusRecord(
      std::move(id), std::move(target), ReferenceStatusKind::Lost, std::move(message)));
}

const ReferenceStatusId& ReferenceStatusRecord::id() const noexcept { return id_; }
ReferenceStatusKind ReferenceStatusRecord::status() const noexcept { return status_; }
const SemanticReferenceTarget& ReferenceStatusRecord::target() const noexcept { return target_; }
const std::string& ReferenceStatusRecord::message() const noexcept { return message_; }

ReferenceStatusRecord::ReferenceStatusRecord(ReferenceStatusId id, SemanticReferenceTarget target,
                                             ReferenceStatusKind status, std::string message)
    : id_(std::move(id)), target_(std::move(target)), status_(status), message_(std::move(message)) {}

Result<ReferenceRemapRecord> ReferenceRemapRecord::create(ReferenceRemapId id,
                                                           SemanticReferenceTarget original,
                                                           SemanticReferenceTarget replacement,
                                                           std::string reason) {
  const auto object_id = id.empty() ? std::string("reference_remap") : id.value();
  if (id.empty()) {
    return Result<ReferenceRemapRecord>::failure(
        validation_error(object_id, "reference remap id must not be empty"));
  }
  if (original.kind() != replacement.kind()) {
    return Result<ReferenceRemapRecord>::failure(
        validation_error(object_id, "reference remap replacement must have the same semantic kind"));
  }
  if (reason.empty()) {
    return Result<ReferenceRemapRecord>::failure(
        validation_error(object_id, "reference remap reason must not be empty"));
  }
  return Result<ReferenceRemapRecord>::success(ReferenceRemapRecord(
      std::move(id), std::move(original), std::move(replacement), std::move(reason)));
}

const ReferenceRemapId& ReferenceRemapRecord::id() const noexcept { return id_; }
const SemanticReferenceTarget& ReferenceRemapRecord::original() const noexcept { return original_; }
const SemanticReferenceTarget& ReferenceRemapRecord::replacement() const noexcept { return replacement_; }
const std::string& ReferenceRemapRecord::reason() const noexcept { return reason_; }

ReferenceRemapRecord::ReferenceRemapRecord(ReferenceRemapId id, SemanticReferenceTarget original,
                                           SemanticReferenceTarget replacement, std::string reason)
    : id_(std::move(id)), original_(std::move(original)), replacement_(std::move(replacement)),
      reason_(std::move(reason)) {}

Result<ReferenceStatusRecord> ReferenceRecoveryEvaluator::evaluate(
    ReferenceStatusId id, const PartDocument& document, SemanticReferenceTarget target) const {
  if (!target_exists_in_current_seed(document, target)) {
    return ReferenceStatusRecord::create_lost(
        std::move(id), std::move(target),
        "semantic reference source is not available in the current generated topology");
  }

  return ReferenceStatusRecord::create_resolved(std::move(id), std::move(target));
}

} // namespace blcad
