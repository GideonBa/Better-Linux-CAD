#pragma once

#include "blcad/core/sketch_modify.hpp"
#include "blcad/gui/gui_document_session.hpp"

#include <cstddef>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace blcad::gui {

struct GuiSketchModifyPreview {
  Sketch candidate;
  std::vector<std::string> invalidated_ids;
};

// Block 116 headless GUI-layer controller for trim/extend/split/fillet/chamfer.
// Each operation previews a disposable candidate Sketch; commit re-derives the
// same operation from the current committed document and publishes exactly one
// atomic transaction. Reusing the generic Part transaction means the Block-114
// constraint and Block-115 dimension catalogs are re-validated against the
// rewritten Sketch, so a modification that removes a catalog-referenced entity
// fails closed with no partial mutation.
class GuiSketchModifyController {
public:
  [[nodiscard]] static Result<GuiSketchModifyController>
  create(const GuiDocumentSession& session, SketchId sketch) {
    if (session.part_document() == nullptr)
      return Result<GuiSketchModifyController>::failure(
          Error::validation(sketch.value(), "sketch modify requires an active Part session"));
    const Sketch* source = session.part_document()->find_sketch(sketch);
    if (source == nullptr)
      return Result<GuiSketchModifyController>::failure(
          Error::validation(sketch.value(), "sketch modify requires an existing Sketch"));
    return Result<GuiSketchModifyController>::success(GuiSketchModifyController(*source));
  }

  [[nodiscard]] const Sketch& source_sketch() const noexcept { return source_; }
  [[nodiscard]] const std::string& pending_label() const noexcept { return label_; }
  [[nodiscard]] bool has_preview() const noexcept { return static_cast<bool>(operation_); }

  [[nodiscard]] Result<GuiSketchModifyPreview> trim(SketchEntityId target, Point2 pick) {
    return set_operation("Trim sketch geometry",
                         [target, pick](const Sketch& sketch) {
                           return SketchModifyService::trim(sketch, target, pick);
                         });
  }
  [[nodiscard]] Result<GuiSketchModifyPreview> extend(SketchEntityId target, Point2 pick) {
    return set_operation("Extend sketch geometry",
                         [target, pick](const Sketch& sketch) {
                           return SketchModifyService::extend(sketch, target, pick);
                         });
  }
  [[nodiscard]] Result<GuiSketchModifyPreview> split(SketchEntityId target, Point2 point) {
    return set_operation("Split sketch geometry",
                         [target, point](const Sketch& sketch) {
                           return SketchModifyService::split(sketch, target, point);
                         });
  }
  [[nodiscard]] Result<GuiSketchModifyPreview> fillet(SketchEntityId first, SketchEntityId second,
                                                      double radius, bool keep_trim = true) {
    return set_operation("Fillet sketch corner",
                         [first, second, radius, keep_trim](const Sketch& sketch) {
                           return SketchModifyService::fillet(sketch, first, second, radius,
                                                              keep_trim);
                         });
  }
  [[nodiscard]] Result<GuiSketchModifyPreview> chamfer(SketchEntityId first, SketchEntityId second,
                                                       double distance, bool keep_trim = true) {
    return set_operation("Chamfer sketch corner",
                         [first, second, distance, keep_trim](const Sketch& sketch) {
                           return SketchModifyService::chamfer(sketch, first, second, distance,
                                                               keep_trim);
                         });
  }

  [[nodiscard]] Result<std::size_t> commit(GuiDocumentSession& session) const {
    if (!operation_)
      return Result<std::size_t>::failure(
          Error::validation(source_.id().value(), "no sketch modification is prepared to commit"));
    if (session.part_document() == nullptr)
      return Result<std::size_t>::failure(
          Error::validation(source_.id().value(), "sketch modify requires an active Part document"));
    const SketchId sketch_id = source_.id();
    const Operation operation = operation_;
    return session.commit_part_transaction(
        label_, [sketch_id, operation](PartDocument& document) -> Result<std::size_t> {
          const Sketch* current = document.find_sketch(sketch_id);
          if (current == nullptr)
            return Result<std::size_t>::failure(
                Error::validation(sketch_id.value(), "sketch modify target no longer exists"));
          auto modified = operation(*current);
          if (modified.has_error()) return Result<std::size_t>::failure(modified.error());
          return document.update_sketch(std::move(modified.value().sketch));
        });
  }

private:
  using Operation = std::function<Result<SketchModifyResult>(const Sketch&)>;

  explicit GuiSketchModifyController(Sketch source) : source_(std::move(source)) {}

  [[nodiscard]] Result<GuiSketchModifyPreview> set_operation(std::string label,
                                                             Operation operation) {
    auto preview = operation(source_);
    if (preview.has_error()) {
      operation_ = nullptr;
      label_.clear();
      return Result<GuiSketchModifyPreview>::failure(preview.error());
    }
    operation_ = std::move(operation);
    label_ = std::move(label);
    return Result<GuiSketchModifyPreview>::success(
        {std::move(preview.value().sketch), std::move(preview.value().invalidated_ids)});
  }

  Sketch source_;
  Operation operation_;
  std::string label_;
};

} // namespace blcad::gui
