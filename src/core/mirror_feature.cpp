#include "blcad/core/mirror_feature.hpp"

#include "blcad/core/error.hpp"

#include <unordered_set>
#include <utility>

namespace blcad {

std::string_view to_string(MirrorSourceKind kind) noexcept {
  return kind == MirrorSourceKind::Feature ? "feature" : "body";
}

Result<MirrorSourceReference> MirrorSourceReference::feature(FeatureId feature) {
  if (feature.empty())
    return Result<MirrorSourceReference>::failure(
        Error::validation("mirror_source", "mirror source feature id must not be empty"));
  return Result<MirrorSourceReference>::success(MirrorSourceReference(std::move(feature)));
}

Result<MirrorSourceReference> MirrorSourceReference::body(BodyId body) {
  if (body.empty())
    return Result<MirrorSourceReference>::failure(
        Error::validation("mirror_source", "mirror source body id must not be empty"));
  return Result<MirrorSourceReference>::success(MirrorSourceReference(std::move(body)));
}

MirrorSourceReference::MirrorSourceReference(MirrorSource source) : source_(std::move(source)) {}

MirrorSourceKind MirrorSourceReference::kind() const noexcept {
  return std::holds_alternative<FeatureId>(source_) ? MirrorSourceKind::Feature
                                                    : MirrorSourceKind::Body;
}

const MirrorSource& MirrorSourceReference::source() const noexcept {
  return source_;
}

std::string MirrorSourceReference::source_node_id() const {
  if (const auto* feature = std::get_if<FeatureId>(&source_))
    return feature->value();
  return "body:" + std::get<BodyId>(source_).value();
}

Result<MirrorFeature> MirrorFeature::create(FeatureId id, std::string name,
                                            std::vector<MirrorSourceReference> sources,
                                            PlaneReference mirror_plane,
                                            FeatureBodyResultContext body_result_context) {
  const std::string object_id = id.empty() ? "mirror_feature" : id.value();
  if (id.empty())
    return Result<MirrorFeature>::failure(
        Error::validation(object_id, "mirror feature id must not be empty"));
  if (name.empty())
    return Result<MirrorFeature>::failure(
        Error::validation(object_id, "mirror feature name must not be empty"));
  if (sources.empty())
    return Result<MirrorFeature>::failure(
        Error::validation(object_id, "mirror feature requires at least one source"));
  std::unordered_set<std::string> unique_sources;
  for (const auto& source : sources)
    if (!unique_sources.insert(source.source_node_id()).second)
      return Result<MirrorFeature>::failure(
          Error::validation(object_id, "mirror feature sources must be unique"));
  if (mirror_plane.role() != PartFeatureInputRole::MirrorPlane ||
      mirror_plane.expected_capability() != PartFeatureInputCapability::Plane)
    return Result<MirrorFeature>::failure(Error::validation(
        object_id, "mirror plane must use mirror_plane role and Plane capability"));
  return Result<MirrorFeature>::success(MirrorFeature(std::move(id), std::move(name),
                                                      std::move(sources), std::move(mirror_plane),
                                                      std::move(body_result_context)));
}

MirrorFeature::MirrorFeature(FeatureId id, std::string name,
                             std::vector<MirrorSourceReference> sources,
                             PlaneReference mirror_plane,
                             FeatureBodyResultContext body_result_context)
    : id_(std::move(id)), name_(std::move(name)), sources_(std::move(sources)),
      mirror_plane_(std::move(mirror_plane)), body_result_context_(std::move(body_result_context)) {
}

const FeatureId& MirrorFeature::id() const noexcept {
  return id_;
}

const std::string& MirrorFeature::name() const noexcept {
  return name_;
}

const std::vector<MirrorSourceReference>& MirrorFeature::sources() const noexcept {
  return sources_;
}

const PlaneReference& MirrorFeature::mirror_plane() const noexcept {
  return mirror_plane_;
}

const FeatureBodyResultContext& MirrorFeature::body_result_context() const noexcept {
  return body_result_context_;
}

} // namespace blcad
