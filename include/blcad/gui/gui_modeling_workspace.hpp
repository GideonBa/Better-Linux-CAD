#pragma once

#include "blcad/gui/gui_document_session.hpp"
#include "blcad/gui/occt_viewport.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace blcad::gui {

enum class GuiModelingArea { Part, Surface, Assembly };

enum class GuiModelingCommand {
  Extrude,
  Revolve,
  PathExtrude,
  Sweep,
  Loft,
  Fillet,
  Chamfer,
  Shell,
  Draft,
  LinearPattern,
  CircularPattern,
  Mirror,
  BodyBoolean,
  BodyTransform,
  BoundaryFill,
  SurfaceTrimExtend,
  SurfaceStitch,
  ClosedShellToSolid,
  ComponentPlacement,
  AssemblyRelationship,
  AssemblyJoint,
  DriveJoint,
};

enum class GuiModelingSelectionFilter {
  All,
  Profiles,
  Datums,
  Faces,
  Edges,
  Bodies,
  Components,
  AssemblyTargets,
};

enum class GuiViewCubeTarget { Home, Isometric, Front, Back, Left, Right, Top, Bottom };

struct GuiModelingCommandDescriptor {
  GuiModelingCommand command{GuiModelingCommand::Extrude};
  std::string_view id;
  std::string_view label;
  GuiModelingArea area{GuiModelingArea::Part};
  GuiDocumentKind document_kind{GuiDocumentKind::Part};
  GuiSelectionKind first_selection_kind{GuiSelectionKind::SketchEntity};
  std::string_view required_capability;
  std::size_t mini_toolbar_priority{0};
  bool repeatable{true};
};

struct GuiModelingPreselection {
  GuiSelection selection;
  std::vector<std::string> capabilities;

  [[nodiscard]] bool supports(std::string_view capability) const noexcept {
    return std::find(capabilities.begin(), capabilities.end(), capability) != capabilities.end();
  }
};

struct GuiNamedCameraBookmark {
  std::string name;
  GuiViewportCameraBookmark camera;
};

class GuiModelingWorkspace {
public:
  [[nodiscard]] static const std::array<std::string_view, 3>& tabs() noexcept {
    static constexpr std::array<std::string_view, 3> values{"Part", "Surface", "Assembly"};
    return values;
  }

