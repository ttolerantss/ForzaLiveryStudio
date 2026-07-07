#pragma once

#include "core_types.h"
#include "native_shape_renderer.h"
#include "shape_geometry_store.h"

#include <QColor>
#include <QElapsedTimer>
#include <QCursor>
#include <QHash>
#include <QImage>
#include <QLineF>
#include <QOpenGLWidget>
#include <QPoint>
#include <QPolygonF>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QTransform>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

class QPainter;
class QMouseEvent;
class QKeyEvent;
class QWheelEvent;

namespace gui {

class CanvasTool;
class EditorState;

class ProjectCanvas final : public QOpenGLWidget {
public:
    explicit ProjectCanvas(QWidget *parent = nullptr);
    ~ProjectCanvas() override;

    void setProject(fh6::Project *project);
    void setEditorState(EditorState *state);
    bool loadGeometry(QString *error = nullptr);
    // Local size of a shape from the geometry store; used by the property panel to
    // pivot multi/group transforms about the selection's true bounding box.
    QSizeF shapeSize(int shapeId) const;
    // Local bounding box of a shape's actual inked geometry (ignoring the
    // declared square canvas / transparent corner markers); used to lay out
    // Place Text glyphs by their true width.
    QRectF shapeInkBounds(int shapeId) const;
    void setTool(const QString &tool);
    QString tool() const;
    void invalidateSelectionCache();
    void invalidateSceneCache();
    void resetRelativeSelectionFrame();
    void refitView();
    QPointF viewCenterWorld();
    void setCanvasColor(const QColor &color);
    QColor canvasColor() const;
    void setTransformRelativeMode(bool relative);
    void setMoveToolAutoSelect(bool enabled);
    bool moveToolAutoSelect() const;
    void setSelectionFlashEnabled(bool enabled);
    bool selectionFlashEnabled() const;
    // Mirror the current selection about its centre: horizontal = left/right,
    // otherwise top/bottom. A whole group mirrors as a unit.
    void flipSelection(bool horizontal);
    // Rotate the whole selection rigidly about its combined centre by `degrees`.
    void rotateSelection(double degrees);
    // Nudge the selection by a screen-pixel delta (arrow keys); converts to world units
    // so the on-screen movement matches at any zoom.
    void nudgeSelection(double screenDx, double screenDy);
    // Align/distribute the selected objects. Each top-level object or group moves as a
    // unit. mode: 0 left, 1 h-center, 2 right, 3 top, 4 v-middle, 5 bottom,
    // 6 distribute horizontally, 7 distribute vertically.
    void alignSelection(int mode);
    // Eyedropper: arm sampling so the next canvas click samples the colour of the shape
    // under the cursor and reports it via the callback (Esc cancels).
    void armEyedropper();
    void setEyedropperCallback(std::function<void(const QColor &)> fn);
    // Rulers along the top/left edges (Ctrl+R). Dragging out from a ruler creates a
    // horizontal/vertical guide line; guides are editor-only (never exported).
    bool rulersVisible() const { return rulersVisible_; }
    void setRulersVisible(bool visible);
    void toggleRulers() { setRulersVisible(!rulersVisible_); }
    void clearGuides();
    // Smart snapping while moving: the dragged selection snaps its edges/centre/corners
    // to other objects and to guides (Illustrator-style). Hold Ctrl to bypass.
    bool snapEnabled() const { return snapEnabled_; }
    void setSnapEnabled(bool enabled) { snapEnabled_ = enabled; }
    // Editor-only preview opacity: dims the current selection (a group's leaves too) on
    // screen to make editing easier, without changing the exported in-game colour.
    void setSelectionPreviewOpacity(double opacity);
    double selectionPreviewOpacity() const;
    void clearPreviewOpacity();
    // Illustrator-style isolation mode: enter a group so only its objects are
    // editable and everything else is dimmed; exitIsolation() steps out one level.
    bool isolationActive() const { return !isolatedGroupId_.isEmpty(); }
    void exitIsolation();
    // Called whenever a canvas transform drag mutates the selection (live and on
    // finish), so the property panel can refresh its transform fields.
    void setTransformChangedCallback(std::function<void()> fn);

protected:
    void initializeGL() override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void leaveEvent(QEvent *event) override;
    // Return false so Tab is delivered to keyPressEvent (flip) instead of moving focus.
    bool focusNextPrevChild(bool next) override;

private:
    // Per-tool strategies (canvas_tools.h). Tools hold a reference to the
    // canvas and reach into the shared drag state directly.
    friend class CanvasTool;
    friend class SelectTool;
    friend class MoveTool;
    friend class MarqueeTool;
    friend class TransformTool;
    friend class RotateTool;

