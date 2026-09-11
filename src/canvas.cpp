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
    const DrawingState &before() const { return before_; }
    const DrawingState &after() const { return after_; }
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
    initial.horizonY = initial.vanishing.y();
    setDocument(initial);
}

void Canvas::setDocument(const DrawingState &state, bool clean) {
    DrawingHistory history;
    history.states.append(state);
    setDocument(history,clean);
}

void Canvas::setDocument(const DrawingHistory &history, bool clean) {
    dragging_ = panning_ = movingPoint_ = movingHorizon_ = horizonCarriesPoint_ = false;
    straightStroke_ = shiftPressed_ = controlPressed_ = false;
    hasPaintAnchor_ = hasHoverPoint_ = false;
    state_ = history.states.first();
    undo_.clear();
    const int memoryLimit=qBound(1,int(128000000/qMax(qint64(1),qint64(state_.image.sizeInBytes()))),30);
    undo_.setUndoLimit(qMax(memoryLimit,history.labels.size()));
    for (int i=0;i<history.labels.size();++i)
        undo_.push(new StateCommand(this,history.states[i],history.states[i+1],history.labels[i]));
    undo_.setIndex(history.index);
    if (clean) undo_.setClean(); else undo_.resetClean();
    pan_ = QPointF();
    emit stateChanged();
    update();
}

DrawingHistory Canvas::history() const {
    DrawingHistory result;
    result.index=undo_.index();
    if (undo_.count()==0) { result.states.append(state_);return result; }
    const auto *first=dynamic_cast<const StateCommand*>(undo_.command(0));
    if (!first) { result.states.append(state_);result.index=0;return result; }
    result.states.append(first->before());
    for (int i=0;i<undo_.count();++i) {
        const auto *command=dynamic_cast<const StateCommand*>(undo_.command(i));
        if (!command) { result.states.clear();result.labels.clear();result.states.append(state_);result.index=0;return result; }
        result.states.append(command->after());
        result.labels.append(command->text());
    }
    return result;
}

