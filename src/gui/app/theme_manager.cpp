#include "theme_manager.h"

#include <QApplication>
#include <QSettings>
#include <QStyleFactory>

namespace gui {
namespace {

UiTheme currentTheme = UiTheme::Dark;

} // namespace

QString themeSettingsValue(UiTheme theme)
{
    return theme == UiTheme::Light ? QStringLiteral("light") : QStringLiteral("dark");
}

UiTheme themeFromSettingsValue(const QString &value)
{
    return value.compare(QStringLiteral("light"), Qt::CaseInsensitive) == 0 ? UiTheme::Light : UiTheme::Dark;
}

UiTheme loadUiTheme()
{
    return themeFromSettingsValue(QSettings().value(QStringLiteral("ui/theme"), QStringLiteral("dark")).toString());
}

void saveUiTheme(UiTheme theme)
{
    QSettings().setValue(QStringLiteral("ui/theme"), themeSettingsValue(theme));
}

QColor defaultCanvasColor(UiTheme theme)
{
    return theme == UiTheme::Light ? QColor(244, 245, 247) : QColor(56, 56, 56);   // dark: #383838
}

CanvasColorSettings loadCanvasColorSettings()
{
    QSettings settings;
    CanvasColorSettings result;
    result.darkMode = settings.value(QStringLiteral("ui/canvas/darkMode"), QStringLiteral("default")).toString() == QStringLiteral("custom")
        ? CanvasColorMode::Custom
        : CanvasColorMode::ThemeDefault;
    result.lightMode = settings.value(QStringLiteral("ui/canvas/lightMode"), QStringLiteral("default")).toString() == QStringLiteral("custom")
        ? CanvasColorMode::Custom
        : CanvasColorMode::ThemeDefault;

    const QColor legacy(settings.value(QStringLiteral("ui/canvasColor")).toString());
    const QColor dark(settings.value(QStringLiteral("ui/canvas/darkCustom"),
                                     legacy.isValid() ? legacy.name(QColor::HexRgb)
                                                      : defaultCanvasColor(UiTheme::Dark).name(QColor::HexRgb)).toString());
    const QColor light(settings.value(QStringLiteral("ui/canvas/lightCustom"),
                                      defaultCanvasColor(UiTheme::Light).name(QColor::HexRgb)).toString());
    result.darkCustom = dark.isValid() ? dark : defaultCanvasColor(UiTheme::Dark);
    result.lightCustom = light.isValid() ? light : defaultCanvasColor(UiTheme::Light);
    if (legacy.isValid() && !settings.contains(QStringLiteral("ui/canvas/darkMode"))) {
        result.darkMode = CanvasColorMode::Custom;
    }
    return result;
}

void saveCanvasColorSettings(const CanvasColorSettings &settings)
{
    QSettings qsettings;
    qsettings.remove(QStringLiteral("ui/canvasColor"));
    qsettings.setValue(QStringLiteral("ui/canvas/darkMode"), settings.darkMode == CanvasColorMode::Custom ? QStringLiteral("custom") : QStringLiteral("default"));
    qsettings.setValue(QStringLiteral("ui/canvas/lightMode"), settings.lightMode == CanvasColorMode::Custom ? QStringLiteral("custom") : QStringLiteral("default"));
    qsettings.setValue(QStringLiteral("ui/canvas/darkCustom"),
                       (settings.darkCustom.isValid() ? settings.darkCustom : defaultCanvasColor(UiTheme::Dark)).name(QColor::HexRgb));
    qsettings.setValue(QStringLiteral("ui/canvas/lightCustom"),
                       (settings.lightCustom.isValid() ? settings.lightCustom : defaultCanvasColor(UiTheme::Light)).name(QColor::HexRgb));
}

TransformModeSettings loadTransformModeSettings()
{
    TransformModeSettings result;
    result.relativeMode = QSettings().value(QStringLiteral("ui/transform/relativeModeOption"), false).toBool();
    return result;
}

void saveTransformModeSettings(const TransformModeSettings &settings)
{
    QSettings qsettings;
    qsettings.remove(QStringLiteral("ui/transform/relativeMode"));
    qsettings.setValue(QStringLiteral("ui/transform/relativeModeOption"), settings.relativeMode);
}

BehaviorSettings loadBehaviorSettings()
{
    QSettings settings;
    BehaviorSettings result;
    result.insertShapeWithLastSelectedColor = settings.value(QStringLiteral("ui/behavior/insertShapeWithLastSelectedColor"), true).toBool();
    result.insertShapeWithLastSelectedScale = settings.value(QStringLiteral("ui/behavior/insertShapeWithLastSelectedScale"), false).toBool();
    result.showPropertyDebug = settings.value(QStringLiteral("ui/behavior/showPropertyDebug"), false).toBool();
    result.moveToolAutoSelect = settings.value(QStringLiteral("ui/behavior/moveToolAutoSelect"), false).toBool();
    result.selectionFlashEnabled = settings.value(QStringLiteral("ui/behavior/selectionFlashEnabled"), true).toBool();
    return result;
}

void saveBehaviorSettings(const BehaviorSettings &settings)
{
    QSettings qsettings;
    qsettings.setValue(QStringLiteral("ui/behavior/insertShapeWithLastSelectedColor"), settings.insertShapeWithLastSelectedColor);
    qsettings.setValue(QStringLiteral("ui/behavior/insertShapeWithLastSelectedScale"), settings.insertShapeWithLastSelectedScale);
    qsettings.setValue(QStringLiteral("ui/behavior/showPropertyDebug"), settings.showPropertyDebug);
    qsettings.setValue(QStringLiteral("ui/behavior/moveToolAutoSelect"), settings.moveToolAutoSelect);
    qsettings.setValue(QStringLiteral("ui/behavior/selectionFlashEnabled"), settings.selectionFlashEnabled);
}

QColor canvasColorForTheme(UiTheme theme, const CanvasColorSettings &settings)
{
    if (theme == UiTheme::Light) {
        return settings.lightMode == CanvasColorMode::Custom && settings.lightCustom.isValid()
            ? settings.lightCustom
            : defaultCanvasColor(UiTheme::Light);
    }
    return settings.darkMode == CanvasColorMode::Custom && settings.darkCustom.isValid()
        ? settings.darkCustom
        : defaultCanvasColor(UiTheme::Dark);
}

bool isDarkTheme(UiTheme theme)
{
    return theme == UiTheme::Dark;
}

QColor iconColorForTheme(UiTheme theme)
{
    return isDarkTheme(theme) ? QColor(182, 182, 182) : QColor(32, 34, 37);   // dark icons: #b6b6b6
}

QPalette paletteForTheme(UiTheme theme)
{
    QPalette palette;
    if (theme == UiTheme::Light) {
        palette.setColor(QPalette::Window, QColor(244, 245, 247));
        palette.setColor(QPalette::WindowText, QColor(28, 30, 33));
        palette.setColor(QPalette::Base, QColor(255, 255, 255));
        palette.setColor(QPalette::AlternateBase, QColor(235, 237, 240));
        palette.setColor(QPalette::ToolTipBase, QColor(255, 255, 255));
        palette.setColor(QPalette::ToolTipText, QColor(28, 30, 33));
        palette.setColor(QPalette::Text, QColor(28, 30, 33));
        palette.setColor(QPalette::Button, QColor(238, 240, 243));
        palette.setColor(QPalette::ButtonText, QColor(28, 30, 33));
        palette.setColor(QPalette::BrightText, QColor(255, 255, 255));
        palette.setColor(QPalette::Highlight, QColor(42, 130, 218));
        palette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
        palette.setColor(QPalette::Link, QColor(0, 102, 204));
        palette.setColor(QPalette::Disabled, QPalette::Text, QColor(128, 132, 138));
        palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(128, 132, 138));
        return palette;
    }

