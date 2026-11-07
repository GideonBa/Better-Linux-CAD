#include "blcad/core/engineering_resource.hpp"

#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <limits>

using namespace blcad;
namespace {
template <typename T> T checked(Result<T> result) {
  INFO((result ? std::string{} : result.error().message()));
  REQUIRE(result);
  return std::move(result.value());
}
EngineeringResourceKey key(std::string id = "synthetic.material", std::string revision = "r1") {
  return checked(
      EngineeringResourceKey::create(EngineeringResourceId(std::move(id)), std::move(revision)));
}
EngineeringProvenance source(std::string label = "Synthetic user fixture") {
  return checked(
      EngineeringProvenance::create(EngineeringDataOrigin::UserDefined, std::move(label)));
}
EngineeringMaterial material(std::string revision = "r1", double density = 4000,
                             std::string id = "synthetic.material") {
  EngineeringMaterialProperties properties;
  properties.density =
      SourcedEngineeringValue{checked(MassDensity::from_kg_per_m3(density)), source()};
  return checked(EngineeringMaterial::create(key(std::move(id), std::move(revision)),
                                             "Synthetic material", source(), properties));
}
std::vector<EngineeringTableColumn> columns() {
  using K = EngineeringTableValueKind;
  return {{"diameter", "Diameter", K::Length},
          {"offset", "Offset", K::LinearDisplacement},
          {"angle", "Angle", K::Angle},
          {"count", "Count", K::Count},
          {"density", "Density", K::Density},
          {"pressure", "Pressure", K::Pressure},
          {"ratio", "Ratio", K::Scalar},
          {"label", "Label", K::Text},
          {"enabled", "Enabled", K::Boolean, false}};
}
std::vector<EngineeringTableRow> rows() {
  return {{"synthetic.row",
           "Synthetic row",
           {{"diameter", checked(Quantity::length_mm(3)), source()},
            {"offset", checked(Quantity::linear_displacement_mm(-2)), source()},
            {"angle", checked(Quantity::angle_deg(-30)), source()},
            {"count", checked(Quantity::count(4)), source()},
            {"density", checked(MassDensity::from_g_per_cm3(4)), source()},
            {"pressure", checked(EngineeringPressure::from_mpa(250)), source()},
            {"ratio", 0.25, source()},
            {"label", std::string("Synthetic size"), source()},
            {"enabled", false, source()}}}};
}
Result<EngineeringStandardTable> table(std::vector<EngineeringTableColumn> c = columns(),
                                       std::vector<EngineeringTableRow> r = rows(),
                                       EngineeringResourceKey k = key("synthetic.table")) {
  return EngineeringStandardTable::create(std::move(k), "Synthetic table", source(), std::move(c),
                                          std::move(r));
}
} // namespace

