#include "blcad/gui/gui_appearance.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QEventLoop>
#include <QFile>
#include <QFontInfo>
#include <QImage>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollBar>
#include <QSpinBox>
#include <QStyle>
#include <QStyleOption>
#include <QTabBar>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

using namespace blcad::gui;

namespace {

QImage rendered(QWidget& widget) {
  widget.ensurePolished();
  // Quiet ribbon buttons are deliberately transparent. Composite their real
  // parent surface instead of treating an isolated transparent pixel as a fill.
  QWidget* surface = widget.window();
  QImage image(surface->size(), QImage::Format_RGB32);
  image.fill(Qt::magenta);
  surface->render(&image);
  return image.copy(QRect(widget.mapTo(surface, QPoint()), widget.size()));
}

void check_square_frame(QWidget& widget, QColor frame) {
  const auto image = rendered(widget);
  CHECK(image.pixelColor(0, 0) == frame);
  CHECK(image.pixelColor(image.width() - 1, 0) == frame);
  CHECK(image.pixelColor(0, image.height() - 1) == frame);
  CHECK(image.pixelColor(image.width() - 1, image.height() - 1) == frame);
}

void check_uniform_fill(QWidget& widget, QColor fill) {
  const auto image = rendered(widget);
  // A clear interior column catches gradients and desktop-theme state fills
  // without assuming the rasterized text's exact position or glyph shape.
  for (int y = 3; y < image.height() - 3; ++y)
    CHECK(image.pixelColor(4, y) == fill);
}

double luminance(QColor color) {
  const auto linear = [](double channel) {
    return channel <= 0.04045 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
  };
  return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF()) +
         0.0722 * linear(color.blueF());
}

double contrast(QColor first, QColor second) {
  const auto a = luminance(first);
  const auto b = luminance(second);
  return (std::max(a, b) + 0.05) / (std::min(a, b) + 0.05);
}

void capture(QWidget& widget, const QString& name) {
  const QString directory = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (!directory.isEmpty())
    REQUIRE(rendered(widget).save(directory + QLatin1Char('/') + name));
}

std::size_t matching_pixels(const QImage& image, QRect area, QColor color) {
  std::size_t count = 0;
  area = area.intersected(image.rect());
  for (int y = area.top(); y <= area.bottom(); ++y)
    for (int x = area.left(); x <= area.right(); ++x)
      if (image.pixelColor(x, y) == color)
        ++count;
  return count;
}

} // namespace

