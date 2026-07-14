#pragma once

#include "blcad/core/feature.hpp"
#include "blcad/core/part_feature_input_reference.hpp"

#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace blcad {

enum class MirrorSourceKind { Feature, Body };

[[nodiscard]] std::string_view to_string(MirrorSourceKind kind) noexcept;

using MirrorSource = std::variant<FeatureId, BodyId>;

class MirrorSourceReference {
public:
  [[nodiscard]] static Result<MirrorSourceReference> feature(FeatureId feature);
  [[nodiscard]] static Result<MirrorSourceReference> body(BodyId body);

  [[nodiscard]] MirrorSourceKind kind() const noexcept;
  [[nodiscard]] const MirrorSource& source() const noexcept;
  [[nodiscard]] std::string source_node_id() const;

  friend bool operator==(const MirrorSourceReference&, const MirrorSourceReference&) = default;

private:
  explicit MirrorSourceReference(MirrorSource source);
  MirrorSource source_;
};

class MirrorFeature {
public:
  [[nodiscard]] static Result<MirrorFeature> create(FeatureId id, std::string name,
                                                    std::vector<MirrorSourceReference> sources,
                                                    PlaneReference mirror_plane,
                                                    FeatureBodyResultContext body_result_context);

  [[nodiscard]] const FeatureId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] const std::vector<MirrorSourceReference>& sources() const noexcept;
  [[nodiscard]] const PlaneReference& mirror_plane() const noexcept;
  [[nodiscard]] const FeatureBodyResultContext& body_result_context() const noexcept;

private:
  MirrorFeature(FeatureId id, std::string name, std::vector<MirrorSourceReference> sources,
                PlaneReference mirror_plane, FeatureBodyResultContext body_result_context);

  FeatureId id_;
  std::string name_;
  std::vector<MirrorSourceReference> sources_;
  PlaneReference mirror_plane_;
  FeatureBodyResultContext body_result_context_;
};

} // namespace blcad
