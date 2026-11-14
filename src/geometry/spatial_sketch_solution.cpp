#include "blcad/geometry/spatial_constraint_geometry.hpp"
#include "part_history_evaluation.hpp"
#include <algorithm>
#include <map>

namespace blcad::geometry {
namespace detail {
struct SpatialSketchSolutions {
  std::map<std::string, Sketch3D> owners;
};
} // namespace detail
const Sketch3D* ShapeCache::find_spatial_sketch_solution(const Sketch3DId& id) const noexcept {
  if (!spatial_sketch_solutions_)
    return nullptr;
  const auto found = spatial_sketch_solutions_->owners.find(id.value());
  return found == spatial_sketch_solutions_->owners.end() ? nullptr : &found->second;
}
void ShapeCache::store_spatial_sketch_solution(Sketch3D owner) {
  auto copy = std::make_shared<detail::SpatialSketchSolutions>();
  if (spatial_sketch_solutions_)
    *copy = *spatial_sketch_solutions_;
  const auto id = owner.id().value();
  copy->owners.insert_or_assign(id, std::move(owner));
  spatial_sketch_solutions_ = std::move(copy);
}
namespace {
struct SolutionContext {
  const PartDocument& part;
  const ShapeCache& cache;
  StepImportTopologyContext* imports;
  SpatialGeometryScope scope;
  std::map<std::string, Sketch3D> solved{};
  std::vector<std::string> active{};

  Result<Sketch3D> resolve(Sketch3DId id) {
    if (const auto old = solved.find(id.value()); old != solved.end())
      return Result<Sketch3D>::success(old->second);
    const auto* source = part.find_sketch_3d(id);
    if (!source)
      return Result<Sketch3D>::failure(Error::geometry(id.value(), "Spatial owner is unavailable"));
    if (source->constraints().empty())
      return Result<Sketch3D>::success(*source);
    auto sources =
        SpatialConstraintGeometry{}.queries(part, cache, imports, scope).validate_sources();
    if (!sources)
      return Result<Sketch3D>::failure(sources.error());
    if (const auto* cached = cache.find_spatial_sketch_solution(id))
      return Result<Sketch3D>::success(*cached);
    if (std::find(active.begin(), active.end(), id.value()) != active.end())
      return Result<Sketch3D>::failure(
          Error::dependency(id.value(), "Spatial solve sources form a cycle"));
    auto available = detail::validate_history_source(part, id.value(), &cache);
    if (!available)
      return Result<Sketch3D>::failure(available.error());
    active.push_back(id.value());
    struct Pop {
      std::vector<std::string>& ids;
      ~Pop() {
        ids.pop_back();
      }
    } pop{active};
    auto query =
        SpatialConstraintGeometry{}.queries(part, cache, imports, scope, [this](Sketch3DId other) {
          return resolve(std::move(other));
        });
    auto result = solve_spatial_sketch(part, *source, query);
    if (!result.accepted())
      return Result<Sketch3D>::failure(Error::geometry(id.value(), result.diagnostic));
    auto [entry, inserted] = solved.emplace(id.value(), std::move(result.sketch));
    (void)inserted;
    return Result<Sketch3D>::success(entry->second);
  }
};
} // namespace
Result<Sketch3D> resolve_spatial_sketch_geometry(const PartDocument& part, Sketch3DId id,
                                                 const ShapeCache& cache,
                                                 StepImportTopologyContext* imports,
                                                 SpatialGeometryScope scope) {
  SolutionContext context{part, cache, imports, std::move(scope)};
  return context.resolve(std::move(id));
}
} // namespace blcad::geometry