TEST_CASE("Hermes-inspired GUI uses readable offline fonts and square blue-paper controls",
          "[gui][appearance]") {
  const QColor blue = QColor::fromRgb(appearance::blue);
  const QColor paper = QColor::fromRgb(appearance::paper);
  const QColor ink = QColor::fromRgb(appearance::ink);
  const QColor muted = QColor::fromRgb(appearance::muted);
  const QColor line = QColor::fromRgb(appearance::line);
  const QColor edge = QColor::fromRgb(appearance::edge);
  const QColor white = Qt::white;

  QWidget panel;
  auto* layout = new QVBoxLayout(&panel);
  auto* button = new QPushButton(QStringLiteral("Secondary action"), &panel);
  auto* primary = new QPushButton(QStringLiteral("Apply feature"), &panel);
  primary->setProperty("guiRole", "primary");
  auto* disabled = new QPushButton(QStringLiteral("Unavailable action"), &panel);
  disabled->setEnabled(false);
  auto* pressed = new QPushButton(QStringLiteral("Pressed action"), &panel);
  pressed->setDown(true);
  auto* input = new QLineEdit(QStringLiteral("12.5"), &panel);
  auto* choice = new QComboBox(&panel);
  choice->addItems({QStringLiteral("Distance"), QStringLiteral("Radius")});
  auto* check = new QCheckBox(QStringLiteral("Reference dimension"), &panel);
  auto* scroll = new QScrollBar(Qt::Horizontal, &panel);
  scroll->setRange(0, 100);
  scroll->setPageStep(20);
  layout->addWidget(button);
  layout->addWidget(primary);
  layout->addWidget(disabled);
  layout->addWidget(pressed);
  layout->addWidget(input);
  layout->addWidget(choice);
  layout->addWidget(check);
  layout->addWidget(scroll);
  panel.resize(380, 340);
  panel.show();
  QApplication::processEvents();
  button->clearFocus();
  button->setAttribute(Qt::WA_UnderMouse, false);
  primary->clearFocus();
  primary->setAttribute(Qt::WA_UnderMouse, false);

  CHECK(QLocale().language() == QLocale::English);
  CHECK(QLocale().decimalPoint() == QStringLiteral("."));
  CHECK(QApplication::testAttribute(Qt::AA_DontUseNativeDialogs));
  CHECK(QFontInfo(QApplication::font()).family().startsWith(QStringLiteral("Barlow")));
  CHECK(QApplication::font().pixelSize() == 14);
  CHECK(QFontInfo(appearance::heading_font())
            .family()
            .startsWith(QStringLiteral("Barlow Condensed")));
  CHECK(QFontInfo(appearance::numeric_font()).family() == QStringLiteral("IBM Plex Mono"));
  CHECK(QFontInfo(appearance::numeric_font()).fixedPitch());
  CHECK(QFontInfo(input->font()).family() == QStringLiteral("IBM Plex Mono"));
  for (const auto* font : {"Barlow-Regular.ttf", "Barlow-Medium.ttf", "BarlowCondensed-Medium.ttf",
                           "IBMPlexMono-Regular.ttf"})
    CHECK(QFile::exists(QStringLiteral(":/blcad/gui/fonts/") + QString::fromLatin1(font)));

  const QPalette palette = QApplication::palette();
  for (const auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
    const QColor foreground = group == QPalette::Disabled ? muted : ink;
    CHECK(palette.color(group, QPalette::Window) == paper);
    CHECK(palette.color(group, QPalette::Button) == paper);
    CHECK(palette.color(group, QPalette::Base) == white);
    CHECK(palette.color(group, QPalette::WindowText) == foreground);
    CHECK(palette.color(group, QPalette::Text) == foreground);
    CHECK(palette.color(group, QPalette::ButtonText) == foreground);
    CHECK(palette.color(group, QPalette::Highlight) == blue);
    CHECK(palette.color(group, QPalette::HighlightedText) == white);
    CHECK(contrast(foreground, paper) >= 4.5);
    CHECK(contrast(foreground, white) >= 4.5);
  }
  CHECK(contrast(paper, blue) >= 4.5);
  CHECK(contrast(white, blue) >= 4.5);
  CHECK(contrast(edge, paper) >= 3.0);

  check_square_frame(*button, edge);
  check_uniform_fill(*button, paper);
  check_square_frame(*primary, blue);
  check_uniform_fill(*primary, blue);
  CHECK(primary->palette().color(QPalette::ButtonText) == paper);
  check_square_frame(*disabled, line);
  check_uniform_fill(*disabled, paper);
  CHECK(disabled->palette().color(QPalette::Disabled, QPalette::ButtonText) == muted);

  button->setDown(true);
  check_square_frame(*button, blue);
  check_uniform_fill(*button, blue);
  button->setDown(false);
  button->setCheckable(true);
  button->setChecked(true);
  check_square_frame(*button, blue);
  check_uniform_fill(*button, blue);
  check->setChecked(true);
  check->setEnabled(false);
  QStyleOptionButton check_option;
  check_option.initFrom(check);
  const QRect indicator =
      check->style()->subElementRect(QStyle::SE_CheckBoxIndicator, &check_option, check);
  const QRect check_interior = indicator.adjusted(3, 3, -3, -3);
  CHECK(matching_pixels(rendered(*check), check_interior, muted) > 8U);
  check->setChecked(false);
  CHECK(matching_pixels(rendered(*check), check_interior, muted) == 0U);
  check->setEnabled(true);
  check->setChecked(true);
  QStyleOptionSlider scroll_option;
  scroll_option.initFrom(scroll);
  scroll_option.orientation = Qt::Horizontal;
  scroll_option.minimum = scroll->minimum();
  scroll_option.maximum = scroll->maximum();
  scroll_option.pageStep = scroll->pageStep();
  scroll_option.sliderPosition = scroll->sliderPosition();
  scroll_option.sliderValue = scroll->value();
  const QRect thumb = scroll->style()->subControlRect(QStyle::CC_ScrollBar, &scroll_option,
                                                      QStyle::SC_ScrollBarSlider, scroll);
  REQUIRE(thumb.width() > 8);
  CHECK(contrast(rendered(*scroll).pixelColor(thumb.center()), paper) >= 3.0);
  choice->setCurrentIndex(1);
  CHECK(input->text() == QStringLiteral("12.5"));
  CHECK(choice->currentText() == QStringLiteral("Radius"));

  const bool native = QApplication::platformName() == QStringLiteral("xcb");
  if (native) {
    panel.raise();
    panel.activateWindow();
    QGuiApplication::sync();
  }
  input->setFocus(Qt::OtherFocusReason);
  QApplication::processEvents();
  const auto input_focused = [&] {
    return (!native || panel.isActiveWindow()) && input->hasFocus();
  };
  if (!input_focused()) {
    // Native activation and focus arrive through separate window-system events.
    QEventLoop transition;
    QTimer poll;
    QTimer deadline;
    deadline.setSingleShot(true);
    QObject::connect(&poll, &QTimer::timeout, &transition, [&] {
      if (panel.isActiveWindow() && !input->hasFocus())
        input->setFocus(Qt::OtherFocusReason);
      if (input_focused())
        transition.quit();
    });
    QObject::connect(&deadline, &QTimer::timeout, &transition, &QEventLoop::quit);
    poll.start(5);
    deadline.start(2000);
    transition.exec();
  }
  if (native)
    REQUIRE(panel.isActiveWindow());
  REQUIRE(input->hasFocus());
  check_square_frame(*input, blue);
  check_uniform_fill(*input, white);
  // Focus changes color while retaining the same one-pixel field frame.
  const auto focused_input = rendered(*input);
  CHECK(focused_input.pixelColor(0, focused_input.height() / 2) == blue);
  CHECK(focused_input.pixelColor(1, focused_input.height() / 2) == white);
  CHECK(rendered(panel).pixelColor(0, 0) == paper);
  capture(panel, QStringLiteral("hermes-controls-states.png"));
}

