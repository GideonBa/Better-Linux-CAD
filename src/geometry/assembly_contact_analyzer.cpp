#include "blcad/geometry/assembly_contact_analyzer.hpp"

#include "assembly_posed_leaf_shapes.hpp"

#include <BRepAlgoAPI_Common.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
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

constexpr const char* kContactAnalyzerId = "geometry.assembly_contact_analyzer";

[[nodiscard]] Error make_geometry_error(std::string message) {
  return Error::geometry(kContactAnalyzerId, std::move(message));
}

[[nodiscard]] std::string standard_failure_message(const Standard_Failure& failure) {
  const char* const message = failure.GetMessageString();
  if (message == nullptr || *message == '\0') {
    return "OCCT operation failed";
  }
  return message;
}

[[nodiscard]] bool path_less(const std::vector<SubassemblyInstanceId>& lhs,
                             const std::vector<SubassemblyInstanceId>& rhs) {
  return std::lexicographical_compare(
      lhs.begin(), lhs.end(), rhs.begin(), rhs.end(),
      [](const SubassemblyInstanceId& left, const SubassemblyInstanceId& right) {
        return left.value() < right.value();
      });
}

[[nodiscard]] AssemblyExchangeComponentOccurrenceIdentity occurrence_identity(
    const detail::PosedLeafShape& posed) {
  return AssemblyExchangeComponentOccurrenceIdentity{
      AssemblyExchangeAssemblyOccurrenceIdentity{posed.leaf.subassembly_path},
      posed.leaf.component_instance};
}

[[nodiscard]] bool occurrence_less(const AssemblyExchangeComponentOccurrenceIdentity& lhs,
                                   const AssemblyExchangeComponentOccurrenceIdentity& rhs) {
  const auto& lhs_path = lhs.containing_assembly_occurrence.occurrence_path;
  const auto& rhs_path = rhs.containing_assembly_occurrence.occurrence_path;
  if (path_less(lhs_path, rhs_path)) {
    return true;
  }
  if (path_less(rhs_path, lhs_path)) {
    return false;
  }
  return lhs.component_instance.value() < rhs.component_instance.value();
}

[[nodiscard]] std::string occurrence_debug_text(
    const AssemblyExchangeComponentOccurrenceIdentity& occurrence) {
  std::string text = "[";
  const auto& path = occurrence.containing_assembly_occurrence.occurrence_path;
  for (std::size_t index = 0U; index < path.size(); ++index) {
    if (index != 0U) {
      text += "/";
    }
    text += path[index].value();
  }
  text += "]/" + occurrence.component_instance.value();
  return text;
}

[[nodiscard]] Result<double> common_solid_volume_mm3(const TopoDS_Shape& shape_a,
                                                      const TopoDS_Shape& shape_b,
                                                      const std::string& pair_text) {
  BRepAlgoAPI_Common common_builder(shape_a, shape_b);
  if (!common_builder.IsDone()) {
    return Result<double>::failure(
        make_geometry_error("contact boolean common operation failed for pair " + pair_text));
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
        make_geometry_error("contact overlap volume is not finite for pair " + pair_text));
  }
  return Result<double>::success(volume);
}

[[nodiscard]] Result<double> minimum_distance_mm(const TopoDS_Shape& shape_a,
                                                 const TopoDS_Shape& shape_b,
                                                 const std::string& pair_text) {
  BRepExtrema_DistShapeShape distance_builder(shape_a, shape_b);
  if (!distance_builder.IsDone()) {
    return Result<double>::failure(
        make_geometry_error("contact minimum-distance computation failed for pair " + pair_text));
  }
  const double distance = distance_builder.Value();
  if (!std::isfinite(distance)) {
    return Result<double>::failure(
        make_geometry_error("contact minimum distance is not finite for pair " + pair_text));
  }
  return Result<double>::success(distance);
}

} // namespace