    enum class DragMode {
        None,
        Pan,
        Move,
        Marquee,
        TransformMove,
        Scale,
        Skew,
        Rotate,
    };

    struct HitEntry {
        int layerIndex = -1;
        QString layerId;
        QPolygonF screenPolygon;
        QRectF screenBounds;
    };

    // Transform fields captured at drag start, shared by shapes and guides
    // (guides have no skew and leave it at 0; shear(0, 0) is the identity).
    struct EntryStart {
        double x = 0.0;
        double y = 0.0;
        double scaleX = 1.0;
        double scaleY = 1.0;
        double rotation = 0.0;
        double skew = 0.0;
    };

    // The Transform tool's selection box described in its own local frame. In Absolute mode
    // localToWorld is the identity and localRect is the world-axis AABB (so everything reduces
    // to the legacy behaviour). In Relative mode localToWorld follows the shape/group
    // orientation, so the box, handles and scale axes track the shape.
    struct SelectionBox {
        bool valid = false;
        QRectF localRect;          // box rectangle in its own local coords (world units)
        QTransform localToWorld;   // maps localRect coords -> world
    };

    QRectF projectBounds() const;
    void updateViewTransform();
    QPointF worldToScreen(const QPointF &point) const;
    QPointF screenToWorld(const QPointF &point) const;
    QPolygonF screenQuad(const QTransform &entryToWorld, const QRectF &localRect) const;
    QPolygonF layerScreenPolygon(const fh6::ShapeLayer &layer) const;
    QPolygonF guideScreenPolygon(const fh6::GuideLayer &guide) const;
    QVector<HitEntry> hitEntries();
    QVector<QString> layersAtScreenPoint(const QPointF &point);
    QString selectTargetAtScreenPoint(const QPointF &point, Qt::KeyboardModifiers modifiers);
    QRectF selectedScreenBounds() const;
    QRectF selectedWorldBounds() const;
    const QRectF &cachedSelectionWorldBounds() const;
    SelectionBox currentSelectionBox() const;
    QTransform boxToScreen(const SelectionBox &box) const;
    bool boxContainsScreenPoint(const SelectionBox &box, const QPointF &screenPoint) const;
    void cycleFlipSelection();
    QVector<fh6::ShapeLayer *> selectedLayers() const;
    QVector<fh6::GuideLayer *> selectedGuideLayers() const;
    void captureDragStarts();
    void captureScaleReference();
    // Chooses the drag mode (and captures rotate/scale references) for the active
    // tool once a selection and its drag-start box exist. Leaves dragMode_ as None
    // when the press does not begin a drag. Delegates to activeTool_->beginDrag().
    void beginToolDrag(const QPointF &screenPos, const QPointF &boxCenterWorld);
    // Shared by the Rotate tool and the Transform tool's rotate zone: enters the
    // rotate drag about the given world-space pivot.
    void beginRotateDrag(const QPointF &boxCenterWorld);
    void applyMoveDrag(const QPointF &screenPoint, Qt::KeyboardModifiers modifiers);
    void applyScaleDrag(const QPointF &screenPoint, Qt::KeyboardModifiers modifiers);
    void applySkewDrag(const QPointF &screenPoint);
    // Applies a linear-transform gesture (scale/skew) to every dragged item by composing it
    // with each item's drag-start transform, then decomposes the result back to per-item
    // fields. preMultiply picks the side: true -> transform * start (Relative single-item scale),
    // false -> start * transform (world/box-frame scale and group skew). Shared by both gestures.
    void applyDragTransform(const QTransform &transform, bool preMultiply);
    void applyRotateDrag(const QPointF &screenPoint, Qt::KeyboardModifiers modifiers);
    void clearCursorHint();
    void setCursorHint(const QPointF &point, const QStringList &lines);
    void drawCursorHint(QPainter &painter);
    // True while a drag that mutates entry transforms is active (not Pan/Marquee).
    bool isTransformDrag() const;
    void resetDragState();
    void finishDrag();
    void cancelDrag();
    QString transformHandleAt(const QPointF &point, const SelectionBox &box) const;
    bool rotateZoneAt(const QPointF &point, const SelectionBox &box) const;
    Qt::CursorShape cursorForScaleHandle(const QString &handle, const SelectionBox &box = {}) const;
    QCursor cursorForTransformHandle(const QString &handle, const SelectionBox &box = {}) const;
    Qt::CursorShape cursorForPoint(const QPointF &point);
    void updateCursorForPoint(const QPointF &point);
    QCursor rotateCursor() const;
    QCursor rotateCursorForPoint(const QPointF &point, const SelectionBox &box) const;
    void drawOverlay(QPainter &painter);
    bool selectionIsGroupLike() const;
    void updateSelectionFlashState();
    void setFlashingLayerIds(const QSet<QString> &ids);
    void scheduleSelectionFlashTimer();
    // Flash progress in [0,1) while inside the flash window, nullopt otherwise.
    std::optional<double> selectionFlashProgress() const;
    double selectionFlashHue() const;
    double selectionFlashStrength() const;
    void refreshHover(const QPointF &point, Qt::KeyboardModifiers modifiers);
    void selectByMarquee(Qt::KeyboardModifiers modifiers);
    bool pointInPolygon(const QPointF &point, const QPolygonF &polygon) const;
    // Precise pick test against a shape's actual inked triangles (falls back to
    // the shape quad when no mesh geometry is available for the shape).
    bool layerContainsScreenPoint(const fh6::ShapeLayer &layer, const QPointF &screenPoint) const;
    // True when a screen-space rect overlaps the shape's actual inked triangles
    // (falls back to the shape quad when no mesh is available). Used by marquee
    // selection so touching any part of a shape's art selects it.
    bool layerIntersectsRect(const fh6::ShapeLayer &layer, const QRectF &screenRect) const;
    // Boundary edges (silhouette) of a shape's triangle mesh, in local coords.
    const QVector<QLineF> &shapeOutlineLocal(int shapeId);
    bool movedPastClickThreshold(const QPointF &point) const;
    QString guideAtScreenPoint(const QPointF &point);
    void drawGuideLayers(QPainter &painter);
    QImage guideImage(const fh6::GuideLayer &guide) const;
    QString sectionCanvasCacheKey() const;
    void storeSectionCanvasCache(const QString &key);


