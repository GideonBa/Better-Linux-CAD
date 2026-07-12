#pragma once

#include "blcad/core/datum_plane.hpp"
#include "blcad/core/id.hpp"
#include "blcad/core/result.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace blcad {

class PartDocument;

enum class SemanticCylindricalFace {
  Wall,
};

[[nodiscard]] std::string_view to_string(SemanticCylindricalFace face) noexcept;

enum class SemanticCircularEdge {
  SourceRim,
  OppositeRim,
};

[[nodiscard]] std::string_view to_string(SemanticCircularEdge edge) noexcept;

class SemanticCylindricalFaceReference {
public:
  [[nodiscard]] static Result<SemanticCylindricalFaceReference>
  create(FeatureId source_feature, ProfileId source_profile,
         SemanticCylindricalFace face = SemanticCylindricalFace::Wall);

  [[nodiscard]] const FeatureId& source_feature() const noexcept;
  [[nodiscard]] const ProfileId& source_profile() const noexcept;
  [[nodiscard]] SemanticCylindricalFace face() const noexcept;

private:
  SemanticCylindricalFaceReference(FeatureId source_feature, ProfileId source_profile,
                                   SemanticCylindricalFace face);

  FeatureId source_feature_;
  ProfileId source_profile_;
  SemanticCylindricalFace face_;
};

class SemanticCircularEdgeReference {
public:
  [[nodiscard]] static Result<SemanticCircularEdgeReference>
  create(FeatureId source_feature, ProfileId source_profile, SemanticCircularEdge edge);

  [[nodiscard]] const FeatureId& source_feature() const noexcept;
  [[nodiscard]] const ProfileId& source_profile() const noexcept;
  [[nodiscard]] SemanticCircularEdge edge() const noexcept;

private:
  SemanticCircularEdgeReference(FeatureId source_feature, ProfileId source_profile,
                                SemanticCircularEdge edge);

  FeatureId source_feature_;
  ProfileId source_profile_;
  SemanticCircularEdge edge_;
};

enum class GeneratedTopologyReferenceFamily {
  CylindricalFace,
  LinearEdge,
  CircularEdge,
  Vertex,
};

[[nodiscard]] std::string_view to_string(GeneratedTopologyReferenceFamily family) noexcept;

using GeneratedTopologyReferenceIdentity =
    std::variant<SemanticCylindricalFaceReference, SemanticEdgeReference,
                 SemanticCircularEdgeReference, SemanticVertexReference>;

[[nodiscard]] GeneratedTopologyReferenceFamily
generated_topology_reference_family(const GeneratedTopologyReferenceIdentity& identity) noexcept;
[[nodiscard]] const FeatureId&
generated_topology_reference_source_feature(const GeneratedTopologyReferenceIdentity& identity);
[[nodiscard]] std::optional<ProfileId>
generated_topology_reference_source_profile(const GeneratedTopologyReferenceIdentity& identity);
[[nodiscard]] std::string_view
generated_topology_reference_role(const GeneratedTopologyReferenceIdentity& identity) noexcept;

enum class GeneratedTopologyProducerKind {
  RectangularAdditiveExtrude,
  SingleCircleSubtractiveExtrude,
};

[[nodiscard]] std::string_view to_string(GeneratedTopologyProducerKind kind) noexcept;

struct GeneratedTopologyRoleRule {
  GeneratedTopologyReferenceFamily family;
  std::string role;
  std::size_t expected_cardinality = 0U;

  friend bool operator==(const GeneratedTopologyRoleRule&,
                         const GeneratedTopologyRoleRule&) = default;
};

// Producer-owned semantic role contract. Every listed role has exactly the
// published expected cardinality. Unsupported producer/family/role combinations
// fail closed instead of falling back to kernel traversal order.
[[nodiscard]] std::vector<GeneratedTopologyRoleRule>
generated_topology_producer_role_matrix(GeneratedTopologyProducerKind producer);

[[nodiscard]] Result<GeneratedTopologyProducerKind>
classify_generated_topology_producer(const PartDocument& document, FeatureId source_feature);

// Validates producer support, exact profile ownership where required, semantic
// role support, and expected role cardinality. This function executes no OCCT
// geometry and never derives identity from a TopoDS traversal or hash.
[[nodiscard]] Result<std::size_t>
validate_generated_topology_reference(const PartDocument& document,
                                      const GeneratedTopologyReferenceIdentity& identity);

// Canonical persistent assembly semantic spelling:
//
//   topo:cylindrical_face:<feature-id>:<profile-id>:wall
//   topo:linear_edge:<feature-id>:<role>
//   topo:circular_edge:<feature-id>:<profile-id>:<source_rim|opposite_rim>
//   topo:vertex:<feature-id>:<role>
//
// Every id byte outside [A-Za-z0-9_-] is escaped as uppercase %HH. Valid topo:
// spellings contain no '.', remain disjoint from legacy <feature>.<role> and
// ref: spellings, and preserve arbitrary typed-id bytes without ambiguity.
[[nodiscard]] Result<std::string>
make_generated_topology_target_spelling(const GeneratedTopologyReferenceIdentity& identity);
[[nodiscard]] Result<GeneratedTopologyReferenceIdentity>
parse_generated_topology_target_spelling(std::string_view spelling);
[[nodiscard]] bool is_generated_topology_target_spelling(std::string_view spelling) noexcept;

} // namespace blcad
