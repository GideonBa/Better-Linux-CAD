#include "blcad/core/generated_topology_reference.hpp"

#include "blcad/core/error.hpp"
#include "blcad/core/feature.hpp"
#include "blcad/core/part_document.hpp"
#include "blcad/core/sketch.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace blcad {
namespace {

constexpr std::string_view kTopologyPrefix = "topo:";
constexpr std::string_view kCylindricalFaceToken = "cylindrical_face";
constexpr std::string_view kLinearEdgeToken = "linear_edge";
constexpr std::string_view kCircularEdgeToken = "circular_edge";
constexpr std::string_view kVertexToken = "vertex";

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
}

[[nodiscard]] bool is_unreserved(unsigned char byte) noexcept {
  return (byte >= static_cast<unsigned char>('A') && byte <= static_cast<unsigned char>('Z')) ||
         (byte >= static_cast<unsigned char>('a') && byte <= static_cast<unsigned char>('z')) ||
         (byte >= static_cast<unsigned char>('0') && byte <= static_cast<unsigned char>('9')) ||
         byte == static_cast<unsigned char>('_') || byte == static_cast<unsigned char>('-');
}

[[nodiscard]] std::string encode_id(const std::string& value) {
  constexpr char kHexDigits[] = "0123456789ABCDEF";
  std::string encoded;
  encoded.reserve(value.size());
  for (const unsigned char byte : value) {
    if (is_unreserved(byte)) {
      encoded.push_back(static_cast<char>(byte));
      continue;
    }
    encoded.push_back('%');
    encoded.push_back(kHexDigits[(byte >> 4U) & 0x0FU]);
    encoded.push_back(kHexDigits[byte & 0x0FU]);
  }
  return encoded;
}

[[nodiscard]] std::optional<unsigned char> decode_hex_digit(char digit) noexcept {
  if (digit >= '0' && digit <= '9')
    return static_cast<unsigned char>(digit - '0');
  if (digit >= 'A' && digit <= 'F')
    return static_cast<unsigned char>(10 + digit - 'A');
  return std::nullopt;
}

[[nodiscard]] Result<std::string> decode_id(std::string_view spelling, std::string_view encoded) {
  std::string decoded;
  decoded.reserve(encoded.size());
  for (std::size_t index = 0U; index < encoded.size(); ++index) {
    const char byte = encoded[index];
    if (byte == '%') {
      if (index + 2U >= encoded.size()) {
        return Result<std::string>::failure(validation_error(
            std::string(spelling), "generated topology escape sequence is truncated"));
      }
      const auto high = decode_hex_digit(encoded[index + 1U]);
      const auto low = decode_hex_digit(encoded[index + 2U]);
      if (!high.has_value() || !low.has_value()) {
        return Result<std::string>::failure(validation_error(
            std::string(spelling),
            "generated topology escape sequence must use two uppercase hex digits"));
      }
      const auto decoded_byte =
          static_cast<unsigned char>((high.value() << 4U) | low.value());
      if (is_unreserved(decoded_byte)) {
        return Result<std::string>::failure(validation_error(
            std::string(spelling),
            "generated topology spelling must not escape unreserved id bytes"));
      }
      decoded.push_back(static_cast<char>(decoded_byte));
      index += 2U;
      continue;
    }
    if (!is_unreserved(static_cast<unsigned char>(byte))) {
      return Result<std::string>::failure(validation_error(
          std::string(spelling), "generated topology id byte must be unreserved or escaped"));
    }
    decoded.push_back(byte);
  }
  if (decoded.empty()) {
    return Result<std::string>::failure(
        validation_error(std::string(spelling), "generated topology id must not be empty"));
  }
  return Result<std::string>::success(std::move(decoded));
}