    EditorState *state_ = nullptr;
    fh6::Project *project_ = nullptr;
    ShapeGeometryStore geometry_;
    bool geometryLoaded_ = false;
    QString tool_ = QStringLiteral("select");
    std::vector<std::unique_ptr<CanvasTool>> tools_;
    CanvasTool *activeTool_ = nullptr;
    QTransform worldToScreen_;
    QTransform screenToWorld_;
    QRectF currentBounds_;
    QRectF viewBounds_;
    double baseScale_ = 1.0;
    double zoom_ = 1.0;
    QPointF pan_;
    bool spaceDown_ = false;
    DragMode dragMode_ = DragMode::None;
    QPointF dragStartScreen_;
    QPointF dragLastScreen_;
    QPointF dragStartWorld_;
    QRectF dragStartSelectionBounds_;
    // Full selection box captured at drag start, so scaling/rotation references survive
    // pan/zoom and follow the box's (possibly rotated) local frame.
    SelectionBox dragStartBox_;
    QPointF rotateCenterWorld_;
    double rotateStartAngle_ = 0.0;
    // Relative transform mode: the selection box follows the shape/group orientation.
    bool transformRelativeMode_ = false;
    bool moveToolAutoSelect_ = false;
    std::function<void()> transformChangedCallback_;
    // Set on a Select-tool press that landed on the current selection's transform
    // box (a handle, rotate zone, or a selected shape); tells SelectTool::beginDrag
    // to grab a transform handle rather than force a plain move.
    bool selectPressOnBox_ = false;
    bool selectionFlashEnabled_ = true;
    // Isolation mode: the entered group (empty = not isolated) and the cached set of
    // leaf layer ids inside it (the only pickable/vivid layers while isolated).
    QString isolatedGroupId_;
    QSet<QString> isolatedLeafIds_;
    bool eyedropperArmed_ = false;
    std::function<void(const QColor &)> eyedropperCallback_;
    // Editor-only ruler guides. A horizontal guide is a constant world-Y line; a
    // vertical guide a constant world-X line.
    struct GuideLine {
        bool horizontal = false;
        double worldPos = 0.0;
    };
    bool rulersVisible_ = false;
    QVector<GuideLine> guideLines_;
    bool draggingGuide_ = false;
    int activeGuideIndex_ = -1;      // -1 while dragging out a brand-new guide
    bool activeGuideHorizontal_ = false;
    double activeGuideWorldPos_ = 0.0;
    int guideAtScreenPos(const QPointF &screenPoint) const;
    void drawRulersAndGuides(QPainter &painter);

