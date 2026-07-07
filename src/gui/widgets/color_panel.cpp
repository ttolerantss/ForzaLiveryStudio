#include "color_panel.h"

#include "editor_state.h"
#include "theme_manager.h"

#include <QColorDialog>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QResizeEvent>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace gui {
namespace {

constexpr const char *RecentColorsSettingsKey = "color/recentColors";
constexpr int MaxRecentColors = 16;
constexpr int SliderHandleHalf = 5;

} // namespace

// ---------------------------------------------------------------------------
// ChannelSlider
// ---------------------------------------------------------------------------

ChannelSlider::ChannelSlider(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(18);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setCursor(Qt::PointingHandCursor);
}

void ChannelSlider::setValue(int value)
{
    value_ = std::clamp(value, 0, 255);
    update();
}

void ChannelSlider::setGradient(const QColor &low, const QColor &high)
{
    low_ = low;
    high_ = high;
    update();
}

QRect ChannelSlider::trackRect() const
{
    return rect().adjusted(SliderHandleHalf, 3, -SliderHandleHalf, -3);
}

int ChannelSlider::valueForX(int x) const
{
    const QRect track = trackRect();
    if (track.width() <= 0) {
        return value_;
    }
    const double t = static_cast<double>(x - track.left()) / static_cast<double>(track.width());
    return std::clamp(static_cast<int>(std::round(t * 255.0)), 0, 255);
}

void ChannelSlider::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QRect track = trackRect();

    QLinearGradient gradient(track.topLeft(), track.topRight());
    gradient.setColorAt(0.0, low_);
    gradient.setColorAt(1.0, high_);
    painter.setPen(Qt::NoPen);
    painter.setBrush(gradient);
    painter.drawRoundedRect(track, 3, 3);

    const double t = value_ / 255.0;
    const int handleX = track.left() + static_cast<int>(std::round(t * track.width()));
    const QRect handle(handleX - SliderHandleHalf, 1, SliderHandleHalf * 2, height() - 2);
    painter.setBrush(QColor(230, 230, 230));
    painter.setPen(QPen(QColor(20, 20, 20), 1));
    painter.drawRoundedRect(handle, 3, 3);
}

void ChannelSlider::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    dragging_ = true;
    if (beginCallback_) {
        beginCallback_();
    }
    value_ = valueForX(event->position().toPoint().x());
    update();
    if (changedCallback_) {
        changedCallback_(value_);
    }
}

void ChannelSlider::mouseMoveEvent(QMouseEvent *event)
{
    if (!dragging_) {
        return;
    }
    const int next = valueForX(event->position().toPoint().x());
    if (next != value_) {
        value_ = next;
        update();
        if (changedCallback_) {
            changedCallback_(value_);
        }
    }
}

void ChannelSlider::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !dragging_) {
        QWidget::mouseReleaseEvent(event);
        return;
    }
    dragging_ = false;
    if (commitCallback_) {
        commitCallback_();
    }
}

// ---------------------------------------------------------------------------
// ColorSpectrum
// ---------------------------------------------------------------------------

ColorSpectrum::ColorSpectrum(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(90);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setCursor(Qt::CrossCursor);
}

void ColorSpectrum::setMarkerColor(const QColor &color)
{
    markerColor_ = color;
    haveMarker_ = true;
    update();
}

void ColorSpectrum::rebuildCache()
{
    const QSize target = size();
    if (target.isEmpty()) {
        cache_ = QImage();
        return;
    }
    cache_ = QImage(target, QImage::Format_RGB32);
    for (int y = 0; y < target.height(); ++y) {
        const double value = 1.0 - static_cast<double>(y) / std::max(1, target.height() - 1);
        QRgb *line = reinterpret_cast<QRgb *>(cache_.scanLine(y));
        for (int x = 0; x < target.width(); ++x) {
            const double hue = static_cast<double>(x) / std::max(1, target.width() - 1) * 359.0;
            const QColor color = QColor::fromHsvF(hue / 360.0, 1.0, value);
            line[x] = color.rgb();
        }
    }
}

