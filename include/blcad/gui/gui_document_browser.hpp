#pragma once

#include "blcad/gui/gui_document_session.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace blcad::gui {

enum class GuiBrowserNodeKind {
  Document,
  Group,
  Parameter,
  Datum,
  Construction,
  Sketch,
  Path,
  Feature,
  Body,
  Assembly,
  Component,
  Subassembly,
  Constraint,
  Joint,
  Analysis,
};

enum class GuiPropertyKind { Identifier, Text, Quantity, Expression, Enumeration, Boolean, Reference, OrderedInputs, Status };

struct GuiPropertyValue {
  std::string key;
  std::string label;
  std::string value;
  GuiPropertyKind kind{GuiPropertyKind::Text};
  bool editable{false};
  std::vector<std::string> choices;
  std::string read_only_reason;
};

struct GuiBrowserNode {
  GuiBrowserNode() = default;
  GuiBrowserNode(GuiBrowserNodeKind node_kind, std::string id, std::string owner,
                 std::string display_label,
                 std::optional<GuiSelectionKind> semantic_selection = std::nullopt,
                 std::vector<GuiPropertyValue> node_properties = {},
                 std::vector<GuiBrowserNode> node_children = {},
                 std::string presentation_semantic_id = {})
      : kind(node_kind), semantic_id(std::move(id)), owner_document_id(std::move(owner)),
        label(std::move(display_label)), selection_kind(semantic_selection),
        properties(std::move(node_properties)), children(std::move(node_children)),
        viewport_semantic_id(std::move(presentation_semantic_id)) {}

  GuiBrowserNodeKind kind{GuiBrowserNodeKind::Group};
  std::string semantic_id;
  std::string owner_document_id;
  std::string label;
  std::optional<GuiSelectionKind> selection_kind;
  std::vector<GuiPropertyValue> properties;
  std::vector<GuiBrowserNode> children;
  std::string viewport_semantic_id;
};

class GuiDocumentBrowser {
public:
  [[nodiscard]] static GuiDocumentBrowser build(const GuiDocumentSession& session);

  [[nodiscard]] const std::vector<GuiBrowserNode>& roots() const noexcept;
  [[nodiscard]] const GuiBrowserNode* find(std::string_view semantic_id) const noexcept;

private:
  std::vector<GuiBrowserNode> roots_;
};

// Validates and commits only edits explicitly supported by current Core contracts.
// The same function powers Preview (commit=false) and Apply (commit=true).
[[nodiscard]] Result<std::size_t> edit_browser_property(GuiDocumentSession& session,
                                                        const GuiBrowserNode& node,
                                                        std::string_view key,
                                                        std::string_view value,
                                                        bool commit);

} // namespace blcad::gui
