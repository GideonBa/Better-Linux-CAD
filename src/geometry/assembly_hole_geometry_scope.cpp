#include "assembly_hole_geometry_scope.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/step_import_topology_context.hpp"

namespace blcad::geometry::detail {
thread_local AssemblyHoleGeometryScope* AssemblyHoleGeometryScope::current_ = nullptr;
AssemblyHoleGeometryScope::AssemblyHoleGeometryScope() : previous_(current_) {
  if (!current_)
    current_ = this;
}
AssemblyHoleGeometryScope::~AssemblyHoleGeometryScope() {
  current_ = previous_;
}
Result<std::shared_ptr<const ShapeCache>>
assembly_hole_part_geometry(const PartDocument& part, StepImportTopologyContext* imports) {
  using R = Result<std::shared_ptr<const ShapeCache>>;
  auto fresh = observe_imported_part_sources(part, imports);
  if (!fresh)
    return R::failure(fresh.error());
  auto intent = serialize_part_document_to_json(part);
  if (!intent)
    return R::failure(intent.error());
  auto* scope = AssemblyHoleGeometryScope::current_;
  if (scope) {
    const auto found = scope->entries_.find(part.id().value());
    if (found != scope->entries_.end() && found->second.intent == intent.value() &&
        found->second.imports == imports)
      return R::success(found->second.cache);
  }
  auto created = ShapeCache::create(ShapeCacheId("assembly.hole." + part.id().value()));
  if (!created)
    return R::failure(created.error());
  auto cache = std::make_shared<ShapeCache>(std::move(created.value()));
  auto evaluated = GeometryRecomputeExecutor{imports}.execute_document(part, *cache);
  if (!evaluated)
    return R::failure(evaluated.error());
  if (scope) {
    if (scope->entries_.size() >= 16)
      scope->entries_.clear();
    scope->entries_.insert_or_assign(
        part.id().value(),
        AssemblyHoleGeometryScope::Entry{std::move(intent.value()), imports, cache});
  }
  return R::success(std::move(cache));
}
} // namespace blcad::geometry::detail
