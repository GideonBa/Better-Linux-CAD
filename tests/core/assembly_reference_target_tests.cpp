#include "blcad/core/assembly_constraint.hpp"
#include "blcad/core/assembly_reference_target.hpp"
#include "blcad/core/project_json.hpp"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <string>
#include <variant>

using namespace blcad;

TEST_CASE("Reference target spellings use one canonical prefix per family",
          "[core][assembly-reference-target-intent]") {
  const auto plane = make_assembly_reference_target_spelling(DatumPlaneId("datum.xy"));
  const auto axis = make_assembly_reference_target_spelling(DatumAxisId("datum_axis.spindle"));
  const auto line =
      make_assembly_reference_target_spelling(ConstructionLineId("construction.center"));
  const auto point =
      make_assembly_reference_target_spelling(ConstructionPointId("construction.origin"));

  REQUIRE(plane);
  REQUIRE(axis);
  REQUIRE(line);
  REQUIRE(point);
  CHECK(plane.value() == "ref:datum_plane:datum%2Exy");
  CHECK(axis.value() == "ref:datum_axis:datum_axis%2Espindle");
  CHECK(line.value() == "ref:construction_line:construction%2Ecenter");
  CHECK(point.value() == "ref:construction_point:construction%2Eorigin");
}

TEST_CASE("Reference target spellings roundtrip ids containing reserved bytes",
          "[core][assembly-reference-target-intent]") {
  const std::string adversarial_ids[] = {
      "a.b",       "a/b", "a%b", "a:b",  "hole.axis", "ref:datum_axis:x",
      "a.b/c%d:e", "%2E", "top", "axis", "seat",
  };

  for (const std::string& raw : adversarial_ids) {
    const AssemblyReferenceTargetIdentity identity = DatumAxisId(raw);
    const auto spelling = make_assembly_reference_target_spelling(identity);
    REQUIRE(spelling);

    const auto parsed = parse_assembly_reference_target_spelling(spelling.value());
    REQUIRE(parsed);
    CHECK(assembly_reference_target_family(parsed.value()) ==
          AssemblyReferenceTargetFamily::DatumAxis);
    REQUIRE(std::holds_alternative<DatumAxisId>(parsed.value()));
    CHECK(std::get<DatumAxisId>(parsed.value()).value() == raw);
  }
}

TEST_CASE("Reference target spellings keep escape-like ids distinct",
          "[core][assembly-reference-target-intent]") {
  const auto dotted = make_assembly_reference_target_spelling(DatumAxisId("a.b"));
  const auto literal = make_assembly_reference_target_spelling(DatumAxisId("a%2Eb"));

  REQUIRE(dotted);
  REQUIRE(literal);
  CHECK(dotted.value() == "ref:datum_axis:a%2Eb");
  CHECK(literal.value() == "ref:datum_axis:a%252Eb");
  CHECK(dotted.value() != literal.value());

  const auto dotted_parsed = parse_assembly_reference_target_spelling(dotted.value());
  const auto literal_parsed = parse_assembly_reference_target_spelling(literal.value());
  REQUIRE(dotted_parsed);
  REQUIRE(literal_parsed);
  CHECK(std::get<DatumAxisId>(dotted_parsed.value()).value() == "a.b");
  CHECK(std::get<DatumAxisId>(literal_parsed.value()).value() == "a%2Eb");
}

TEST_CASE("Reference target spellings stay disjoint from feature target spellings",
          "[core][assembly-reference-target-intent]") {
  // Feature target spellings always contain a '.' role separator. Reference
  // spellings escape every '.' so the two valid string sets cannot intersect.
  const std::string adversarial_ids[] = {"a.b", "hole.axis", "base.top", "x.seat"};
  for (const std::string& raw : adversarial_ids) {
    const auto spelling = make_assembly_reference_target_spelling(DatumPlaneId(raw));
    REQUIRE(spelling);
    CHECK(spelling.value().find('.') == std::string::npos);
  }

  // Existing feature target spellings never carry the reserved prefix shape
  // with an escaped id, so they fail closed at this parser.
  CHECK(parse_assembly_reference_target_spelling("hole.axis").has_error());
  CHECK(parse_assembly_reference_target_spelling("base.top").has_error());
  CHECK(parse_assembly_reference_target_spelling("seat.seat").has_error());

  // A feature id deliberately mimicking the prefix still cannot form a valid
  // reference spelling because its '.' role separator stays unescaped.
  CHECK(parse_assembly_reference_target_spelling("ref:datum_axis:x.axis").has_error());
}

