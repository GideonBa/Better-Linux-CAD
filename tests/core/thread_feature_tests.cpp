#include "../support/thread_feature_fixture.hpp"
#include "blcad/core/part_document_json.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <limits>

using namespace blcad;
namespace f = blcad::test::thread_fixture;
namespace resources = blcad::test::engineering_fixture;
using f::accept;
using f::checked;

TEST_CASE("One Thread definition retains explicit modes parameters and standard pins",
          "[core][parity-b155][thread-intent]") {
  const auto custom = f::definition();
  CHECK(custom.handedness() == ThreadHandedness::Right);
  CHECK(custom.representation() == ThreadRepresentation::Cosmetic);
  CHECK_FALSE(custom.start());
  CHECK(custom.standard_dependencies().empty());
  CHECK(custom.parameter_dependencies() ==
        std::vector{ParameterId("thread.major"), ParameterId("thread.minor"),
                    ParameterId("thread.pitch"), ParameterId("thread.angle"),
                    ParameterId("thread.length")});
  const auto standard =
      f::definition(f::selected(), true, ThreadHandedness::Left, ThreadRepresentation::Modeled);
  CHECK(standard.handedness() == ThreadHandedness::Left);
  CHECK(standard.representation() == ThreadRepresentation::Modeled);
  CHECK(standard.standard_dependencies() == std::vector{f::selected().table});
  CHECK(standard.parameter_dependencies() ==
        std::vector{ParameterId("thread.start"), ParameterId("thread.length")});
  auto profile = f::custom(true);
  profile.pitch = profile.major_diameter;
  const auto shared = checked(
      ThreadDefinition::create(profile, ParameterId("thread.major"), ParameterId("thread.minor")));
  CHECK(shared.parameter_dependencies() ==
        std::vector{ParameterId("thread.major"), ParameterId("thread.minor"),
                    ParameterId("thread.angle"), ParameterId("thread.taper")});
  CHECK(to_string(ThreadHandedness::Right) == "right");
  CHECK(to_string(ThreadHandedness::Left) == "left");
  CHECK(to_string(ThreadRepresentation::Cosmetic) == "cosmetic");
  CHECK(to_string(ThreadRepresentation::Modeled) == "modeled");
  CHECK(to_string(ThreadDirection::AlongSupportAxis) == "along_support_axis");
  CHECK(to_string(ThreadDirection::OppositeSupportAxis) == "opposite_support_axis");
  CHECK(to_string(static_cast<ThreadHandedness>(99)) == "unknown");
  CHECK(to_string(static_cast<ThreadRepresentation>(99)) == "unknown");
  CHECK(to_string(static_cast<ThreadDirection>(99)) == "unknown");
}

TEST_CASE("Thread factories reject incomplete selectors and unknown modes without repair",
          "[core][parity-b155][thread-intent]") {
  const auto create = [](ThreadProfile p) {
    return ThreadDefinition::create(std::move(p), ParameterId("thread.length"));
  };
  for (auto member : {&ThreadParameterProfile::system, &ThreadParameterProfile::designation,
                      &ThreadParameterProfile::tolerance}) {
    auto p = f::custom();
    p.*member = " \t\n";
    CHECK_FALSE(create(p));
  }
  for (auto member :
       {&ThreadParameterProfile::major_diameter, &ThreadParameterProfile::minor_diameter,
        &ThreadParameterProfile::pitch, &ThreadParameterProfile::included_angle}) {
    auto p = f::custom();
    p.*member = ParameterId{};
    CHECK_FALSE(create(p));
  }
  auto p = f::custom();
  p.taper_angle = ParameterId{};
  CHECK_FALSE(create(p));
  CHECK_FALSE(ThreadDefinition::create(f::custom(), ParameterId{}));
  CHECK_FALSE(ThreadDefinition::create(f::custom(), ParameterId("thread.length"), ParameterId{}));
  CHECK_FALSE(ThreadDefinition::create(f::custom(), ParameterId("thread.length"), {},
                                       static_cast<ThreadHandedness>(99)));
  CHECK_FALSE(ThreadDefinition::create(f::custom(), ParameterId("thread.length"), {},
                                       ThreadHandedness::Right,
                                       static_cast<ThreadRepresentation>(99)));
  auto selected = f::selected();
  selected.row = " ";
  CHECK_FALSE(create(selected));
  for (auto member : {&ThreadStandardColumns::system, &ThreadStandardColumns::designation,
                      &ThreadStandardColumns::tolerance, &ThreadStandardColumns::major_diameter,
                      &ThreadStandardColumns::minor_diameter, &ThreadStandardColumns::pitch,
                      &ThreadStandardColumns::included_angle}) {
    auto s = f::selected();
    s.columns.*member = "";
    CHECK_FALSE(create(s));
  }
  selected = f::selected();
  selected.columns.taper_angle = " ";
  CHECK_FALSE(create(selected));
}

