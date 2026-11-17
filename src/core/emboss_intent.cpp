#include "blcad/core/emboss_intent.hpp"
namespace blcad {
std::optional<PinnedOutlineFont> bundled_outline_font(std::string_view family) {
  if (family == "Barlow")
    return PinnedOutlineFont{"Barlow-Regular.ttf",
                             "95aa02c7c43096e0dd44d787ba6216864a67157e402adab59b35572e0c1577ea"};
  if (family == "IBM Plex Mono")
    return PinnedOutlineFont{"IBMPlexMono-Regular.ttf",
                             "6a3412f058c7d8dfd9170c41e85ade48e5156ecb89356110ca57a0a27734af46"};
  return std::nullopt;
}
SketchId emboss_source_sketch(const EmbossIntent& e) {
  return std::visit(
      [](const auto& source) -> SketchId {
        using T = std::decay_t<decltype(source)>;
        if constexpr (std::is_same_v<T, ProfileRegionReference>)
          return source.sketch();
        else
          return source.text.sketch();
      },
      e.source);
}
} // namespace blcad
