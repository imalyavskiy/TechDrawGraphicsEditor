#include "canvas.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <cmath>

class StateCommand : public QUndoCommand {
public:
    StateCommand(Canvas *canvas, DrawingState before, DrawingState after, const QString &label)
        : canvas_(canvas), before_(std::move(before)), after_(std::move(after)) { setText(label); }
    void undo() override { canvas_->apply(before_); }
    void redo() override { canvas_->apply(after_); }
private:
    Canvas *canvas_;
    DrawingState before_, after_;
};

Canvas::Canvas(QWidget *parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumSize(200, 160);
    setObjectName("drawingCanvas");
    DrawingState initial;
    initial.image = QImage(1000, 620, QImage::Format_ARGB32_Premultiplied);
    initial.image.fill(Qt::white);
    initial.vanishing = QPointF(650, 240);
    setDocument(initial);
}

void Canvas::setDocument(const DrawingState &state, bool clean) {
    dragging_ = panning_ = movingPoint_ = false;
    straightStroke_ = shiftPressed_ = controlPressed_ = false;
    hasPaintAnchor_ = hasHoverPoint_ = false;
    state_ = state;
    undo_.clear();
    undo_.setUndoLimit(qBound(1, int(128000000 / qMax(qint64(1), qint64(state.image.sizeInBytes()))), 30));
    if (clean) undo_.setClean(); else undo_.resetClean();
    pan_ = QPointF();
    emit stateChanged();
    update();
}

void Canvas::apply(const DrawingState &state) { state_ = state; emit stateChanged(); update(); }
void Canvas::commit(const DrawingState &before, const QString &label) { undo_.push(new StateCommand(this, before, state_, label)); }
void Canvas::setTool(Tool tool) { finish(); if (tool_ != tool) hasPaintAnchor_ = false; tool_ = tool; setCursor(tool == Pan ? Qt::OpenHandCursor : Qt::CrossCursor); update(); }
void Canvas::setGridVisible(bool visible) { if (state_.gridVisible == visible) return; auto before = state_; state_.gridVisible = visible; commit(before, tr("видимость перспективы")); }
void Canvas::setRayCount(int count) { if (state_.rays == count) return; auto before = state_; state_.rays = count; commit(before, tr("число направляющих")); }
void Canvas::setGridColor(QColor color) { if (!color.isValid() || state_.gridColor == color) return; auto before = state_; state_.gridColor = color; commit(before, tr("цвет направляющих")); }
QPointF Canvas::toImage(QPointF p) const { return (p - QPointF(width()/2.0, height()/2.0) - pan_) / zoom_ + QPointF(state_.image.width()/2.0, state_.image.height()/2.0); }
QPointF Canvas::toView(QPointF p) const { return (p - QPointF(state_.image.width()/2.0, state_.image.height()/2.0))*zoom_ + QPointF(width()/2.0, height()/2.0) + pan_; }
void Canvas::setZoom(double zoom, QPointF anchor) {
    if (anchor.x() < 0) anchor = QPointF(width()/2.0, height()/2.0);
    const QPointF imagePoint = toImage(anchor);
    zoom_ = qBound(0.05, zoom, 16.0);
    pan_ += anchor - toView(imagePoint);
    emit viewChanged(); update();
}
void Canvas::fit() { pan_ = QPointF(); zoom_ = qBound(0.05, qMin((width()-60.0)/state_.image.width(), (height()-60.0)/state_.image.height()), 16.0); emit viewChanged(); update(); }

void Canvas::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.fillRect(rect(), QColor("#dce0e5"));
    const QRectF paper(toView(QPointF()), QSizeF(state_.image.size())*zoom_);
    p.fillRect(paper.translated(3, 4), QColor(0,0,0,35));
    p.fillRect(paper, Qt::white);
    p.save();
    p.setClipRect(paper);
    p.translate(toView(QPointF()));
    p.scale(zoom_, zoom_);
    p.setRenderHint(QPainter::SmoothPixmapTransform, zoom_ < 1);
    p.drawImage(QPointF(), state_.image);
    if (state_.gridVisible) {
        p.setRenderHint(QPainter::Antialiasing);
        QColor color = state_.gridColor; color.setAlpha(110);
        QPen grid(color, 1); grid.setCosmetic(true); p.setPen(grid);
        const double radius = std::hypot(state_.image.width(), state_.image.height()) + std::hypot(state_.vanishing.x(), state_.vanishing.y());
        for (int i=0; i<state_.rays; ++i) {
            double angle = i * 6.283185307179586 / state_.rays;
            p.drawLine(state_.vanishing, state_.vanishing + QPointF(std::cos(angle), std::sin(angle))*radius*2);
        }
        p.drawLine(QPointF(0, state_.vanishing.y()), QPointF(state_.image.width(), state_.vanishing.y()));
    }
    p.restore();
    if (state_.gridVisible) {
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(state_.gridColor, 2)); p.setBrush(Qt::white);
        p.drawEllipse(toView(state_.vanishing), 6, 6);
        p.setPen(QColor("#355274"));
        p.drawText(toView(state_.vanishing) + QPointF(11, -10), tr("Точка схода"));
    }
    if (isPaintTool() && hasPaintAnchor_ && hasHoverPoint_ && shiftPressed_ && !dragging_) {
        const QPointF endpoint=constrainedPoint(hoverPoint_,controlPressed_);
        p.save();p.setClipRect(paper);p.setRenderHint(QPainter::Antialiasing);
        QPen preview(QColor(40,52,68,170),1,Qt::DashLine);preview.setCosmetic(true);p.setPen(preview);
        p.drawLine(toView(paintAnchor_),toView(endpoint));p.restore();
    }
}