TEST_CASE("Reference target parsing fails closed on malformed spellings",
          "[core][assembly-reference-target-intent]") {
  CHECK(parse_assembly_reference_target_spelling("").has_error());
  CHECK(parse_assembly_reference_target_spelling("ref:").has_error());
  CHECK(parse_assembly_reference_target_spelling("ref:datum_plane").has_error());
  CHECK(parse_assembly_reference_target_spelling("ref:datum_plane:").has_error());
  CHECK(parse_assembly_reference_target_spelling("ref:unknown_family:abc").has_error());
  CHECK(parse_assembly_reference_target_spelling("ref:datum_axis:a%2").has_error());
  CHECK(parse_assembly_reference_target_spelling("ref:datum_axis:a%2e").has_error());
  CHECK(parse_assembly_reference_target_spelling("ref:datum_axis:a%GG").has_error());
  CHECK(parse_assembly_reference_target_spelling("ref:datum_axis:%41").has_error());
  CHECK(parse_assembly_reference_target_spelling("ref:datum_axis:a b").has_error());
  CHECK(parse_assembly_reference_target_spelling("ref:datum_axis:a/b").has_error());
  CHECK(parse_assembly_reference_target_spelling("REF:datum_axis:abc").has_error());
}

TEST_CASE("Reference target spelling construction rejects empty ids",
          "[core][assembly-reference-target-intent]") {
  const auto spelling = make_assembly_reference_target_spelling(DatumPlaneId());
  REQUIRE(spelling.has_error());
  CHECK(spelling.error().message() == "assembly reference target id must not be empty");
}

TEST_CASE("Reference target prefix detection stays a cheap syntactic check",
          "[core][assembly-reference-target-intent]") {
  CHECK(is_assembly_reference_target_spelling("ref:datum_plane:datum%2Exy"));
  CHECK(is_assembly_reference_target_spelling("ref:junk"));
  CHECK_FALSE(is_assembly_reference_target_spelling("ref:"));
  CHECK_FALSE(is_assembly_reference_target_spelling("hole.axis"));
  CHECK_FALSE(is_assembly_reference_target_spelling(""));
}

TEST_CASE("Assembly endpoints carry reference spellings unchanged",
          "[core][assembly-reference-target-intent]") {
  const auto spelling = make_assembly_reference_target_spelling(DatumAxisId("datum_axis.spindle"));
  REQUIRE(spelling);

  const auto local_target =
      AssemblyConstraintTarget::create(ComponentInstanceId("component.shaft"), spelling.value());
  REQUIRE(local_target);
  CHECK(local_target.value().semantic_reference() == spelling.value());

  const auto hierarchy_endpoint = AssemblyHierarchyConstraintEndpoint::create(
      {SubassemblyInstanceId("occurrence.drive")}, ComponentInstanceId("component.shaft"),
      spelling.value());
  REQUIRE(hierarchy_endpoint);
  CHECK(hierarchy_endpoint.value().semantic_reference() == spelling.value());

  const auto roundtrip =
      parse_assembly_reference_target_spelling(hierarchy_endpoint.value().semantic_reference());
  REQUIRE(roundtrip);
  CHECK(std::get<DatumAxisId>(roundtrip.value()).value() == "datum_axis.spindle");
}