QColor ColorSpectrum::colorAt(const QPoint &pos) const
{
    if (width() <= 1 || height() <= 1) {
        return markerColor_;
    }
    const int x = std::clamp(pos.x(), 0, width() - 1);
    const int y = std::clamp(pos.y(), 0, height() - 1);
    const double hue = static_cast<double>(x) / (width() - 1) * 359.0;
    const double value = 1.0 - static_cast<double>(y) / (height() - 1);
    return QColor::fromHsvF(hue / 360.0, 1.0, value);
}

void ColorSpectrum::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    if (cache_.size() != size()) {
        rebuildCache();
    }
    if (!cache_.isNull()) {
        painter.drawImage(0, 0, cache_);
    }
    if (haveMarker_ && width() > 1 && height() > 1) {
        const double hue = std::max(0.0f, markerColor_.hueF());
        const double value = markerColor_.valueF();
        const int x = static_cast<int>(std::round(hue * (width() - 1)));
        const int y = static_cast<int>(std::round((1.0 - value) * (height() - 1)));
        painter.setPen(QPen(Qt::white, 1.5));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(QPoint(x, y), 5, 5);
        painter.setPen(QPen(Qt::black, 1.0));
        painter.drawEllipse(QPoint(x, y), 6, 6);
    }
}

void ColorSpectrum::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    cache_ = QImage();
}

void ColorSpectrum::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    dragging_ = true;
    if (beginCallback_) {
        beginCallback_();
    }
    if (pickedCallback_) {
        pickedCallback_(colorAt(event->position().toPoint()));
    }
}

void ColorSpectrum::mouseMoveEvent(QMouseEvent *event)
{
    if (!dragging_) {
        return;
    }
    if (pickedCallback_) {
        pickedCallback_(colorAt(event->position().toPoint()));
    }
}

void ColorSpectrum::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !dragging_) {
        QWidget::mouseReleaseEvent(event);
        return;
    }
    dragging_ = false;
    if (commitCallback_) {
        commitCallback_();
    }
}

// ---------------------------------------------------------------------------
// ColorSwatch
// ---------------------------------------------------------------------------

ColorSwatch::ColorSwatch(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(34, 34);
    setCursor(Qt::PointingHandCursor);
}

void ColorSwatch::setColor(const QColor &color)
{
    color_ = color;
    update();
}

void ColorSwatch::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    const QRect r = rect().adjusted(0, 0, -1, -1);
    painter.fillRect(r, color_);
    painter.setPen(QPen(QColor(20, 20, 20), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(r);
    painter.setPen(QPen(QColor(140, 140, 140), 1));
    painter.drawRect(r.adjusted(1, 1, -1, -1));
}

void ColorSwatch::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && clickedCallback_) {
        clickedCallback_();
    }
    QWidget::mousePressEvent(event);
}

void ColorSwatch::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && doubleClickedCallback_) {
        doubleClickedCallback_();
    }
    QWidget::mouseDoubleClickEvent(event);
}

// ---------------------------------------------------------------------------
// RecentColorsBar
// ---------------------------------------------------------------------------

RecentColorsBar::RecentColorsBar(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(18);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setToolTip(QStringLiteral("Recent colours — click to apply, double-click to edit"));
}

void RecentColorsBar::setColors(const QVector<QColor> &colors)
{
    colors_ = colors;
    update();
}

int RecentColorsBar::indexAt(const QPoint &pos) const
{
    if (colors_.isEmpty()) {
        return -1;
    }
    const int cell = width() / MaxRecentColors;
    if (cell <= 0) {
        return -1;
    }
    const int index = pos.x() / cell;
    return index >= 0 && index < colors_.size() ? index : -1;
}

void RecentColorsBar::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    const int cell = width() / MaxRecentColors;
    if (cell <= 0) {
        return;
    }
    for (int i = 0; i < colors_.size(); ++i) {
        const QRect r(i * cell, 0, cell - 1, height() - 1);
        painter.fillRect(r, colors_[i]);
        painter.setPen(QPen(QColor(0, 0, 0, 120), 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(r);
    }
}

void RecentColorsBar::mousePressEvent(QMouseEvent *event)
{
    const int index = indexAt(event->position().toPoint());
    if (index >= 0 && pickedCallback_) {
        pickedCallback_(colors_[index]);
    }
    QWidget::mousePressEvent(event);
}

void RecentColorsBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    const int index = indexAt(event->position().toPoint());
    if (index >= 0 && activatedCallback_) {
        activatedCallback_(colors_[index]);
    }
    QWidget::mouseDoubleClickEvent(event);
}

