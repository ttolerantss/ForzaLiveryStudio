#include "canvas_tools.h"

#include "editor_state.h"
#include "project_canvas.h"

#include <QMouseEvent>

namespace gui {

bool CanvasTool::handlePress(QMouseEvent *event)
{
    Q_UNUSED(event);
    return false;
}

void CanvasTool::beginDrag(const QPointF &screenPos, const QPointF &boxCenterWorld)
{
    Q_UNUSED(screenPos);
    Q_UNUSED(boxCenterWorld);
}

bool CanvasTool::handleRelease(QMouseEvent *event)
{
    Q_UNUSED(event);
    return false;
}

bool CanvasTool::hoverCursor(const QPointF &point, QCursor *cursor) const
{
    Q_UNUSED(point);
    Q_UNUSED(cursor);
    return false;
}

Qt::CursorShape CanvasTool::idleCursorShape(const QPointF &point) const
{
    Q_UNUSED(point);
    return Qt::ArrowCursor;
}

// --- Select ---------------------------------------------------------------
// The Select tool doubles as the move tool: ProjectCanvas::mousePressEvent
// picks the item under the cursor on press, and beginDrag() below arms a move
// so a drag translates the selection (Illustrator's black-arrow behaviour).

QString SelectTool::name() const
{
    return QStringLiteral("select");
}

void SelectTool::beginDrag(const QPointF &screenPos, const QPointF &boxCenterWorld)
{
    ProjectCanvas &c = canvas_;
    // A press that freshly picked a shape just moves it. Only a press that
    // landed on the existing selection's box may grab a transform handle, so
    // selecting a shape never accidentally scales/rotates it.
    if (c.selectPressOnBox_) {
        c.activeHandle_ = c.transformHandleAt(screenPos, c.dragStartBox_);
        if (c.activeHandle_ == QStringLiteral("skew")) {
            c.dragMode_ = ProjectCanvas::DragMode::Skew;
            return;
        }
        if (!c.activeHandle_.isEmpty()) {
            c.dragMode_ = ProjectCanvas::DragMode::Scale;
            c.captureScaleReference();
            return;
        }
        if (c.rotateZoneAt(screenPos, c.dragStartBox_)) {
            c.beginRotateDrag(boxCenterWorld);
            return;
        }
    }
    c.dragMode_ = ProjectCanvas::DragMode::Move;
}

bool SelectTool::handleRelease(QMouseEvent *event)
{
    ProjectCanvas &c = canvas_;
    // Only the empty-canvas rubber-band marquee is resolved here; move/transform
    // drags fall through to the canvas's shared finishDrag path.
    if (event->button() != Qt::LeftButton || c.dragMode_ != ProjectCanvas::DragMode::Marquee) {
        return false;
    }
    if (c.movedPastClickThreshold(event->position())) {
        c.selectByMarquee(event->modifiers());
    } else if (c.state_ != nullptr
               && !(event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier))) {
        c.state_->clearSelection();
    }
    c.finishDrag();
    c.update();
    event->accept();
    return true;
}

bool SelectTool::hoverCursor(const QPointF &point, QCursor *cursor) const
{
    ProjectCanvas &c = canvas_;
    if (c.state_ == nullptr
        || (c.state_->selectedLayerIds().isEmpty() && c.state_->selectedGuideLayerIds().isEmpty())) {
        return false;
    }
    c.updateViewTransform();
    const ProjectCanvas::SelectionBox box = c.currentSelectionBox();
    const QString handle = c.transformHandleAt(point, box);
    if (!handle.isEmpty()) {
        *cursor = c.cursorForTransformHandle(handle, box);
        return true;
    }
    if (c.rotateZoneAt(point, box)) {
        *cursor = c.rotateCursorForPoint(point, box);
        return true;
    }
    return false;
}

Qt::CursorShape SelectTool::idleCursorShape(const QPointF &point) const
{
    ProjectCanvas &c = canvas_;
    if (c.state_ != nullptr
        && (!c.state_->selectedLayerIds().isEmpty() || !c.state_->selectedGuideLayerIds().isEmpty())) {
        c.updateViewTransform();
        const ProjectCanvas::SelectionBox box = c.currentSelectionBox();
        const QString handle = c.transformHandleAt(point, box);
        if (!handle.isEmpty()) {
            return c.cursorForScaleHandle(handle, box);
        }
        if (c.rotateZoneAt(point, box)) {
            return Qt::ArrowCursor;
        }
        if (c.boxContainsScreenPoint(box, point)) {
            return Qt::SizeAllCursor;
        }
    }
    return Qt::ArrowCursor;
}

// --- Move -----------------------------------------------------------------

QString MoveTool::name() const
{
    return QStringLiteral("move");
}

void MoveTool::beginDrag(const QPointF &screenPos, const QPointF &boxCenterWorld)
{
    Q_UNUSED(screenPos);
    Q_UNUSED(boxCenterWorld);
    canvas_.dragMode_ = ProjectCanvas::DragMode::Move;
}

