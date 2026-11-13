#pragma once
#include "blcad/core/id.hpp"
#include "blcad/core/result.hpp"
#include "blcad/core/sketch.hpp"
#include <optional>
#include <string>
namespace blcad {
enum class SketchPointReference3DSource { LocalPoint, ConstructionPoint, PlanarSketchPoint };

class SketchPointReference3D {
public:
  [[nodiscard]] static Result<SketchPointReference3D> create_local_point(SketchEntityId point);
  [[nodiscard]] static Result<SketchPointReference3D>
  create_construction_point(ConstructionPointId point);
  [[nodiscard]] static Result<SketchPointReference3D>
  create_planar_sketch_point(SketchId sketch, SketchReferenceTarget point);

  [[nodiscard]] SketchPointReference3DSource source() const noexcept;
  [[nodiscard]] const std::optional<SketchEntityId>& local_point() const noexcept;
  [[nodiscard]] const std::optional<ConstructionPointId>& construction_point() const noexcept;
  [[nodiscard]] const std::optional<SketchId>& planar_sketch() const noexcept;
  [[nodiscard]] const std::optional<SketchReferenceTarget>& planar_point() const noexcept;
  [[nodiscard]] std::optional<std::string> referenced_node_id() const;
  [[nodiscard]] bool same_source(const SketchPointReference3D& other) const noexcept;

private:
  SketchPointReference3D(SketchPointReference3DSource source,
                         std::optional<SketchEntityId> local_point,
                         std::optional<ConstructionPointId> construction_point,
                         std::optional<SketchId> planar_sketch,
                         std::optional<SketchReferenceTarget> planar_point);

  SketchPointReference3DSource source_;
  std::optional<SketchEntityId> local_point_;
  std::optional<ConstructionPointId> construction_point_;
  std::optional<SketchId> planar_sketch_;
  std::optional<SketchReferenceTarget> planar_point_;
};

} // namespace blcad