// ---------------------------------------------------------------------------
// ColorPanel
// ---------------------------------------------------------------------------

ColorPanel::ColorPanel(EditorState *state, QWidget *parent)
    : QWidget(parent)
    , state_(state)
{
    loadRecentColors();

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    recentLabel_ = new QLabel(QStringLiteral("Recent Colors"), this);
    layout->addWidget(recentLabel_);
    recentBar_ = new RecentColorsBar(this);
    recentBar_->setPickedCallback([this](const QColor &color) { applyOneShot(color); });
    recentBar_->setActivatedCallback([this](const QColor &color) { openPickerDialog(color); });
    layout->addWidget(recentBar_);

    // Swatch column on the left, RGB sliders on the right.
    auto *body = new QHBoxLayout();
    body->setSpacing(10);

    auto *swatchColumn = new QVBoxLayout();
    swatchColumn->setSpacing(4);
    swatch_ = new ColorSwatch(this);
    swatch_->setToolTip(QStringLiteral("Fill — double-click to open the colour picker"));
    swatch_->setDoubleClickedCallback([this]() { openPickerDialog(working_); });
    swatchColumn->addWidget(swatch_, 0, Qt::AlignLeft);

    auto *defaultsRow = new QHBoxLayout();
    defaultsRow->setSpacing(4);
    blackSwatch_ = new ColorSwatch(this);
    blackSwatch_->setFixedSize(15, 15);
    blackSwatch_->setColor(Qt::black);
    blackSwatch_->setToolTip(QStringLiteral("Black"));
    blackSwatch_->setClickedCallback([this]() { applyOneShot(QColor(0, 0, 0, working_.alpha())); });
    whiteSwatch_ = new ColorSwatch(this);
    whiteSwatch_->setFixedSize(15, 15);
    whiteSwatch_->setColor(Qt::white);
    whiteSwatch_->setToolTip(QStringLiteral("White"));
    whiteSwatch_->setClickedCallback([this]() { applyOneShot(QColor(255, 255, 255, working_.alpha())); });
    defaultsRow->addWidget(blackSwatch_);
    defaultsRow->addWidget(whiteSwatch_);
    defaultsRow->addStretch(1);
    swatchColumn->addLayout(defaultsRow);
    swatchColumn->addStretch(1);
    body->addLayout(swatchColumn);

    auto *slidersGrid = new QGridLayout();
    slidersGrid->setHorizontalSpacing(8);
    slidersGrid->setVerticalSpacing(6);
    const char *labels[3] = {"R", "G", "B"};
    ChannelSlider **sliders[3] = {&rSlider_, &gSlider_, &bSlider_};
    QSpinBox **spins[3] = {&rSpin_, &gSpin_, &bSpin_};
    for (int i = 0; i < 3; ++i) {
        auto *label = new QLabel(QString::fromLatin1(labels[i]), this);
        auto *slider = new ChannelSlider(this);
        auto *spin = new QSpinBox(this);
        spin->setRange(0, 255);
        spin->setFixedWidth(52);
        spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
        slider->setBeginCallback([this]() { beginEdit(); });
        slider->setChangedCallback([this](int) { onSpinChanged(); });
        slider->setCommitCallback([this]() { endEdit(true); });
        *sliders[i] = slider;
        *spins[i] = spin;
        slidersGrid->addWidget(label, i, 0);
        slidersGrid->addWidget(slider, i, 1);
        slidersGrid->addWidget(spin, i, 2);
    }
    slidersGrid->setColumnStretch(1, 1);
    body->addLayout(slidersGrid, 1);
    layout->addLayout(body);

    // Hex row.
    auto *hexRow = new QHBoxLayout();
    hexRow->setSpacing(6);
    auto *hexHash = new QLabel(QStringLiteral("#"), this);
    hex_ = new QLineEdit(this);
    hex_->setMaxLength(6);
    hex_->setFixedWidth(80);
    hex_->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("[0-9A-Fa-f]{0,6}")), hex_));
    hexRow->addStretch(1);
    hexRow->addWidget(hexHash);
    hexRow->addWidget(hex_);
    layout->addLayout(hexRow);

    spectrum_ = new ColorSpectrum(this);
    spectrum_->setBeginCallback([this]() { beginEdit(); });
    spectrum_->setPickedCallback([this](const QColor &color) {
        setWorkingColor(QColor(color.red(), color.green(), color.blue(), working_.alpha()), true);
    });
    spectrum_->setCommitCallback([this]() { endEdit(true); });
    layout->addWidget(spectrum_, 1);

    // Spin boxes drive a single edit session: value changes preview live, focus-out
    // / Enter commits, so spinning does not flood the undo stack.
    for (QSpinBox *spin : {rSpin_, gSpin_, bSpin_}) {
        connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() {
            if (loading_) {
                return;
            }
            if (!editing_) {
                beginEdit();
            }
            onSpinChanged();
        });
        connect(spin, &QSpinBox::editingFinished, this, [this]() {
            if (editing_) {
                endEdit(true);
            }
        });
    }
    connect(hex_, &QLineEdit::editingFinished, this, [this]() { onHexEntered(); });
    connect(hex_, &QLineEdit::returnPressed, this, [this]() { onHexEntered(); });

    recentBar_->setColors(recentColors_);
    refreshTheme();
    updateEnabledState();
    updateChildWidgets();
}