TEST_CASE("Engineering density and pressure normalize explicit physical units",
          "[core][parity-b149][engineering-resource]") {
  const auto density = checked(MassDensity::from_g_per_cm3(4));
  CHECK(density.kg_per_m3() == Catch::Approx(4000));
  CHECK(density.kg_per_mm3() == Catch::Approx(4e-6));
  CHECK(density.g_per_cm3() == Catch::Approx(4));
  CHECK(checked(MassDensity::from_kg_per_mm3(4e-6)).kg_per_m3() == Catch::Approx(4000));
  const auto pressure = checked(EngineeringPressure::from_gpa(0.25));
  CHECK(pressure.pascals() == Catch::Approx(250e6));
  CHECK(pressure.megapascals() == Catch::Approx(250));
  CHECK(pressure.gigapascals() == Catch::Approx(0.25));
  CHECK(pressure == checked(EngineeringPressure::from_mpa(250)));
  for (double value :
       {0.0, -1.0, std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
    CAPTURE(value);
    CHECK_FALSE(MassDensity::from_kg_per_m3(value));
    CHECK_FALSE(MassDensity::from_g_per_cm3(value));
    CHECK_FALSE(MassDensity::from_kg_per_mm3(value));
    CHECK_FALSE(EngineeringPressure::from_pa(value));
    CHECK_FALSE(EngineeringPressure::from_mpa(value));
    CHECK_FALSE(EngineeringPressure::from_gpa(value));
  }
  const double smallest = std::numeric_limits<double>::denorm_min();
  CHECK_FALSE(MassDensity::from_kg_per_m3(smallest));
  CHECK_FALSE(MassDensity::from_g_per_cm3(smallest));
  CHECK_FALSE(EngineeringPressure::from_pa(smallest));
  CHECK_FALSE(EngineeringPressure::from_mpa(smallest));
  CHECK(checked(MassDensity::from_kg_per_mm3(smallest)).kg_per_mm3() == smallest);
  CHECK(checked(EngineeringPressure::from_gpa(smallest)).gigapascals() == smallest);
  CHECK_FALSE(EngineeringPressure::from_gpa(-1, "synthetic.modulus"));
  CHECK(EngineeringPressure::from_gpa(-1, "synthetic.modulus").error().object_id() ==
        "synthetic.modulus");
  const double largest = std::numeric_limits<double>::max();
  REQUIRE(MassDensity::from_kg_per_m3(largest));
  REQUIRE(EngineeringPressure::from_pa(largest));
  CHECK_FALSE(MassDensity::from_g_per_cm3(largest));
  CHECK_FALSE(MassDensity::from_kg_per_mm3(largest));
  CHECK_FALSE(EngineeringPressure::from_mpa(largest));
  CHECK_FALSE(EngineeringPressure::from_gpa(largest));
}

TEST_CASE("Resource revision identities and provenance reject incomplete or unknown metadata",
          "[core][parity-b149][engineering-resource]") {
  CHECK_FALSE(EngineeringResourceKey::create(EngineeringResourceId{}, "r1"));
  CHECK_FALSE(EngineeringResourceKey::create(EngineeringResourceId("valid"), " "));
  CHECK_FALSE(EngineeringResourceKey::create(EngineeringResourceId("bad\nidentifier"), "r1"));
  CHECK(key("material.one", "r1") != key("material.one", "r2"));
  CHECK(key("material.one", "r1") != key("material.two", "r1"));
  CHECK_FALSE(EngineeringProvenance::create(static_cast<EngineeringDataOrigin>(99), "Synthetic"));
  CHECK_FALSE(EngineeringProvenance::create(EngineeringDataOrigin::UserDefined, ""));
  CHECK_FALSE(EngineeringProvenance::create(EngineeringDataOrigin::Manufacturer, "Synthetic", ""));
  CHECK_FALSE(
      EngineeringProvenance::create(EngineeringDataOrigin::Normative, "Synthetic metadata only"));
  const auto provenance = checked(EngineeringProvenance::create(
      EngineeringDataOrigin::Normative, "Synthetic metadata only", "Fictional edition r1",
      "https://example.invalid/synthetic", "Synthetic license metadata"));
  CHECK(provenance.origin() == EngineeringDataOrigin::Normative);
  CHECK(provenance.reference() == "Fictional edition r1");
  CHECK(provenance.uri() == "https://example.invalid/synthetic");
  CHECK(provenance.license() == "Synthetic license metadata");
}

TEST_CASE("Material snapshots retain optional density and distinct per-property sources",
          "[core][parity-b149][engineering-resource]") {
  const auto incomplete =
      checked(EngineeringMaterial::create(key(), "Unassigned density", source()));
  CHECK_FALSE(incomplete.properties().density);
  EngineeringMaterialProperties properties;
  properties.density = SourcedEngineeringValue{checked(MassDensity::from_kg_per_m3(4000)),
                                               source("Synthetic density")};
  const auto producer = checked(EngineeringProvenance::create(
      EngineeringDataOrigin::Manufacturer, "Fictional producer", "Synthetic sheet r2"));
  properties.young_modulus =
      SourcedEngineeringValue{checked(EngineeringPressure::from_gpa(100)), producer};
  properties.yield_strength = SourcedEngineeringValue{checked(EngineeringPressure::from_mpa(40)),
                                                      source("Synthetic yield")};
  properties.tensile_strength = SourcedEngineeringValue{checked(EngineeringPressure::from_mpa(60)),
                                                        source("Synthetic tensile")};
  const auto complete =
      checked(EngineeringMaterial::create(key(), "Synthetic", source(), properties));
  CHECK(complete.properties() == properties);
  CHECK(complete.properties().young_modulus->provenance == producer);
  CHECK(complete.properties().density->provenance != producer);
  CHECK_FALSE(EngineeringMaterial::create(key(), "\n", source(), properties));
  CHECK_FALSE(incomplete.properties().density);
}

TEST_CASE("A typed standards table keeps row identities and canonicalizes cell order",
          "[core][parity-b149][engineering-resource]") {
  auto reversed = rows();
  std::reverse(reversed.front().cells.begin(), reversed.front().cells.end());
  const auto standard = checked(table(columns(), reversed));
  CHECK(standard == checked(table()));
  REQUIRE(standard.find_row("synthetic.row"));
  CHECK_FALSE(standard.find_row("missing"));
  const auto& cells = standard.find_row("synthetic.row")->cells;
  CHECK(cells.front().column == "diameter");
  CHECK(cells.back().column == "enabled");
  CHECK(std::get<Quantity>(cells[1].value).millimeters() == Catch::Approx(-2));
  CHECK(std::get<MassDensity>(cells[4].value).kg_per_m3() == Catch::Approx(4000));
  CHECK_FALSE(std::get<bool>(cells[8].value));
  const double limit = std::ldexp(1.0, std::numeric_limits<std::size_t>::digits);
  const double largest_count =
      std::numeric_limits<std::size_t>::digits <= 53 ? limit - 1 : std::nextafter(limit, 0.0);
  auto counts = rows();
  counts.front().cells[3].value = checked(Quantity::count(largest_count));
  REQUIRE(table(columns(), counts));
  counts.front().cells[3].value = checked(Quantity::count(limit));
  CHECK_FALSE(table(columns(), counts));
  auto optional = rows();
  optional.front().cells.pop_back();
  REQUIRE(table(columns(), optional));
  auto repeated_label = rows();
  repeated_label.push_back(repeated_label.front());
  repeated_label.back().key = "other.row";
  REQUIRE(table(columns(), repeated_label));
}

TEST_CASE("Standards schemas reject duplicate missing unknown and incompatible cells atomically",
          "[core][parity-b149][engineering-resource]") {
  const auto original = checked(table());
  auto c = columns();
  auto r = rows();
  SECTION("empty schema") {
    c.clear();
  }
  SECTION("duplicate columns") {
    c.push_back(c.front());
  }
  SECTION("unknown kind") {
    c.front().kind = static_cast<EngineeringTableValueKind>(99);
  }
  SECTION("duplicate rows") {
    r.push_back(r.front());
  }
  SECTION("missing required cell") {
    r.front().cells.erase(r.front().cells.begin());
  }
  SECTION("unknown column") {
    r.front().cells.front().column = "unknown";
  }
  SECTION("duplicate cells") {
    r.front().cells.push_back(r.front().cells.front());
  }
  SECTION("wrong physical dimension") {
    r.front().cells.front().value = checked(EngineeringPressure::from_pa(3));
  }
  SECTION("length is not a signed displacement") {
    r.front().cells[1].value = checked(Quantity::length_mm(2));
  }
  SECTION("unrepresentable count") {
    r.front().cells[3].value = checked(Quantity::count(1e300));
  }
  SECTION("nonfinite scalar") {
    r.front().cells[6].value = std::numeric_limits<double>::quiet_NaN();
  }
  SECTION("scalar is not text") {
    r.front().cells[7].value = 3.0;
  }
  SECTION("scalar is not boolean") {
    r.front().cells[8].value = 0.0;
  }
  const auto rejected = table(c, r);
  REQUIRE_FALSE(rejected);
  CHECK(rejected.error().category() == ErrorCategory::Validation);
  CHECK(rejected.error().object_id() == "synthetic.table");
  CHECK(original == checked(table()));
}

TEST_CASE("Resource catalogs deduplicate immutable keys and retain explicitly addressed revisions",
          "[core][parity-b149][engineering-resource]") {
  EngineeringResourceCatalog catalog;
  const auto r1 = material();
  CHECK(checked(catalog.insert(r1)) == 1);
  auto unit_properties = r1.properties();
  unit_properties.density =
      SourcedEngineeringValue{checked(MassDensity::from_g_per_cm3(4)), source()};
  const auto equivalent =
      checked(EngineeringMaterial::create(r1.key(), r1.name(), r1.provenance(), unit_properties));
  CHECK(checked(catalog.insert(equivalent)) == 0);
  CHECK(checked(catalog.insert(material("r2", 5000))) == 1);
  REQUIRE(catalog.find_material(r1.key()));
  CHECK(catalog.find_material(r1.key())->properties().density->value.kg_per_m3() ==
        Catch::Approx(4000));
  CHECK(catalog.find_material(key("synthetic.material", "r2"))
            ->properties()
            .density->value.kg_per_m3() == Catch::Approx(5000));
  const auto before = catalog;
  CHECK_FALSE(catalog.insert(material("r1", 6000)));
  CHECK(catalog == before);
  CHECK_FALSE(catalog.insert(checked(table(columns(), rows(), key("synthetic.material", "r3")))));
  CHECK(catalog == before);
  CHECK_FALSE(catalog.find_table(r1.key()));
  CHECK_FALSE(catalog.find(key("missing")));
  CHECK(checked(catalog.insert(material("r1", 4000, "different.id"))) == 1);
}

TEST_CASE("A late merge conflict preserves every target and supplier snapshot",
          "[core][parity-b149][engineering-resource]") {
  EngineeringResourceCatalog target, supplier;
  REQUIRE(target.insert(material()));
  REQUIRE(supplier.insert(checked(table())));
  REQUIRE(supplier.insert(material("r1", 6000)));
  const auto target_before = target;
  const auto source_before = supplier;
  CHECK_FALSE(target.merge_from(supplier));
  CHECK(target == target_before);
  CHECK(supplier == source_before);
  EngineeringResourceCatalog good;
  REQUIRE(good.insert(material()));
  REQUIRE(good.insert(material("r2", 5000)));
  REQUIRE(good.insert(checked(table())));
  CHECK(checked(target.merge_from(good)) == 2);
  CHECK(target.entries().size() == 3);
  CHECK(checked(target.merge_from(target)) == 0);
  CHECK(target_before.entries().size() == 1);
  CHECK(target_before.find_material(key())->properties().density->value.kg_per_m3() ==
        Catch::Approx(4000));
}

TEST_CASE("Revision collision includes table contents source metadata and authored row order",
          "[core][parity-b149][engineering-resource]") {
  auto r = rows();
  r.push_back(r.front());
  r.back().key = "second.row";
  auto first = checked(table(columns(), r));
  EngineeringResourceCatalog catalog;
  REQUIRE(catalog.insert(first));
  std::reverse(r.begin(), r.end());
  CHECK_FALSE(catalog.insert(checked(table(columns(), r))));
  CHECK(catalog.find_table(first.key())->find_row("second.row"));
  const auto changed_source = checked(EngineeringStandardTable::create(
      first.key(), first.name(), source("Different synthetic provenance"), first.columns(),
      first.rows()));
  CHECK_FALSE(catalog.insert(changed_source));
  CHECK(catalog.entries().size() == 1);
  CHECK(*catalog.find_table(first.key()) == first);
}