[[nodiscard]] std::vector<std::string_view> split_segments(std::string_view value) {
  std::vector<std::string_view> segments;
  std::size_t begin = 0U;
  while (begin <= value.size()) {
    const std::size_t separator = value.find(':', begin);
    if (separator == std::string_view::npos) {
      segments.push_back(value.substr(begin));
      break;
    }
    segments.push_back(value.substr(begin, separator - begin));
    begin = separator + 1U;
  }
  return segments;
}

[[nodiscard]] std::string_view family_token(GeneratedTopologyReferenceFamily family) noexcept {
  switch (family) {
  case GeneratedTopologyReferenceFamily::CylindricalFace:
    return kCylindricalFaceToken;
  case GeneratedTopologyReferenceFamily::LinearEdge:
    return kLinearEdgeToken;
  case GeneratedTopologyReferenceFamily::CircularEdge:
    return kCircularEdgeToken;
  case GeneratedTopologyReferenceFamily::Vertex:
    return kVertexToken;
  }
  return "unknown";
}

[[nodiscard]] std::optional<GeneratedTopologyReferenceFamily>
parse_family_token(std::string_view token) noexcept {
  if (token == kCylindricalFaceToken)
    return GeneratedTopologyReferenceFamily::CylindricalFace;
  if (token == kLinearEdgeToken)
    return GeneratedTopologyReferenceFamily::LinearEdge;
  if (token == kCircularEdgeToken)
    return GeneratedTopologyReferenceFamily::CircularEdge;
  if (token == kVertexToken)
    return GeneratedTopologyReferenceFamily::Vertex;
  return std::nullopt;
}

[[nodiscard]] std::optional<SemanticEdge> parse_linear_edge_role(std::string_view role) noexcept {
  if (role == "top_front")
    return SemanticEdge::TopFront;
  if (role == "top_back")
    return SemanticEdge::TopBack;
  if (role == "top_right")
    return SemanticEdge::TopRight;
  if (role == "top_left")
    return SemanticEdge::TopLeft;
  if (role == "bottom_front")
    return SemanticEdge::BottomFront;
  if (role == "bottom_back")
    return SemanticEdge::BottomBack;
  if (role == "bottom_right")
    return SemanticEdge::BottomRight;
  if (role == "bottom_left")
    return SemanticEdge::BottomLeft;
  if (role == "front_right")
    return SemanticEdge::FrontRight;
  if (role == "front_left")
    return SemanticEdge::FrontLeft;
  if (role == "back_right")
    return SemanticEdge::BackRight;
  if (role == "back_left")
    return SemanticEdge::BackLeft;
  return std::nullopt;
}

[[nodiscard]] std::optional<SemanticVertex> parse_vertex_role(std::string_view role) noexcept {
  if (role == "top_front_right")
    return SemanticVertex::TopFrontRight;
  if (role == "top_front_left")
    return SemanticVertex::TopFrontLeft;
  if (role == "top_back_right")
    return SemanticVertex::TopBackRight;
  if (role == "top_back_left")
    return SemanticVertex::TopBackLeft;
  if (role == "bottom_front_right")
    return SemanticVertex::BottomFrontRight;
  if (role == "bottom_front_left")
    return SemanticVertex::BottomFrontLeft;
  if (role == "bottom_back_right")
    return SemanticVertex::BottomBackRight;
  if (role == "bottom_back_left")
    return SemanticVertex::BottomBackLeft;
  return std::nullopt;
}

[[nodiscard]] std::optional<SemanticCircularEdge>
parse_circular_edge_role(std::string_view role) noexcept {
  if (role == "source_rim")
    return SemanticCircularEdge::SourceRim;
  if (role == "opposite_rim")
    return SemanticCircularEdge::OppositeRim;
  return std::nullopt;
}

[[nodiscard]] bool is_rectangular_additive_extrude(const PartDocument& document,
                                                   const Feature& feature) noexcept {
  if (feature.type() != FeatureType::AdditiveExtrude)
    return false;
  const Sketch* sketch = document.find_sketch(feature.input_sketch());
  return sketch != nullptr && sketch->rectangle_profiles().size() == 1U &&
         sketch->profile_count() == 1U;
}

