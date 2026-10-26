#include "blcad/gui/gui_appearance.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QImage>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QPushButton>
#include <QSpinBox>
#include <QStyle>
#include <QStyleOption>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

using namespace blcad::gui;

namespace {

QImage rendered(QWidget& widget) {
  widget.ensurePolished();
  QImage image(widget.size(), QImage::Format_RGB32);
  image.fill(Qt::magenta);
  widget.render(&image);
  return image;
}

void check_square_white_frame(QWidget& widget) {
  const auto image = rendered(widget);
  CHECK(image.pixelColor(0, 0) == QColor(Qt::white));
  CHECK(image.pixelColor(image.width() - 1, 0) == QColor(Qt::white));
  CHECK(image.pixelColor(0, image.height() - 1) == QColor(Qt::white));
  CHECK(image.pixelColor(image.width() - 1, image.height() - 1) == QColor(Qt::white));
}

} // namespace

TEST_CASE("GUI appearance keeps controls square and monochrome through state changes",
          "[gui][appearance]") {
  QWidget panel;
  auto* layout = new QVBoxLayout(&panel);
  auto* button = new QPushButton(QStringLiteral("Apply"), &panel);
  auto* input = new QLineEdit(&panel);
  input->setPlaceholderText(QStringLiteral("Distance (mm)"));
  auto* choice = new QComboBox(&panel);
  choice->addItems({QStringLiteral("Distance"), QStringLiteral("Radius")});
  auto* check = new QCheckBox(QStringLiteral("Reference dimension"), &panel);
  layout->addWidget(button);
  layout->addWidget(input);
  layout->addWidget(choice);
  layout->addWidget(check);
  panel.resize(300, 200);
  panel.show();
  QApplication::processEvents();

  CHECK(QLocale().language() == QLocale::English);
  CHECK(QLocale().decimalPoint() == QStringLiteral("."));
  CHECK(QApplication::testAttribute(Qt::AA_DontUseNativeDialogs));
  for (const auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
    CHECK(panel.palette().color(group, QPalette::Window) == QColor(Qt::black));
    CHECK(panel.palette().color(group, QPalette::WindowText) == QColor(Qt::white));
    CHECK(button->palette().color(group, QPalette::Button) == QColor(Qt::black));
    CHECK(button->palette().color(group, QPalette::ButtonText) == QColor(Qt::white));
  }
  check_square_white_frame(*button);
  check_square_white_frame(*input);
  check_square_white_frame(*choice);
  button->setDown(true);
  check_square_white_frame(*button);
  button->setDown(false);
  button->setEnabled(false);
  CHECK(button->palette().color(QPalette::Disabled, QPalette::ButtonText) == QColor(Qt::white));
  button->setEnabled(true);
  button->setCheckable(true);
  button->setChecked(true);
  check_square_white_frame(*button);
  check->setChecked(true);
  input->setText(QStringLiteral("12.5"));
  choice->setCurrentIndex(1);
  CHECK(input->text() == QStringLiteral("12.5"));
  CHECK(choice->currentText() == QStringLiteral("Radius"));

  // Sample clear button interior rows: state changes must not reintroduce a
  // desktop-style gradient, colored fill, or antialiased rounded corners.
  const auto image = rendered(*button);
  for (int y = 3; y < image.height() - 3; ++y)
    CHECK(image.pixelColor(4, y) == QColor(Qt::black));
  CHECK(rendered(panel).pixelColor(0, 0) == QColor(Qt::black));
}

