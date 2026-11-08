#pragma once

#include "blcad/gui/gui_document_session.hpp"

#include <functional>
#include <variant>

namespace blcad::gui {

// The shell owns task reservation/prefix-child presentation. This controller
// owns only a disposable typed candidate until one existing transaction applies.
class GuiMaterialController {
public:
  static constexpr std::string_view task_id = "manage.materials";
  [[nodiscard]] Result<std::size_t> begin(const GuiDocumentSession& session);
  [[nodiscard]] bool active() const noexcept {
    return active_;
  }
  [[nodiscard]] bool current(const GuiDocumentSession& session) const;
  [[nodiscard]] bool changed() const;
  [[nodiscard]] const PartDocument* part() const noexcept;
  [[nodiscard]] const Project* project() const noexcept;
  [[nodiscard]] const EngineeringResourceCatalog* catalog() const noexcept;
  [[nodiscard]] Result<std::size_t> stage_material(const GuiDocumentSession&, EngineeringMaterial);
  [[nodiscard]] Result<std::size_t>
  stage_assignment(const GuiDocumentSession&, DocumentId part, std::optional<BodyId> body,
                   std::optional<EngineeringResourceKey> material);
  [[nodiscard]] Result<std::size_t> stage_revision(const GuiDocumentSession&,
                                                   const EngineeringResourceKey& source,
                                                   EngineeringMaterial replacement,
                                                   std::vector<DocumentId> selected_parts = {});
  [[nodiscard]] Result<std::size_t> stage_remove(const GuiDocumentSession&,
                                                 const EngineeringResourceKey&,
                                                 std::vector<DocumentId> selected_parts = {});
  // Number of accepted transactions: zero for identical intent, one for a change.
  [[nodiscard]] Result<std::size_t> apply(GuiDocumentSession&, std::string label);
  void cancel() noexcept;

private:
  using Draft = std::variant<std::monostate, PartDocument, Project>;
  using Mutation = std::function<Result<std::size_t>(Draft&)>;
  [[nodiscard]] Result<std::size_t> stage(const GuiDocumentSession&,
                                          const std::function<Result<std::size_t>(Draft&)>&);
  Draft draft_;
  std::vector<Mutation> mutations_;
  std::string source_;
  std::vector<std::pair<DocumentId, std::uint64_t>> generations_;
  std::optional<std::size_t> prefix_;
  bool active_ = false;
};
} // namespace blcad::gui
