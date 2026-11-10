#pragma once

#include "blcad/core/thread_feature.hpp"
#include "engineering_resource_fixture.hpp"
#include "part_history_fixture.hpp"

namespace blcad::test::thread_fixture {
using history_fixture::accept;
using history_fixture::checked;
inline ThreadParameterProfile custom(bool tapered = false) {
  return {"Synthetic system",
          "Synthetic 10",
          "Authored tolerance",
          ParameterId("thread.major"),
          ParameterId("thread.minor"),
          ParameterId("thread.pitch"),
          ParameterId("thread.angle"),
          tapered ? std::optional(ParameterId("thread.taper")) : std::nullopt};
}
inline ThreadStandardProfile selected(std::string revision = "r1", bool tapered = false) {
  return {engineering_fixture::key("synthetic.threads", std::move(revision)),
          "synthetic.row",
          {"system", "designation", "tolerance", "major", "minor", "pitch", "angle",
           tapered ? std::optional(std::string("taper")) : std::nullopt}};
}
inline ThreadDefinition definition(ThreadProfile profile = custom(), bool offset = false,
                                   ThreadHandedness handedness = ThreadHandedness::Right,
                                   ThreadRepresentation mode = ThreadRepresentation::Cosmetic) {
  return checked(ThreadDefinition::create(
      std::move(profile), ParameterId("thread.length"),
      offset ? std::optional(ParameterId("thread.start")) : std::nullopt, handedness, mode));
}
inline EngineeringStandardTable table(std::string revision = "r1", double pitch = 1.7,
                                      std::string omitted = {}) {
  using K = EngineeringTableValueKind;
  const auto provenance = [&](const std::string& column) {
    return checked(EngineeringProvenance::create(EngineeringDataOrigin::UserDefined,
                                                 "Synthetic " + revision + "/" + column));
  };
  std::vector<EngineeringTableColumn> columns;
  std::vector<EngineeringTableCell> cells;
  for (const auto& [column, value] : {std::pair{"system", "Synthetic table system"},
                                      {"designation", "Synthetic table size"},
                                      {"tolerance", "Synthetic table tolerance"}}) {
    columns.push_back({column, column, K::Text, omitted != column});
    if (omitted != column)
      cells.push_back({column, std::string(value), provenance(column)});
  }
  for (const auto& [column, value] :
       {std::pair{"major", 10.}, {"minor", 8.}, {"pitch", pitch}, {"angle", 55.}, {"taper", -2.}}) {
    const bool angle = std::string_view(column) == "angle" || std::string_view(column) == "taper";
    columns.push_back({column, column, angle ? K::Angle : K::Length, omitted != column});
    if (omitted != column)
      cells.push_back({column,
                       checked(angle ? Quantity::angle_deg(value) : Quantity::length_mm(value)),
                       provenance(column)});
  }
  return checked(EngineeringStandardTable::create(
      engineering_fixture::key("synthetic.threads", std::move(revision)), "Synthetic threads",
      engineering_fixture::provenance(), std::move(columns),
      {{"synthetic.row", "Synthetic row", std::move(cells)}}));
}
inline PartDocument part() {
  auto part = history_fixture::native_base();
  for (const auto& [id, value] :
       {std::pair{"major", 10.}, {"minor", 8.}, {"pitch", 1.7}, {"start", 1.}, {"length", 4.}})
    accept(part.add_parameter(checked(Parameter::create_length(
        ParameterId(std::string("thread.") + id), id, checked(Quantity::length_mm(value))))));
  for (const auto& [id, value] : {std::pair{"angle", 55.}, {"taper", -2.}})
    accept(part.add_parameter(checked(Parameter::create_angle(
        ParameterId(std::string("thread.") + id), id, checked(Quantity::angle_deg(value))))));
  accept(part.add_engineering_resource(table()));
  accept(part.add_engineering_resource(table("r2", 2.2)));
  return part;
}
} // namespace blcad::test::thread_fixture