[[nodiscard]] const CircleProfile*
single_circle_profile(const PartDocument& document, const Feature& feature) noexcept {
  const Sketch* sketch = document.find_sketch(feature.input_sketch());
  if (sketch == nullptr || sketch->circle_profiles().size() != 1U || sketch->profile_count() != 1U)
    return nullptr;
  return &sketch->circle_profiles().front();
}

[[nodiscard]] Result<std::size_t>
validate_role(const GeneratedTopologyReferenceIdentity& identity,
              GeneratedTopologyProducerKind producer) {
  const auto family = generated_topology_reference_family(identity);
  const auto role = generated_topology_reference_role(identity);
  const auto matrix = generated_topology_producer_role_matrix(producer);
  const auto rule = std::find_if(matrix.begin(), matrix.end(), [&](const auto& candidate) {
    return candidate.family == family && candidate.role == role;
  });
  if (rule == matrix.end()) {
    return Result<std::size_t>::failure(validation_error(
        std::string(to_string(producer)),
        "generated topology reference family/role is unsupported by the source feature producer"));
  }
  if (rule->expected_cardinality != 1U) {
    return Result<std::size_t>::failure(validation_error(
        std::string(to_string(producer)),
        "generated topology reference role is ambiguous because expected cardinality is not one"));
  }
  return Result<std::size_t>::success(rule->expected_cardinality);
}

} // namespace

std::string_view to_string(SemanticCylindricalFace face) noexcept {
  switch (face) {
  case SemanticCylindricalFace::Wall:
    return "wall";
  }
  return "wall";
}

std::string_view to_string(SemanticCircularEdge edge) noexcept {
  switch (edge) {
  case SemanticCircularEdge::SourceRim:
    return "source_rim";
  case SemanticCircularEdge::OppositeRim:
    return "opposite_rim";
  }
  return "source_rim";
}

Result<SemanticCylindricalFaceReference> SemanticCylindricalFaceReference::create(
    FeatureId source_feature, ProfileId source_profile, SemanticCylindricalFace face) {
  const auto object_id = source_feature.empty() ? std::string("semantic_cylindrical_face")
                                                : source_feature.value();
  if (source_feature.empty()) {
    return Result<SemanticCylindricalFaceReference>::failure(
        validation_error(object_id, "cylindrical face source feature id must not be empty"));
  }
  if (source_profile.empty()) {
    return Result<SemanticCylindricalFaceReference>::failure(
        validation_error(object_id, "cylindrical face source profile id must not be empty"));
  }
  return Result<SemanticCylindricalFaceReference>::success(SemanticCylindricalFaceReference(
      std::move(source_feature), std::move(source_profile), face));
}

const FeatureId& SemanticCylindricalFaceReference::source_feature() const noexcept {
  return source_feature_;
}

const ProfileId& SemanticCylindricalFaceReference::source_profile() const noexcept {
  return source_profile_;
}

SemanticCylindricalFace SemanticCylindricalFaceReference::face() const noexcept {
  return face_;
}

SemanticCylindricalFaceReference::SemanticCylindricalFaceReference(
    FeatureId source_feature, ProfileId source_profile, SemanticCylindricalFace face)
    : source_feature_(std::move(source_feature)), source_profile_(std::move(source_profile)),
      face_(face) {}

Result<SemanticCircularEdgeReference>
SemanticCircularEdgeReference::create(FeatureId source_feature, ProfileId source_profile,
                                      SemanticCircularEdge edge) {
  const auto object_id = source_feature.empty() ? std::string("semantic_circular_edge")
                                                : source_feature.value();
  if (source_feature.empty()) {
    return Result<SemanticCircularEdgeReference>::failure(
        validation_error(object_id, "circular edge source feature id must not be empty"));
  }
  if (source_profile.empty()) {
    return Result<SemanticCircularEdgeReference>::failure(
        validation_error(object_id, "circular edge source profile id must not be empty"));
  }
  return Result<SemanticCircularEdgeReference>::success(SemanticCircularEdgeReference(
      std::move(source_feature), std::move(source_profile), edge));
}

