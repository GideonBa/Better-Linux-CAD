#include "blcad/core/project.hpp"

#include <unordered_set>
#include <utility>

namespace blcad {
namespace {
Result<EngineeringResourceCatalog> collect(const Project& project) {
  auto resources = project.engineering_resources();
  for (const auto& part : project.part_documents()) {
    const auto merged = resources.merge_from(part.engineering_resources());
    if (!merged)
      return Result<EngineeringResourceCatalog>::failure(merged.error());
  }
  return Result<EngineeringResourceCatalog>::success(std::move(resources));
}
} // namespace

Result<std::size_t> Project::add_engineering_resource(EngineeringResource resource) {
  return engineering_resources_.insert(std::move(resource));
}
Result<std::size_t> Project::synchronize_engineering_resources() {
  auto resources = collect(*this);
  if (!resources)
    return Result<std::size_t>::failure(resources.error());
  const auto added = resources.value().entries().size() - engineering_resources_.entries().size();
  engineering_resources_ = std::move(resources.value());
  return Result<std::size_t>::success(added);
}
Result<std::size_t> Project::validate_engineering_resources() const {
  const auto resources = collect(*this);
  if (!resources)
    return Result<std::size_t>::failure(resources.error());
  if (resources.value() != engineering_resources_)
    return Result<std::size_t>::failure(Error::validation(
        id_.value(), "Project member resources require explicit catalog synchronization"));
  return Result<std::size_t>::success(engineering_resources_.entries().size());
}
Result<std::size_t>
Project::adopt_engineering_resource_revision(const EngineeringResourceKey& source,
                                             EngineeringResource replacement,
                                             const std::vector<DocumentId>& part_documents) {
  const auto valid = validate_engineering_resources();
  if (!valid)
    return valid;
  if (part_documents.empty() || !engineering_resources_.find(source) ||
      source.id() != engineering_resource_key(replacement).id())
    return Result<std::size_t>::failure(Error::validation(
        id_.value(), "Revision adoption requires a known resource and explicit member selection"));
  auto candidate = *this;
  const auto added = candidate.add_engineering_resource(replacement);
  if (!added)
    return added;
  std::unordered_set<std::string> seen;
  std::size_t changed = 0;
  for (const auto& id : part_documents) {
    auto* part = candidate.find_part_document(id);
    if (!seen.insert(id.value()).second || !part)
      return Result<std::size_t>::failure(Error::validation(
          id.value(), "Revision adoption requires unique existing owned Part ids"));
    const auto adopted = part->adopt_engineering_resource_revision(source, replacement);
    if (!adopted)
      return adopted;
    changed += adopted.value() != 0;
  }
  *this = std::move(candidate);
  return Result<std::size_t>::success(changed);
}
Result<std::size_t> Project::remove_engineering_resource(const EngineeringResourceKey& key) {
  const auto valid = validate_engineering_resources();
  if (!valid)
    return valid;
  for (const auto& part : part_documents_)
    if (part.engineering_resources().find(key))
      return Result<std::size_t>::failure(Error::validation(
          part.id().value(), "Project revision is still referenced by a member resource snapshot"));
  return engineering_resources_.remove(key);
}
} // namespace blcad
