#pragma once

#include "blcad/core/sketch_text.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace blcad::geometry {

struct SketchTextStroke {
  Point2 start;
  Point2 end;
};

struct SketchTextGlyphLayout {
  char character{' '};
  Point2 origin;
  double advance_mm{0.0};
  std::vector<SketchTextStroke> strokes;
};

struct SketchTextLayout {
  SketchTextId id;
  std::string resolved_text;
  std::string resolved_font;
  bool used_fallback{false};
  Point2 anchor;
  double height_mm{0.0};
  double rotation_deg{0.0};
  double width_mm{0.0};
  std::vector<SketchTextGlyphLayout> glyphs;
  std::vector<SketchTextDiagnostic> diagnostics;
};

// Deterministic vector-layout authority. Installed fonts are supplied by the
// caller; if none of the requested/fallback families are available, BLCAD's
// built-in single-stroke font is used and the fallback is diagnosed explicitly.
class SketchTextLayoutResolver {
public:
  [[nodiscard]] Result<SketchTextLayout>
  resolve(const PartDocument& document, const SketchText& text,
          std::vector<std::string> available_fonts,
          std::vector<std::string> fallback_fonts = {"Liberation Sans", "DejaVu Sans"}) const {
    auto resolved = text.resolve(document);
    if (resolved.has_error()) return Result<SketchTextLayout>::failure(resolved.error());

    std::sort(available_fonts.begin(), available_fonts.end());
    available_fonts.erase(std::unique(available_fonts.begin(), available_fonts.end()),
                          available_fonts.end());
    const auto has_font = [&available_fonts](std::string_view family) {
      return std::binary_search(available_fonts.begin(), available_fonts.end(), std::string(family));
    };

    SketchTextLayout layout;
    layout.id = resolved.value().id;
    layout.resolved_text = resolved.value().text;
    layout.anchor = resolved.value().anchor;
    layout.height_mm = resolved.value().height_mm;
    layout.rotation_deg = resolved.value().rotation_deg;
    layout.resolved_font = resolved.value().requested_font;
    if (!has_font(layout.resolved_font)) {
      layout.used_fallback = true;
      layout.diagnostics.push_back(
          {"font_missing", "Requested font is unavailable: " + layout.resolved_font});
      layout.resolved_font.clear();
      for (const auto& fallback : fallback_fonts)
        if (has_font(fallback)) {
          layout.resolved_font = fallback;
          layout.diagnostics.push_back(
              {"font_fallback", "Using fallback font: " + fallback});
          break;
        }
      if (layout.resolved_font.empty()) {
        layout.resolved_font = "BLCAD Simplex Stroke";
        layout.diagnostics.push_back(
            {"font_builtin_fallback", "Using built-in BLCAD Simplex Stroke font"});
      }
    }

    const double scale = layout.height_mm / 7.0;
    const double advance = 6.0 * scale;
    double cursor = 0.0;
    for (const char character : layout.resolved_text) {
      if (character == '\n') {
        cursor = 0.0;
        continue;
      }
      SketchTextGlyphLayout glyph;
      glyph.character = character;
      glyph.origin = transform({cursor, 0.0}, layout.anchor, layout.rotation_deg);
      glyph.advance_mm = advance;
      for (const auto& stroke : glyph_strokes(character)) {
        const Point2 start{cursor + stroke.start.x * scale, stroke.start.y * scale};
        const Point2 end{cursor + stroke.end.x * scale, stroke.end.y * scale};
        glyph.strokes.push_back({transform(start, layout.anchor, layout.rotation_deg),
                                 transform(end, layout.anchor, layout.rotation_deg)});
      }
      layout.glyphs.push_back(std::move(glyph));
      cursor += advance;
    }
    layout.width_mm = cursor;
    return Result<SketchTextLayout>::success(std::move(layout));
  }

private:
  enum Segment : unsigned {
    Top = 1U << 0U,
    UpperLeft = 1U << 1U,
    UpperRight = 1U << 2U,
    Middle = 1U << 3U,
    LowerLeft = 1U << 4U,
    LowerRight = 1U << 5U,
    Bottom = 1U << 6U,
    DiagonalDown = 1U << 7U,
    DiagonalUp = 1U << 8U,
    CenterVertical = 1U << 9U,
  };

  [[nodiscard]] static Point2 transform(Point2 local, Point2 anchor,
                                        double rotation_deg) noexcept {
    constexpr double pi = 3.141592653589793238462643383279502884;
    const double radians = rotation_deg * pi / 180.0;
    const double cosine = std::cos(radians);
    const double sine = std::sin(radians);
    return {anchor.x + local.x * cosine - local.y * sine,
            anchor.y + local.x * sine + local.y * cosine};
  }

