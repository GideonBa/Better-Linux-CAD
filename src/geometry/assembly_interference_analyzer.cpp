#include "blcad/geometry/assembly_interference_analyzer.hpp"

#include "assembly_posed_leaf_shapes.hpp"

#include <BRepAlgoAPI_Common.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS_Shape.hxx>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <exception>
#include <string>
#include <utility>
#include <vector>

namespace blcad::geometry {
namespace {

constexpr const char* kInterferenceAnalyzerId = "geometry.assembly_interference_analyzer";

[[nodiscard]] Error make_geometry_error(std::string message) {
  return Error::geometry(kInterferenceAnalyzerId, std::move(message));
}

[[nodiscard]] std::string standard_failure_message(const Standard_Failure& failure) {
  const char* const message = failure.GetMessageString();
  if (message == nullptr || *message == '\0') {
    return "OCCT operation failed";
  }
  return message;
}

[[nodiscard]] AssemblyLeafIdentity make_leaf_identity(const detail::PosedLeafShape& posed) {
  return AssemblyLeafIdentity{posed.leaf.assembly_document, posed.leaf.subassembly_path,
                              posed.leaf.component_instance,
                              detail::leaf_occurrence_key(posed.leaf)};
}

[[nodiscard]] Result<double> common_solid_volume_mm3(const TopoDS_Shape& shape_a,
                                                     const TopoDS_Shape& shape_b,
                                                     const std::string& pair_key) {
  BRepAlgoAPI_Common common_builder(shape_a, shape_b);
  if (!common_builder.IsDone()) {
    return Result<double>::failure(
        make_geometry_error("interference boolean common operation failed for pair " + pair_key));
  }

  const TopoDS_Shape common_shape = common_builder.Shape();
  if (common_shape.IsNull()) {
    return Result<double>::success(0.0);
  }

  GProp_GProps volume_properties;
  BRepGProp::VolumeProperties(common_shape, volume_properties);
  const double volume = volume_properties.Mass();
  if (!std::isfinite(volume)) {
    return Result<double>::failure(
        make_geometry_error("interference overlap volume is not finite for pair " + pair_key));
  }
  return Result<double>::success(volume);
}

} // namespace

Result<AssemblyInterferenceAnalysis>
AssemblyInterferenceAnalyzer::analyze(const Project& project,
                                      AssemblyInterferenceAnalysisOptions options) const {
  if (!std::isfinite(options.minimum_overlap_volume_mm3) ||
      options.minimum_overlap_volume_mm3 <= 0.0) {
    return Result<AssemblyInterferenceAnalysis>::failure(
        Error::validation(kInterferenceAnalyzerId,
                          "interference overlap volume tolerance must be finite and positive"));
  }

  auto posed_leaves = detail::build_posed_leaf_shapes(project);
  if (posed_leaves.has_error()) {
    return Result<AssemblyInterferenceAnalysis>::failure(posed_leaves.error());
  }

  // Deterministic lexicographic occurrence-key order, independent of authoring
  // or resolver order.
  std::vector<const detail::PosedLeafShape*> ordered_leaves;
  ordered_leaves.reserve(posed_leaves.value().leaves.size());
  for (const detail::PosedLeafShape& posed : posed_leaves.value().leaves) {
    ordered_leaves.push_back(&posed);
  }
  std::sort(ordered_leaves.begin(), ordered_leaves.end(),
            [](const detail::PosedLeafShape* lhs, const detail::PosedLeafShape* rhs) {
              return detail::leaf_occurrence_key(lhs->leaf) <
                     detail::leaf_occurrence_key(rhs->leaf);
            });

  AssemblyInterferenceAnalysis analysis;
  analysis.leaf_count = ordered_leaves.size();
  analysis.recomputed_part_count = posed_leaves.value().recomputed_part_count;

  try {
    for (std::size_t first = 0U; first < ordered_leaves.size(); ++first) {
      for (std::size_t second = first + 1U; second < ordered_leaves.size(); ++second) {
        const detail::PosedLeafShape& leaf_a = *ordered_leaves[first];
        const detail::PosedLeafShape& leaf_b = *ordered_leaves[second];
        ++analysis.evaluated_pair_count;

        const std::string pair_key = detail::leaf_occurrence_key(leaf_a.leaf) + " x " +
                                     detail::leaf_occurrence_key(leaf_b.leaf);
        auto volume = common_solid_volume_mm3(leaf_a.shape, leaf_b.shape, pair_key);
        if (volume.has_error()) {
          return Result<AssemblyInterferenceAnalysis>::failure(volume.error());
        }

        if (volume.value() > options.minimum_overlap_volume_mm3) {
          analysis.interferences.push_back(AssemblyLeafInterferenceRecord{
              make_leaf_identity(leaf_a), make_leaf_identity(leaf_b), volume.value()});
        }
      }
    }
  } catch (const Standard_Failure& failure) {
    return Result<AssemblyInterferenceAnalysis>::failure(
        make_geometry_error(standard_failure_message(failure)));
  } catch (const std::exception& exception) {
    return Result<AssemblyInterferenceAnalysis>::failure(make_geometry_error(exception.what()));
  } catch (...) {
    return Result<AssemblyInterferenceAnalysis>::failure(
        make_geometry_error("unknown assembly interference analysis error"));
  }

  return Result<AssemblyInterferenceAnalysis>::success(std::move(analysis));
}

} // namespace blcad::geometry