const FeatureId& SemanticCircularEdgeReference::source_feature() const noexcept {
  return source_feature_;
}

const ProfileId& SemanticCircularEdgeReference::source_profile() const noexcept {
  return source_profile_;
}

SemanticCircularEdge SemanticCircularEdgeReference::edge() const noexcept {
  return edge_;
}

SemanticCircularEdgeReference::SemanticCircularEdgeReference(FeatureId source_feature,
                                                             ProfileId source_profile,
                                                             SemanticCircularEdge edge)
    : source_feature_(std::move(source_feature)), source_profile_(std::move(source_profile)),
      edge_(edge) {}

std::string_view to_string(GeneratedTopologyReferenceFamily family) noexcept {
  switch (family) {
  case GeneratedTopologyReferenceFamily::CylindricalFace:
    return "GeneratedCylindricalFace";
  case GeneratedTopologyReferenceFamily::LinearEdge:
    return "GeneratedLinearEdge";
  case GeneratedTopologyReferenceFamily::CircularEdge:
    return "GeneratedCircularEdge";
  case GeneratedTopologyReferenceFamily::Vertex:
    return "GeneratedVertex";
  }
  return "Unknown";
}

GeneratedTopologyReferenceFamily
generated_topology_reference_family(const GeneratedTopologyReferenceIdentity& identity) noexcept {
  switch (identity.index()) {
  case 0U:
    return GeneratedTopologyReferenceFamily::CylindricalFace;
  case 1U:
    return GeneratedTopologyReferenceFamily::LinearEdge;
  case 2U:
    return GeneratedTopologyReferenceFamily::CircularEdge;
  default:
    return GeneratedTopologyReferenceFamily::Vertex;
  }
}

const FeatureId&
generated_topology_reference_source_feature(const GeneratedTopologyReferenceIdentity& identity) {
  return std::visit(
      [](const auto& reference) -> const FeatureId& { return reference.source_feature(); }, identity);
}

std::optional<ProfileId>
generated_topology_reference_source_profile(const GeneratedTopologyReferenceIdentity& identity) {
  return std::visit(
      [](const auto& reference) -> std::optional<ProfileId> {
        using Reference = std::decay_t<decltype(reference)>;
        if constexpr (std::is_same_v<Reference, SemanticCylindricalFaceReference> ||
                      std::is_same_v<Reference, SemanticCircularEdgeReference>) {
          return reference.source_profile();
        } else {
          return std::nullopt;
        }
      },
      identity);
}

std::string_view
generated_topology_reference_role(const GeneratedTopologyReferenceIdentity& identity) noexcept {
  return std::visit(
      [](const auto& reference) -> std::string_view {
        using Reference = std::decay_t<decltype(reference)>;
        if constexpr (std::is_same_v<Reference, SemanticCylindricalFaceReference>) {
          return to_string(reference.face());
        } else if constexpr (std::is_same_v<Reference, SemanticEdgeReference>) {
          return to_string(reference.edge());
        } else if constexpr (std::is_same_v<Reference, SemanticCircularEdgeReference>) {
          return to_string(reference.edge());
        } else {
          return to_string(reference.vertex());
        }
      },
      identity);
}

std::string_view to_string(GeneratedTopologyProducerKind kind) noexcept {
  switch (kind) {
  case GeneratedTopologyProducerKind::RectangularAdditiveExtrude:
    return "RectangularAdditiveExtrude";
  case GeneratedTopologyProducerKind::SingleCircleSubtractiveExtrude:
    return "SingleCircleSubtractiveExtrude";
  }
  return "Unknown";
}

