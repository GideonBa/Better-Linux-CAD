#include "blcad/gui/shell/shell_property_controls.hpp"

#include <QAbstractItemModel>
#include <QComboBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>

#include <utility>

namespace blcad::gui {

ShellPropertyGroup::ShellPropertyGroup(QString id, QString title, QWidget* parent)
    : QWidget(parent), title_(std::move(title)), heading_(new QToolButton(this)),
      content_(new QWidget(this)), content_layout_(new QVBoxLayout(content_)),
      diagnostic_(new QLabel(content_)) {
  setObjectName(id);
  setProperty("guiRole", "propertyGroup");
  auto* column = new QVBoxLayout(this);
  column->setContentsMargins(0, 0, 0, 0);
  heading_->setObjectName(id + ".toggle");
  heading_->setCheckable(true);
  heading_->setChecked(true);
  heading_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  heading_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  heading_->setAccessibleName(title_);
  content_->setObjectName(id + ".content");
  content_layout_->setContentsMargins(4, 0, 4, 4);
  diagnostic_->setObjectName(id + ".diagnostic");
  diagnostic_->setTextFormat(Qt::PlainText);
  diagnostic_->setWordWrap(true);
  diagnostic_->hide();
  content_layout_->addWidget(diagnostic_);
  column->addWidget(heading_);
  column->addWidget(content_);
  connect(heading_, &QToolButton::toggled, this, [this](bool expanded) {
    content_->setVisible(expanded);
    refresh_heading();
  });
  refresh_heading();
}

QWidget& ShellPropertyGroup::content() const noexcept {
  return *content_;
}
QVBoxLayout& ShellPropertyGroup::content_layout() const noexcept {
  return *content_layout_;
}
bool ShellPropertyGroup::expanded() const noexcept {
  return heading_->isChecked();
}
void ShellPropertyGroup::set_expanded(bool expanded) {
  heading_->setChecked(expanded);
}

void ShellPropertyGroup::set_diagnostic(QString text) {
  diagnostic_->setText(std::move(text));
  diagnostic_->setVisible(!diagnostic_->text().isEmpty());
  refresh_heading();
}

void ShellPropertyGroup::refresh_heading() {
  const bool invalid = !diagnostic_->text().isEmpty();
  heading_->setText(title_ + (invalid ? QStringLiteral(" (!)") : QString{}));
  heading_->setArrowType(expanded() ? Qt::DownArrow : Qt::RightArrow);
  heading_->setToolTip(invalid ? diagnostic_->text()
                               : (expanded() ? tr("Collapse %1") : tr("Expand %1")).arg(title_));
  heading_->setAccessibleDescription(invalid ? diagnostic_->text() : heading_->toolTip());
}

ShellPropertySelector::ShellPropertySelector(QString id, QString title, QComboBox& choices,
                                             QObject& owner, std::function<void(bool)> pick,
                                             QWidget* parent)
    : QWidget(parent), choices_(choices), pick_(new QToolButton(this)),
      clear_(new QToolButton(this)), count_(new QLabel(this)), diagnostic_(new QLabel(this)) {
  setObjectName(id);
  setProperty("guiRole", "propertySelector");
  auto* column = new QVBoxLayout(this);
  column->setContentsMargins(0, 0, 0, 0);
  auto* row = new QHBoxLayout();
  auto* label = new QLabel(std::move(title), this);
  label->setTextFormat(Qt::PlainText);
  label->setBuddy(&choices_);
  row->addWidget(label);
  row->addStretch();
  count_->setObjectName(id + ".count");
  row->addWidget(count_);
  pick_->setObjectName(id + ".pick");
  pick_->setText(tr("Pick"));
  pick_->setCheckable(true);
  pick_->setAccessibleName(tr("Pick %1").arg(label->text()));
  clear_->setObjectName(id + ".clear");
  clear_->setText(tr("Clear"));
  clear_->setAccessibleName(tr("Clear %1").arg(label->text()));
  row->addWidget(pick_);
  row->addWidget(clear_);
  column->addLayout(row);
  column->addWidget(&choices_);
  choices_.installEventFilter(this);
  diagnostic_->setObjectName(id + ".diagnostic");
  diagnostic_->setTextFormat(Qt::PlainText);
  diagnostic_->setWordWrap(true);
  diagnostic_->hide();
  column->addWidget(diagnostic_);
  connect(pick_, &QToolButton::toggled, &owner, [pick = std::move(pick)](bool active) {
    if (pick)
      pick(active);
  });
  connect(clear_, &QToolButton::clicked, this, [this] { choices_.setCurrentIndex(-1); });
  connect(&choices_, &QComboBox::currentIndexChanged, this, [this] { refresh_count(); });
  // Combo contents can change without a different current index (for example
  // when a new command replaces the model). Refresh on those model changes too.
  connect(choices_.model(), &QAbstractItemModel::modelReset, this, [this] { refresh_count(); });
  connect(choices_.model(), &QAbstractItemModel::rowsInserted, this, [this] { refresh_count(); });
  connect(choices_.model(), &QAbstractItemModel::rowsRemoved, this, [this] { refresh_count(); });
  connect(choices_.model(), &QAbstractItemModel::dataChanged, this, [this] { refresh_count(); });
  refresh_count();
}

void ShellPropertySelector::refresh_count() {
  const bool selected = choices_.currentIndex() >= 0 && choices_.currentData().isValid();
  count_->setText(selected ? tr("1 selected") : tr("0 selected"));
  clear_->setEnabled(selected && choices_.isEnabled());
  pick_->setEnabled(choices_.count() > 0 && choices_.isEnabled());
}

void ShellPropertySelector::set_active(bool active) {
  const QSignalBlocker guard(pick_);
  // Command owners also synchronize combos with signals temporarily blocked.
  refresh_count();
  pick_->setChecked(active);
  pick_->setToolTip(active ? tr("Finish selecting this input")
                           : tr("Select this input in the model"));
}

bool ShellPropertySelector::active() const noexcept {
  return pick_->isChecked();
}

bool ShellPropertySelector::eventFilter(QObject* watched, QEvent* event) {
  if (watched == &choices_ && event->type() == QEvent::EnabledChange)
    refresh_count();
  return QWidget::eventFilter(watched, event);
}

void ShellPropertySelector::set_diagnostic(QString text) {
  diagnostic_->setText(std::move(text));
  diagnostic_->setVisible(!diagnostic_->text().isEmpty());
  choices_.setAccessibleDescription(diagnostic_->text());
}

} // namespace blcad::gui
