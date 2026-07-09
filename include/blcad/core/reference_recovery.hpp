#pragma once

#include "blcad/core/datum_plane.hpp"
#include "blcad/core/id.hpp"
#include "blcad/core/result.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace blcad {

enum class SemanticReferenceKind {
  Face,
  Edge,
  Vertex,
};

[[nodiscard]] std::string_view to_string(SemanticReferenceKind kind) noexcept;

enum class ReferenceStatusKind {
  Resolved,
  Lost,
};

[[nodiscard]] std::string_view to_string(ReferenceStatusKind status) noexcept;

class SemanticReferenceTarget {
public:
  [[nodiscard]] static Result<SemanticReferenceTarget> create_face(FeatureId source_feature,
                                                                   SemanticFace face);
  [[nodiscard]] static Result<SemanticReferenceTarget> create_edge(FeatureId source_feature,
                                                                   SemanticEdge edge);
  [[nodiscard]] static Result<SemanticReferenceTarget> create_vertex(FeatureId source_feature,
                                                                     SemanticVertex vertex);

  [[nodiscard]] SemanticReferenceKind kind() const noexcept;
  [[nodiscard]] const FeatureId& source_feature() const noexcept;
  [[nodiscard]] const std::optional<SemanticFace>& face() const noexcept;
  [[nodiscard]] const std::optional<SemanticEdge>& edge() const noexcept;
  [[nodiscard]] const std::optional<SemanticVertex>& vertex() const noexcept;
  [[nodiscard]] std::string node_id() const;

private:
  SemanticReferenceTarget(FeatureId source_feature, SemanticReferenceKind kind,
                          std::optional<SemanticFace> face, std::optional<SemanticEdge> edge,
                          std::optional<SemanticVertex> vertex);

  FeatureId source_feature_;
  SemanticReferenceKind kind_;
  std::optional<SemanticFace> face_;
  std::optional<SemanticEdge> edge_;
  std::optional<SemanticVertex> vertex_;
};

class ReferenceStatusRecord {
public:
  [[nodiscard]] static Result<ReferenceStatusRecord> create_resolved(ReferenceStatusId id,
                                                                     SemanticReferenceTarget target);
  [[nodiscard]] static Result<ReferenceStatusRecord> create_lost(ReferenceStatusId id,
                                                                 SemanticReferenceTarget target,
                                                                 std::string message);

  [[nodiscard]] const ReferenceStatusId& id() const noexcept;
  [[nodiscard]] ReferenceStatusKind status() const noexcept;
  [[nodiscard]] const SemanticReferenceTarget& target() const noexcept;
  [[nodiscard]] const std::string& message() const noexcept;

private:
  ReferenceStatusRecord(ReferenceStatusId id, SemanticReferenceTarget target,
                        ReferenceStatusKind status, std::string message);

  ReferenceStatusId id_;
  SemanticReferenceTarget target_;
  ReferenceStatusKind status_;
  std::string message_;
};

class ReferenceRemapRecord {
public:
  [[nodiscard]] static Result<ReferenceRemapRecord> create(ReferenceRemapId id,
                                                           SemanticReferenceTarget original,
                                                           SemanticReferenceTarget replacement,
                                                           std::string reason);

  [[nodiscard]] const ReferenceRemapId& id() const noexcept;
  [[nodiscard]] const SemanticReferenceTarget& original() const noexcept;
  [[nodiscard]] const SemanticReferenceTarget& replacement() const noexcept;
  [[nodiscard]] const std::string& reason() const noexcept;

private:
  ReferenceRemapRecord(ReferenceRemapId id, SemanticReferenceTarget original,
                       SemanticReferenceTarget replacement, std::string reason);

  ReferenceRemapId id_;
  SemanticReferenceTarget original_;
  SemanticReferenceTarget replacement_;
  std::string reason_;
};

class ReferenceRecoveryEvaluator {
public:
  [[nodiscard]] Result<ReferenceStatusRecord> evaluate(ReferenceStatusId id,
                                                       const PartDocument& document,
                                                       SemanticReferenceTarget target) const;
};

} // namespace blcad
