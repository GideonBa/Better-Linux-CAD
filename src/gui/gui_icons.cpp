#include "blcad/gui/gui_icons.hpp"

#include "blcad/gui/gui_appearance.hpp"
#include "blcad/gui/gui_document_browser.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QHash>
#include <QIconEngine>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QResource>

#include <algorithm>
#include <utility>

static void initialize_pixelart_assets() {
  Q_INIT_RESOURCE(blcad_gui_assets);
}

namespace blcad::gui {
namespace {

const QJsonObject& catalog() {
  static const QJsonObject value = [] {
    initialize_pixelart_assets();
    QFile file(QStringLiteral(":/blcad/gui/pixelarticons/catalog.json"));
    if (!file.open(QIODevice::ReadOnly))
      return QJsonObject{};
    return QJsonDocument::fromJson(file.readAll()).object();
  }();
  return value;
}

QString mapped_asset(const char* group, const QString& value) {
  return catalog().value(QString::fromLatin1(group)).toObject().value(value).toString();
}

class PixelartIconEngine final : public QIconEngine {
public:
  PixelartIconEngine(QString name, QImage mask) : name_(std::move(name)), mask_(std::move(mask)) {}

  QIconEngine* clone() const override {
    return new PixelartIconEngine(*this);
  }
  QString key() const override {
    return QStringLiteral("blcad.pixelarticons");
  }
  QString iconName() override {
    return name_;
  }
  bool isNull() override {
    return mask_.isNull();
  }

  QSize actualSize(const QSize& size, QIcon::Mode, QIcon::State) override {
    const int side = std::min(size.width(), size.height());
    return side > 0 ? QSize(side, side) : QSize{};
  }

  QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override {
    return render(size, mode, state);
  }

  QPixmap scaledPixmap(const QSize& size, QIcon::Mode mode, QIcon::State state,
                       qreal scale) override {
    // Qt 6.4 passes physical pixels; Qt 6.8+ passes device-independent pixels.
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    auto image = render(size * scale, mode, state);
#else
    auto image = render(size, mode, state);
#endif
    image.setDevicePixelRatio(scale);
    return image;
  }

  void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State state) override {
    const int side = std::min(rect.width(), rect.height());
    if (side <= 0)
      return;
    const qreal ratio = painter->device()->devicePixelRatioF();
    auto image = render(QSize(side, side) * ratio, mode, state);
    image.setDevicePixelRatio(ratio);
    painter->save();
    painter->setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter->drawPixmap(rect.x() + (rect.width() - side) / 2, rect.y() + (rect.height() - side) / 2,
                        image);
    painter->restore();
  }

private:
  QPixmap render(const QSize& size, QIcon::Mode mode, QIcon::State state) const {
    const int side = std::min(size.width(), size.height());
    if (side <= 0 || mask_.isNull())
      return {};
    auto image = mask_.scaled(side, side, Qt::IgnoreAspectRatio, Qt::FastTransformation)
                     .convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const QRgb foreground = mode == QIcon::Disabled                       ? appearance::muted
                            : mode == QIcon::Active || state == QIcon::On ? appearance::accent
                                                                          : appearance::text;
    QPainter tint(&image);
    tint.setCompositionMode(QPainter::CompositionMode_SourceIn);
    tint.fillRect(image.rect(), QColor::fromRgb(foreground));
    tint.end();
    return QPixmap::fromImage(image);
  }

  QString name_;
  QImage mask_;
};

} // namespace

QIcon pixelart_icon(QStringView name) {
  if (name.isEmpty())
    return {};
  (void)catalog();
  static QHash<QString, QIcon> cache;
  const QString key = name.toString();
  if (const auto found = cache.constFind(key); found != cache.cend())
    return *found;
  const QImage mask(QStringLiteral(":/blcad/gui/pixelarticons/masks/") + key + ".xpm");
  if (mask.isNull())
    return {};
  const QIcon icon(new PixelartIconEngine(key, mask));
  cache.insert(key, icon);
  return icon;
}