  [[nodiscard]] static const std::array<GuiModelingCommandDescriptor, 22>& command_catalog() noexcept {
    static constexpr std::array<GuiModelingCommandDescriptor, 22> commands{{
        {GuiModelingCommand::Extrude, "part.extrude", "Extrude", GuiModelingArea::Part,
         GuiDocumentKind::Part, GuiSelectionKind::SketchEntity, "ProfileRegion", 0, true},
        {GuiModelingCommand::Revolve, "part.revolve", "Revolve", GuiModelingArea::Part,
         GuiDocumentKind::Part, GuiSelectionKind::SketchEntity, "ProfileRegion", 1, true},
        {GuiModelingCommand::PathExtrude, "part.path_extrude", "Path Extrude",
         GuiModelingArea::Part, GuiDocumentKind::Part, GuiSelectionKind::SketchEntity,
         "ProfileRegion", 4, true},
        {GuiModelingCommand::Sweep, "part.sweep", "Sweep", GuiModelingArea::Part,
         GuiDocumentKind::Part, GuiSelectionKind::SketchEntity, "ProfileRegion", 2, true},
        {GuiModelingCommand::Loft, "part.loft", "Loft", GuiModelingArea::Part,
         GuiDocumentKind::Part, GuiSelectionKind::SketchEntity, "ProfileRegion", 3, true},
        {GuiModelingCommand::Fillet, "part.fillet", "Fillet", GuiModelingArea::Part,
         GuiDocumentKind::Part, GuiSelectionKind::Edge, "Edge", 0, true},
        {GuiModelingCommand::Chamfer, "part.chamfer", "Chamfer", GuiModelingArea::Part,
         GuiDocumentKind::Part, GuiSelectionKind::Edge, "Edge", 1, true},
        {GuiModelingCommand::Shell, "part.shell", "Shell", GuiModelingArea::Part,
         GuiDocumentKind::Part, GuiSelectionKind::Face, "Face", 0, true},
        {GuiModelingCommand::Draft, "part.draft", "Draft", GuiModelingArea::Part,
         GuiDocumentKind::Part, GuiSelectionKind::Face, "Face", 1, true},
        {GuiModelingCommand::LinearPattern, "part.linear_pattern", "Linear Pattern",
         GuiModelingArea::Part, GuiDocumentKind::Part, GuiSelectionKind::Body, "Body", 2, true},
        {GuiModelingCommand::CircularPattern, "part.circular_pattern", "Circular Pattern",
         GuiModelingArea::Part, GuiDocumentKind::Part, GuiSelectionKind::Body, "Body", 3, true},
        {GuiModelingCommand::Mirror, "part.mirror", "Mirror", GuiModelingArea::Part,
         GuiDocumentKind::Part, GuiSelectionKind::Body, "Body", 4, true},
        {GuiModelingCommand::BodyBoolean, "part.body_boolean", "Boolean",
         GuiModelingArea::Part, GuiDocumentKind::Part, GuiSelectionKind::Body, "Body", 0, true},
        {GuiModelingCommand::BodyTransform, "part.body_transform", "Move Body",
         GuiModelingArea::Part, GuiDocumentKind::Part, GuiSelectionKind::Body, "Body", 1, true},
        {GuiModelingCommand::BoundaryFill, "surface.boundary_fill", "Boundary / Fill",
         GuiModelingArea::Surface, GuiDocumentKind::Part, GuiSelectionKind::Edge, "Edge", 0, true},
        {GuiModelingCommand::SurfaceTrimExtend, "surface.trim_extend", "Trim / Extend",
         GuiModelingArea::Surface, GuiDocumentKind::Part, GuiSelectionKind::Face, "Face", 0, true},
        {GuiModelingCommand::SurfaceStitch, "surface.stitch", "Stitch",
         GuiModelingArea::Surface, GuiDocumentKind::Part, GuiSelectionKind::Body, "Body", 0, true},
        {GuiModelingCommand::ClosedShellToSolid, "surface.closed_shell_to_solid",
         "Convert to Solid", GuiModelingArea::Surface, GuiDocumentKind::Part,
         GuiSelectionKind::Body, "ClosedShell", 1, true},
        {GuiModelingCommand::ComponentPlacement, "assembly.component_placement", "Place Component",
         GuiModelingArea::Assembly, GuiDocumentKind::Project, GuiSelectionKind::Component,
         "Component", 0, true},
        {GuiModelingCommand::AssemblyRelationship, "assembly.relationship", "Relationship",
         GuiModelingArea::Assembly, GuiDocumentKind::Project, GuiSelectionKind::AssemblyTarget,
         "AssemblyTarget", 0, true},
        {GuiModelingCommand::AssemblyJoint, "assembly.joint", "Joint",
         GuiModelingArea::Assembly, GuiDocumentKind::Project, GuiSelectionKind::AssemblyTarget,
         "AssemblyTarget", 1, true},
        {GuiModelingCommand::DriveJoint, "assembly.drive_joint", "Drive Joint",
         GuiModelingArea::Assembly, GuiDocumentKind::Project, GuiSelectionKind::AssemblyTarget,
         "DrivableJointCoordinate", 0, true},
    }};
    return commands;
  }

