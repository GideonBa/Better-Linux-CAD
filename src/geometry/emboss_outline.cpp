#include "emboss_outline.hpp"
#include "blcad/core/step_import_freshness.hpp"
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include <GCE2d_MakeSegment.hxx>
#include <Geom2d_BezierCurve.hxx>
#include <TColgp_Array1OfPnt2d.hxx>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <stdexcept>
namespace blcad::geometry::detail {
namespace {
void require(bool condition, const char* message) {
  if (!condition)
    throw std::runtime_error(message);
}
struct FontLibrary {
  FT_Library value{};
  FontLibrary() {
    require(!FT_Init_FreeType(&value), "Cannot initialize outline font engine");
  }
  ~FontLibrary() {
    FT_Done_FreeType(value);
  }
};
struct FontFace {
  FT_Face value{};
  ~FontFace() {
    if (value)
      FT_Done_Face(value);
  }
};
std::string font_bytes(const PinnedOutlineFont& font) {
  std::vector<std::filesystem::path> roots;
  if (const char* path = std::getenv("BLCAD_FONT_DIR"))
    roots.emplace_back(path);
  else {
    roots.emplace_back(BLCAD_OUTLINE_FONT_SOURCE_DIR);
    roots.emplace_back(BLCAD_OUTLINE_FONT_INSTALL_DIR);
  }
  for (const auto& root : roots) {
    const auto path = root / font.asset;
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream)
      continue;
    const auto size = stream.tellg();
    require(size > 0 && size < 16 * 1024 * 1024,
            "Pinned font asset exceeds the outline size limit");
    std::string bytes(static_cast<std::size_t>(size), '\0');
    stream.seekg(0);
    require(bool(stream.read(bytes.data(), bytes.size())),
            "Pinned font asset cannot be read completely");
    auto digest = step_import_sha256(bytes);
    require(digest && digest.value() == font.sha256,
            "Pinned outline font digest does not match the authored font");
    return bytes;
  }
  throw std::runtime_error("Pinned outline font asset is unavailable");
}
std::vector<FT_ULong> unicode(const std::string& text) {
  std::vector<FT_ULong> result;
  for (std::size_t i = 0; i < text.size();) {
    unsigned char c = text[i++];
    FT_ULong value = c;
    int continuation = 0;
    if (c >= 0xc2 && c <= 0xdf) {
      value = c & 31;
      continuation = 1;
    } else if (c >= 0xe0 && c <= 0xef) {
      value = c & 15;
      continuation = 2;
    } else if (c >= 0xf0 && c <= 0xf4) {
      value = c & 7;
      continuation = 3;
    } else
      require(c < 128, "Emboss text contains invalid UTF-8");
    for (int j = 0; j < continuation; ++j) {
      require(i < text.size(), "Emboss text has incomplete UTF-8");
      c = text[i++];
      require((c & 0xc0) == 0x80, "Emboss text contains invalid UTF-8");
      value = (value << 6) | (c & 63);
    }
    require(value <= 0x10ffff && (value < 0xd800 || value > 0xdfff) &&
                (!continuation || value >= (continuation == 1   ? 0x80
                                            : continuation == 2 ? 0x800
                                                                : 0x10000)),
            "Emboss text contains noncanonical UTF-8");
    result.push_back(value);
  }
  return result;
}
struct Decompose {
  std::vector<EmbossOutlineLoop> loops;
  std::vector<Handle(Geom2d_Curve)> active;
  gp_Pnt2d start, last;
  double scale, pen_x, pen_y, cosine, sine;
  Point2 anchor;
  std::string prefix;
  std::size_t contour{};
  std::exception_ptr failure{};
  gp_Pnt2d point(const FT_Vector* v) const {
    const double x = (v->x + pen_x) * scale, y = (v->y + pen_y) * scale;
    return {anchor.x + x * cosine - y * sine, anchor.y + x * sine + y * cosine};
  }
  void line(gp_Pnt2d end) {
    if (last.Distance(end) > 1e-12)
      active.emplace_back(GCE2d_MakeSegment(last, end).Value());
    last = end;
  }
  void close() {
    if (active.empty())
      return;
    line(start);
    loops.push_back({prefix + "/contour/" + std::to_string(contour++), std::move(active)});
    active.clear();
  }
  static int move(const FT_Vector* v, void* context) {
    auto& self = *static_cast<Decompose*>(context);
    self.close();
    self.start = self.last = self.point(v);
    return 0;
  }
  static int segment(const FT_Vector* v, void* context) {
    auto& self = *static_cast<Decompose*>(context);
    self.line(self.point(v));
    return 0;
  }
  static int conic(const FT_Vector* control, const FT_Vector* end, void* context) {
    auto& s = *static_cast<Decompose*>(context);
    const auto c = s.point(control), p = s.point(end);
    TColgp_Array1OfPnt2d poles(1, 4);
    poles(1) = s.last;
    poles(2) = gp_Pnt2d(s.last.X() + 2 * (c.X() - s.last.X()) / 3,
                        s.last.Y() + 2 * (c.Y() - s.last.Y()) / 3);
    poles(3) = gp_Pnt2d(p.X() + 2 * (c.X() - p.X()) / 3, p.Y() + 2 * (c.Y() - p.Y()) / 3);
    poles(4) = p;
    s.active.emplace_back(new Geom2d_BezierCurve(poles));
    s.last = p;
    return 0;
  }
  static int cubic(const FT_Vector* first, const FT_Vector* second, const FT_Vector* end,
                   void* context) {
    auto& s = *static_cast<Decompose*>(context);
    TColgp_Array1OfPnt2d poles(1, 4);
    poles(1) = s.last;
    poles(2) = s.point(first);
    poles(3) = s.point(second);
    poles(4) = s.point(end);
    s.active.emplace_back(new Geom2d_BezierCurve(poles));
    s.last = poles(4);
    return 0;
  }
};
} // namespace
std::vector<EmbossOutlineLoop> emboss_text_outlines(const PartDocument& p,
                                                    const EmbossTextSource& source) {
  auto resolved = source.text.resolve(p);
  require(bool(resolved), "Emboss text bindings could not be resolved");
  const auto bytes = font_bytes(source.font);
  FontLibrary library;
  FontFace face;
  require(!FT_New_Memory_Face(library.value, reinterpret_cast<const FT_Byte*>(bytes.data()),
                              bytes.size(), 0, &face.value),
          "Pinned outline font could not be decoded");
  require(face.value->units_per_EM > 0, "Pinned outline font has invalid units");
  const auto& text = resolved.value();
  const auto angle = text.rotation_deg * std::numbers::pi / 180;
  Decompose out{{},
                {},
                {},
                {},
                text.height_mm / face.value->units_per_EM,
                0,
                0,
                std::cos(angle),
                std::sin(angle),
                text.anchor,
                "",
                0};
  FT_UInt previous = 0;
  std::size_t position = 0;
  for (const auto code : unicode(text.text)) {
    if (code == '\n') {
      out.pen_x = 0;
      out.pen_y -= face.value->height;
      previous = 0;
      ++position;
      continue;
    }
    const auto glyph = FT_Get_Char_Index(face.value, code);
    require(glyph != 0, "Pinned outline font has no glyph for the resolved text");
    if (previous && FT_HAS_KERNING(face.value)) {
      FT_Vector kerning{};
      require(!FT_Get_Kerning(face.value, previous, glyph, FT_KERNING_UNSCALED, &kerning),
              "Font kerning could not be resolved");
      out.pen_x += kerning.x;
    }
    require(!FT_Load_Glyph(face.value, glyph,
                           FT_LOAD_NO_SCALE | FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP),
            "Pinned glyph outline is unavailable");
    require(face.value->glyph->format == FT_GLYPH_FORMAT_OUTLINE,
            "Pinned font glyph has no vector outline");
    out.prefix = "text/" + source.text.id().value() + "/glyph/" + std::to_string(position++) +
                 "/unicode/" + std::to_string(code);
    out.contour = 0;
    FT_Outline_Funcs callbacks{
        [](const FT_Vector* a, void* c) noexcept {
          try {
            return Decompose::move(a, c);
          } catch (...) {
            static_cast<Decompose*>(c)->failure = std::current_exception();
            return 1;
          }
        },
        [](const FT_Vector* a, void* c) noexcept {
          try {
            return Decompose::segment(a, c);
          } catch (...) {
            static_cast<Decompose*>(c)->failure = std::current_exception();
            return 1;
          }
        },
        [](const FT_Vector* a, const FT_Vector* b, void* c) noexcept {
          try {
            return Decompose::conic(a, b, c);
          } catch (...) {
            static_cast<Decompose*>(c)->failure = std::current_exception();
            return 1;
          }
        },
        [](const FT_Vector* a, const FT_Vector* b, const FT_Vector* d, void* c) noexcept {
          try {
            return Decompose::cubic(a, b, d, c);
          } catch (...) {
            static_cast<Decompose*>(c)->failure = std::current_exception();
            return 1;
          }
        },
        0,
        0};
    const auto decomposed = FT_Outline_Decompose(&face.value->glyph->outline, &callbacks, &out);
    if (out.failure)
      std::rethrow_exception(out.failure);
    require(!decomposed, "Pinned glyph outline decomposition failed");
    out.close();
    out.pen_x += face.value->glyph->advance.x;
    previous = glyph;
  }
  require(!out.loops.empty(), "Resolved Emboss text contains no material outline");
  return out.loops;
}
} // namespace blcad::geometry::detail