std::vector<GeneratedTopologyRoleRule>
generated_topology_producer_role_matrix(GeneratedTopologyProducerKind producer) {
  std::vector<GeneratedTopologyRoleRule> matrix;
  if (producer == GeneratedTopologyProducerKind::RectangularAdditiveExtrude) {
    for (const SemanticEdge edge : {SemanticEdge::TopFront, SemanticEdge::TopBack,
                                    SemanticEdge::TopRight, SemanticEdge::TopLeft,
                                    SemanticEdge::BottomFront, SemanticEdge::BottomBack,
                                    SemanticEdge::BottomRight, SemanticEdge::BottomLeft,
                                    SemanticEdge::FrontRight, SemanticEdge::FrontLeft,
                                    SemanticEdge::BackRight, SemanticEdge::BackLeft}) {
      matrix.push_back(GeneratedTopologyRoleRule{GeneratedTopologyReferenceFamily::LinearEdge,
                                                 std::string(to_string(edge)), 1U});
    }
    for (const SemanticVertex vertex : {
             SemanticVertex::TopFrontRight, SemanticVertex::TopFrontLeft,
             SemanticVertex::TopBackRight, SemanticVertex::TopBackLeft,
             SemanticVertex::BottomFrontRight, SemanticVertex::BottomFrontLeft,
             SemanticVertex::BottomBackRight, SemanticVertex::BottomBackLeft}) {
      matrix.push_back(GeneratedTopologyRoleRule{GeneratedTopologyReferenceFamily::Vertex,
                                                 std::string(to_string(vertex)), 1U});
    }
    return matrix;
  }

  matrix.push_back(GeneratedTopologyRoleRule{GeneratedTopologyReferenceFamily::CylindricalFace,
                                             std::string(to_string(SemanticCylindricalFace::Wall)),
                                             1U});
  matrix.push_back(GeneratedTopologyRoleRule{GeneratedTopologyReferenceFamily::CircularEdge,
                                             std::string(to_string(SemanticCircularEdge::SourceRim)),
                                             1U});
  matrix.push_back(
      GeneratedTopologyRoleRule{GeneratedTopologyReferenceFamily::CircularEdge,
                                std::string(to_string(SemanticCircularEdge::OppositeRim)), 1U});
  return matrix;
}

Result<GeneratedTopologyProducerKind>
classify_generated_topology_producer(const PartDocument& document, FeatureId source_feature) {
  const auto object_id = source_feature.empty() ? std::string("generated_topology")
                                                : source_feature.value();
  if (source_feature.empty()) {
    return Result<GeneratedTopologyProducerKind>::failure(
        validation_error(object_id, "generated topology source feature id must not be empty"));
  }

  const Feature* feature = document.find_feature(source_feature);
  if (feature == nullptr) {
    return Result<GeneratedTopologyProducerKind>::failure(validation_error(
        object_id, "generated topology source feature must exist in part document"));
  }
  const Sketch* sketch = document.find_sketch(feature->input_sketch());
  if (sketch == nullptr) {
    return Result<GeneratedTopologyProducerKind>::failure(validation_error(
        object_id, "generated topology source feature input sketch must exist in part document"));
  }

  if (feature->type() == FeatureType::AdditiveExtrude) {
    if (is_rectangular_additive_extrude(document, *feature)) {
      return Result<GeneratedTopologyProducerKind>::success(
          GeneratedTopologyProducerKind::RectangularAdditiveExtrude);
    }
    return Result<GeneratedTopologyProducerKind>::failure(validation_error(
        object_id,
        "additive-extrude generated topology is supported only for exactly one RectangleProfile and no other profile"));
  }

  if (!sketch->circular_hole_patterns().empty()) {
    return Result<GeneratedTopologyProducerKind>::failure(validation_error(
        object_id,
        "patterned generated topology is unavailable until stable per-instance semantic identity exists"));
  }
  if (sketch->circle_profiles().size() != 1U || sketch->profile_count() != 1U) {
    const bool ambiguous = sketch->circle_profiles().size() > 1U ||
                           (sketch->circle_profiles().size() == 1U &&
                            sketch->profile_count() != 1U);
    return Result<GeneratedTopologyProducerKind>::failure(validation_error(
        object_id,
        ambiguous
            ? "subtractive-extrude generated topology is ambiguous unless the source sketch contains exactly one CircleProfile and no other profile"
            : "subtractive-extrude generated topology is supported only for exactly one CircleProfile and no other profile"));
  }

  const Feature* target = document.find_feature(feature->target_feature());
  if (target == nullptr || !is_rectangular_additive_extrude(document, *target)) {
    return Result<GeneratedTopologyProducerKind>::failure(validation_error(
        object_id,
        "single-circle subtractive generated topology currently requires one direct rectangular additive-extrude target"));
  }

  return Result<GeneratedTopologyProducerKind>::success(
      GeneratedTopologyProducerKind::SingleCircleSubtractiveExtrude);
}

