#include "blcad/core/hole_instance_reference.hpp"
#include "blcad/core/part_document.hpp"

#include <algorithm>
#include <charconv>

namespace blcad {
namespace {
Error error(std::string message) {
  return Error::validation("hole_instance", std::move(message));
}
std::string encode(std::string_view value) {
  constexpr char hex[] = "0123456789ABCDEF";
  std::string result;
  for (unsigned char c : value) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
        c == '-')
      result += char(c);
    else {
      result += '%';
      result += hex[c >> 4];
      result += hex[c & 15];
    }
  }
  return result;
}
Result<std::string> decode(std::string_view value) {
  const auto digit = [](char c) -> int {
    return c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
  };
  std::string result;
  for (std::size_t i = 0; i < value.size(); ++i) {
    if (value[i] != '%') {
      result += value[i];
      continue;
    }
    if (i + 2 >= value.size() || digit(value[i + 1]) < 0 || digit(value[i + 2]) < 0)
      return Result<std::string>::failure(error("Malformed Hole target byte escape"));
    result += char(16 * digit(value[i + 1]) + digit(value[i + 2]));
    i += 2;
  }
  if (encode(result) != value)
    return Result<std::string>::failure(error("Noncanonical Hole target ID"));
  return Result<std::string>::success(std::move(result));
}
} // namespace
std::string_view to_string(HoleReferenceRole role) noexcept {
  switch (role) {
  case HoleReferenceRole::Axis:
    return "axis";
  case HoleReferenceRole::Entry:
    return "entry";
  case HoleReferenceRole::Seat:
    return "seat";
  case HoleReferenceRole::Step:
    return "step";
  }
  return "unknown";
}
Result<HoleInstanceReference>
HoleInstanceReference::create(FeatureId hole, HolePlacementId placement, HoleReferenceRole role,
                              std::optional<HolePatternOccurrence> occurrence, std::string step) {
  if (hole.empty() || placement.empty() || to_string(role) == "unknown" ||
      (role == HoleReferenceRole::Step) != !step.empty() ||
      (occurrence &&
       (occurrence->pattern.empty() || occurrence->index.first >= pattern_placement_limit ||
        occurrence->index.second >= pattern_placement_limit)))
    return Result<HoleInstanceReference>::failure(
        error("Hole instance requires typed identities, bounded coordinates and an explicit Step "
              "only for step seats"));
  return Result<HoleInstanceReference>::success(HoleInstanceReference(
      std::move(hole), std::move(placement), role, std::move(occurrence), std::move(step)));
}
ResultFaceOrigin HoleInstanceReference::origin(std::string suffix) const {
  ResultFaceOrigin root{hole_, "hole/" + std::to_string(placement_.value().size()) + ":" +
                                   placement_.value() + "/" + suffix};
  if (!occurrence_)
    return root;
  return copy_result_face_origin(
      occurrence_->pattern,
      pattern_source_occurrence_key(PatternSourceReference::feature(hole_).value(),
                                    occurrence_->index),
      root);
}
Result<std::string> make_hole_target_spelling(const HoleInstanceReference& ref) {
  const auto index = ref.occurrence() ? ref.occurrence()->index : PatternOccurrenceIndex{};
  return Result<std::string>::success(
      "hole:" + encode(ref.hole().value()) + ":" + encode(ref.placement().value()) + ":" +
      std::string(to_string(ref.role())) + ":" + encode(ref.step()) + ":" +
      (ref.occurrence() ? encode(ref.occurrence()->pattern.value()) : "") + ":" +
      std::to_string(index.first) + ":" + std::to_string(index.second));
}
bool is_hole_target_spelling(std::string_view value) noexcept {
  return value.starts_with("hole:");
}
Result<HoleInstanceReference> parse_hole_target_spelling(std::string_view value) {
  const auto fail = [] {
    return Result<HoleInstanceReference>::failure(error("Malformed or noncanonical Hole target"));
  };
  std::vector<std::string_view> fields;
  std::size_t from = 0;
  for (std::size_t i = 0; i <= value.size(); ++i)
    if (i == value.size() || value[i] == ':') {
      fields.push_back(value.substr(from, i - from));
      from = i + 1;
    }
  if (fields.size() != 8 || fields[0] != "hole")
    return fail();
  auto hole = decode(fields[1]), placement = decode(fields[2]), step = decode(fields[4]),
       pattern = decode(fields[5]);
  if (!hole || !placement || !step || !pattern)
    return fail();
  std::optional<HoleReferenceRole> role;
  for (auto r : {HoleReferenceRole::Axis, HoleReferenceRole::Entry, HoleReferenceRole::Seat,
                 HoleReferenceRole::Step})
    if (fields[3] == to_string(r))
      role = r;
  if (!role)
    return fail();
  PatternOccurrenceIndex index;
  for (const auto& [text, destination] :
       {std::pair{fields[6], &index.first}, std::pair{fields[7], &index.second}}) {
    auto parsed = std::from_chars(text.data(), text.data() + text.size(), *destination);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() ||
        std::to_string(*destination) != text)
      return fail();
  }
  if (pattern.value().empty() && index != PatternOccurrenceIndex{})
    return fail();
  auto result = HoleInstanceReference::create(
      FeatureId(hole.value()), HolePlacementId(placement.value()), *role,
      pattern.value().empty()
          ? std::nullopt
          : std::optional(HolePatternOccurrence{FeatureId(pattern.value()), index}),
      step.value());
  if (!result || make_hole_target_spelling(result.value()).value() != value)
    return fail();
  return result;
}
Result<BodyId> validate_hole_instance_reference(const PartDocument& part,
                                                const HoleInstanceReference& ref) {
  const auto fail = [&] {
    return Result<BodyId>::failure(Error::dependency(
        ref.producer().value(),
        "Hole placement or Pattern occurrence is missing, excluded or incompatible"));
  };
  const auto* hole = part.find_hole_feature(ref.hole());
  if (!hole || std::holds_alternative<HoleSlot>(hole->manufacturing()) ||
      std::none_of(hole->placements().begin(), hole->placements().end(),
                   [&](const auto& p) { return p.id == ref.placement(); }))
    return fail();
  if (ref.role() == HoleReferenceRole::Seat &&
      !std::holds_alternative<HoleCounterbore>(hole->manufacturing()) &&
      !std::holds_alternative<HoleSpotface>(hole->manufacturing()))
    return fail();
  if (ref.role() == HoleReferenceRole::Step) {
    const auto* steps = std::get_if<HoleStepped>(&hole->manufacturing());
    if (!steps || std::none_of(steps->steps.begin(), steps->steps.end(),
                               [&](const auto& s) { return s.id == ref.step(); }))
      return fail();
  }
  if (ref.occurrence()) {
    const auto check = [&](const auto* pattern) {
      if (!pattern || !pattern->placements() ||
          std::none_of(pattern->sources().begin(), pattern->sources().end(), [&](const auto& s) {
            return s.kind() == PatternSourceKind::Feature &&
                   std::get<FeatureId>(s.source()) == ref.hole();
          }))
        return false;
      auto layout = evaluate_pattern_placements(part, *pattern);
      return layout &&
             std::any_of(layout.value().begin(), layout.value().end(), [&](const auto& p) {
               return p.index == ref.occurrence()->index && !p.skipped;
             });
    };
    if (!check(part.find_linear_pattern_feature(ref.occurrence()->pattern)) &&
        !check(part.find_circular_pattern_feature(ref.occurrence()->pattern)))
      return fail();
  }
  return Result<BodyId>::success(hole->target_body());
}
} // namespace blcad
