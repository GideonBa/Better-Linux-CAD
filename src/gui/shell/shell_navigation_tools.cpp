#include "blcad/gui/shell/shell_navigation_tools.hpp"

#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QComboBox>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QToolButton>

#include <algorithm>

namespace blcad::gui {
ShellNavigationTools::ShellNavigationTools(ShellWindow& window, ShellRibbon& ribbon, int view_tab)
    : QObject(&window), window_(window) {
  const auto action = [&](const char* id, const char* label) {
    auto* result = new QAction(QString::fromUtf8(label), this);
    result->setObjectName(QStringLiteral("blcad.action.") + QString::fromLatin1(id));
    return result;
  };
  auto* navigation = ribbon.add_group(view_tab, QStringLiteral("Navigation"));
  fit_ = action("fit", "Einpassen");
  connect(fit_, &QAction::triggered, this, [this] {
    // Fit remains available in a planar Sketch, whose camera owns orientation.
    if (window_.session().has_document() && QWidget::mouseGrabber() != &window_.viewport()) {
      window_.viewport().fit_all();
      window_.refresh();
    }
  });
  ribbon.add_button(navigation, fit_);
  const struct {
    const char* id;
    const char* label;
    GuiViewCubeTarget target;
    bool direct;
  } definitions[] = {{"view_isometric", "Isometrisch", GuiViewCubeTarget::Isometric, true},
                     {"view_front", "Vorne", GuiViewCubeTarget::Front, true},
                     {"view_top", "Oben", GuiViewCubeTarget::Top, true},
                     {"view_right", "Rechts", GuiViewCubeTarget::Right, true},
                     {"view_back", "Hinten", GuiViewCubeTarget::Back, false},
                     {"view_left", "Links", GuiViewCubeTarget::Left, false},
                     {"view_bottom", "Unten", GuiViewCubeTarget::Bottom, false}};
  QList<QAction*> cube;
  for (const auto& definition : definitions) {
    auto* view = action(definition.id, definition.label);
    views_.push_back(view);
    cube.push_back(view);
    connect(view, &QAction::triggered, this, [this, target = definition.target] {
      if (available())
        restored(workspace_.activate_view_cube_target(window_.viewport(), target));
    });
    if (definition.direct)
      ribbon.add_button(navigation, view);
  }
  home_ = action("view_home", "Home");
  connect(home_, &QAction::triggered, this, [this] {
    if (available() && has_home_)
      restored(workspace_.activate_view_cube_target(window_.viewport(), GuiViewCubeTarget::Home));
  });
  cube.push_back(home_);
  auto* cube_button = ribbon.add_menu_button(navigation, QStringLiteral("Ansichtswürfel"), cube);
  cube_button->setObjectName(QStringLiteral("blcad.navigation.view_cube"));
  ribbon.add_small_button(navigation, home_);
  capture_home_ = action("capture_home", "Home setzen");
  connect(capture_home_, &QAction::triggered, this, [this] {
    if (!available())
      return;
    workspace_.capture_home_view(window_.viewport());
    has_home_ = true;
    refresh();
    window_.statusBar()->showMessage(QStringLiteral("Home-Ansicht gespeichert"));
  });
  ribbon.add_small_button(navigation, capture_home_);

  auto* marks = ribbon.add_group(view_tab, QStringLiteral("Ansichtsmarken (Sitzung)"));
  bookmarks_ = new QComboBox(marks);
  bookmarks_->setObjectName(QStringLiteral("blcad.navigation.bookmarks"));
  bookmarks_->setEditable(true);
  bookmarks_->setInsertPolicy(QComboBox::NoInsert);
  bookmarks_->setMinimumWidth(170);
  bookmarks_->setMaximumWidth(220);
  bookmarks_->lineEdit()->setPlaceholderText(QStringLiteral("Name der Ansicht"));
  bookmarks_->setToolTip(QStringLiteral("Ansicht wählen oder einen Namen zum Speichern eingeben"));
  ribbon.add_widget(marks, bookmarks_);
  connect(bookmarks_, &QComboBox::editTextChanged, this, [this] { refresh(); });
  save_ = action("bookmark_save", "Speichern");
  connect(save_, &QAction::triggered, this, [this] {
    if (!available())
      return;
    const auto name = bookmarks_->currentText().trimmed();
    if (workspace_.save_camera_bookmark(name.toStdString(), window_.viewport())) {
      reload_bookmarks(name);
      window_.statusBar()->showMessage(QStringLiteral("Ansichtsmarke gespeichert: %1").arg(name));
    }
  });
  restore_ = action("bookmark_restore", "Aufrufen");
  connect(restore_, &QAction::triggered, this, [this] {
    if (available())
      restored(workspace_.restore_camera_bookmark(bookmarks_->currentText().trimmed().toStdString(),
                                                  window_.viewport()));
  });
  remove_ = action("bookmark_remove", "Entfernen");
  connect(remove_, &QAction::triggered, this, [this] {
    if (available() &&
        workspace_.remove_camera_bookmark(bookmarks_->currentText().trimmed().toStdString()))
      reload_bookmarks({});
  });
  for (auto* button : {save_, restore_, remove_})
    ribbon.add_small_button(marks, button);
  refresh();
}

bool ShellNavigationTools::available() const {
  return window_.session().has_document() && !window_.active_sketch() &&
         !window_.viewport().sketch_focus_active() &&
         QWidget::mouseGrabber() != &window_.viewport();
}

void ShellNavigationTools::restored(bool success) {
  if (!success)
    window_.statusBar()->showMessage(
        QStringLiteral("Ansicht konnte nicht wiederhergestellt werden"));
  window_.refresh();
}

void ShellNavigationTools::reload_bookmarks(const QString& selected) {
  {
    const QSignalBlocker blocker(bookmarks_);
    bookmarks_->clear();
    for (const auto& bookmark : workspace_.camera_bookmarks())
      bookmarks_->addItem(QString::fromStdString(bookmark.name));
    bookmarks_->setEditText(selected);
  }
  refresh();
}

void ShellNavigationTools::refresh() {
  const bool enabled = available();
  fit_->setEnabled(window_.session().has_document() &&
                   QWidget::mouseGrabber() != &window_.viewport());
  for (auto* view : views_)
    view->setEnabled(enabled);
  home_->setEnabled(enabled && has_home_);
  capture_home_->setEnabled(enabled);
  bookmarks_->setEnabled(enabled);
  const auto name = bookmarks_->currentText().trimmed();
  const bool known =
      std::any_of(workspace_.camera_bookmarks().begin(), workspace_.camera_bookmarks().end(),
                  [&](const auto& bookmark) { return bookmark.name == name.toStdString(); });
  save_->setEnabled(enabled && !name.isEmpty());
  restore_->setEnabled(enabled && known);
  remove_->setEnabled(enabled && known);
}

void ShellNavigationTools::reset() {
  workspace_ = GuiModelingWorkspace{};
  has_home_ = false;
  reload_bookmarks({});
}
} // namespace blcad::gui