TEST_CASE("Explicit DatumPlane frame targets preserve canonical identity without changing plain "
          "references",
          "[core][step-reference-part][assembly-reference-target-intent]") {
  const auto frame = make_assembly_frame_target_spelling(DatumPlaneId("datum.xy"));
  REQUIRE(frame);
  CHECK(frame.value() == "frame:ref:datum_plane:datum%2Exy");
  const auto parsed = parse_assembly_frame_target_spelling(frame.value());
  REQUIRE(parsed);
  CHECK(parsed.value() == DatumPlaneId("datum.xy"));
  CHECK_FALSE(parse_assembly_reference_target_spelling(frame.value()));
  const auto plain = make_assembly_reference_target_spelling(DatumPlaneId("datum.xy"));
  REQUIRE(plain);
  CHECK(plain.value() == "ref:datum_plane:datum%2Exy");
  CHECK_FALSE(parse_assembly_frame_target_spelling(plain.value()));
  for (const std::string raw : {"a.b/c%d:e", "frame:ref:datum_plane:x", "a%2Eb", "a.b"}) {
    const auto spelled = make_assembly_frame_target_spelling(DatumPlaneId(raw));
    REQUIRE(spelled);
    const auto loaded = parse_assembly_frame_target_spelling(spelled.value());
    REQUIRE(loaded);
    CHECK(loaded.value().value() == raw);
    CHECK(spelled.value().find('.') == std::string::npos);
  }
  CHECK_FALSE(make_assembly_frame_target_spelling(DatumPlaneId{}));
  CHECK(is_assembly_frame_target_spelling("frame:"));
  CHECK_FALSE(is_assembly_frame_target_spelling("ref:datum_plane:datum%2Exy"));
  for (const auto* invalid :
       {"", "frame:", "FRAME:ref:datum_plane:x", "frame:frame:ref:datum_plane:x",
        "frame:ref:ref:datum_plane:x", "frame:ref:datum_axis:x", "frame:ref:construction_point:x",
        "frame:ref:construction_line:x", "frame:ref:datum_plane:", "frame:ref:datum_plane:datum.xy",
        "frame:ref:datum_plane:x%2e", "frame:ref:datum_plane:x%2", "frame:ref:datum_plane:x%GG",
        "frame:ref:datum_plane:%41", "frame:ref:datum_plane:x:y", "frame:ref:datum_plane:x/y"}) {
    INFO(invalid);
    CHECK_FALSE(parse_assembly_frame_target_spelling(invalid));
    if (is_assembly_frame_target_spelling(invalid)) {
      CHECK_FALSE(AssemblyConstraintTarget::create(ComponentInstanceId("component"), invalid));
      CHECK_FALSE(AssemblyHierarchyConstraintEndpoint::create({}, ComponentInstanceId("component"),
                                                              invalid));
    }
  }
}

TEST_CASE("Explicit frame joint endpoints roundtrip through ordinary Project persistence",
          "[core][step-reference-part][assembly-reference-target-intent]") {
  auto part = PartDocument::create(DocumentId("part"), "Part");
  REQUIRE(part);
  REQUIRE(part.value().add_datum_plane(DatumPlane::xy().value()));
  auto assembly = AssemblyDocument::create(DocumentId("assembly"), "Assembly");
  REQUIRE(assembly);
  REQUIRE(assembly.value().add_member_part(part.value().id()));
  for (const auto* id : {"a", "b"}) {
    auto instance = ComponentInstance::create(ComponentInstanceId(id), id, part.value().id());
    REQUIRE(instance);
    REQUIRE(assembly.value().add_component_instance(instance.value()));
  }
  const auto frame = make_assembly_frame_target_spelling(DatumPlaneId("datum.xy"));
  REQUIRE(frame);
  auto a = AssemblyConstraintTarget::create(ComponentInstanceId("a"), frame.value());
  auto b = AssemblyConstraintTarget::create(ComponentInstanceId("b"), frame.value());
  REQUIRE(a);
  REQUIRE(b);
  auto joint = AssemblyJoint::create(
      AssemblyJointId("joint"), "Explicit frame joint", AssemblyJointType::Revolute, a.value(),
      b.value(), AssemblyJointState::Active, Quantity::angle_deg(-180).value(),
      Quantity::angle_deg(180).value(), Quantity::angle_deg(0).value());
  REQUIRE(joint);
  REQUIRE(assembly.value().add_joint(joint.value()));
  auto project = Project::create(DocumentId("project"), "Project", assembly.value());
  REQUIRE(project);
  REQUIRE(project.value().add_part_document(part.value()));
  const auto saved = serialize_project_to_json(project.value());
  REQUIRE(saved);
  const auto loaded = deserialize_project_from_json(saved.value());
  REQUIRE(loaded);
  CHECK(loaded.value().assembly().joints()[0].target_a().semantic_reference() == frame.value());
  CHECK(serialize_project_to_json(loaded.value()).value() == saved.value());
  auto invalid = nlohmann::json::parse(saved.value());
  invalid["assembly"]["assembly_joints"][0]["target_a"]["semantic_reference"] =
      "frame:ref:datum_axis:x";
  CHECK_FALSE(deserialize_project_from_json(invalid.dump()));
}
