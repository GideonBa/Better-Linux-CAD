#pragma once
#include <QBoxLayout>
#include <QEvent>
#include <QFormLayout>
#include <QMetaObject>
#include <QWidget>
#include <algorithm>
#include <vector>
namespace blcad::gui {
inline void set_form_row_visible(QFormLayout& form, QWidget* field, bool visible) {
  if (auto* label = form.labelForField(field))
    label->setVisible(visible);
  field->setVisible(visible);
}
// Retain the owned controls/focus while discarding obsolete wrapped-row geometry.
inline QFormLayout* rebuild_form_rows(QFormLayout* old) {
  struct Row {
    QWidget* label;
    QWidget* field;
    bool label_hidden;
    bool field_hidden;
  };
  std::vector<Row> rows;
  const auto margins = old->contentsMargins();
  const auto growth = old->fieldGrowthPolicy();
  const auto wrap = old->rowWrapPolicy();
  const auto labels = old->labelAlignment();
  const auto alignment = old->formAlignment();
  const int horizontal = old->horizontalSpacing(), vertical = old->verticalSpacing();
  auto* host = old->parentWidget();
  auto* box = qobject_cast<QBoxLayout*>(old->parent());
  int index = -1;
  if (box)
    for (int i = 0; i < box->count(); ++i)
      if (box->itemAt(i)->layout() == old)
        index = i;
  while (old->rowCount()) {
    const auto items = old->takeRow(0);
    auto* label = items.labelItem ? items.labelItem->widget() : nullptr;
    auto* field = items.fieldItem ? items.fieldItem->widget() : nullptr;
    Q_ASSERT(field);
    rows.push_back({label, field, label ? label->isHidden() : true, field->isHidden()});
    delete items.labelItem;
    delete items.fieldItem;
  }
  if (box && index >= 0)
    (void)box->takeAt(index);
  delete old;
  auto* form = box ? new QFormLayout : new QFormLayout(host);
  if (box)
    box->insertLayout(index, form);
  form->setContentsMargins(margins);
  form->setFieldGrowthPolicy(growth);
  form->setRowWrapPolicy(wrap);
  form->setLabelAlignment(labels);
  form->setFormAlignment(alignment);
  form->setHorizontalSpacing(horizontal);
  form->setVerticalSpacing(vertical);
  for (const auto& row : rows) {
    if (row.label)
      form->addRow(row.label, row.field);
    else
      form->addRow(row.field);
    if (row.label)
      row.label->setVisible(!row.label_hidden);
    row.field->setVisible(!row.field_hidden);
  }
  return form;
}
// QFormLayout can compress shown fields below their minimum row heights after
// wrapping or mode changes in a resizable scroll child. Reserve visible rows
// from their widgets' actual preferred heights, including nested forms.
class ScrollFormPanel final : public QWidget {
public:
  explicit ScrollFormPanel(QWidget* parent) : QWidget(parent) {}
  void refresh_minimum_height() {
    if (adjusting_ || !layout())
      return;
    adjusting_ = true;
    const int preferred = layout_height(*layout(), width());
    if (minimumHeight() != preferred) {
      setMinimumHeight(preferred);
      updateGeometry();
    }
    adjusting_ = false;
  }

protected:
  bool event(QEvent* event) override {
    const bool handled = QWidget::event(event);
    if (event->type() == QEvent::Resize || event->type() == QEvent::LayoutRequest ||
        event->type() == QEvent::Show) {
      refresh_minimum_height();
      if (!queued_) {
        queued_ = true;
        QMetaObject::invokeMethod(
            this,
            [this] {
              queued_ = false;
              refresh_minimum_height();
            },
            Qt::QueuedConnection);
      }
    }
    return handled;
  }

private:
  static int item_height(QLayoutItem* item, int width) {
    if (!item || item->isEmpty())
      return 0;
    if (auto* child = item->layout())
      return layout_height(*child, width);
    if (auto* widget = item->widget()) {
      int preferred = widget->sizeHint().height();
      if (widget->layout())
        preferred = layout_height(*widget->layout(), width);
      else if (widget->hasHeightForWidth())
        preferred = std::max(preferred, widget->heightForWidth(width));
      const int minimum = dynamic_cast<ScrollFormPanel*>(widget) ? 0 : widget->minimumHeight();
      return std::clamp(preferred, minimum, widget->maximumHeight());
    }
    return std::max(0, item->sizeHint().height());
  }
  static int layout_height(QLayout& layout, int width) {
    const auto margins = layout.contentsMargins();
    const int available = std::max(1, width - margins.left() - margins.right());
    const int spacing = std::max(0, layout.spacing());
    int height = 0, visible = 0;
    if (auto* form = qobject_cast<QFormLayout*>(&layout)) {
      int label_width = 0;
      for (int r = 0; r < form->rowCount(); ++r)
        if (auto* label = form->itemAt(r, QFormLayout::LabelRole); label && !label->isEmpty())
          label_width = std::max(label_width, label->sizeHint().width());
      const int horizontal = std::max(0, form->horizontalSpacing());
      for (int r = 0; r < form->rowCount(); ++r) {
        auto* label = form->itemAt(r, QFormLayout::LabelRole);
        auto* field = form->itemAt(r, QFormLayout::FieldRole);
        if (!field || field->isEmpty())
          continue;
        const bool labelled = label && !label->isEmpty();
        const int field_width =
            labelled ? std::max(1, available - label_width - horizontal) : available;
        const bool wrap = labelled && (form->rowWrapPolicy() == QFormLayout::WrapAllRows ||
                                       (form->rowWrapPolicy() == QFormLayout::WrapLongRows &&
                                        field->minimumSize().width() > field_width));
        const int label_height = labelled ? item_height(label, wrap ? available : label_width) : 0;
        const int field_height = item_height(field, wrap ? available : field_width);
        height +=
            wrap ? label_height + spacing + field_height : std::max(label_height, field_height);
        ++visible;
      }
    } else if (auto* box = qobject_cast<QBoxLayout*>(&layout);
               box && (box->direction() == QBoxLayout::TopToBottom ||
                       box->direction() == QBoxLayout::BottomToTop)) {
      for (int i = 0; i < layout.count(); ++i)
        if (auto* item = layout.itemAt(i); !item->isEmpty()) {
          height += item_height(item, available);
          ++visible;
        }
    } else
      return std::max(layout.sizeHint().height(), layout.totalHeightForWidth(width));
    return margins.top() + margins.bottom() + height + std::max(0, visible - 1) * spacing;
  }
  bool adjusting_ = false, queued_ = false;
};
} // namespace blcad::gui
