#pragma once

#include "blcad/geometry/shape_cache.hpp"
#include <map>
#include <memory>

namespace blcad::geometry {
class StepImportTopologyContext;
namespace detail {
// Request-local reuse while the numeric engine changes component poses. Part
// intent and verified source assets remain authoritative; no geometry survives
// the outer scope. Nested numeric solves share that same bounded request.
class AssemblyHoleGeometryScope {
public:
  AssemblyHoleGeometryScope();
  ~AssemblyHoleGeometryScope();
  AssemblyHoleGeometryScope(const AssemblyHoleGeometryScope&) = delete;
  AssemblyHoleGeometryScope& operator=(const AssemblyHoleGeometryScope&) = delete;

private:
  struct Entry {
    std::string intent;
    StepImportTopologyContext* imports;
    std::shared_ptr<const ShapeCache> cache;
  };
  std::map<std::string, Entry> entries_;
  AssemblyHoleGeometryScope* previous_;
  static thread_local AssemblyHoleGeometryScope* current_;
  friend Result<std::shared_ptr<const ShapeCache>>
  assembly_hole_part_geometry(const PartDocument&, StepImportTopologyContext*);
};
[[nodiscard]] Result<std::shared_ptr<const ShapeCache>>
assembly_hole_part_geometry(const PartDocument&, StepImportTopologyContext* = nullptr);
} // namespace detail
} // namespace blcad::geometry