  [[nodiscard]] static constexpr std::uint32_t
  selection_filter_mask(GuiModelingSelectionFilter filter) noexcept {
    switch (filter) {
    case GuiModelingSelectionFilter::Profiles:
      return selection_kind_bit(GuiSelectionKind::SketchEntity);
    case GuiModelingSelectionFilter::Datums:
      return selection_kind_bit(GuiSelectionKind::Datum);
    case GuiModelingSelectionFilter::Faces:
      return selection_kind_bit(GuiSelectionKind::Face);
    case GuiModelingSelectionFilter::Edges:
      return selection_kind_bit(GuiSelectionKind::Edge);
    case GuiModelingSelectionFilter::Bodies:
      return selection_kind_bit(GuiSelectionKind::Body);
    case GuiModelingSelectionFilter::Components:
      return selection_kind_bit(GuiSelectionKind::Component);
    case GuiModelingSelectionFilter::AssemblyTargets:
      return selection_kind_bit(GuiSelectionKind::AssemblyTarget);
    case GuiModelingSelectionFilter::All:
      return selection_kind_bit(GuiSelectionKind::SketchEntity) |
             selection_kind_bit(GuiSelectionKind::Datum) |
             selection_kind_bit(GuiSelectionKind::Face) |
             selection_kind_bit(GuiSelectionKind::Edge) |
             selection_kind_bit(GuiSelectionKind::Body) |
             selection_kind_bit(GuiSelectionKind::Component) |
             selection_kind_bit(GuiSelectionKind::AssemblyTarget);
    }
    return 0;
  }

  [[nodiscard]] bool set_area(GuiDocumentSession& session, GuiModelingArea area) noexcept {
    if (session.task().active()) return false;
    const bool assembly = area == GuiModelingArea::Assembly;
    if ((assembly && session.document_kind() != GuiDocumentKind::Project) ||
        (!assembly && session.document_kind() != GuiDocumentKind::Part))
      return false;
    if (!session.set_workspace(assembly ? GuiWorkspace::Assembly : GuiWorkspace::Part)) return false;
    area_ = area;
    preselection_.reset();
    consumed_preselection_context_.reset();
    consumed_preselection_.clear();
    session.selection().clear();
    apply_selection_filter(session, GuiModelingSelectionFilter::All);
    return true;
  }

  [[nodiscard]] GuiModelingArea area() const noexcept { return area_; }

  [[nodiscard]] bool set_preselection(GuiDocumentSession& session,
                                      GuiModelingPreselection preselection) {
    if (session.task().active() || preselection.selection.semantic_id.empty() ||
        preselection.capabilities.empty() ||
        !session.selection().allows(preselection.selection.kind))
      return false;

    preselection.capabilities.erase(
        std::remove_if(preselection.capabilities.begin(), preselection.capabilities.end(),
                       [](const std::string& value) { return value.empty(); }),
        preselection.capabilities.end());
    std::sort(preselection.capabilities.begin(), preselection.capabilities.end());
    preselection.capabilities.erase(
        std::unique(preselection.capabilities.begin(), preselection.capabilities.end()),
        preselection.capabilities.end());
    if (preselection.capabilities.empty()) return false;

    session.selection().clear();
    if (!session.selection().add(preselection.selection)) return false;
    preselection_ = std::move(preselection);
    return true;
  }

  void clear_preselection(GuiDocumentSession& session) noexcept {
    preselection_.reset();
    session.selection().clear();
  }

  [[nodiscard]] const std::optional<GuiModelingPreselection>& preselection() const noexcept {
    return preselection_;
  }

  [[nodiscard]] std::vector<GuiModelingCommandDescriptor>
  enabled_commands(const GuiDocumentSession& session) const {
    std::vector<GuiModelingCommandDescriptor> enabled;
    for (const auto& command : command_catalog())
      if (eligible(session, command)) enabled.push_back(command);
    return enabled;
  }

  [[nodiscard]] std::vector<GuiModelingCommandDescriptor>
  mini_toolbar_commands(const GuiDocumentSession& session, std::size_t limit = 4) const {
    auto enabled = enabled_commands(session);
    std::sort(enabled.begin(), enabled.end(), [](const auto& left, const auto& right) {
      if (left.mini_toolbar_priority != right.mini_toolbar_priority)
        return left.mini_toolbar_priority < right.mini_toolbar_priority;
      return left.label < right.label;
    });
    if (enabled.size() > limit) enabled.resize(limit);
    return enabled;
  }