TEST_CASE("Thread parameter resolution is read only and keeps start length and hand independent",
          "[core][parity-b155][thread-inputs]") {
  auto part = f::part();
  const auto json = checked(serialize_part_document_to_json(part));
  const auto generation = part.executable_generation();
  for (const auto hand : {ThreadHandedness::Right, ThreadHandedness::Left})
    for (const auto mode : {ThreadRepresentation::Cosmetic, ThreadRepresentation::Modeled}) {
      const auto r =
          checked(resolve_thread_definition(part, f::definition(f::custom(), true, hand, mode)));
      CHECK(r.system == "Synthetic system");
      CHECK(r.designation == "Synthetic 10");
      CHECK(r.tolerance == "Authored tolerance");
      CHECK(r.major_diameter_mm == 10);
      CHECK(r.minor_diameter_mm == 8);
      CHECK(r.pitch_mm == Catch::Approx(1.7));
      CHECK(r.included_angle_deg == 55);
      CHECK(r.taper_angle_deg == 0);
      CHECK(r.start_mm == 1);
      CHECK(r.length_mm == 4);
      CHECK(r.handedness == hand);
      CHECK(r.representation == mode);
      CHECK_FALSE(r.standard);
      CHECK(r.provenance.empty());
    }
  CHECK(checked(resolve_thread_definition(part, f::definition())).start_mm == 0);
  CHECK(part.executable_generation() == generation);
  CHECK(checked(serialize_part_document_to_json(part)) == json);
  checked(part.set_parameter_value(ParameterId("thread.pitch"), checked(Quantity::length_mm(2.3))));
  CHECK(checked(resolve_thread_definition(part, f::definition())).pitch_mm == Catch::Approx(2.3));
  CHECK(part.find_parameter(ParameterId("depth"))->value().millimeters() == 8);
}

TEST_CASE("Pinned Thread rows resolve all metadata dimensions and exact cell provenance",
          "[core][parity-b155][thread-standards]") {
  auto part = f::part();
  const auto before = checked(serialize_part_document_to_json(part));
  const auto generation = part.executable_generation();
  const auto definition = f::definition(f::selected(), true);
  const auto r = checked(resolve_thread_definition(part, definition));
  CHECK(r.system == "Synthetic table system");
  CHECK(r.designation == "Synthetic table size");
  CHECK(r.tolerance == "Synthetic table tolerance");
  CHECK(r.major_diameter_mm == 10);
  CHECK(r.minor_diameter_mm == 8);
  CHECK(r.pitch_mm == Catch::Approx(1.7));
  CHECK(r.included_angle_deg == 55);
  CHECK(r.standard == f::selected().table);
  REQUIRE(r.provenance.size() == 7);
  const std::vector<std::pair<std::string, std::string>> fields{
      {"system", "system"},        {"designation", "designation"}, {"tolerance", "tolerance"},
      {"major_diameter", "major"}, {"minor_diameter", "minor"},    {"pitch", "pitch"},
      {"included_angle", "angle"}};
  for (std::size_t i = 0; i < fields.size(); ++i) {
    CHECK(r.provenance[i].field == fields[i].first);
    CHECK(r.provenance[i].provenance.source() == "Synthetic r1/" + fields[i].second);
  }
  const auto next =
      checked(definition.remap_standard_revision(f::selected().table, f::selected("r2").table));
  const auto r2 = checked(resolve_thread_definition(part, next));
  CHECK(r2.pitch_mm == Catch::Approx(2.2));
  CHECK(r2.standard == f::selected("r2").table);
  CHECK(r2.provenance[5].provenance.source() == "Synthetic r2/pitch");
  CHECK(checked(resolve_thread_definition(part, definition)).pitch_mm == Catch::Approx(1.7));
  const auto unrelated =
      checked(definition.remap_standard_revision(resources::key(), f::selected("r2").table));
  CHECK(checked(resolve_thread_definition(part, unrelated)).standard == f::selected().table);
  CHECK(checked(serialize_part_document_to_json(part)) == before);
  CHECK(part.executable_generation() == generation);
  auto reopened = checked(deserialize_part_document_from_json(before));
  CHECK(checked(resolve_thread_definition(reopened, next)).pitch_mm == Catch::Approx(2.2));
}

