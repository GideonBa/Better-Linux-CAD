#pragma once

#include "blcad/core/part_document.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace blcad {

struct SketchTextIdTag;
using SketchTextId = TypedId<SketchTextIdTag>;

struct SketchTextBinding {
  std::string token;
  ParameterId parameter;
  std::size_t precision{3U};

  friend bool operator==(const SketchTextBinding&, const SketchTextBinding&) = default;
};

struct SketchTextDiagnostic {
  std::string code;
  std::string message;

  friend bool operator==(const SketchTextDiagnostic&, const SketchTextDiagnostic&) = default;
};

struct ResolvedSketchText {
  SketchTextId id;
  SketchId sketch;
  std::string text;
  std::string requested_font;
  double height_mm{0.0};
  double rotation_deg{0.0};
  Point2 anchor;
  std::vector<SketchTextDiagnostic> diagnostics;
};

// Persistent Block-113 Sketch annotation intent. Parameter bindings are
// explicit and therefore expression-backed parameters resolve through the same
// PartDocument evaluation authority as every other parametric value.
class SketchText {
public:
  [[nodiscard]] static Result<SketchText>
  create(SketchTextId id, SketchId sketch, std::string template_text,
         std::string requested_font, ParameterId height_parameter, Point2 anchor,
         std::optional<ParameterId> rotation_parameter = std::nullopt,
         std::vector<SketchTextBinding> bindings = {}) {
    const std::string object_id = id.empty() ? "sketch_text" : id.value();
    if (id.empty() || sketch.empty())
      return Result<SketchText>::failure(
          Error::validation(object_id, "Sketch text requires non-empty text and Sketch ids"));
    if (template_text.empty())
      return Result<SketchText>::failure(
          Error::validation(object_id, "Sketch text template must not be empty"));
    if (requested_font.empty())
      return Result<SketchText>::failure(
          Error::validation(object_id, "Sketch text requested font must not be empty"));
    if (height_parameter.empty())
      return Result<SketchText>::failure(
          Error::validation(object_id, "Sketch text height parameter must not be empty"));
    if (!std::isfinite(anchor.x) || !std::isfinite(anchor.y))
      return Result<SketchText>::failure(
          Error::validation(object_id, "Sketch text anchor coordinates must be finite"));
    if (rotation_parameter.has_value() && rotation_parameter->empty())
      return Result<SketchText>::failure(
          Error::validation(object_id, "Sketch text rotation parameter must not be empty"));

    std::set<std::string> tokens;
    for (const auto& binding : bindings) {
      if (binding.token.empty() || binding.parameter.empty())
        return Result<SketchText>::failure(Error::validation(
            object_id, "Sketch text bindings require non-empty token and parameter ids"));
      if (!tokens.insert(binding.token).second)
        return Result<SketchText>::failure(
            Error::validation(object_id, "Sketch text binding tokens must be unique"));
      if (binding.precision > 12U)
        return Result<SketchText>::failure(
            Error::validation(object_id, "Sketch text binding precision must not exceed 12"));
      const std::string marker = "${" + binding.token + "}";
      if (template_text.find(marker) == std::string::npos)
        return Result<SketchText>::failure(Error::validation(
            object_id, "Sketch text binding token is not present in the template"));
    }

    return Result<SketchText>::success(SketchText(
        std::move(id), std::move(sketch), std::move(template_text), std::move(requested_font),
        std::move(height_parameter), anchor, std::move(rotation_parameter), std::move(bindings)));
  }

  [[nodiscard]] const SketchTextId& id() const noexcept { return id_; }
  [[nodiscard]] const SketchId& sketch() const noexcept { return sketch_; }
  [[nodiscard]] const std::string& template_text() const noexcept { return template_text_; }
  [[nodiscard]] const std::string& requested_font() const noexcept { return requested_font_; }
  [[nodiscard]] const ParameterId& height_parameter() const noexcept { return height_parameter_; }
  [[nodiscard]] Point2 anchor() const noexcept { return anchor_; }
  [[nodiscard]] const std::optional<ParameterId>& rotation_parameter() const noexcept {
    return rotation_parameter_;
  }
  [[nodiscard]] const std::vector<SketchTextBinding>& bindings() const noexcept { return bindings_; }