    // Snapping state, rebuilt at the start of a move drag.
    bool snapEnabled_ = true;
    // Editor-only preview opacity per layer id (0..1); never exported.
    QHash<QString, double> previewOpacity_;
    QVector<double> snapTargetsX_;   // world X lines (other objects' edges/centres + vertical guides)
    QVector<double> snapTargetsY_;   // world Y lines
    QRectF dragStartBboxWorld_;      // the moving selection's world bounds at drag start
    struct SnapLineHit {
        bool vertical = false;
        double worldPos = 0.0;
    };
    QVector<SnapLineHit> activeSnapLines_;
    void buildSnapTargets();
    QPointF applyMoveSnap(const QPointF &delta);
    void enterIsolation(const QString &groupId);
    void refreshIsolationLeafCache();
    // Which entry a click should select: normally the outermost enclosing group, but in
    // isolation mode only up to a direct child of the isolated group (so its own objects
    // select individually, as if ungrouped). Empty result = select the leaf itself.
    QString selectionGroupForEntry(const QString &entryId) const;
    // Frame angle for a Relative-mode multi-selection box. It is NOT stored directly (that
    // would go stale on undo/redo); instead it is derived live as
    // (primary selected item's current rotation - frameReferenceRotation_). The reference is
    // captured when the selection set changes, so the box starts axis-aligned and then follows
    // any rotation applied afterwards. Because every rotate gesture adds the same delta to all
    // selected items, the primary's rotation tracks the cumulative box rotation, and undo/redo
    // restore it automatically. Scale/skew drags are the exception: they shear children and drift
    // the primary's decomposed rotation, so currentSelectionBox() pins the frame to the drag-start
    // angle and rebases this reference to keep it steady (see there). Mutable + signature-tracked
    // so currentSelectionBox() can refresh the reference lazily.
    mutable double frameReferenceRotation_ = 0.0;
    mutable QSet<QString> frameLayerSignature_;
    mutable QSet<QString> frameGuideSignature_;
    // Scale handle/anchor/centre expressed in the drag-start box's local coords, used to
    // compute pure per-axis scale factors in the box frame.
    QPointF scaleHandleLocal_;
    QPointF scaleAnchorLocal_;
    QPointF scaleCenterLocal_;
    // World-space scale references captured at drag start so scaling stays correct across
    // pan/zoom and tracks the grabbed handle exactly (anchor = fixed opposite side/corner).
    QPointF scaleAnchorWorld_;
    QPointF scaleHandleStartWorld_;
    // Selection centre in world space; used as the scale anchor when Alt is held so the
    // selection scales about its centre (the centre stays fixed) instead of the opposite handle.
    QPointF scaleCenterWorld_;
    QRectF marqueeRect_;
    QPointF cursorHintPoint_;
    QStringList cursorHintLines_;
    QString activeHandle_;
    // True when the in-progress drag began with an Alt-duplicate, so finishing the drag
    // refreshes the layer tree to show the newly inserted clones.
    bool dragDuplicated_ = false;
    QString hoverLayerId_;
    // Precise hover highlight: the hovered shape's silhouette in screen space.
    QVector<QLineF> hoverOutline_;
    // Cached per-shape silhouette (boundary edges of the triangle mesh) in the
    // shape's local frame; built on demand since shape geometry is static.
    QHash<int, QVector<QLineF>> shapeOutlineCache_;
    QVector<HitEntry> hitCache_;
    bool hitCacheDirty_ = true;
    NativeShapeRenderer renderer_;
    bool rendererGeometryDirty_ = true;
    QSet<QString> flashingLayerIds_;
    QElapsedTimer selectionFlashClock_;
    QTimer selectionFlashTimer_;
    QHash<QString, EntryStart> dragStarts_;
    QHash<QString, EntryStart> dragGuideStarts_;
    // Selected (unlocked) layers resolved once at drag start so per-move handlers
    // don't rebuild the entry/lock maps and rescan the project every mouse event.
    QVector<fh6::ShapeLayer *> dragLayers_;
    QVector<fh6::GuideLayer *> dragGuides_;
    mutable QHash<QString, QImage> guideImageCache_;
    mutable QHash<QString, QImage> sectionCanvasCache_;
    // Cached world-space union of the selection's bounds; mapped through the view transform to
    // produce the screen-space selection box without rescanning layers on every repaint.
    mutable std::optional<QRectF> selectionWorldBoundsCache_;
    QColor canvasColor_;
};

} // namespace gui