Result<std::size_t>
validate_generated_topology_reference(const PartDocument& document,
                                      const GeneratedTopologyReferenceIdentity& identity) {
  const FeatureId& source_feature = generated_topology_reference_source_feature(identity);
  auto producer = classify_generated_topology_producer(document, source_feature);
  if (producer.has_error())
    return Result<std::size_t>::failure(producer.error());

  if (producer.value() == GeneratedTopologyProducerKind::RectangularAdditiveExtrude) {
    if (generated_topology_reference_source_profile(identity).has_value()) {
      return Result<std::size_t>::failure(validation_error(
          source_feature.value(),
          "rectangular additive generated edge/vertex identity must not carry a source profile"));
    }
    return validate_role(identity, producer.value());
  }

  const Feature* feature = document.find_feature(source_feature);
  const CircleProfile* profile =
      feature == nullptr ? nullptr : single_circle_profile(document, *feature);
  if (profile == nullptr) {
    return Result<std::size_t>::failure(validation_error(
        source_feature.value(), "single-circle generated topology source profile is unavailable"));
  }
  const auto source_profile = generated_topology_reference_source_profile(identity);
  if (!source_profile.has_value() || source_profile.value() != profile->id()) {
    return Result<std::size_t>::failure(validation_error(
        source_feature.value(),
        "single-circle generated topology reference must retain the exact source CircleProfile id"));
  }
  return validate_role(identity, producer.value());
}

Result<std::string>
make_generated_topology_target_spelling(const GeneratedTopologyReferenceIdentity& identity) {
  const FeatureId& source_feature = generated_topology_reference_source_feature(identity);
  if (source_feature.empty()) {
    return Result<std::string>::failure(validation_error(
        "generated_topology", "generated topology source feature id must not be empty"));
  }

  const auto family = generated_topology_reference_family(identity);
  std::string spelling(kTopologyPrefix);
  spelling += family_token(family);
  spelling += ':';
  spelling += encode_id(source_feature.value());

  const auto source_profile = generated_topology_reference_source_profile(identity);
  if (source_profile.has_value()) {
    if (source_profile->empty()) {
      return Result<std::string>::failure(validation_error(
          source_feature.value(), "generated topology source profile id must not be empty"));
    }
    spelling += ':';
    spelling += encode_id(source_profile->value());
  }
  spelling += ':';
  spelling += generated_topology_reference_role(identity);
  return Result<std::string>::success(std::move(spelling));
}