TEST_CASE("Hermes-inspired ribbon and menus retain English labels and distinct tool states",
          "[gui][appearance][ribbon]") {
  const QColor blue = QColor::fromRgb(appearance::blue);
  const QColor paper = QColor::fromRgb(appearance::paper);
  ShellRibbon ribbon;
  const int model = ribbon.add_tab(QStringLiteral("3D Model"));
  const int sketch = ribbon.add_tab(QStringLiteral("Sketch"));
  (void)ribbon.add_tab(QStringLiteral("View"));
  const int assembly = ribbon.add_tab(QStringLiteral("Assembly"));
  auto* group = ribbon.add_group(model, QStringLiteral("Create"));
  QAction extrude(QStringLiteral("Extrude"), &ribbon);
  auto* command = ribbon.add_button(group, &extrude);
  QAction fillet(QStringLiteral("Fillet"), &ribbon);
  QAction chamfer(QStringLiteral("Chamfer"), &ribbon);
  auto* menu_button = ribbon.add_menu_button(group, QStringLiteral("Modify"), {&fillet, &chamfer});
  ribbon.resize(560, 180);
  ribbon.show();
  QApplication::processEvents();
  command->clearFocus();
  command->setAttribute(Qt::WA_UnderMouse, false);
  menu_button->setAttribute(Qt::WA_UnderMouse, false);

  CHECK(ribbon.tab_title(model) == QStringLiteral("3D Model"));
  CHECK(ribbon.tab_title(sketch) == QStringLiteral("Sketch"));
  CHECK(command->text() == QStringLiteral("Extrude"));
  CHECK(menu_button->menu()->actions().at(0)->text() == QStringLiteral("Fillet"));
  CHECK(menu_button->menu()->actions().at(1)->text() == QStringLiteral("Chamfer"));
  check_uniform_fill(*command, paper);
  command->setCheckable(true);
  command->setChecked(true);
  check_square_frame(*command, blue);
  check_uniform_fill(*command, blue);
  ribbon.set_current_tab(sketch);
  CHECK(ribbon.current_tab() == sketch);
  ribbon.set_current_tab(model);
  CHECK(command->isVisible());
  check_uniform_fill(*menu_button, paper);
  capture(ribbon, QStringLiteral("hermes-english-ribbon.png"));

  ribbon.resize(360, 180);
  QApplication::processEvents();
  auto* tabs = ribbon.findChild<QTabBar*>(QStringLiteral("blcad_ribbon_tabs"));
  REQUIRE(tabs);
  auto* next = ribbon.findChild<QToolButton*>(QStringLiteral("blcad.ribbon.tab_next"));
  REQUIRE(next);
  const auto navigation_ready = [&] {
    const QRect bounds(next->mapTo(&ribbon, QPoint()), next->size());
    return ribbon.size() == QSize(360, 180) && next->isVisible() && next->isEnabled() &&
           ribbon.rect().contains(bounds) &&
           ribbon.childAt(next->mapTo(&ribbon, next->rect().center())) == next;
  };
  if (!navigation_ready()) {
    // Native resize/configure events can arrive after processEvents returns.
    // Await the requested compact layout without accepting a larger window.
    QEventLoop transition;
    QTimer poll;
    QTimer deadline;
    deadline.setSingleShot(true);
    QObject::connect(&poll, &QTimer::timeout, &transition, [&] {
      if (navigation_ready())
        transition.quit();
    });
    QObject::connect(&deadline, &QTimer::timeout, &transition, &QEventLoop::quit);
    poll.start(5);
    deadline.start(2000);
    transition.exec();
  }
  REQUIRE(ribbon.size() == QSize(360, 180));
  REQUIRE(next->isVisible());
  REQUIRE(next->isEnabled());
  INFO("Navigation button " << next->width() << 'x' << next->height() << " at "
                            << next->mapTo(&ribbon, QPoint()).x() << ','
                            << next->mapTo(&ribbon, QPoint()).y() << "; ribbon " << ribbon.width()
                            << 'x' << ribbon.height() << "; null icon " << next->icon().isNull());
  const auto* cluster = next->parentWidget();
  const auto* header = cluster->parentWidget();
  INFO("Navigation cluster " << cluster->width() << 'x' << cluster->height() << " at "
                             << cluster->mapTo(&ribbon, QPoint()).x() << ','
                             << cluster->mapTo(&ribbon, QPoint()).y() << "; header "
                             << header->width() << 'x' << header->height());
  capture(*next, QStringLiteral("hermes-navigation-next.png"));
  capture(ribbon, QStringLiteral("hermes-navigation-before-scroll.png"));
  CHECK(ribbon.rect().contains(QRect(next->mapTo(&ribbon, QPoint()), next->size())));
  CHECK(ribbon.childAt(next->mapTo(&ribbon, next->rect().center())) == next);
  CHECK(matching_pixels(rendered(*next), next->rect().adjusted(2, 2, -2, -2), Qt::white) >= 3U);
  const auto click = [](QWidget& control, QPoint point) {
    const QPoint global = control.mapToGlobal(point);
    QMouseEvent press(QEvent::MouseButtonPress, point, global, Qt::LeftButton, Qt::LeftButton,
                      Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, point, global, Qt::LeftButton, Qt::NoButton,
                        Qt::NoModifier);
    QApplication::sendEvent(&control, &press);
    QApplication::sendEvent(&control, &release);
    QApplication::processEvents();
  };
  for (int attempt = 0; next->isEnabled() && attempt < 8; ++attempt)
    click(*next, next->rect().center());
  const QPoint last_tab = tabs->tabRect(assembly).center();
  const QPoint visible_tab = tabs->mapTo(&ribbon, last_tab);
  REQUIRE(ribbon.rect().contains(visible_tab));
  CHECK(ribbon.childAt(visible_tab) == tabs);
  click(*tabs, last_tab);
  CHECK(ribbon.current_tab() == assembly);
  capture(ribbon, QStringLiteral("hermes-navigation-compact.png"));
}