  [[nodiscard]] Result<ResolvedSketchText> resolve(const PartDocument& document) const {
    const Sketch* target = document.find_sketch(sketch_);
    if (target == nullptr)
      return Result<ResolvedSketchText>::failure(
          Error::validation(id_.value(), "Sketch text target Sketch does not exist"));

    const Parameter* height = document.find_parameter(height_parameter_);
    if (height == nullptr)
      return Result<ResolvedSketchText>::failure(
          Error::validation(id_.value(), "Sketch text height parameter does not exist"));
    if (height->type() != ParameterType::Length || !height->value().is_positive_length())
      return Result<ResolvedSketchText>::failure(Error::validation(
          id_.value(), "Sketch text height parameter must be a positive length"));

    double rotation = 0.0;
    if (rotation_parameter_.has_value()) {
      const Parameter* angle = document.find_parameter(*rotation_parameter_);
      if (angle == nullptr)
        return Result<ResolvedSketchText>::failure(
            Error::validation(id_.value(), "Sketch text rotation parameter does not exist"));
      if (angle->type() != ParameterType::Angle || !angle->value().is_valid_angle())
        return Result<ResolvedSketchText>::failure(
            Error::validation(id_.value(), "Sketch text rotation parameter must be an angle"));
      rotation = angle->value().degrees();
    }

    std::string resolved = template_text_;
    for (const auto& binding : bindings_) {
      const Parameter* parameter = document.find_parameter(binding.parameter);
      if (parameter == nullptr)
        return Result<ResolvedSketchText>::failure(Error::validation(
            id_.value(), "Sketch text binding parameter does not exist: " +
                             binding.parameter.value()));
      const std::string marker = "${" + binding.token + "}";
      const std::string replacement = format_parameter(*parameter, binding.precision);
      std::size_t offset = 0U;
      while ((offset = resolved.find(marker, offset)) != std::string::npos) {
        resolved.replace(offset, marker.size(), replacement);
        offset += replacement.size();
      }
    }

    // Unbound markers fail closed rather than becoming silently stale labels.
    if (resolved.find("${") != std::string::npos)
      return Result<ResolvedSketchText>::failure(
          Error::validation(id_.value(), "Sketch text template contains an unresolved token"));

    return Result<ResolvedSketchText>::success(
        {id_, sketch_, std::move(resolved), requested_font_, height->value().millimeters(),
         rotation, anchor_, {}});
  }

private:
  SketchText(SketchTextId id, SketchId sketch, std::string template_text,
             std::string requested_font, ParameterId height_parameter, Point2 anchor,
             std::optional<ParameterId> rotation_parameter,
             std::vector<SketchTextBinding> bindings)
      : id_(std::move(id)), sketch_(std::move(sketch)), template_text_(std::move(template_text)),
        requested_font_(std::move(requested_font)), height_parameter_(std::move(height_parameter)),
        anchor_(anchor), rotation_parameter_(std::move(rotation_parameter)),
        bindings_(std::move(bindings)) {}

  [[nodiscard]] static std::string trim_number(std::string value) {
    const auto exponent = value.find_first_of("eE");
    const std::size_t limit = exponent == std::string::npos ? value.size() : exponent;
    const auto decimal = value.find('.');
    if (decimal != std::string::npos && decimal < limit) {
      std::size_t end = limit;
      while (end > decimal + 1U && value[end - 1U] == '0') --end;
      if (end == decimal + 1U) --end;
      value.erase(end, limit - end);
    }
    return value;
  }

  [[nodiscard]] static std::string format_parameter(const Parameter& parameter,
                                                    std::size_t precision) {
    if (parameter.type() == ParameterType::Count)
      return std::to_string(parameter.value().count_value());
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(static_cast<int>(precision));
    if (parameter.type() == ParameterType::Angle) {
      stream << parameter.value().degrees();
      return trim_number(stream.str()) + " deg";
    }
    stream << parameter.value().millimeters();
    return trim_number(stream.str()) + " mm";
  }

  SketchTextId id_;
  SketchId sketch_;
  std::string template_text_;
  std::string requested_font_;
  ParameterId height_parameter_;
  Point2 anchor_;
  std::optional<ParameterId> rotation_parameter_;
  std::vector<SketchTextBinding> bindings_;
};

class SketchTextCatalog {
public:
  [[nodiscard]] static Result<SketchTextCatalog> create(DocumentId document,
                                                        std::vector<SketchText> texts = {}) {
    const std::string object_id = document.empty() ? "sketch_text_catalog" : document.value();
    if (document.empty())
      return Result<SketchTextCatalog>::failure(
          Error::validation(object_id, "Sketch text catalog requires a document id"));
    std::sort(texts.begin(), texts.end(), [](const auto& left, const auto& right) {
      return left.id().value() < right.id().value();
    });
    for (std::size_t index = 1U; index < texts.size(); ++index)
      if (texts[index - 1U].id() == texts[index].id())
        return Result<SketchTextCatalog>::failure(
            Error::validation(texts[index].id().value(), "Sketch text ids must be unique"));
    return Result<SketchTextCatalog>::success(
        SketchTextCatalog(std::move(document), std::move(texts)));
  }

  [[nodiscard]] const DocumentId& document() const noexcept { return document_; }
  [[nodiscard]] const std::vector<SketchText>& texts() const noexcept { return texts_; }
  [[nodiscard]] const SketchText* find(SketchTextId id) const noexcept {
    const auto found = std::lower_bound(texts_.begin(), texts_.end(), id.value(),
                                        [](const auto& text, std::string_view value) {
                                          return text.id().value() < value;
                                        });
    return found != texts_.end() && found->id() == id ? &*found : nullptr;
  }
  [[nodiscard]] Result<std::size_t> add(SketchText text) {
    if (find(text.id()) != nullptr)
      return Result<std::size_t>::failure(
          Error::validation(text.id().value(), "Sketch text id already exists"));
    texts_.push_back(std::move(text));
    std::sort(texts_.begin(), texts_.end(), [](const auto& left, const auto& right) {
      return left.id().value() < right.id().value();
    });
    return Result<std::size_t>::success(texts_.size());
  }

private:
  SketchTextCatalog(DocumentId document, std::vector<SketchText> texts)
      : document_(std::move(document)), texts_(std::move(texts)) {}

  DocumentId document_;
  std::vector<SketchText> texts_;
};

} // namespace blcad