TEST_CASE("Thread standard resolution rejects every missing or mistyped field without fallback",
          "[core][parity-b155][thread-standards]") {
  auto part = f::part();
  for (const auto* field :
       {"system", "designation", "tolerance", "major", "minor", "pitch", "angle", "taper"}) {
    auto s = f::selected(field, true);
    accept(part.add_engineering_resource(f::table(field, 1.7, field)));
    CHECK_FALSE(resolve_thread_definition(part, f::definition(s)));
  }
  for (auto member : {&ThreadStandardColumns::system, &ThreadStandardColumns::designation,
                      &ThreadStandardColumns::tolerance}) {
    auto s = f::selected();
    s.columns.*member = "pitch";
    CHECK_FALSE(resolve_thread_definition(part, f::definition(s)));
    s.columns.*member = "missing";
    CHECK_FALSE(resolve_thread_definition(part, f::definition(s)));
  }
  for (auto member : {&ThreadStandardColumns::major_diameter,
                      &ThreadStandardColumns::minor_diameter, &ThreadStandardColumns::pitch}) {
    auto s = f::selected();
    s.columns.*member = "angle";
    CHECK_FALSE(resolve_thread_definition(part, f::definition(s)));
    s.columns.*member = "missing";
    CHECK_FALSE(resolve_thread_definition(part, f::definition(s)));
  }
  auto s = f::selected();
  s.columns.included_angle = "pitch";
  CHECK_FALSE(resolve_thread_definition(part, f::definition(s)));
  s = f::selected();
  s.columns.taper_angle = "pitch";
  CHECK_FALSE(resolve_thread_definition(part, f::definition(s)));
  s = f::selected();
  s.row = "unknown";
  CHECK_FALSE(resolve_thread_definition(part, f::definition(s)));
  CHECK_FALSE(resolve_thread_definition(part, f::definition(f::selected("latest"))));
  accept(part.add_engineering_resource(resources::material()));
  s = f::selected();
  s.table = resources::key();
  CHECK_FALSE(resolve_thread_definition(part, f::definition(s)));
}

TEST_CASE("Pinned taper and semantic labels keep their own sources through resolution and remap",
          "[core][parity-b155][thread-standards]") {
  auto part = f::part();
  checked(part.set_parameter_value(ParameterId("thread.taper"), checked(Quantity::angle_deg(3))));
  const auto definition = f::definition(f::selected("r1", true), true, ThreadHandedness::Left);
  const auto before = checked(serialize_part_document_to_json(part));
  const auto resolved = checked(resolve_thread_definition(part, definition));
  CHECK(resolved.taper_angle_deg == -2);
  CHECK(resolved.handedness == ThreadHandedness::Left);
  REQUIRE(resolved.provenance.size() == 8);
  CHECK(resolved.provenance.back().field == "taper_angle");
  CHECK(resolved.provenance.back().provenance.source() == "Synthetic r1/taper");
  const auto next = checked(
      definition.remap_standard_revision(f::selected().table, f::selected("unavailable").table));
  CHECK_FALSE(resolve_thread_definition(part, next));
  CHECK(checked(resolve_thread_definition(part, definition)).standard == f::selected().table);
  CHECK(checked(serialize_part_document_to_json(part)) == before);

  for (const auto* field : {"system", "designation", "tolerance"}) {
    const auto base = f::table(field);
    auto rows = base.rows();
    for (auto& cell : rows.front().cells)
      if (cell.column == field)
        cell.value = std::string(" \t\n");
    accept(part.add_engineering_resource(checked(EngineeringStandardTable::create(
        base.key(), base.name(), base.provenance(), base.columns(), std::move(rows)))));
    CHECK_FALSE(resolve_thread_definition(part, f::definition(f::selected(field))));
  }
}

TEST_CASE("Thread bounds reject invalid diameter angle taper and arithmetic ranges",
          "[core][parity-b155][thread-inputs]") {
  auto part = f::part();
  const auto set = [&](const char* id, double value, bool angle = false) {
    checked(part.set_parameter_value(
        ParameterId(id), checked(angle ? Quantity::angle_deg(value) : Quantity::length_mm(value))));
  };
  for (double minor : {10., 11.}) {
    set("thread.minor", minor);
    CHECK_FALSE(resolve_thread_definition(part, f::definition()));
  }
  set("thread.minor", 8);
  for (double angle : {-1., 0., 180., 181.}) {
    set("thread.angle", angle, true);
    CHECK_FALSE(resolve_thread_definition(part, f::definition()));
  }
  set("thread.angle", 55, true);
  for (double angle : {-90., 0., 90., 91., -80.}) {
    set("thread.taper", angle, true);
    CHECK_FALSE(resolve_thread_definition(part, f::definition(f::custom(true))));
  }
  set("thread.taper", -2, true);
  for (const auto sign : {-1., 1.}) {
    set("thread.taper", 2 * sign, true);
    const auto r = checked(resolve_thread_definition(part, f::definition(f::custom(true))));
    CHECK(r.taper_angle_deg == 2 * sign);
  }
  set("thread.length", std::numeric_limits<double>::max());
  set("thread.start", std::numeric_limits<double>::max());
  CHECK_FALSE(resolve_thread_definition(part, f::definition(f::custom(), true)));
  set("thread.pitch", std::numeric_limits<double>::min());
  CHECK_FALSE(resolve_thread_definition(part, f::definition()));
}