Result<GeneratedTopologyReferenceIdentity>
parse_generated_topology_target_spelling(std::string_view spelling) {
  if (!is_generated_topology_target_spelling(spelling)) {
    return Result<GeneratedTopologyReferenceIdentity>::failure(validation_error(
        std::string(spelling),
        "generated topology target spelling must start with the topo: prefix"));
  }

  const auto segments = split_segments(spelling.substr(kTopologyPrefix.size()));
  const auto family = segments.empty() ? std::nullopt : parse_family_token(segments.front());
  if (!family.has_value()) {
    return Result<GeneratedTopologyReferenceIdentity>::failure(validation_error(
        std::string(spelling), "generated topology target family is unsupported"));
  }

  const bool profile_family = family.value() == GeneratedTopologyReferenceFamily::CylindricalFace ||
                              family.value() == GeneratedTopologyReferenceFamily::CircularEdge;
  const std::size_t expected_segments = profile_family ? 4U : 3U;
  if (segments.size() != expected_segments) {
    return Result<GeneratedTopologyReferenceIdentity>::failure(validation_error(
        std::string(spelling),
        "generated topology target spelling has the wrong number of family/id/role segments"));
  }

  auto feature_id = decode_id(spelling, segments[1U]);
  if (feature_id.has_error())
    return Result<GeneratedTopologyReferenceIdentity>::failure(feature_id.error());

  if (family.value() == GeneratedTopologyReferenceFamily::LinearEdge) {
    const auto role = parse_linear_edge_role(segments[2U]);
    if (!role.has_value()) {
      return Result<GeneratedTopologyReferenceIdentity>::failure(validation_error(
          std::string(spelling), "generated linear-edge semantic role is unsupported"));
    }
    auto reference = SemanticEdgeReference::create(FeatureId(std::move(feature_id.value())),
                                                   role.value());
    if (reference.has_error())
      return Result<GeneratedTopologyReferenceIdentity>::failure(reference.error());
    return Result<GeneratedTopologyReferenceIdentity>::success(std::move(reference.value()));
  }

  if (family.value() == GeneratedTopologyReferenceFamily::Vertex) {
    const auto role = parse_vertex_role(segments[2U]);
    if (!role.has_value()) {
      return Result<GeneratedTopologyReferenceIdentity>::failure(validation_error(
          std::string(spelling), "generated vertex semantic role is unsupported"));
    }
    auto reference = SemanticVertexReference::create(FeatureId(std::move(feature_id.value())),
                                                     role.value());
    if (reference.has_error())
      return Result<GeneratedTopologyReferenceIdentity>::failure(reference.error());
    return Result<GeneratedTopologyReferenceIdentity>::success(std::move(reference.value()));
  }

  auto profile_id = decode_id(spelling, segments[2U]);
  if (profile_id.has_error())
    return Result<GeneratedTopologyReferenceIdentity>::failure(profile_id.error());

  if (family.value() == GeneratedTopologyReferenceFamily::CylindricalFace) {
    if (segments[3U] != to_string(SemanticCylindricalFace::Wall)) {
      return Result<GeneratedTopologyReferenceIdentity>::failure(validation_error(
          std::string(spelling), "generated cylindrical-face semantic role is unsupported"));
    }
    auto reference = SemanticCylindricalFaceReference::create(
        FeatureId(std::move(feature_id.value())), ProfileId(std::move(profile_id.value())),
        SemanticCylindricalFace::Wall);
    if (reference.has_error())
      return Result<GeneratedTopologyReferenceIdentity>::failure(reference.error());
    return Result<GeneratedTopologyReferenceIdentity>::success(std::move(reference.value()));
  }

  const auto role = parse_circular_edge_role(segments[3U]);
  if (!role.has_value()) {
    return Result<GeneratedTopologyReferenceIdentity>::failure(validation_error(
        std::string(spelling), "generated circular-edge semantic role is unsupported"));
  }
  auto reference = SemanticCircularEdgeReference::create(
      FeatureId(std::move(feature_id.value())), ProfileId(std::move(profile_id.value())),
      role.value());
  if (reference.has_error())
    return Result<GeneratedTopologyReferenceIdentity>::failure(reference.error());
  return Result<GeneratedTopologyReferenceIdentity>::success(std::move(reference.value()));
}

bool is_generated_topology_target_spelling(std::string_view spelling) noexcept {
  return spelling.size() > kTopologyPrefix.size() &&
         spelling.substr(0U, kTopologyPrefix.size()) == kTopologyPrefix;
}

} // namespace blcad