void Canvas::apply(const DrawingState &state) { state_ = state; emit stateChanged(); update(); }
void Canvas::commit(const DrawingState &before, const QString &label) { undo_.push(new StateCommand(this, before, state_, label)); }
void Canvas::setTool(Tool tool) { finish(); if (tool_ != tool) hasPaintAnchor_ = false; tool_ = tool; setCursor(tool == Pan ? Qt::OpenHandCursor : Qt::CrossCursor); update(); }
void Canvas::setGridVisible(bool visible) { if (state_.gridVisible == visible) return; auto before = state_; state_.gridVisible = visible; commit(before, tr("видимость перспективы")); }
void Canvas::setRayStep(double degrees) { if (qFuzzyCompare(state_.rayStepDegrees, degrees)) return; auto before = state_; state_.rayStepDegrees = degrees; commit(before, tr("угловой шаг направляющих")); }
void Canvas::setGridColor(QColor color) { if (!color.isValid() || state_.gridColor == color) return; auto before = state_; state_.gridColor = color; commit(before, tr("цвет направляющих")); }
void Canvas::setRayGap(int gap) { if (state_.rayGap == gap) return; auto before = state_; state_.rayGap = gap; commit(before, tr("отступ направляющих")); }
void Canvas::setRayStartOpacity(int opacity) { if (state_.rayStartOpacity == opacity) return; auto before = state_; state_.rayStartOpacity = opacity; commit(before, tr("начальную непрозрачность направляющих")); }
void Canvas::setRayEndOpacity(int opacity) { if (state_.rayEndOpacity == opacity) return; auto before = state_; state_.rayEndOpacity = opacity; commit(before, tr("конечную непрозрачность направляющих")); }
void Canvas::setRayFadeLength(int length) { if (state_.rayFadeLength == length) return; auto before = state_; state_.rayFadeLength = length; commit(before, tr("длину нарастания направляющих")); }
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
    p.restore();
    if (state_.gridVisible) {
        p.setRenderHint(QPainter::Antialiasing);
        const QPointF vanishing = toView(state_.vanishing);
        double radius = 0;
        const QPointF corners[] = {paper.topLeft(), paper.topRight(), paper.bottomLeft(), paper.bottomRight(),
                                   rect().topLeft(), rect().topRight(), rect().bottomLeft(), rect().bottomRight()};
        for (const QPointF &corner : corners)
            radius = qMax(radius, QLineF(vanishing, corner).length());
        const QLineF edges[] = {QLineF(paper.topLeft(),paper.topRight()),QLineF(paper.topRight(),paper.bottomRight()),
                                QLineF(paper.bottomRight(),paper.bottomLeft()),QLineF(paper.bottomLeft(),paper.topLeft())};
        for (double degrees=0; degrees<360.0; degrees+=state_.rayStepDegrees) {
            const double angle = degrees * 3.14159265358979323846 / 180.0;
            const QPointF direction(std::cos(angle), std::sin(angle));
            const QPointF start = vanishing + direction*state_.rayGap;
            const QPointF viewportEnd = vanishing + direction*(radius + 2);
            if (QLineF(vanishing, start).length() >= QLineF(vanishing, viewportEnd).length()) continue;
            const QLineF visibleRay(start,viewportEnd);
            QPointF intersection;
            bool crossesCanvas = paper.contains(start);
            double exitDistance = -1;
            for (const QLineF &edge : edges) {
                if (visibleRay.intersects(edge,&intersection)!=QLineF::BoundedIntersection) continue;
                crossesCanvas=true;
                exitDistance=qMax(exitDistance,QPointF::dotProduct(intersection-vanishing,direction));
            }
            if (!crossesCanvas||exitDistance<state_.rayGap) continue;
            const QPointF end = vanishing + direction*exitDistance;
            QColor startColor = state_.gridColor; startColor.setAlphaF(state_.rayStartOpacity/100.0);
            QColor endColor = state_.gridColor; endColor.setAlphaF(state_.rayEndOpacity/100.0);
            QPen ray;
            if (state_.rayFadeLength > 0) {
                QLinearGradient fade(start, start + direction*state_.rayFadeLength);
                fade.setColorAt(0, startColor); fade.setColorAt(1, endColor);
                ray = QPen(QBrush(fade), 1);
            } else ray = QPen(endColor, 1);
            ray.setCosmetic(true); p.setPen(ray); p.drawLine(start, end);
        }
        const double horizonY = toView(QPointF(0, state_.horizonY)).y();
        QColor horizonColor = state_.gridColor; horizonColor.setAlphaF(state_.rayEndOpacity/100.0);
        QPen horizon(horizonColor, 1); horizon.setCosmetic(true); p.setPen(horizon);
        p.drawLine(QPointF(0, horizonY), QPointF(width(), horizonY));
        p.setPen(QPen(state_.gridColor, 2)); p.setBrush(Qt::white);
        p.drawEllipse(vanishing, 6, 6);
        if (qFuzzyCompare(state_.vanishing.y()+1,state_.horizonY+1)) {
            p.setPen(Qt::NoPen);p.setBrush(state_.gridColor);p.drawEllipse(vanishing,2.5,2.5);
        }
        p.setPen(QColor("#355274"));
        p.drawText(vanishing + QPointF(11, -10), tr("Точка схода"));
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
        if (!state_.gridVisible) return;
        const QPointF vanishing=toView(state_.vanishing);
        const double horizonY=toView(QPointF(0,state_.horizonY)).y();
        if (QLineF(e->localPos(),vanishing).length()<=15) { before_=state_;movingPoint_=dragging_=true;return; }
        if (std::abs(e->localPos().y()-horizonY)<=8) { before_=state_;horizonCarriesPoint_=qFuzzyCompare(state_.vanishing.y()+1,state_.horizonY+1);movingHorizon_=dragging_=true;return; }
        return;
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
    else if (movingPoint_) { QPointF point=toImage(e->localPos());if(std::abs(e->localPos().y()-toView(QPointF(0,state_.horizonY)).y())<=10)point.setY(state_.horizonY);state_.vanishing=point;update(); }
    else if (movingHorizon_) { state_.horizonY=toImage(e->localPos()).y();if(horizonCarriesPoint_)state_.vanishing.setY(state_.horizonY);update(); }
    else if (!straightStroke_) { stroke(last_, hoverPoint_); last_ = hoverPoint_; }
}
void Canvas::finish() {
    if (!dragging_) return;
    if (!panning_ && (movingPoint_ ? state_.vanishing != before_.vanishing : movingHorizon_ ? state_.horizonY != before_.horizonY : state_.image != before_.image))
        commit(before_, movingPoint_ ? tr("точку схода") : movingHorizon_ ? tr("горизонт") : (tool_ == Eraser ? tr("ластик") : tr("штрих")));
    if (!panning_ && !movingPoint_ && !movingHorizon_ && isPaintTool()) { paintAnchor_=last_;hasPaintAnchor_=true; }
    before_ = DrawingState();
    dragging_ = panning_ = movingPoint_ = movingHorizon_ = horizonCarriesPoint_ = straightStroke_ = false;
    setCursor(tool_ == Pan ? Qt::OpenHandCursor : Qt::CrossCursor);
}
void Canvas::mouseReleaseEvent(QMouseEvent *) { finish(); }
void Canvas::wheelEvent(QWheelEvent *e) { setZoom(zoom_*std::pow(1.15, e->angleDelta().y()/120.0), e->position()); e->accept(); }
void Canvas::keyPressEvent(QKeyEvent *e) {
    if (e->key() == Qt::Key_Space) { space_ = true; setCursor(Qt::OpenHandCursor); e->accept(); }
    else if (e->key() == Qt::Key_Shift) { shiftPressed_=true;update();e->accept(); }
    else if (e->key() == Qt::Key_Control) { controlPressed_=true;update();e->accept(); }
    else if (e->key() == Qt::Key_Escape && dragging_) { if (!panning_) state_ = before_; dragging_ = panning_ = movingPoint_ = movingHorizon_ = horizonCarriesPoint_ = false; before_ = DrawingState(); update(); }
    else QWidget::keyPressEvent(e);
}
void Canvas::keyReleaseEvent(QKeyEvent *e) {
    if (e->key() == Qt::Key_Space) { space_ = false; setCursor(tool_ == Pan ? Qt::OpenHandCursor : Qt::CrossCursor); }
    else if (e->key() == Qt::Key_Shift) { shiftPressed_=false;update(); }
    else if (e->key() == Qt::Key_Control) { controlPressed_=false;update(); }
    else QWidget::keyReleaseEvent(e);
}
void Canvas::focusOutEvent(QFocusEvent *e) { finish(); space_ = shiftPressed_ = controlPressed_ = false; update(); QWidget::focusOutEvent(e); }