std::array<quint8, 4> ColorPanel::toArray(const QColor &color)
{
    return {static_cast<quint8>(color.blue()),
            static_cast<quint8>(color.green()),
            static_cast<quint8>(color.red()),
            static_cast<quint8>(color.alpha())};
}

QColor ColorPanel::fromArray(const std::array<quint8, 4> &color)
{
    return QColor(color[2], color[1], color[0], color[3]);
}

void ColorPanel::setSelection(const QVector<fh6::ShapeLayer *> &layers,
                              const QVector<fh6::LayerGroup *> &groups)
{
    if (editing_) {
        // Ignore refreshes triggered by our own live edits; the commit path reseats
        // the selection afterwards.
        return;
    }
    layers_ = layers;
    groups_ = groups;
    syncFromSelection();
}

QColor ColorPanel::selectionColor(bool &any, bool &mixed) const
{
    any = false;
    mixed = false;
    QVector<std::array<quint8, 4>> colors;
    for (const fh6::ShapeLayer *layer : layers_) {
        colors.push_back(layer->color);
    }
    if (!groups_.isEmpty() && state_ != nullptr) {
        if (const fh6::Project *project = state_->project()) {
            QHash<QString, const fh6::ShapeLayer *> layerById;
            layerById.reserve(project->layers.size());
            for (const fh6::ShapeLayer &layer : project->layers) {
                layerById.insert(layer.id, &layer);
            }
            for (const fh6::LayerGroup *group : groups_) {
                for (const QString &id : state_->leafLayerIdsForEntry(group->id)) {
                    const auto it = layerById.constFind(id);
                    if (it != layerById.constEnd()) {
                        colors.push_back(it.value()->color);
                    }
                }
            }
        }
    }
    if (colors.isEmpty()) {
        return Qt::white;
    }
    any = true;
    const std::array<quint8, 4> first = colors.front();
    mixed = std::any_of(colors.begin(), colors.end(), [&](const std::array<quint8, 4> &other) {
        return other != first;
    });
    return fromArray(first);
}

void ColorPanel::updateEnabledState()
{
    const bool enabled = hasSelection_;
    swatch_->setEnabled(enabled);
    blackSwatch_->setEnabled(enabled);
    whiteSwatch_->setEnabled(enabled);
    rSlider_->setEnabled(enabled);
    gSlider_->setEnabled(enabled);
    bSlider_->setEnabled(enabled);
    rSpin_->setEnabled(enabled);
    gSpin_->setEnabled(enabled);
    bSpin_->setEnabled(enabled);
    hex_->setEnabled(enabled);
    spectrum_->setEnabled(enabled);
}

