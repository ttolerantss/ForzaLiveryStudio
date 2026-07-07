#pragma once

#include "core_types.h"

#include <QColor>
#include <QImage>
#include <QVector>
#include <QWidget>

#include <array>
#include <functional>

class QLabel;
class QLineEdit;
class QSpinBox;

namespace gui {

class EditorState;

// A single RGB channel slider with a gradient track that reflects the other two
// channels. Uses std::function callbacks (not Qt signals) to match the widget
// style used elsewhere in this codebase, so no moc is required.
class ChannelSlider final : public QWidget {
public:
    explicit ChannelSlider(QWidget *parent = nullptr);

    void setValue(int value);          // 0..255, no callback
    int value() const { return value_; }
    void setGradient(const QColor &low, const QColor &high);

    void setBeginCallback(std::function<void()> fn) { beginCallback_ = std::move(fn); }
    void setChangedCallback(std::function<void(int)> fn) { changedCallback_ = std::move(fn); }
    void setCommitCallback(std::function<void()> fn) { commitCallback_ = std::move(fn); }

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    int valueForX(int x) const;
    QRect trackRect() const;

    int value_ = 255;
    QColor low_ = Qt::black;
    QColor high_ = Qt::white;
    bool dragging_ = false;
    std::function<void()> beginCallback_;
    std::function<void(int)> changedCallback_;
    std::function<void()> commitCallback_;
};

// The large hue/value spectrum at the bottom of the panel: x = hue, y = value
// (bright at the top, dark at the bottom) at full saturation. Clicking or
// dragging picks a colour.
class ColorSpectrum final : public QWidget {
public:
    explicit ColorSpectrum(QWidget *parent = nullptr);

    void setBeginCallback(std::function<void()> fn) { beginCallback_ = std::move(fn); }
    void setPickedCallback(std::function<void(const QColor &)> fn) { pickedCallback_ = std::move(fn); }
    void setCommitCallback(std::function<void()> fn) { commitCallback_ = std::move(fn); }
    // Move the marker to match an externally-set colour without firing callbacks.
    void setMarkerColor(const QColor &color);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void rebuildCache();
    QColor colorAt(const QPoint &pos) const;

    QImage cache_;
    QColor markerColor_ = Qt::white;
    bool haveMarker_ = false;
    bool dragging_ = false;
    std::function<void()> beginCallback_;
    std::function<void(const QColor &)> pickedCallback_;
    std::function<void()> commitCallback_;
};

// A flat colour swatch. Single click and double click each fire a callback so the
// panel can distinguish "apply" from "open the picker dialog".
class ColorSwatch final : public QWidget {
public:
    explicit ColorSwatch(QWidget *parent = nullptr);

    void setColor(const QColor &color);
    QColor color() const { return color_; }

    void setClickedCallback(std::function<void()> fn) { clickedCallback_ = std::move(fn); }
    void setDoubleClickedCallback(std::function<void()> fn) { doubleClickedCallback_ = std::move(fn); }

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    QColor color_ = Qt::white;
    std::function<void()> clickedCallback_;
    std::function<void()> doubleClickedCallback_;
};

// A horizontal row of recent-colour swatches. Single click applies a swatch;
// double click opens the picker seeded with it.
class RecentColorsBar final : public QWidget {
public:
    explicit RecentColorsBar(QWidget *parent = nullptr);

    void setColors(const QVector<QColor> &colors);

    void setPickedCallback(std::function<void(const QColor &)> fn) { pickedCallback_ = std::move(fn); }
    void setActivatedCallback(std::function<void(const QColor &)> fn) { activatedCallback_ = std::move(fn); }

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    int indexAt(const QPoint &pos) const;

    QVector<QColor> colors_;
    std::function<void(const QColor &)> pickedCallback_;
    std::function<void(const QColor &)> activatedCallback_;
};

class ColorPanel final : public QWidget {
public:
    explicit ColorPanel(EditorState *state, QWidget *parent = nullptr);

    // Mirrors PropertyPanel::setSelection minus guides (guides have no colour).
    void setSelection(const QVector<fh6::ShapeLayer *> &layers,
                      const QVector<fh6::LayerGroup *> &groups);
    void refreshTheme();
    // Invoked when the panel's eyedropper button is pressed (arms canvas sampling).
    void setEyedropperRequestedCallback(std::function<void()> fn) { eyedropperRequestedCallback_ = std::move(fn); }

private:
    static std::array<quint8, 4> toArray(const QColor &color);
    static QColor fromArray(const std::array<quint8, 4> &color);

    // Reads the first selected colour; sets any=false when nothing is selected and
    // mixed=true when the selection holds more than one colour.
    QColor selectionColor(bool &any, bool &mixed) const;

    void updateEnabledState();
    void syncFromSelection();
    // Push currentColor_ into every child widget (guarded so it does not recurse).
    void updateChildWidgets();

    void beginEdit();
    void applyLive(const QColor &color);
    void endEdit(bool commit);
    // begin + apply + commit in one step (recent swatch, hex, spin box commit).
    void applyOneShot(const QColor &color);

    void setWorkingColor(const QColor &color, bool apply);
    void onSpinChanged();
    void onSliderChanged();
    void onHexEntered();
    void openPickerDialog(const QColor &seed);

    void addRecentColor(const QColor &color);
    void loadRecentColors();
    void saveRecentColors() const;

    EditorState *state_ = nullptr;
    QVector<fh6::ShapeLayer *> layers_;
    QVector<fh6::LayerGroup *> groups_;

    QColor working_ = Qt::white;
    bool editing_ = false;
    bool loading_ = false;
    bool hasSelection_ = false;

    QVector<QColor> recentColors_;

    std::function<void()> eyedropperRequestedCallback_;
    QLabel *recentLabel_ = nullptr;
    RecentColorsBar *recentBar_ = nullptr;
    ColorSwatch *swatch_ = nullptr;
    ColorSwatch *blackSwatch_ = nullptr;
    ColorSwatch *whiteSwatch_ = nullptr;
    ChannelSlider *rSlider_ = nullptr;
    ChannelSlider *gSlider_ = nullptr;
    ChannelSlider *bSlider_ = nullptr;
    QSpinBox *rSpin_ = nullptr;
    QSpinBox *gSpin_ = nullptr;
    QSpinBox *bSpin_ = nullptr;
    QLineEdit *hex_ = nullptr;
    ColorSpectrum *spectrum_ = nullptr;
};

} // namespace gui
