#include "blcad/core/datum_axis.hpp"
#include "blcad/core/parameter.hpp"
#include "blcad/core/part_document.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace blcad;

namespace {

Parameter make_length_parameter(const char* id, const char* name, double value_mm) {
  auto quantity = Quantity::length_mm(value_mm, id);
  REQUIRE(quantity);

  auto parameter = Parameter::create_length(ParameterId(id), name, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

} // namespace

TEST_CASE("DatumAxis creates explicit axis intent", "[core][datum-axis]") {
  const auto axis = DatumAxis::create_explicit(DatumAxisId("datum_axis.spindle"), "Spindle",
                                               Point3{1.0, 2.0, 3.0}, Vector3{0.0, 0.0, 1.0},
                                               {ParameterId("part.height")});

  REQUIRE(axis);
  CHECK(axis.value().id().value() == "datum_axis.spindle");
  CHECK(axis.value().name() == "Spindle");
  CHECK(axis.value().kind() == DatumAxisKind::Explicit);
  CHECK(axis.value().origin() == Point3{1.0, 2.0, 3.0});
  CHECK(axis.value().direction() == Vector3{0.0, 0.0, 1.0});
  REQUIRE(axis.value().parameter_dependencies().size() == 1U);
  CHECK(axis.value().parameter_dependencies().front().value() == "part.height");
  CHECK(axis.value().source_construction_line().empty());
}

TEST_CASE("DatumAxis rejects invalid explicit intent", "[core][datum-axis]") {
  SECTION("empty id") {
    const auto axis =
        DatumAxis::create_explicit(DatumAxisId(), "Spindle", Point3{}, Vector3{0.0, 0.0, 1.0});
    REQUIRE(axis.has_error());
    CHECK(axis.error().message() == "datum axis id must not be empty");
  }
  SECTION("empty name") {
    const auto axis = DatumAxis::create_explicit(DatumAxisId("datum_axis.spindle"), "", Point3{},
                                                 Vector3{0.0, 0.0, 1.0});
    REQUIRE(axis.has_error());
    CHECK(axis.error().message() == "datum axis name must not be empty");
  }
  SECTION("zero direction") {
    const auto axis = DatumAxis::create_explicit(DatumAxisId("datum_axis.spindle"), "Spindle",
                                                 Point3{}, Vector3{});
    REQUIRE(axis.has_error());
    CHECK(axis.error().message() == "datum axis direction must not be zero length");
  }
  SECTION("non-unit direction") {
    const auto axis = DatumAxis::create_explicit(DatumAxisId("datum_axis.spindle"), "Spindle",
                                                 Point3{}, Vector3{0.0, 0.0, 2.0});
    REQUIRE(axis.has_error());
    CHECK(axis.error().message() == "datum axis direction must be unit length");
  }
  SECTION("empty parameter dependency") {
    const auto axis = DatumAxis::create_explicit(DatumAxisId("datum_axis.spindle"), "Spindle",
                                                 Point3{}, Vector3{0.0, 0.0, 1.0}, {ParameterId()});
    REQUIRE(axis.has_error());
    CHECK(axis.error().message() == "datum axis parameter dependencies must not be empty");
  }
  SECTION("duplicate parameter dependencies") {
    const auto axis = DatumAxis::create_explicit(
        DatumAxisId("datum_axis.spindle"), "Spindle", Point3{}, Vector3{0.0, 0.0, 1.0},
        {ParameterId("part.height"), ParameterId("part.height")});
    REQUIRE(axis.has_error());
    CHECK(axis.error().message() == "datum axis parameter dependencies must be unique");
  }
}

TEST_CASE("DatumAxis creates construction-line-derived axis intent", "[core][datum-axis]") {
  const auto axis = DatumAxis::create_from_construction_line(
      DatumAxisId("datum_axis.from_line"), "FromLine", ConstructionLineId("construction.center"));

  REQUIRE(axis);
  CHECK(axis.value().kind() == DatumAxisKind::FromConstructionLine);
  CHECK(axis.value().source_construction_line().value() == "construction.center");
  CHECK(axis.value().parameter_dependencies().empty());
  CHECK(axis.value().origin() == Point3{});
  CHECK(axis.value().direction() == Vector3{});
}

TEST_CASE("DatumAxis rejects empty derived source line", "[core][datum-axis]") {
  const auto axis = DatumAxis::create_from_construction_line(DatumAxisId("datum_axis.from_line"),
                                                             "FromLine", ConstructionLineId());
  REQUIRE(axis.has_error());
  CHECK(axis.error().message() ==
        "construction-line-derived datum axis source line id must not be empty");
}

TEST_CASE("PartDocument owns datum axes with unique ids", "[core][datum-axis]") {
  auto document = PartDocument::create(DocumentId("part.axis_owner"), "AxisOwner");
  REQUIRE(document);

  auto axis = DatumAxis::create_explicit(DatumAxisId("datum_axis.spindle"), "Spindle", Point3{},
                                         Vector3{0.0, 0.0, 1.0});
  REQUIRE(axis);
  REQUIRE(document.value().add_datum_axis(axis.value()));
  CHECK(document.value().datum_axis_count() == 1U);
  REQUIRE(document.value().find_datum_axis(DatumAxisId("datum_axis.spindle")) != nullptr);
  CHECK(document.value().find_datum_axis(DatumAxisId("datum_axis.other")) == nullptr);

  const auto duplicate = document.value().add_datum_axis(axis.value());
  REQUIRE(duplicate.has_error());
  CHECK(duplicate.error().message() == "datum axis id must be unique within part document");
}

TEST_CASE("PartDocument validates datum axis dependencies", "[core][datum-axis]") {
  auto document = PartDocument::create(DocumentId("part.axis_dependencies"), "AxisDependencies");
  REQUIRE(document);

  SECTION("missing parameter dependency") {
    auto axis = DatumAxis::create_explicit(DatumAxisId("datum_axis.spindle"), "Spindle", Point3{},
                                           Vector3{0.0, 0.0, 1.0}, {ParameterId("part.missing")});
    REQUIRE(axis);
    const auto added = document.value().add_datum_axis(std::move(axis.value()));
    REQUIRE(added.has_error());
    CHECK(added.error().message() == "datum axis parameter dependency must exist in part document");
  }

  SECTION("missing source construction line") {
    auto axis =
        DatumAxis::create_from_construction_line(DatumAxisId("datum_axis.from_line"), "FromLine",
                                                 ConstructionLineId("construction.missing"));
    REQUIRE(axis);
    const auto added = document.value().add_datum_axis(std::move(axis.value()));
    REQUIRE(added.has_error());
    CHECK(added.error().message() ==
          "construction-line-derived datum axis source line must exist in part document");
  }
}

TEST_CASE("PartDocument invalidates datum axes through parameter dependencies",
          "[core][datum-axis]") {
  auto document = PartDocument::create(DocumentId("part.axis_invalidation"), "AxisInvalidation");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 25.0)));

  auto axis = DatumAxis::create_explicit(DatumAxisId("datum_axis.spindle"), "Spindle", Point3{},
                                         Vector3{0.0, 0.0, 1.0}, {ParameterId("part.height")});
  REQUIRE(axis);
  REQUIRE(document.value().add_datum_axis(std::move(axis.value())));
  CHECK(document.value().dependency_graph().has_dependency("part.height", "datum_axis.spindle"));

  document.value().mark_all_clean();
  REQUIRE(document.value().mark_parameter_changed(ParameterId("part.height")));
  const auto plan = document.value().create_recompute_plan();
  REQUIRE(plan);
  CHECK(plan.value().contains("datum_axis.spindle"));
}