void ColorPanel::syncFromSelection()
{
    bool any = false;
    bool mixed = false;
    const QColor color = selectionColor(any, mixed);
    hasSelection_ = any;
    updateEnabledState();
    if (any) {
        working_ = color;
    }
    updateChildWidgets();
}

void ColorPanel::updateChildWidgets()
{
    loading_ = true;
    const int r = working_.red();
    const int g = working_.green();
    const int b = working_.blue();

    rSpin_->setValue(r);
    gSpin_->setValue(g);
    bSpin_->setValue(b);
    rSlider_->setValue(r);
    gSlider_->setValue(g);
    bSlider_->setValue(b);
    rSlider_->setGradient(QColor(0, g, b), QColor(255, g, b));
    gSlider_->setGradient(QColor(r, 0, b), QColor(r, 255, b));
    bSlider_->setGradient(QColor(r, g, 0), QColor(r, g, 255));

    swatch_->setColor(working_);
    spectrum_->setMarkerColor(working_);

    const QString hex = QStringLiteral("%1%2%3")
                            .arg(r, 2, 16, QLatin1Char('0'))
                            .arg(g, 2, 16, QLatin1Char('0'))
                            .arg(b, 2, 16, QLatin1Char('0'))
                            .toUpper();
    if (hex_->text().compare(hex, Qt::CaseInsensitive) != 0) {
        hex_->setText(hex);
    }
    loading_ = false;
}

void ColorPanel::beginEdit()
{
    if (editing_ || !hasSelection_ || state_ == nullptr) {
        return;
    }
    const QSet<QString> selectedLayerIds = state_->selectedLayerIds();
    QVector<QString> groupIds;
    groupIds.reserve(groups_.size());
    for (const fh6::LayerGroup *group : groups_) {
        if (group != nullptr) {
            groupIds.push_back(group->id);
        }
    }

    state_->beginProjectEdit();
    // Detach the copy-on-write containers and re-resolve our cached pointers so live
    // writes cannot mutate the undo "before" snapshot (mirrors PropertyPanel::pickColor).
    if (fh6::Project *project = state_->project()) {
        project->layers.data();
        project->groups.data();
        layers_.clear();
        groups_.clear();
        for (fh6::ShapeLayer &layer : project->layers) {
            if (selectedLayerIds.contains(layer.id)) {
                layers_.push_back(&layer);
            }
        }
        for (const QString &groupId : groupIds) {
            for (fh6::LayerGroup &group : project->groups) {
                if (group.id == groupId) {
                    groups_.push_back(&group);
                    break;
                }
            }
        }
    }
    editing_ = true;
}

void ColorPanel::applyLive(const QColor &color)
{
    if (!editing_ || state_ == nullptr) {
        return;
    }
    const std::array<quint8, 4> value = toArray(color);
    for (fh6::ShapeLayer *layer : layers_) {
        layer->color = value;
    }
    for (fh6::LayerGroup *group : groups_) {
        state_->setGroupDescendantColor(group->id, value);
    }
    state_->noteCanvasRepaint();
}

void ColorPanel::endEdit(bool commit)
{
    if (!editing_ || state_ == nullptr) {
        return;
    }
    editing_ = false;
    if (commit) {
        state_->commitProjectEdit();
        state_->noteProjectGeometryChanged(true);
        addRecentColor(working_);
    } else {
        state_->cancelProjectEdit();
    }
    // The refresh triggered by the commit reseats layers_/groups_ via setSelection.
}

void ColorPanel::applyOneShot(const QColor &color)
{
    if (!hasSelection_) {
        return;
    }
    beginEdit();
    setWorkingColor(color, true);
    endEdit(true);
}

void ColorPanel::setWorkingColor(const QColor &color, bool apply)
{
    working_ = QColor(color.red(), color.green(), color.blue(),
                      color.alpha() >= 0 ? color.alpha() : working_.alpha());
    updateChildWidgets();
    if (apply) {
        applyLive(working_);
    }
}