Qt::CursorShape MoveTool::idleCursorShape(const QPointF &point) const
{
    Q_UNUSED(point);
    return Qt::SizeAllCursor;
}

// --- Marquee ----------------------------------------------------------------

QString MarqueeTool::name() const
{
    return QStringLiteral("marquee");
}

bool MarqueeTool::handlePress(QMouseEvent *event)
{
    ProjectCanvas &c = canvas_;
    c.dragMode_ = ProjectCanvas::DragMode::Marquee;
    c.marqueeRect_ = QRectF(c.dragStartScreen_, c.dragStartScreen_).normalized();
    c.updateCursorForPoint(event->position());
    event->accept();
    return true;
}

bool MarqueeTool::handleRelease(QMouseEvent *event)
{
    ProjectCanvas &c = canvas_;
    if (c.dragMode_ != ProjectCanvas::DragMode::Marquee || event->button() != Qt::LeftButton) {
        return false;
    }
    if (c.movedPastClickThreshold(event->position())) {
        c.selectByMarquee(event->modifiers());
    } else if (c.state_ != nullptr) {
        c.state_->clearSelection();
    }
    c.finishDrag();
    c.update();
    event->accept();
    return true;
}

Qt::CursorShape MarqueeTool::idleCursorShape(const QPointF &point) const
{
    Q_UNUSED(point);
    return Qt::CrossCursor;
}

// --- Transform --------------------------------------------------------------

QString TransformTool::name() const
{
    return QStringLiteral("transform");
}

void TransformTool::beginDrag(const QPointF &screenPos, const QPointF &boxCenterWorld)
{
    ProjectCanvas &c = canvas_;
    c.activeHandle_ = c.transformHandleAt(screenPos, c.dragStartBox_);
    if (c.activeHandle_ == QStringLiteral("skew")) {
        c.dragMode_ = ProjectCanvas::DragMode::Skew;
    } else if (!c.activeHandle_.isEmpty()) {
        c.dragMode_ = ProjectCanvas::DragMode::Scale;
        c.captureScaleReference();
    } else if (c.rotateZoneAt(screenPos, c.dragStartBox_)) {
        c.beginRotateDrag(boxCenterWorld);
    } else if (c.boxContainsScreenPoint(c.dragStartBox_, screenPos)) {
        c.dragMode_ = ProjectCanvas::DragMode::TransformMove;
    }
}

bool TransformTool::hoverCursor(const QPointF &point, QCursor *cursor) const
{
    ProjectCanvas &c = canvas_;
    if (c.state_ == nullptr
        || (c.state_->selectedLayerIds().isEmpty() && c.state_->selectedGuideLayerIds().isEmpty())) {
        return false;
    }
    c.updateViewTransform();
    const ProjectCanvas::SelectionBox box = c.currentSelectionBox();
    const QString handle = c.transformHandleAt(point, box);
    if (!handle.isEmpty()) {
        *cursor = c.cursorForTransformHandle(handle, box);
        return true;
    }
    if (c.rotateZoneAt(point, box)) {
        *cursor = c.rotateCursorForPoint(point, box);
        return true;
    }
    return false;
}

Qt::CursorShape TransformTool::idleCursorShape(const QPointF &point) const
{
    ProjectCanvas &c = canvas_;
    if (c.state_ != nullptr
        && (!c.state_->selectedLayerIds().isEmpty() || !c.state_->selectedGuideLayerIds().isEmpty())) {
        c.updateViewTransform();
        const ProjectCanvas::SelectionBox box = c.currentSelectionBox();
        const QString handle = c.transformHandleAt(point, box);
        if (!handle.isEmpty()) {
            return c.cursorForScaleHandle(handle, box);
        }
        if (c.rotateZoneAt(point, box)) {
            return Qt::ArrowCursor;
        }
        if (c.boxContainsScreenPoint(box, point)) {
            return Qt::SizeAllCursor;
        }
    }
    return Qt::ArrowCursor;
}

// --- Rotate -----------------------------------------------------------------

QString RotateTool::name() const
{
    return QStringLiteral("rotate");
}

void RotateTool::beginDrag(const QPointF &screenPos, const QPointF &boxCenterWorld)
{
    Q_UNUSED(screenPos);
    canvas_.beginRotateDrag(boxCenterWorld);
}

bool RotateTool::hoverCursor(const QPointF &point, QCursor *cursor) const
{
    ProjectCanvas &c = canvas_;
    if (c.state_ != nullptr
        && (!c.state_->selectedLayerIds().isEmpty() || !c.state_->selectedGuideLayerIds().isEmpty())) {
        c.updateViewTransform();
        const ProjectCanvas::SelectionBox box = c.currentSelectionBox();
        if (c.rotateZoneAt(point, box)) {
            *cursor = c.rotateCursorForPoint(point, box);
            return true;
        }
    }
    *cursor = c.rotateCursor();
    return true;
}

} // namespace gui