  [[nodiscard]] static unsigned glyph_mask(char character) noexcept {
    const char c = character >= 'a' && character <= 'z'
                       ? static_cast<char>(character - 'a' + 'A')
                       : character;
    switch (c) {
    case '0': return Top | UpperLeft | UpperRight | LowerLeft | LowerRight | Bottom;
    case '1': return UpperRight | LowerRight;
    case '2': return Top | UpperRight | Middle | LowerLeft | Bottom;
    case '3': return Top | UpperRight | Middle | LowerRight | Bottom;
    case '4': return UpperLeft | UpperRight | Middle | LowerRight;
    case '5': return Top | UpperLeft | Middle | LowerRight | Bottom;
    case '6': return Top | UpperLeft | Middle | LowerLeft | LowerRight | Bottom;
    case '7': return Top | UpperRight | LowerRight;
    case '8': return Top | UpperLeft | UpperRight | Middle | LowerLeft | LowerRight | Bottom;
    case '9': return Top | UpperLeft | UpperRight | Middle | LowerRight | Bottom;
    case 'A': return Top | UpperLeft | UpperRight | Middle | LowerLeft | LowerRight;
    case 'B': return UpperLeft | Middle | LowerLeft | LowerRight | Bottom | CenterVertical;
    case 'C': return Top | UpperLeft | LowerLeft | Bottom;
    case 'D': return UpperLeft | UpperRight | LowerLeft | LowerRight | Bottom;
    case 'E': return Top | UpperLeft | Middle | LowerLeft | Bottom;
    case 'F': return Top | UpperLeft | Middle | LowerLeft;
    case 'G': return Top | UpperLeft | LowerLeft | Bottom | LowerRight | Middle;
    case 'H': return UpperLeft | UpperRight | Middle | LowerLeft | LowerRight;
    case 'I': return Top | Bottom | CenterVertical;
    case 'J': return UpperRight | LowerRight | Bottom | LowerLeft;
    case 'K': return UpperLeft | LowerLeft | DiagonalDown | DiagonalUp;
    case 'L': return UpperLeft | LowerLeft | Bottom;
    case 'M': return UpperLeft | UpperRight | LowerLeft | LowerRight | DiagonalDown | DiagonalUp;
    case 'N': return UpperLeft | UpperRight | LowerLeft | LowerRight | DiagonalDown;
    case 'O': return Top | UpperLeft | UpperRight | LowerLeft | LowerRight | Bottom;
    case 'P': return Top | UpperLeft | UpperRight | Middle | LowerLeft;
    case 'Q': return Top | UpperLeft | UpperRight | LowerLeft | LowerRight | Bottom | DiagonalDown;
    case 'R': return Top | UpperLeft | UpperRight | Middle | LowerLeft | DiagonalDown;
    case 'S': return Top | UpperLeft | Middle | LowerRight | Bottom;
    case 'T': return Top | CenterVertical;
    case 'U': return UpperLeft | UpperRight | LowerLeft | LowerRight | Bottom;
    case 'V': return UpperLeft | UpperRight | DiagonalDown | DiagonalUp;
    case 'W': return UpperLeft | UpperRight | LowerLeft | LowerRight | DiagonalDown | DiagonalUp;
    case 'X': return DiagonalDown | DiagonalUp;
    case 'Y': return DiagonalDown | DiagonalUp | CenterVertical;
    case 'Z': return Top | DiagonalUp | Bottom;
    case '-': return Middle;
    case '_': return Bottom;
    case '+': return Middle | CenterVertical;
    case '/': return DiagonalUp;
    case '\\': return DiagonalDown;
    case '.': return LowerRight;
    case ':': return UpperRight | LowerRight;
    case ' ': return 0U;
    default: return Top | UpperLeft | UpperRight | LowerLeft | LowerRight | Bottom | DiagonalDown;
    }
  }

  [[nodiscard]] static std::vector<SketchTextStroke> glyph_strokes(char character) {
    const unsigned mask = glyph_mask(character);
    const std::array<std::pair<unsigned, SketchTextStroke>, 10U> segments{{
        {Top, {{0.0, 7.0}, {4.0, 7.0}}},
        {UpperLeft, {{0.0, 7.0}, {0.0, 3.5}}},
        {UpperRight, {{4.0, 7.0}, {4.0, 3.5}}},
        {Middle, {{0.0, 3.5}, {4.0, 3.5}}},
        {LowerLeft, {{0.0, 3.5}, {0.0, 0.0}}},
        {LowerRight, {{4.0, 3.5}, {4.0, 0.0}}},
        {Bottom, {{0.0, 0.0}, {4.0, 0.0}}},
        {DiagonalDown, {{0.0, 7.0}, {4.0, 0.0}}},
        {DiagonalUp, {{0.0, 0.0}, {4.0, 7.0}}},
        {CenterVertical, {{2.0, 7.0}, {2.0, 0.0}}},
    }};
    std::vector<SketchTextStroke> result;
    for (const auto& [bit, stroke] : segments)
      if ((mask & bit) != 0U) result.push_back(stroke);
    return result;
  }
};

} // namespace blcad::geometry