QIcon gui_icon_for_label(QStringView label) {
  QString text = label.toString();
  text.remove(QLatin1Char('&')); // Qt dialog mnemonic markers are presentation only.
  return pixelart_icon(mapped_asset("labels", text));
}

void apply_gui_action_icon(QAction& action) {
  if (!action.icon().isNull())
    return;
  QString id = action.objectName();
  id.replace(QStringLiteral("blcad.action.context_"), QStringLiteral("blcad.action."));
  const QString asset = mapped_asset("commands", id);
  action.setIcon(asset.isEmpty() ? gui_icon_for_label(action.text()) : pixelart_icon(asset));
}

void apply_gui_button_icon(QAbstractButton& button) {
  if (button.icon().isNull())
    button.setIcon(gui_icon_for_label(button.text()));
  if (!button.icon().isNull())
    button.setIconSize(QSize(24, 24));
}

QPushButton* make_gui_button(const QString& label, QWidget* parent) {
  auto* button = new QPushButton(label, parent);
  apply_gui_button_icon(*button);
  return button;
}

QPushButton* make_gui_button(QWidget* parent) {
  return make_gui_button(QString{}, parent);
}

void apply_gui_choice_icons(QComboBox& choices) {
  bool has_icons = false;
  for (int index = 0; index < choices.count(); ++index)
    if (choices.itemIcon(index).isNull()) {
      choices.setItemIcon(index, gui_icon_for_label(choices.itemText(index)));
      has_icons = has_icons || !choices.itemIcon(index).isNull();
    }
  if (has_icons)
    choices.setIconSize(QSize(24, 24));
}

void apply_gui_dialog_icons(QDialogButtonBox& buttons) {
  for (auto* button : buttons.buttons())
    apply_gui_button_icon(*button);
}

QIcon gui_icon_for_browser_node(const GuiBrowserNode& node) {
  using Kind = GuiBrowserNodeKind;
  QString kind;
  switch (node.kind) {
  case Kind::Document:
    kind = "Document";
    break;
  case Kind::Group:
    kind = "Group";
    break;
  case Kind::Parameter:
    kind = "Parameter";
    break;
  case Kind::Datum:
    if (!node.viewport_semantic_id.starts_with("datum-plane/") &&
        !node.viewport_semantic_id.starts_with("derived-workplane/"))
      return {};
    kind = "Datum";
    break;
  case Kind::Sketch:
    for (const auto& property : node.properties)
      if (property.key == "family")
        return pixelart_icon(mapped_asset("records", QString::fromStdString(property.value)));
    kind = "Sketch";
    break;
  case Kind::Path:
    kind = "Path";
    break;
  case Kind::Body:
    kind = "Body";
    break;
  case Kind::Assembly:
    kind = "Assembly";
    break;
  case Kind::Component:
    kind = "Component";
    break;
  case Kind::Subassembly:
    kind = "Subassembly";
    break;
  case Kind::Constraint:
    kind = "Constraint";
    break;
  case Kind::ExternalSource:
    kind = "ExternalSource";
    break;
  case Kind::Analysis:
    for (const auto& property : node.properties) {
      if (property.key == "severity") {
        if (property.value == "warning")
          return pixelart_icon(u"warning-diamond");
        if (property.value == "error")
          return pixelart_icon(u"square-alert");
        return pixelart_icon(u"circle-info");
      }
    }
    kind = "Analysis";
    break;
  case Kind::Construction:
    if (node.viewport_semantic_id.starts_with("construction-point/"))
      return pixelart_icon(u"gps");
    if (node.viewport_semantic_id.starts_with("construction-plane/"))
      return pixelart_icon(u"section");
    return {};
  case Kind::Feature:
  case Kind::Joint:
    return {}; // The pack has no faithful generic CAD feature/joint symbol.
  }
  return pixelart_icon(mapped_asset("browser", kind));
}

} // namespace blcad::gui