TEST_CASE("Thread resolution checks actual parameter types and preserves failure context",
          "[core][parity-b155][thread-inputs]") {
  const auto part = f::part();
  for (auto member : {&ThreadParameterProfile::major_diameter,
                      &ThreadParameterProfile::minor_diameter, &ThreadParameterProfile::pitch})
    for (const auto* invalid : {"missing", "count", "thread.angle"}) {
      auto p = f::custom();
      p.*member = ParameterId(invalid);
      const auto result = resolve_thread_definition(part, f::definition(p), "external.thread");
      REQUIRE_FALSE(result);
      CHECK(result.error().object_id() == "external.thread");
    }
  for (const auto* invalid : {"missing", "count", "thread.major"}) {
    auto p = f::custom();
    p.included_angle = ParameterId(invalid);
    CHECK_FALSE(resolve_thread_definition(part, f::definition(p)));
    p = f::custom();
    p.taper_angle = ParameterId(invalid);
    CHECK_FALSE(resolve_thread_definition(part, f::definition(p)));
  }
  for (const auto* invalid : {"missing", "count", "thread.angle"}) {
    CHECK_FALSE(resolve_thread_definition(
        part, checked(ThreadDefinition::create(f::custom(), ParameterId(invalid)))));
    CHECK_FALSE(resolve_thread_definition(
        part, checked(ThreadDefinition::create(f::custom(), ParameterId("thread.length"),
                                               ParameterId(invalid)))));
  }
}

TEST_CASE("External Thread input records share typed Body support and read-only boundaries",
          "[core][parity-b155][thread-inputs]") {
  auto part = f::part();
  const auto support = blcad::test::history_fixture::face(PartFeatureInputRole::ThreadSupportFace);
  CHECK(to_string(support.role()) == "thread_support_face");
  CHECK(part_feature_input_role_accepts(support.role(), PartFeatureInputCapability::Face));
  CHECK_FALSE(part_feature_input_role_accepts(support.role(), PartFeatureInputCapability::Axis));
  const auto make = [&](FeatureId id, std::string name, BodyId body, FaceReference face,
                        ThreadDirection direction = ThreadDirection::AlongSupportAxis) {
    return ThreadFeature::create(std::move(id), std::move(name), std::move(body), std::move(face),
                                 f::definition(), direction);
  };
  const auto feature =
      checked(make(FeatureId("thread"), "External thread", BodyId("body.base"), support));
  const auto before = checked(serialize_part_document_to_json(part));
  REQUIRE(validate_thread_feature_inputs(part, feature));
  CHECK(feature.id() == FeatureId("thread"));
  CHECK(feature.name() == "External thread");
  CHECK(feature.target_body() == BodyId("body.base"));
  CHECK(feature.support().role() == support.role());
  CHECK(feature.direction() == ThreadDirection::AlongSupportAxis);
  CHECK(feature.thread().representation() == ThreadRepresentation::Cosmetic);
  CHECK(checked(serialize_part_document_to_json(part)) == before);
  CHECK_FALSE(make({}, "Thread", BodyId("body.base"), support));
  CHECK_FALSE(make(FeatureId("thread"), " ", BodyId("body.base"), support));
  CHECK_FALSE(make(FeatureId("thread"), "Thread", {}, support));
  CHECK_FALSE(make(FeatureId("thread"), "Thread", BodyId("body.base"),
                   blcad::test::history_fixture::face(PartFeatureInputRole::DraftFace)));
  CHECK_FALSE(make(FeatureId("thread"), "Thread", BodyId("body.base"), support,
                   static_cast<ThreadDirection>(99)));
  CHECK_FALSE(validate_thread_feature_inputs(
      part, checked(make(FeatureId("thread"), "Thread", BodyId("missing"), support))));
  accept(part.add_body(checked(Body::create(BodyId("surface"), "Surface", BodyKind::Surface))));
  CHECK_FALSE(validate_thread_feature_inputs(
      part, checked(make(FeatureId("thread"), "Thread", BodyId("surface"), support))));
  const auto reference = blcad::test::history_fixture::imported(true);
  CHECK_FALSE(validate_thread_feature_inputs(reference, feature));
}