  [[nodiscard]] bool begin_command(GuiDocumentSession& session, GuiModelingCommand command) {
    const auto* descriptor = find_command(command);
    if (descriptor == nullptr || !eligible(session, *descriptor)) return false;

    consumed_preselection_ = session.selection().entries();
    consumed_preselection_context_ = preselection_;
    session.selection().clear();
    if (!session.task().begin(std::string(descriptor->id))) {
      restore_consumed_preselection(session);
      return false;
    }
    active_command_ = command;
    preselection_.reset();
    return true;
  }

  [[nodiscard]] bool finish_sketch_handoff(GuiDocumentSession& session, SketchId sketch,
                                            ProfileId profile) {
    if (session.document_kind() != GuiDocumentKind::Part || session.task().active() ||
        sketch.value().empty() || profile.value().empty())
      return false;
    if (!set_area(session, GuiModelingArea::Part)) return false;
    return set_preselection(
        session, {{GuiSelectionKind::SketchEntity,
                   "profile-region:" + sketch.value() + ":" + profile.value()},
                  {"ProfileRegion"}});
  }

  [[nodiscard]] bool mark_committed(GuiDocumentSession& session) noexcept {
    if (!active_command_.has_value() || !session.task().apply()) return false;
    const auto* descriptor = find_command(*active_command_);
    if (descriptor != nullptr && descriptor->repeatable) last_repeatable_command_ = *active_command_;
    active_command_.reset();
    consumed_preselection_.clear();
    consumed_preselection_context_.reset();
    return true;
  }

  [[nodiscard]] bool cancel_command(GuiDocumentSession& session) {
    if (!active_command_.has_value() || !session.task().cancel()) return false;
    restore_consumed_preselection(session);
    active_command_.reset();
    return true;
  }

  [[nodiscard]] bool repeat_last_command(GuiDocumentSession& session) {
    if (!last_repeatable_command_.has_value() || session.task().active()) return false;
    const auto* descriptor = find_command(*last_repeatable_command_);
    if (descriptor == nullptr || descriptor->area != area_ ||
        descriptor->document_kind != session.document_kind())
      return false;
    if (!session.task().begin(std::string(descriptor->id))) return false;
    active_command_ = descriptor->command;
    consumed_preselection_.clear();
    consumed_preselection_context_.reset();
    preselection_.reset();
    session.selection().clear();
    return true;
  }

  [[nodiscard]] std::string_view active_command_id() const noexcept {
    const auto* descriptor = active_command_ ? find_command(*active_command_) : nullptr;
    return descriptor ? descriptor->id : std::string_view{};
  }

  [[nodiscard]] std::string_view last_repeatable_command_id() const noexcept {
    const auto* descriptor = last_repeatable_command_ ? find_command(*last_repeatable_command_) : nullptr;
    return descriptor ? descriptor->id : std::string_view{};
  }

  void apply_selection_filter(GuiDocumentSession& session,
                              GuiModelingSelectionFilter filter) noexcept {
    selection_filter_ = filter;
    session.selection().set_filter_mask(selection_filter_mask(filter));
    if (preselection_.has_value() && !session.selection().allows(preselection_->selection.kind)) {
      preselection_.reset();
      session.selection().clear();
    }
  }

  void apply_selection_filter(GuiDocumentSession& session, OcctViewport& viewport,
                              GuiModelingSelectionFilter filter) noexcept {
    apply_selection_filter(session, filter);
    viewport.set_selection_filter_mask(selection_filter_mask(filter));
  }

  [[nodiscard]] GuiModelingSelectionFilter selection_filter() const noexcept {
    return selection_filter_;
  }

  void capture_home_view(const OcctViewport& viewport) { home_view_ = viewport.camera_bookmark(); }

