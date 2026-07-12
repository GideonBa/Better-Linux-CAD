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

struct OrderedPosedOccurrence {
  AssemblyExchangeComponentOccurrenceIdentity identity;
  const detail::PosedLeafShape* posed = nullptr;
};

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

[[nodiscard]] bool leaf_matches_occurrence(
    const detail::PosedLeafShape& posed,
    const AssemblyExchangeComponentOccurrenceIdentity& identity) {
  return posed.leaf.subassembly_path == identity.containing_assembly_occurrence.occurrence_path &&
         posed.leaf.component_instance == identity.component_instance;
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

[[nodiscard]] Result<std::vector<OrderedPosedOccurrence>> build_ordered_posed_occurrences(
    const Project& project, const detail::PosedLeafShapesBuild& posed_leaves) {
  auto exchange_graph = AssemblyExchangeGraph::build(project);
  if (exchange_graph.has_error()) {
    return Result<std::vector<OrderedPosedOccurrence>>::failure(exchange_graph.error());
  }
  if (exchange_graph.value().component_occurrence_count() != posed_leaves.leaves.size()) {
    return Result<std::vector<OrderedPosedOccurrence>>::failure(make_geometry_error(
        "contact posed-leaf set does not match canonical exchange component occurrence set"));
  }

  std::vector<OrderedPosedOccurrence> ordered;
  ordered.reserve(exchange_graph.value().component_occurrence_count());
  for (const AssemblyExchangeComponentOccurrence& occurrence :
       exchange_graph.value().component_occurrences()) {
    const auto found = std::find_if(
        posed_leaves.leaves.begin(), posed_leaves.leaves.end(),
        [&occurrence](const detail::PosedLeafShape& posed) {
          return leaf_matches_occurrence(posed, occurrence.identity);
        });
    if (found == posed_leaves.leaves.end()) {
      return Result<std::vector<OrderedPosedOccurrence>>::failure(make_geometry_error(
          "contact exchange component occurrence does not resolve to one posed leaf: " +
          occurrence_debug_text(occurrence.identity)));
    }
    ordered.push_back(OrderedPosedOccurrence{occurrence.identity, &*found});
  }
  return Result<std::vector<OrderedPosedOccurrence>>::success(std::move(ordered));
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
  auto ordered_occurrences = build_ordered_posed_occurrences(project, posed_leaves.value());
  if (ordered_occurrences.has_error()) {
    return Result<AssemblyContactAnalysis>::failure(ordered_occurrences.error());
  }

  AssemblyContactAnalysis analysis;
  analysis.component_occurrence_count = ordered_occurrences.value().size();
  analysis.recomputed_part_count = posed_leaves.value().recomputed_part_count;
  if (ordered_occurrences.value().size() > 1U) {
    analysis.records.reserve((ordered_occurrences.value().size() *
                              (ordered_occurrences.value().size() - 1U)) /
                             2U);
  }

  try {
    for (std::size_t first = 0U; first < ordered_occurrences.value().size(); ++first) {
      for (std::size_t second = first + 1U; second < ordered_occurrences.value().size(); ++second) {
        const OrderedPosedOccurrence& occurrence_a = ordered_occurrences.value()[first];
        const OrderedPosedOccurrence& occurrence_b = ordered_occurrences.value()[second];
        const std::string pair_text = occurrence_debug_text(occurrence_a.identity) + " x " +
                                      occurrence_debug_text(occurrence_b.identity);
        ++analysis.evaluated_pair_count;

        auto overlap = common_solid_volume_mm3(occurrence_a.posed->shape,
                                               occurrence_b.posed->shape, pair_text);
        if (overlap.has_error()) {
          return Result<AssemblyContactAnalysis>::failure(overlap.error());
        }

        AssemblyContactRecord record{
            AssemblyComponentOccurrencePairIdentity{occurrence_a.identity, occurrence_b.identity},
            AssemblyContactClassification::Separated, overlap.value(), std::nullopt};
        if (overlap.value() > options.minimum_overlap_volume_mm3) {
          record.classification = AssemblyContactClassification::Interfering;
          analysis.records.push_back(std::move(record));
          continue;
        }

        auto distance = minimum_distance_mm(occurrence_a.posed->shape,
                                            occurrence_b.posed->shape, pair_text);
        if (distance.has_error()) {
          return Result<AssemblyContactAnalysis>::failure(distance.error());
        }
        record.minimum_distance_mm = distance.value();
        record.classification = distance.value() <= options.touching_tolerance_mm
                                    ? AssemblyContactClassification::Touching
                                    : AssemblyContactClassification::Separated;
        analysis.records.push_back(std::move(record));
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