    palette.setColor(QPalette::Window, QColor(38, 38, 38));   // sidebar/chrome: #262626
    palette.setColor(QPalette::WindowText, QColor(238, 241, 245));
    palette.setColor(QPalette::Base, QColor(24, 24, 24));           // panels (layers/etc.): #181818, matches buffer
    palette.setColor(QPalette::AlternateBase, QColor(32, 32, 32));
    palette.setColor(QPalette::ToolTipBase, QColor(46, 48, 54));
    palette.setColor(QPalette::ToolTipText, QColor(238, 241, 245));
    palette.setColor(QPalette::Text, QColor(238, 241, 245));
    palette.setColor(QPalette::Button, QColor(43, 45, 50));
    palette.setColor(QPalette::ButtonText, QColor(238, 241, 245));
    palette.setColor(QPalette::BrightText, QColor(255, 255, 255));
    palette.setColor(QPalette::Highlight, QColor(72, 126, 176));
    palette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    palette.setColor(QPalette::Link, QColor(126, 180, 255));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(128, 132, 138));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(128, 132, 138));
    return palette;
}

namespace {

// Flat, modern styling layered over Fusion for the dark theme: removes the
// default gradient/beveled Windows look and gives the app its own identity.
// Neutral greys with a blue accent (matching the on-canvas selection colour).
QString darkStyleSheet()
{
    return QStringLiteral(R"(
/* Menu bar */
QMenuBar { background: #262626; border: none; padding: 2px 2px; }
QMenuBar::item { background: transparent; padding: 5px 10px; border-radius: 4px; }
QMenuBar::item:selected { background: #333333; }
QMenuBar::item:pressed { background: #3a3a3a; }

/* Menus */
QMenu { background: #1f1f1f; border: 1px solid #3a3a3a; padding: 4px; }
QMenu::item { padding: 6px 26px; border-radius: 4px; }
QMenu::item:selected { background: #1482f0; color: #ffffff; }
QMenu::item:disabled { color: #6a6a6a; }
QMenu::separator { height: 1px; background: #3a3a3a; margin: 4px 8px; }

QToolTip { background: #1f1f1f; color: #dcdcdc; border: 1px solid #3a3a3a; padding: 4px 6px; }

/* Push buttons */
QPushButton {
    background: #2f2f2f; color: #dcdcdc;
    border: 1px solid #3a3a3a; border-radius: 5px;
    padding: 5px 14px; min-height: 16px;
}
QPushButton:hover { background: #383838; border-color: #4a4a4a; }
QPushButton:pressed { background: #2a2a2a; }
QPushButton:disabled { color: #6a6a6a; background: #2a2a2a; border-color: #333333; }
QPushButton:default { border-color: #1482f0; }

/* Tool buttons */
QToolButton { background: transparent; border: none; border-radius: 5px; padding: 4px; }
QToolButton:hover { background: #333333; }
QToolButton:pressed { background: #3a3a3a; }
QToolButton:checked { background: #4a4a4a; }

/* Text inputs, spin boxes, combos */
QLineEdit, QPlainTextEdit, QTextEdit, QAbstractSpinBox, QComboBox {
    background: #1c1c1c; color: #dcdcdc;
    border: 1px solid #3a3a3a; border-radius: 5px; padding: 3px 6px;
    selection-background-color: #1482f0; selection-color: #ffffff;
}
QLineEdit:focus, QPlainTextEdit:focus, QTextEdit:focus, QAbstractSpinBox:focus, QComboBox:focus { border-color: #1482f0; }
QAbstractSpinBox::up-button, QAbstractSpinBox::down-button { background: #2a2a2a; border: none; width: 16px; }
QAbstractSpinBox::up-button:hover, QAbstractSpinBox::down-button:hover { background: #383838; }
QComboBox::drop-down { border: none; width: 18px; }
QComboBox QAbstractItemView { background: #1f1f1f; border: 1px solid #3a3a3a; selection-background-color: #1482f0; }

/* Item views */
QTreeView, QListView, QTableView, QListWidget {
    background: #181818; alternate-background-color: #202020;
    border: 1px solid #2e2e2e; outline: none;
}
QTreeView::item:hover, QListView::item:hover, QListWidget::item:hover { background: #232323; }
QTreeView::item:selected, QListView::item:selected, QListWidget::item:selected { background: #1482f0; color: #ffffff; }
QHeaderView::section { background: #262626; color: #b6b6b6; padding: 4px; border: none; border-bottom: 1px solid #3a3a3a; }

/* Tabs (tabified docks) */
QTabWidget::pane { border: 1px solid #2e2e2e; }
QTabBar::tab { background: #242424; color: #b6b6b6; padding: 6px 12px; border: none; border-top-left-radius: 5px; border-top-right-radius: 5px; }
QTabBar::tab:selected { background: #333333; color: #ffffff; }
QTabBar::tab:hover { background: #2c2c2c; }

/* Scrollbars */
QScrollBar:vertical { background: transparent; width: 12px; margin: 0; }
QScrollBar::handle:vertical { background: #3a3a3a; border-radius: 5px; min-height: 24px; margin: 2px; }
QScrollBar::handle:vertical:hover { background: #4d4d4d; }
QScrollBar:horizontal { background: transparent; height: 12px; margin: 0; }
QScrollBar::handle:horizontal { background: #3a3a3a; border-radius: 5px; min-width: 24px; margin: 2px; }
QScrollBar::handle:horizontal:hover { background: #4d4d4d; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; background: none; border: none; }
QScrollBar::add-page, QScrollBar::sub-page { background: none; }

/* Dock titles + splitters */
QDockWidget::title { background: #202020; padding: 6px 8px; border: none; }
QSplitter::handle { background: #2e2e2e; }
QSplitter::handle:horizontal { width: 3px; }
QSplitter::handle:vertical { height: 3px; }

/* Group boxes */
QGroupBox { border: 1px solid #3a3a3a; border-radius: 6px; margin-top: 8px; padding-top: 8px; }
QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; color: #b6b6b6; }

/* Check boxes / radio buttons */
QCheckBox::indicator, QRadioButton::indicator { width: 15px; height: 15px; border: 1px solid #4a4a4a; background: #1c1c1c; }
QCheckBox::indicator { border-radius: 3px; }
QRadioButton::indicator { border-radius: 8px; }
QCheckBox::indicator:checked, QRadioButton::indicator:checked { background: #1482f0; border-color: #1482f0; }

/* Sliders */
QSlider::groove:horizontal { height: 4px; background: #3a3a3a; border-radius: 2px; }
QSlider::sub-page:horizontal { background: #1482f0; border-radius: 2px; }
QSlider::handle:horizontal { background: #dcdcdc; width: 14px; margin: -6px 0; border-radius: 7px; }

/* Progress + status bar */
QProgressBar { border: 1px solid #3a3a3a; border-radius: 5px; background: #1c1c1c; text-align: center; color: #dcdcdc; }
QProgressBar::chunk { background: #1482f0; border-radius: 4px; }
QStatusBar { background: #202020; color: #9a9a9a; }
QStatusBar::item { border: none; }
)");
}

} // namespace

void applyUiTheme(QApplication &app, UiTheme theme)
{
    currentTheme = theme;
    if (QStyleFactory::keys().contains(QStringLiteral("Fusion"))) {
        app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    }
    app.setPalette(paletteForTheme(theme));
    app.setStyleSheet(theme == UiTheme::Dark ? darkStyleSheet() : QString());
}

UiTheme currentUiTheme()
{
    return currentTheme;
}

} // namespace gui
