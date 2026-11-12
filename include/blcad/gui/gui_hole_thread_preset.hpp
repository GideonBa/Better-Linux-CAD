#pragma once

#include "blcad/core/part_document.hpp"
#include <QString>

namespace blcad::gui {
// A canonical Core value template with fixed placeholder references. Applying
// it retains the destination draft's live targets/positions/limits/profile.
struct GuiHoleThreadPreset {
  std::string name;
  PartDocument values;
  [[nodiscard]] bool hole() const noexcept;
};
class GuiHoleThreadPresetCommands {
public:
  [[nodiscard]] static Result<GuiHoleThreadPreset> capture(const PartDocument&, FeatureId,
                                                           std::string name);
  [[nodiscard]] static Result<PartDocument> apply(const GuiHoleThreadPreset&, PartDocument,
                                                  FeatureId, std::string draft_namespace);
  [[nodiscard]] static Result<std::vector<GuiHoleThreadPreset>> load(const QString& path);
  [[nodiscard]] static Result<std::size_t> save(const QString& path, GuiHoleThreadPreset);
  [[nodiscard]] static Result<std::size_t> remove(const QString& path, std::string_view name,
                                                  bool hole);
};
} // namespace blcad::gui
