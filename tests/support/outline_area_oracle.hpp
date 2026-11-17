#pragma once
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>
namespace blcad::test {
// Independent Green-integral oracle: no OCCT curves, nesting, offsets or solids.
struct OutlineAreaOracle {
  FT_Library library{};
  FT_Face face{};
  struct Point {
    double x, y;
  };
  Point first{}, last{};
  double integral{};
  OutlineAreaOracle() {
    if (FT_Init_FreeType(&library) || FT_New_Face(library, BLCAD_TEST_OUTLINE_FONT, 0, &face))
      throw std::runtime_error("Oracle font unavailable");
  }
  ~OutlineAreaOracle() {
    if (face)
      FT_Done_Face(face);
    if (library)
      FT_Done_FreeType(library);
  }
  void line(Point to) {
    integral += last.x * to.y - last.y * to.x;
    last = to;
  }
  static Point point(const FT_Vector* p) {
    return {double(p->x), double(p->y)};
  }
  void polynomial(const std::array<Point, 4>& p, int degree) {
    constexpr double nodes[]{-.906179845938664, -.538469310105683, 0, .538469310105683,
                             .906179845938664};
    constexpr double weights[]{.236926885056189, .478628670499366, .568888888888889,
                               .478628670499366, .236926885056189};
    for (int sample = 0; sample < 5; ++sample) {
      const double t = (nodes[sample] + 1) / 2;
      auto points = p;
      std::array<Point, 4> d{};
      for (int i = 0; i < degree; ++i)
        d[i] = {degree * (p[i + 1].x - p[i].x), degree * (p[i + 1].y - p[i].y)};
      for (int level = degree; level > 0; --level)
        for (int i = 0; i < level; ++i)
          points[i] = {(1 - t) * points[i].x + t * points[i + 1].x,
                       (1 - t) * points[i].y + t * points[i + 1].y};
      for (int level = degree - 1; level > 0; --level)
        for (int i = 0; i < level; ++i)
          d[i] = {(1 - t) * d[i].x + t * d[i + 1].x, (1 - t) * d[i].y + t * d[i + 1].y};
      integral += weights[sample] / 2 * (points[0].x * d[0].y - points[0].y * d[0].x);
    }
    last = p[degree];
  }
  static int move(const FT_Vector* p, void* c) {
    auto& s = *static_cast<OutlineAreaOracle*>(c);
    s.line(s.first);
    s.first = s.last = point(p);
    return 0;
  }
  static int segment(const FT_Vector* p, void* c) {
    static_cast<OutlineAreaOracle*>(c)->line(point(p));
    return 0;
  }
  static int quadratic(const FT_Vector* a, const FT_Vector* b, void* c) {
    auto& s = *static_cast<OutlineAreaOracle*>(c);
    s.polynomial({s.last, point(a), point(b), {}}, 2);
    return 0;
  }
  static int cubic(const FT_Vector* a, const FT_Vector* b, const FT_Vector* d, void* c) {
    auto& s = *static_cast<OutlineAreaOracle*>(c);
    s.polynomial({s.last, point(a), point(b), point(d)}, 3);
    return 0;
  }
  double area(const std::string& text, double height) {
    double result = 0;
    for (unsigned char code : text) {
      if (code == ' ')
        continue;
      if (FT_Load_Char(face, code, FT_LOAD_NO_SCALE | FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP))
        throw std::runtime_error("Oracle glyph unavailable");
      integral = 0;
      first = last = {};
      FT_Outline_Funcs callbacks{move, segment, quadratic, cubic, 0, 0};
      if (FT_Outline_Decompose(&face->glyph->outline, &callbacks, this))
        throw std::runtime_error("Oracle outline failure");
      line(first);
      result += std::abs(integral) / 2;
    }
    return result * std::pow(height / face->units_per_EM, 2);
  }
};
} // namespace blcad::test
