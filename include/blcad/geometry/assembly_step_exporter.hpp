#pragma once

#include "blcad/core/project.hpp"
#include "blcad/core/result.hpp"
#include "blcad/geometry/rectangle_extrusion_adapter.hpp"

#include <cstddef>
#include <string>

namespace blcad::geometry {

struct AssemblyStepExportSummary {
  std::size_t recomputed_part_count = 0U;
  std::size_t exported_component_count = 0U;
  std::size_t written_bytes = 0U;

  friend bool operator==(const AssemblyStepExportSummary&,
                         const AssemblyStepExportSummary&) = default;
};

// Builds and exports derived posed assembly geometry. Part shapes are recomputed
// once per referenced part document, component placement is applied with the
// exact AssemblyTransformEvaluator X-then-Y-then-Z convention, and visible
// active component shapes are combined into one OCCT compound.
class AssemblyStepExporter {
public:
  [[nodiscard]] Result<GeometryShape> build_posed_shape(const Project& project) const;

  [[nodiscard]] Result<AssemblyStepExportSummary> write_step(const Project& project,
                                                              const std::string& path) const;

private:
  struct PosedAssemblyBuild {
    GeometryShape shape;
    std::size_t recomputed_part_count = 0U;
    std::size_t exported_component_count = 0U;
  };

  [[nodiscard]] Result<PosedAssemblyBuild> build(const Project& project) const;
};

} // namespace blcad::geometry