TEST_CASE("GUI ribbon and menus retain English labels and visible selection frames",
          "[gui][appearance][ribbon]") {
  ShellRibbon ribbon;
  const int model = ribbon.add_tab(QStringLiteral("3D Model"));
  const int sketch = ribbon.add_tab(QStringLiteral("Sketch"));
  auto* group = ribbon.add_group(model, QStringLiteral("Create"));
  QAction extrude(QStringLiteral("Extrude"), &ribbon);
  auto* command = ribbon.add_button(group, &extrude);
  QAction fillet(QStringLiteral("Fillet"), &ribbon);
  QAction chamfer(QStringLiteral("Chamfer"), &ribbon);
  auto* menu_button = ribbon.add_menu_button(group, QStringLiteral("Modify"), {&fillet, &chamfer});
  ribbon.resize(560, 160);
  ribbon.show();
  QApplication::processEvents();

  CHECK(ribbon.tab_title(model) == QStringLiteral("3D Model"));
  CHECK(ribbon.tab_title(sketch) == QStringLiteral("Sketch"));
  CHECK(command->text() == QStringLiteral("Extrude"));
  CHECK(menu_button->menu()->actions().at(0)->text() == QStringLiteral("Fillet"));
  CHECK(menu_button->menu()->actions().at(1)->text() == QStringLiteral("Chamfer"));
  command->setCheckable(true);
  command->setChecked(true);
  check_square_white_frame(*command);
  ribbon.set_current_tab(sketch);
  CHECK(ribbon.current_tab() == sketch);
  ribbon.set_current_tab(model);
  CHECK(command->isVisible());
  check_square_white_frame(*menu_button);

  const QString directory = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (!directory.isEmpty())
    REQUIRE(rendered(ribbon).save(directory + QStringLiteral("/monochrome-english-ribbon.png")));
}

TEST_CASE("Monochrome controls retain visible arrows and opaque placeholder text",
          "[gui][appearance]") {
  QWidget panel;
  auto* layout = new QVBoxLayout(&panel);
  auto* choice = new QComboBox(&panel);
  choice->addItem(QStringLiteral("Operation"));
  auto* spin = new QSpinBox(&panel);
  auto* placeholder = new QLineEdit(&panel);
  placeholder->setPlaceholderText(QStringLiteral("Distance (mm)"));
  placeholder->setFocusPolicy(Qt::NoFocus);
  layout->addWidget(choice);
  layout->addWidget(spin);
  layout->addWidget(placeholder);
  panel.resize(300, 160);
  panel.show();
  QApplication::processEvents();

  const auto white_pixels = [](const QImage& image, QRect area) {
    std::size_t count = 0;
    area = area.intersected(image.rect());
    for (int y = area.top(); y <= area.bottom(); ++y)
      for (int x = area.left(); x <= area.right(); ++x)
        if (image.pixelColor(x, y) == QColor(Qt::white))
          ++count;
    return count;
  };
  const auto check_arrow = [&](QWidget& control, QRect arrow) {
    // Exclude the white cell frame: a blank drop-down must fail this proof.
    const QRect interior = arrow.adjusted(2, 2, -2, -2);
    REQUIRE(interior.width() > 0);
    REQUIRE(interior.height() > 0);
    CHECK(white_pixels(rendered(control), interior) >= 5U);
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
    CHECK(placeholder->palette().color(group, QPalette::PlaceholderText) == QColor(Qt::white));
  const auto placeholder_image = rendered(*placeholder);
  const QString directory = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (!directory.isEmpty())
    REQUIRE(placeholder_image.save(directory + QStringLiteral("/opaque-placeholder.png")));
  int maximum_intensity = 0;
  for (int y = 8; y < placeholder_image.height() - 8; ++y)
    for (int x = 8; x < placeholder_image.width() - 8; ++x)
      maximum_intensity = std::max(maximum_intensity, placeholder_image.pixelColor(x, y).red());
  INFO("Placeholder image " << placeholder_image.width() << 'x' << placeholder_image.height()
                            << "; interior maximum intensity " << maximum_intensity);
  // Font antialiasing can leave the brightest opaque-white glyph pixel at 254.
  // The former half-opacity QSS placeholder peaks at 128 and must still fail.
  CHECK(maximum_intensity >= 240);
}