  [[nodiscard]] bool activate_view_cube_target(OcctViewport& viewport,
                                                GuiViewCubeTarget target) const noexcept {
    if (target == GuiViewCubeTarget::Home)
      return home_view_.has_value() && viewport.restore_camera_bookmark(*home_view_);
    switch (target) {
    case GuiViewCubeTarget::Isometric: viewport.set_standard_view(GuiStandardView::Isometric); break;
    case GuiViewCubeTarget::Front: viewport.set_standard_view(GuiStandardView::Front); break;
    case GuiViewCubeTarget::Back: viewport.set_standard_view(GuiStandardView::Back); break;
    case GuiViewCubeTarget::Left: viewport.set_standard_view(GuiStandardView::Left); break;
    case GuiViewCubeTarget::Right: viewport.set_standard_view(GuiStandardView::Right); break;
    case GuiViewCubeTarget::Top: viewport.set_standard_view(GuiStandardView::Top); break;
    case GuiViewCubeTarget::Bottom: viewport.set_standard_view(GuiStandardView::Bottom); break;
    case GuiViewCubeTarget::Home: break;
    }
    return true;
  }

  [[nodiscard]] bool save_camera_bookmark(std::string name, const OcctViewport& viewport) {
    if (name.empty()) return false;
    const auto found = std::find_if(camera_bookmarks_.begin(), camera_bookmarks_.end(),
                                    [&name](const auto& value) { return value.name == name; });
    if (found != camera_bookmarks_.end()) {
      found->camera = viewport.camera_bookmark();
      return true;
    }
    camera_bookmarks_.push_back({std::move(name), viewport.camera_bookmark()});
    return true;
  }

  [[nodiscard]] bool restore_camera_bookmark(std::string_view name,
                                              OcctViewport& viewport) const noexcept {
    const auto found = std::find_if(camera_bookmarks_.begin(), camera_bookmarks_.end(),
                                    [name](const auto& value) { return value.name == name; });
    return found != camera_bookmarks_.end() && viewport.restore_camera_bookmark(found->camera);
  }

  [[nodiscard]] bool remove_camera_bookmark(std::string_view name) noexcept {
    const auto found = std::find_if(camera_bookmarks_.begin(), camera_bookmarks_.end(),
                                    [name](const auto& value) { return value.name == name; });
    if (found == camera_bookmarks_.end()) return false;
    camera_bookmarks_.erase(found);
    return true;
  }

  [[nodiscard]] const std::vector<GuiNamedCameraBookmark>& camera_bookmarks() const noexcept {
    return camera_bookmarks_;
  }

private:
  [[nodiscard]] const GuiModelingCommandDescriptor*
  find_command(GuiModelingCommand command) const noexcept {
    const auto& catalog = command_catalog();
    const auto found = std::find_if(catalog.begin(), catalog.end(), [command](const auto& value) {
      return value.command == command;
    });
    return found == catalog.end() ? nullptr : &*found;
  }

  [[nodiscard]] bool eligible(const GuiDocumentSession& session,
                              const GuiModelingCommandDescriptor& command) const noexcept {
    return preselection_.has_value() && !session.task().active() && command.area == area_ &&
           command.document_kind == session.document_kind() &&
           preselection_->selection.kind == command.first_selection_kind &&
           preselection_->supports(command.required_capability);
  }

  void restore_consumed_preselection(GuiDocumentSession& session) {
    session.selection().clear();
    for (const auto& selection : consumed_preselection_)
      (void)session.selection().add(selection);
    preselection_ = consumed_preselection_context_;
    consumed_preselection_.clear();
    consumed_preselection_context_.reset();
  }

  GuiModelingArea area_{GuiModelingArea::Part};
  GuiModelingSelectionFilter selection_filter_{GuiModelingSelectionFilter::All};
  std::optional<GuiModelingPreselection> preselection_;
  std::optional<GuiModelingPreselection> consumed_preselection_context_;
  std::optional<GuiModelingCommand> active_command_;
  std::optional<GuiModelingCommand> last_repeatable_command_;
  std::vector<GuiSelection> consumed_preselection_;
  std::optional<GuiViewportCameraBookmark> home_view_;
  std::vector<GuiNamedCameraBookmark> camera_bookmarks_;
};

} // namespace blcad::gui