void Canvas::stroke(QPointF a, QPointF b) {
    QPainter p(&state_.image);
    p.setRenderHint(QPainter::Antialiasing,tool_ != Pencil);
    QPen pen(tool_ == Eraser ? back_ : front_, width_, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    if (a == b) { p.setPen(Qt::NoPen); p.setBrush(pen.color()); p.drawEllipse(a, width_/2.0, width_/2.0); }
    else p.drawLine(a, b);
    update();
}
bool Canvas::isPaintTool() const { return tool_ == Pencil || tool_ == Brush || tool_ == Eraser; }
QPointF Canvas::constrainedPoint(QPointF point, bool constrainAngle) const {
    if (!constrainAngle || !hasPaintAnchor_) return point;
    const QPointF delta=point-paintAnchor_;
    const double radius=std::hypot(delta.x(),delta.y());
    if (radius == 0) return point;
    constexpr double step=3.14159265358979323846/12.0;
    const double angle=std::round(std::atan2(delta.y(),delta.x())/step)*step;
    return paintAnchor_+QPointF(std::cos(angle)*radius,std::sin(angle)*radius);
}
void Canvas::mousePressEvent(QMouseEvent *e) {
    if (e->button() != Qt::LeftButton && e->button() != Qt::MiddleButton) return;
    setFocus();
    panning_ = e->button() == Qt::MiddleButton || tool_ == Pan || space_;
    if (panning_) { dragging_ = true; last_ = e->localPos(); setCursor(Qt::ClosedHandCursor); return; }
    if (tool_ == Perspective) {
        if (!state_.gridVisible || QLineF(e->localPos(), toView(state_.vanishing)).length() > 15) return;
        before_ = state_; movingPoint_ = dragging_ = true; return;
    }
    QPointF point = toImage(e->localPos());
    if (!QRectF(QPointF(), state_.image.size()).contains(point)) return;
    before_ = state_; dragging_ = true;
    const bool straight=(e->modifiers()&Qt::ShiftModifier)||shiftPressed_;
    if (straight && hasPaintAnchor_) {
        point=constrainedPoint(point,(e->modifiers()&Qt::ControlModifier)||controlPressed_);
        straightStroke_=true;last_=point;stroke(paintAnchor_,point);
    } else {
        straightStroke_=false;last_=point;stroke(point,point);
    }
}
void Canvas::mouseMoveEvent(QMouseEvent *e) {
    hoverPoint_=toImage(e->localPos());hasHoverPoint_=QRectF(QPointF(),state_.image.size()).contains(hoverPoint_);
    emit positionChanged(hoverPoint_);
    if (!dragging_) { if (shiftPressed_ && hasPaintAnchor_) update(); return; }
    if (panning_) { pan_ += e->localPos() - last_; last_ = e->localPos(); update(); }
    else if (movingPoint_) { state_.vanishing = toImage(e->localPos()); update(); }
    else if (!straightStroke_) { stroke(last_, hoverPoint_); last_ = hoverPoint_; }
}
void Canvas::finish() {
    if (!dragging_) return;
    if (!panning_ && (movingPoint_ ? state_.vanishing != before_.vanishing : state_.image != before_.image))
        commit(before_, movingPoint_ ? tr("точку схода") : (tool_ == Eraser ? tr("ластик") : tr("штрих")));
    if (!panning_ && !movingPoint_ && isPaintTool()) { paintAnchor_=last_;hasPaintAnchor_=true; }
    before_ = DrawingState();
    dragging_ = panning_ = movingPoint_ = straightStroke_ = false;
    setCursor(tool_ == Pan ? Qt::OpenHandCursor : Qt::CrossCursor);
}
void Canvas::mouseReleaseEvent(QMouseEvent *) { finish(); }
void Canvas::wheelEvent(QWheelEvent *e) { setZoom(zoom_*std::pow(1.15, e->angleDelta().y()/120.0), e->position()); e->accept(); }
void Canvas::keyPressEvent(QKeyEvent *e) {
    if (e->key() == Qt::Key_Space) { space_ = true; setCursor(Qt::OpenHandCursor); e->accept(); }
    else if (e->key() == Qt::Key_Shift) { shiftPressed_=true;update();e->accept(); }
    else if (e->key() == Qt::Key_Control) { controlPressed_=true;update();e->accept(); }
    else if (e->key() == Qt::Key_Escape && dragging_) { if (!panning_) state_ = before_; dragging_ = panning_ = movingPoint_ = false; before_ = DrawingState(); update(); }
    else QWidget::keyPressEvent(e);
}
void Canvas::keyReleaseEvent(QKeyEvent *e) {
    if (e->key() == Qt::Key_Space) { space_ = false; setCursor(tool_ == Pan ? Qt::OpenHandCursor : Qt::CrossCursor); }
    else if (e->key() == Qt::Key_Shift) { shiftPressed_=false;update(); }
    else if (e->key() == Qt::Key_Control) { controlPressed_=false;update(); }
    else QWidget::keyReleaseEvent(e);
}
void Canvas::focusOutEvent(QFocusEvent *e) { finish(); space_ = shiftPressed_ = controlPressed_ = false; update(); QWidget::focusOutEvent(e); }
