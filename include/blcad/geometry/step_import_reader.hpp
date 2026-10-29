#pragma once

#include "blcad/core/body.hpp"
#include "blcad/core/step_import_source.hpp"
#include "blcad/geometry/rectangle_extrusion_adapter.hpp"

#include <array>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace blcad::geometry {

// Exact direct occurrence matrix: three row-major rows of rotation and translation in mm.
// These runtime values and all indices below are derived inspection data, never Core identity.
struct StepImportTransform {
  std::array<double, 12> matrix{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
};

enum class StepImportedShapeKind { Solid, Shell, Face, Compound };

struct StepImportedBodyDefinition {
  std::string definition_key;
  std::string name;
  BodyKind kind{BodyKind::Solid};
  StepImportedShapeKind source_kind{StepImportedShapeKind::Solid};
  GeometryShape shape;
  std::array<double, 6> bounds_mm{}; // xmin, ymin, zmin, xmax, ymax, zmax
  double volume_mm3{};
  double area_mm2{};
  std::size_t face_count{};
  bool ambiguous_order{false};
};

struct StepImportedProductDefinition {
  std::string definition_key;
  std::string name;
  bool is_assembly{false};
  bool ambiguous_order{false};
  std::vector<StepImportedBodyDefinition> bodies;
  // Assemblies have no own body topology; CompSolid is reported as Compound.
  std::optional<StepImportedShapeKind> source_kind;
};

struct StepImportedOccurrence {
  std::optional<std::size_t> parent_definition;
  std::size_t product_definition{};
  std::string name;
  StepImportTransform transform;
};

struct StepImportUnitDiagnostic {
  std::string declared_length_unit;
  double length_to_mm{};
};

struct StepImportGeometry {
  StepImportSource source;
  std::vector<StepImportUnitDiagnostic> units;
  std::vector<StepImportedProductDefinition> products;
  // Includes explicit root occurrences (no parent) and direct assembly children.
  std::vector<StepImportedOccurrence> occurrences;
  std::vector<std::string> diagnostics;

  [[nodiscard]] std::size_t body_count() const noexcept;
};

class StepImportReader {
public:
  // Reads one immutable, digest-verified snapshot. Rejects external STEP references before
  // transfer, invalid/empty geometry, ambiguous units and non-rigid/non-finite placements.
  // Bodies stay in product-local coordinates; occurrences carry the separate direct placement.
  [[nodiscard]] Result<StepImportGeometry> read(const StepImportSource& source,
                                                const std::filesystem::path& root_directory) const;
  // Disposable inspection presentation only; does not create flattened imported Part intent.
  [[nodiscard]] Result<std::vector<GeometryShape>>
  preview_shapes(const StepImportGeometry& imported) const;
};

} // namespace blcad::geometry