void ColorPanel::onSpinChanged()
{
    if (loading_) {
        return;
    }
    const QColor color(rSpin_->value(), gSpin_->value(), bSpin_->value(), working_.alpha());
    // Keep the slider values in step with a spin edit (loading_ guards recursion).
    setWorkingColor(color, true);
}

void ColorPanel::onHexEntered()
{
    if (loading_ || !hasSelection_) {
        return;
    }
    QString text = hex_->text().trimmed();
    if (text.size() == 3) {
        // Expand shorthand (#abc -> #aabbcc).
        text = QStringLiteral("%1%1%2%2%3%3").arg(text[0]).arg(text[1]).arg(text[2]);
    }
    if (text.size() != 6) {
        updateChildWidgets(); // revert the field to the working colour
        return;
    }
    const QColor parsed(QStringLiteral("#") + text);
    if (!parsed.isValid()) {
        updateChildWidgets();
        return;
    }
    // editingFinished also fires on focus loss; skip when nothing changed so we do
    // not push a redundant undo step.
    if (parsed.red() == working_.red() && parsed.green() == working_.green()
        && parsed.blue() == working_.blue()) {
        return;
    }
    applyOneShot(QColor(parsed.red(), parsed.green(), parsed.blue(), working_.alpha()));
}

void ColorPanel::openPickerDialog(const QColor &seed)
{
    if (!hasSelection_) {
        return;
    }
    beginEdit();
    QColorDialog dialog(this);
    dialog.setOption(QColorDialog::ShowAlphaChannel, true);
    dialog.setWindowTitle(QStringLiteral("Color Picker"));
    dialog.setCurrentColor(QColor(seed.red(), seed.green(), seed.blue(), working_.alpha()));
    connect(&dialog, &QColorDialog::currentColorChanged, this, [this](const QColor &picked) {
        if (picked.isValid()) {
            setWorkingColor(picked, true);
        }
    });
    if (dialog.exec() == QDialog::Accepted) {
        endEdit(true);
    } else {
        endEdit(false);
        syncFromSelection();
    }
}

void ColorPanel::addRecentColor(const QColor &color)
{
    const QColor opaque(color.red(), color.green(), color.blue());
    recentColors_.removeIf([&](const QColor &other) {
        return other.red() == opaque.red() && other.green() == opaque.green() && other.blue() == opaque.blue();
    });
    recentColors_.prepend(opaque);
    while (recentColors_.size() > MaxRecentColors) {
        recentColors_.removeLast();
    }
    recentBar_->setColors(recentColors_);
    saveRecentColors();
}

void ColorPanel::loadRecentColors()
{
    recentColors_.clear();
    const QString value = QSettings().value(QString::fromLatin1(RecentColorsSettingsKey)).toString();
    for (const QString &part : value.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const QColor color(part);
        if (color.isValid()) {
            recentColors_.push_back(QColor(color.red(), color.green(), color.blue()));
        }
        if (recentColors_.size() >= MaxRecentColors) {
            break;
        }
    }
}

void ColorPanel::saveRecentColors() const
{
    QStringList parts;
    for (const QColor &color : recentColors_) {
        parts.push_back(color.name());
    }
    QSettings().setValue(QString::fromLatin1(RecentColorsSettingsKey), parts.join(QLatin1Char(',')));
}

void ColorPanel::refreshTheme()
{
    const QPalette pal = paletteForTheme(currentUiTheme());
    const QString base = pal.color(QPalette::Base).name();
    const QString text = pal.color(QPalette::Text).name();
    const QString muted = QColor((pal.color(QPalette::Text).red() + pal.color(QPalette::Base).red()) / 2,
                                 (pal.color(QPalette::Text).green() + pal.color(QPalette::Base).green()) / 2,
                                 (pal.color(QPalette::Text).blue() + pal.color(QPalette::Base).blue()) / 2)
                              .name();
    setStyleSheet(QStringLiteral("QLabel { color: %1; } QLineEdit, QSpinBox { background: %2; color: %1; border: 1px solid #4a4a4a; border-radius: 3px; padding: 2px; }")
                      .arg(text, base));
    if (recentLabel_ != nullptr) {
        recentLabel_->setStyleSheet(QStringLiteral("color: %1;").arg(muted));
    }
}

} // namespace gui
