#include "blcad/core/project.hpp"

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
} // namespace blcad
