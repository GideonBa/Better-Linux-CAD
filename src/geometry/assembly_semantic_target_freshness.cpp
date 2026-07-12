#include "assembly_semantic_target_freshness.hpp"

#include "blcad/core/part_document_json.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace blcad::geometry::detail {
namespace {

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
}

[[nodiscard]] std::vector<DocumentId> canonical_part_documents(
    std::vector<DocumentId> part_documents) {
  std::sort(part_documents.begin(), part_documents.end(),
            [](const DocumentId& lhs, const DocumentId& rhs) {
              return lhs.value() < rhs.value();
            });
  part_documents.erase(
      std::unique(part_documents.begin(), part_documents.end()), part_documents.end());
  return part_documents;
}

} // namespace

Result<std::vector<AssemblySemanticTargetPartSnapshot>>
make_semantic_target_part_snapshots(const Project& project,
                                    std::vector<DocumentId> part_documents) {
  part_documents = canonical_part_documents(std::move(part_documents));
  std::vector<AssemblySemanticTargetPartSnapshot> snapshots;
  snapshots.reserve(part_documents.size());

  for (const DocumentId& part_document : part_documents) {
    const PartDocument* part = project.find_part_document(part_document);
    if (part == nullptr) {
      return Result<std::vector<AssemblySemanticTargetPartSnapshot>>::failure(validation_error(
          part_document.value(),
          "assembly semantic target snapshot part document must exist in project"));
    }
    auto serialized = serialize_part_document_to_json(*part);
    if (serialized.has_error()) {
      return Result<std::vector<AssemblySemanticTargetPartSnapshot>>::failure(serialized.error());
    }
    snapshots.push_back(
        AssemblySemanticTargetPartSnapshot{part_document, std::move(serialized.value())});
  }

  return Result<std::vector<AssemblySemanticTargetPartSnapshot>>::success(std::move(snapshots));
}

Result<std::size_t> validate_semantic_target_part_snapshots(
    const Project& project, const std::vector<DocumentId>& expected_part_documents,
    const std::vector<AssemblySemanticTargetPartSnapshot>& snapshots) {
  const std::vector<DocumentId> expected = canonical_part_documents(expected_part_documents);
  if (snapshots.size() != expected.size()) {
    return Result<std::size_t>::failure(validation_error(
        "assembly.semantic_target_freshness",
        "assembly solve result semantic target part snapshot set is incomplete or contains extras"));
  }

  for (std::size_t index = 0U; index < expected.size(); ++index) {
    const AssemblySemanticTargetPartSnapshot& snapshot = snapshots[index];
    if (snapshot.part_document != expected[index]) {
      return Result<std::size_t>::failure(validation_error(
          snapshot.part_document.value(),
          "assembly solve result semantic target part snapshots are not the exact canonical part set"));
    }
    const PartDocument* part = project.find_part_document(snapshot.part_document);
    if (part == nullptr) {
      return Result<std::size_t>::failure(validation_error(
          snapshot.part_document.value(),
          "assembly solve result semantic target part document no longer exists"));
    }
    auto serialized = serialize_part_document_to_json(*part);
    if (serialized.has_error()) {
      return Result<std::size_t>::failure(serialized.error());
    }
    if (serialized.value() != snapshot.canonical_model_intent_json) {
      return Result<std::size_t>::failure(validation_error(
          snapshot.part_document.value(),
          "assembly solve result is stale because semantic target-producing part model intent changed"));
    }
  }

  return Result<std::size_t>::success(snapshots.size());
}

} // namespace blcad::geometry::detail
