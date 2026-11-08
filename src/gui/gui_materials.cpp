#include "blcad/gui/gui_materials.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"

#include <type_traits>
#include <unordered_set>

namespace blcad::gui {
namespace {
Error error(std::string message) {
  return Error::validation("gui.materials", std::move(message));
}
Result<std::size_t> fail(std::string message) {
  return Result<std::size_t>::failure(error(std::move(message)));
}
Result<std::string> source(const GuiDocumentSession& session) {
  if (const auto* part = session.part_document())
    return serialize_part_document_to_json(*part);
  if (const auto* project = session.project())
    return serialize_project_to_json(*project);
  return Result<std::string>::failure(error("Materials require an active document"));
}
bool ready(const GuiDocumentSession& session) {
  if (session.task().active() && session.task().command_id() != GuiMaterialController::task_id)
    return false;
  if (session.part_document())
    return session.part_modeling_ready() &&
           (!session.part_workflow_active() || session.history_prefix_count().has_value());
  return session.project() && session.derived_results_fresh();
}
bool single_part_selection(const PartDocument& part, const std::vector<DocumentId>& selected) {
  return selected.empty() || (selected.size() == 1 && selected.front() == part.id());
}
} // namespace

Result<std::size_t> GuiMaterialController::begin(const GuiDocumentSession& session) {
  if (active_)
    return fail("Material editing is already active");
  if (!ready(session))
    return fail("Materials require a ready document and an idle command context");
  const auto serialized = source(session);
  if (!serialized)
    return Result<std::size_t>::failure(serialized.error());
  generations_.clear();
  mutations_.clear();
  if (const auto* p = session.part_document()) {
    draft_ = *p;
    generations_.push_back({p->id(), p->executable_generation()});
  } else {
    draft_ = *session.project();
    for (const auto& p : session.project()->part_documents())
      generations_.push_back({p.id(), p.executable_generation()});
  }
  source_ = serialized.value();
  prefix_ = session.history_prefix_count();
  active_ = true;
  return Result<std::size_t>::success(1);
}
bool GuiMaterialController::current(const GuiDocumentSession& session) const {
  if (!active_ || !ready(session) || prefix_ != session.history_prefix_count() ||
      bool(part()) != bool(session.part_document()) || bool(project()) != bool(session.project()))
    return false;
  const auto serialized = source(session);
  if (!serialized || serialized.value() != source_)
    return false;
  for (const auto& [id, generation] : generations_) {
    const auto* p = session.part_document() ? session.part_document()
                                            : session.project()->find_part_document(id);
    if (!p || p->id() != id || p->executable_generation() != generation)
      return false;
  }
  return true;
}
const PartDocument* GuiMaterialController::part() const noexcept {
  return std::get_if<PartDocument>(&draft_);
}
const Project* GuiMaterialController::project() const noexcept {
  return std::get_if<Project>(&draft_);
}
const EngineeringResourceCatalog* GuiMaterialController::catalog() const noexcept {
  return part()      ? &part()->engineering_resources()
         : project() ? &project()->engineering_resources()
                     : nullptr;
}
bool GuiMaterialController::changed() const {
  if (!active_)
    return false;
  const auto value =
      part() ? serialize_part_document_to_json(*part()) : serialize_project_to_json(*project());
  return value && value.value() != source_;
}
Result<std::size_t>
GuiMaterialController::stage(const GuiDocumentSession& session,
                             const std::function<Result<std::size_t>(Draft&)>& mutation) {
  if (!current(session))
    return fail("The material source or command context changed; restart editing");
  auto candidate = draft_;
  auto result = mutation(candidate);
  if (!result)
    return result;
  if (auto* p = std::get_if<Project>(&candidate)) {
    const auto synchronized = p->synchronize_engineering_resources();
    if (!synchronized)
      return synchronized;
  }
  const auto valid = std::holds_alternative<PartDocument>(candidate)
                         ? serialize_part_document_to_json(std::get<PartDocument>(candidate))
                         : serialize_project_to_json(std::get<Project>(candidate));
  if (!valid)
    return Result<std::size_t>::failure(valid.error());
  mutations_.push_back(mutation);
  draft_ = std::move(candidate);
  return result;
}
Result<std::size_t> GuiMaterialController::stage_material(const GuiDocumentSession& session,
                                                          EngineeringMaterial material) {
  return stage(session, [material = std::move(material)](Draft& candidate) {
    if (auto* p = std::get_if<PartDocument>(&candidate))
      return p->add_engineering_resource(material);
    return std::get<Project>(candidate).add_engineering_resource(material);
  });
}
Result<std::size_t>
GuiMaterialController::stage_assignment(const GuiDocumentSession& session, DocumentId id,
                                        std::optional<BodyId> body,
                                        std::optional<EngineeringResourceKey> material) {
  return stage(session, [id, body, material](Draft& candidate) {
    auto* p = std::get_if<PartDocument>(&candidate);
    if (auto* root = std::get_if<Project>(&candidate)) {
      p = root->find_part_document(id);
      if (!p)
        return fail("Material target must be an owned Part");
      if (material) {
        const auto* snapshot = root->engineering_resources().find_material(*material);
        if (!snapshot)
          return fail("Choose a material revision from the Project catalogue");
        const auto copied = p->add_engineering_resource(*snapshot);
        if (!copied)
          return copied;
      }
    }
    if (!p || p->id() != id)
      return fail("Material target must be the active Part");
    return body ? p->set_body_material(*body, material) : p->set_default_material(material);
  });
}
Result<std::size_t> GuiMaterialController::stage_revision(const GuiDocumentSession& session,
                                                          const EngineeringResourceKey& key,
                                                          EngineeringMaterial replacement,
                                                          std::vector<DocumentId> selected) {
  return stage(session, [key, replacement = std::move(replacement),
                         selected = std::move(selected)](Draft& candidate) {
    if (auto* p = std::get_if<PartDocument>(&candidate)) {
      if (!single_part_selection(*p, selected))
        return fail("Revision selection must identify the active Part");
      return p->adopt_engineering_resource_revision(key, replacement);
    }
    return std::get<Project>(candidate).adopt_engineering_resource_revision(key, replacement,
                                                                            selected);
  });
}
Result<std::size_t> GuiMaterialController::stage_remove(const GuiDocumentSession& session,
                                                        const EngineeringResourceKey& key,
                                                        std::vector<DocumentId> selected) {
  return stage(session, [key, selected = std::move(selected)](Draft& candidate) {
    if (auto* p = std::get_if<PartDocument>(&candidate)) {
      if (!single_part_selection(*p, selected))
        return fail("Removal selection must identify the active Part");
      return p->remove_engineering_resource(key);
    }
    auto& root = std::get<Project>(candidate);
    std::unordered_set<std::string> seen;
    for (const auto& id : selected) {
      auto* p = root.find_part_document(id);
      if (!p || !seen.insert(id.value()).second)
        return fail("Removal requires unique existing owned Part selections");
      const auto removed = p->remove_engineering_resource(key);
      if (!removed)
        return removed;
    }
    return root.remove_engineering_resource(key);
  });
}
Result<std::size_t> GuiMaterialController::apply(GuiDocumentSession& session, std::string label) {
  if (label.empty() || !current(session))
    return fail("The material source or command context changed; restart editing");
  if (!changed()) {
    cancel();
    return Result<std::size_t>::success(0);
  }
  const auto replay = [&](auto& document) -> Result<std::size_t> {
    Draft candidate = document;
    for (const auto& mutation : mutations_) {
      const auto changed = mutation(candidate);
      if (!changed)
        return changed;
      if (auto* root = std::get_if<Project>(&candidate)) {
        const auto synchronized = root->synchronize_engineering_resources();
        if (!synchronized)
          return synchronized;
      }
    }
    // Replay only typed metadata operations onto the session's validated
    // candidate; never replace freshly normalized geometry/expression state.
    document = std::get<std::decay_t<decltype(document)>>(std::move(candidate));
    return Result<std::size_t>::success(1);
  };
  const auto result = part() ? session.commit_part_transaction(std::move(label), replay)
                             : session.commit_project_transaction(std::move(label), replay);
  if (result)
    cancel();
  return result;
}
void GuiMaterialController::cancel() noexcept {
  draft_ = std::monostate{};
  mutations_.clear();
  source_.clear();
  generations_.clear();
  prefix_.reset();
  active_ = false;
}
} // namespace blcad::gui