TEST_CASE("PartDocument invalidates derived datum axes through their source line",
          "[core][datum-axis]") {
  auto document =
      PartDocument::create(DocumentId("part.axis_line_invalidation"), "AxisLineInvalidation");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(make_length_parameter("part.offset", "offset", 5.0)));

  auto line = ConstructionLine::create_explicit(ConstructionLineId("construction.center"), "Center",
                                                Point3{}, Vector3{1.0, 0.0, 0.0},
                                                {ParameterId("part.offset")});
  REQUIRE(line);
  REQUIRE(document.value().add_construction_line(std::move(line.value())));

  auto axis = DatumAxis::create_from_construction_line(
      DatumAxisId("datum_axis.from_line"), "FromLine", ConstructionLineId("construction.center"));
  REQUIRE(axis);
  REQUIRE(document.value().add_datum_axis(std::move(axis.value())));
  CHECK(document.value().dependency_graph().has_dependency("construction.center",
                                                           "datum_axis.from_line"));

  document.value().mark_all_clean();
  REQUIRE(document.value().mark_parameter_changed(ParameterId("part.offset")));
  const auto plan = document.value().create_recompute_plan();
  REQUIRE(plan);
  CHECK(plan.value().contains("construction.center"));
  CHECK(plan.value().contains("datum_axis.from_line"));
}