TEST_CASE("Hermes-inspired numeric controls retain visible arrows and opaque readable placeholders",
          "[gui][appearance]") {
  const QColor blue = QColor::fromRgb(appearance::blue);
  const QColor muted = QColor::fromRgb(appearance::muted);
  QWidget panel;
  auto* layout = new QVBoxLayout(&panel);
  auto* choice = new QComboBox(&panel);
  choice->addItem(QStringLiteral("Operation"));
  auto* spin = new QSpinBox(&panel);
  spin->setValue(50);
  auto* placeholder = new QLineEdit(&panel);
  placeholder->setPlaceholderText(QStringLiteral("Distance (mm)"));
  placeholder->setFocusPolicy(Qt::NoFocus);
  layout->addWidget(choice);
  layout->addWidget(spin);
  layout->addWidget(placeholder);
  panel.resize(300, 160);
  panel.show();
  QApplication::processEvents();
  CHECK(QFontInfo(spin->font()).family() == QStringLiteral("IBM Plex Mono"));

  const auto check_arrow = [&](QWidget& control, QRect arrow) {
    // Exclude the cell frame and require both glyph and background pixels:
    // an empty white drop-down must not satisfy this rendering proof.
    const QRect interior = arrow.adjusted(2, 2, -2, -2);
    REQUIRE(interior.width() > 0);
    REQUIRE(interior.height() > 0);
    const auto image = rendered(control);
    CHECK(matching_pixels(image, interior, Qt::white) >= 5U);
    CHECK(matching_pixels(image, interior, control.isEnabled() ? blue : muted) >= 10U);
  };

  for (const bool enabled : {true, false}) {
    choice->setEnabled(enabled);
    spin->setEnabled(enabled);
    QStyleOptionComboBox combo_option;
    combo_option.initFrom(choice);
    combo_option.currentText = choice->currentText();
    check_arrow(*choice, choice->style()->subControlRect(QStyle::CC_ComboBox, &combo_option,
                                                         QStyle::SC_ComboBoxArrow, choice));
    QStyleOptionSpinBox spin_option;
    spin_option.initFrom(spin);
    spin_option.buttonSymbols = QAbstractSpinBox::UpDownArrows;
    spin_option.stepEnabled = QAbstractSpinBox::StepUpEnabled | QAbstractSpinBox::StepDownEnabled;
    spin_option.frame = true;
    for (const auto arrow : {QStyle::SC_SpinBoxUp, QStyle::SC_SpinBoxDown})
      check_arrow(*spin,
                  spin->style()->subControlRect(QStyle::CC_SpinBox, &spin_option, arrow, spin));
  }
  for (const auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled})
    CHECK(placeholder->palette().color(group, QPalette::PlaceholderText) == muted);
  const auto placeholder_image = rendered(*placeholder);
  double minimum_luminance = 1.0;
  for (int y = 8; y < placeholder_image.height() - 8; ++y)
    for (int x = 8; x < placeholder_image.width() - 8; ++x)
      minimum_luminance =
          std::min(minimum_luminance, luminance(placeholder_image.pixelColor(x, y)));
  INFO("Placeholder image " << placeholder_image.width() << 'x' << placeholder_image.height()
                            << "; interior minimum luminance " << minimum_luminance);
  // Half-opacity muted text against white is too pale; actual antialiased
  // glyphs must retain dark pixels in addition to the opaque palette contract.
  CHECK(minimum_luminance < 0.3);
  capture(panel, QStringLiteral("hermes-input-affordances.png"));
}
