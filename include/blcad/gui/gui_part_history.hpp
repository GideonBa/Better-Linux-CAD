#pragma once

#include "blcad/gui/gui_document_session.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace blcad::gui {

struct GuiPartHistoryPreview {
  PartDocument document;
  geometry::ShapeCache shapes;
};

// One staged history proposal over the complete committed Part. The shell owns
// task reservation and presentation; this controller never edits source intent
// until a fully revalidated Apply succeeds.
class GuiPartHistoryController {
public:
  static constexpr std::string_view task_id = "part.history";
  [[nodiscard]] Result<std::size_t> begin(const GuiDocumentSession& session);
  [[nodiscard]] bool active() const noexcept {
    return active_;
  }
  [[nodiscard]] bool current(const GuiDocumentSession& session) const;
  [[nodiscard]] const PartFeatureHistoryIntent& intent() const noexcept {
    return intent_;
  }
  [[nodiscard]] Result<std::size_t> stage_order(const GuiDocumentSession& session,
                                                std::vector<std::string> order);
  [[nodiscard]] Result<std::size_t> stage_suppression(const GuiDocumentSession& session,
                                                      std::string feature, bool suppressed,
                                                      bool include_dependents = false);
  [[nodiscard]] Result<std::vector<std::string>>
  blocked_dependents(const GuiDocumentSession& session) const;
  [[nodiscard]] Result<GuiPartHistoryPreview> preview(const GuiDocumentSession& session) const;
  [[nodiscard]] Result<std::size_t> apply(GuiDocumentSession& session, std::string label);
  void cancel() noexcept;

private:
  [[nodiscard]] Result<PartDocument> candidate(const GuiDocumentSession& session) const;
  bool active_ = false;
  std::string source_json_;
  std::uint64_t generation_ = 0;
  PartFeatureHistoryIntent intent_;
  PartFeatureHistoryIntent initial_;
};

} // namespace blcad::gui