Result<AssemblyContactAnalysis>
AssemblyContactAnalyzer::analyze(const Project& project, AssemblyContactAnalysisOptions options) const {
  if (!std::isfinite(options.touching_tolerance_mm) || options.touching_tolerance_mm < 0.0) {
    return Result<AssemblyContactAnalysis>::failure(Error::validation(
        kContactAnalyzerId, "contact touching tolerance must be finite and non-negative"));
  }
  if (!std::isfinite(options.minimum_overlap_volume_mm3) ||
      options.minimum_overlap_volume_mm3 <= 0.0) {
    return Result<AssemblyContactAnalysis>::failure(Error::validation(
        kContactAnalyzerId, "contact overlap volume tolerance must be finite and positive"));
  }

  auto posed_leaves = detail::build_posed_leaf_shapes(project);
  if (posed_leaves.has_error()) {
    return Result<AssemblyContactAnalysis>::failure(posed_leaves.error());
  }

  std::vector<const detail::PosedLeafShape*> ordered_leaves;
  ordered_leaves.reserve(posed_leaves.value().leaves.size());
  for (const detail::PosedLeafShape& posed : posed_leaves.value().leaves) {
    ordered_leaves.push_back(&posed);
  }
  std::sort(ordered_leaves.begin(), ordered_leaves.end(),
            [](const detail::PosedLeafShape* lhs, const detail::PosedLeafShape* rhs) {
              return occurrence_less(occurrence_identity(*lhs), occurrence_identity(*rhs));
            });

  AssemblyContactAnalysis analysis;
  analysis.component_occurrence_count = ordered_leaves.size();
  analysis.recomputed_part_count = posed_leaves.value().recomputed_part_count;
  analysis.contacts.reserve((ordered_leaves.size() * (ordered_leaves.size() -
                                                       (ordered_leaves.empty() ? 0U : 1U))) /
                            2U);

  try {
    for (std::size_t first = 0U; first < ordered_leaves.size(); ++first) {
      for (std::size_t second = first + 1U; second < ordered_leaves.size(); ++second) {
        const detail::PosedLeafShape& leaf_a = *ordered_leaves[first];
        const detail::PosedLeafShape& leaf_b = *ordered_leaves[second];
        const AssemblyExchangeComponentOccurrenceIdentity identity_a = occurrence_identity(leaf_a);
        const AssemblyExchangeComponentOccurrenceIdentity identity_b = occurrence_identity(leaf_b);
        const std::string pair_text = occurrence_debug_text(identity_a) + " x " +
                                      occurrence_debug_text(identity_b);
        ++analysis.evaluated_pair_count;

        auto overlap = common_solid_volume_mm3(leaf_a.shape, leaf_b.shape, pair_text);
        if (overlap.has_error()) {
          return Result<AssemblyContactAnalysis>::failure(overlap.error());
        }

        AssemblyContactRecord record;
        record.pair = AssemblyComponentOccurrencePairIdentity{identity_a, identity_b};
        record.overlap_volume_mm3 = overlap.value();
        if (overlap.value() > options.minimum_overlap_volume_mm3) {
          record.classification = AssemblyContactClassification::Interfering;
          analysis.contacts.push_back(std::move(record));
          continue;
        }

        auto distance = minimum_distance_mm(leaf_a.shape, leaf_b.shape, pair_text);
        if (distance.has_error()) {
          return Result<AssemblyContactAnalysis>::failure(distance.error());
        }
        record.minimum_distance_mm = distance.value();
        record.classification = distance.value() <= options.touching_tolerance_mm
                                    ? AssemblyContactClassification::Touching
                                    : AssemblyContactClassification::Separated;
        analysis.contacts.push_back(std::move(record));
      }
    }
  } catch (const Standard_Failure& failure) {
    return Result<AssemblyContactAnalysis>::failure(
        make_geometry_error(standard_failure_message(failure)));
  } catch (const std::exception& exception) {
    return Result<AssemblyContactAnalysis>::failure(make_geometry_error(exception.what()));
  } catch (...) {
    return Result<AssemblyContactAnalysis>::failure(
        make_geometry_error("unknown assembly contact analysis error"));
  }

  return Result<AssemblyContactAnalysis>::success(std::move(analysis));
}

} // namespace blcad::geometry
