#include "canvas.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QUuid>
#include <cmath>

namespace {
void copyPerspectiveAppearance(const DrawingState &source, DrawingState *target) {
    target->gridVisible=source.gridVisible;target->rayStepDegrees=source.rayStepDegrees;
    target->rayGap=source.rayGap;target->rayStartOpacity=source.rayStartOpacity;target->rayEndOpacity=source.rayEndOpacity;target->rayFadeLength=source.rayFadeLength;
    target->horizonColor=source.horizonColor;target->horizonOpacity=source.horizonOpacity;target->horizonWidth=source.horizonWidth;
    for (auto &point : target->vanishingPoints) {
        for (const auto &existing : source.vanishingPoints) {
            if (existing.id != point.id) continue;
            point.color = existing.color;
            point.visible = existing.visible;
            break;
        }
    }
}
}

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
    initial.horizonY = 240;
    initial.vanishingPoints.append({QStringLiteral("vp-1"), QPointF(650, 240), QStringLiteral("construction"), QStringLiteral("horizon")});
    setDocument(initial);
}

void Canvas::setDocument(const DrawingState &state, bool clean) {
    DrawingHistory history;
    history.states.append(state);
    setDocument(history,clean);
}

void Canvas::setDocument(const DrawingHistory &history, bool clean) {
    dragging_ = panning_ = movingPoint_ = movingHorizon_ = horizonCarriesPoint_ = false;
    movingPointIndex_ = -1;
    straightStroke_ = shiftPressed_ = controlPressed_ = false;
    hasPaintAnchor_ = hasHoverPoint_ = false;
    state_ = history.states.first();
    selectedPointIndex_ = state_.vanishingPoints.isEmpty() ? -1 : qBound(0, selectedPointIndex_, state_.vanishingPoints.size()-1);
    undo_.clear();
    const int memoryLimit=qBound(1,int(128000000/qMax(qint64(1),qint64(state_.image.sizeInBytes()))),30);
    undo_.setUndoLimit(qMax(memoryLimit,history.labels.size()));
    for (int i=0;i<history.labels.size();++i)
        undo_.push(new StateCommand(this,history.states[i],history.states[i+1],history.labels[i]));
    undo_.setIndex(history.index);
    if (clean) undo_.setClean(); else undo_.resetClean();
    pan_ = QPointF();
    emit stateChanged(); emit selectedPointChanged(selectedPointIndex_);
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

void Canvas::apply(const DrawingState &state) { DrawingState next=state;copyPerspectiveAppearance(state_,&next);state_=std::move(next);selectedPointIndex_=state_.vanishingPoints.isEmpty()?-1:qBound(0,selectedPointIndex_,state_.vanishingPoints.size()-1);emit stateChanged();emit selectedPointChanged(selectedPointIndex_);update(); }
void Canvas::commit(const DrawingState &before, const QString &label) { undo_.push(new StateCommand(this, before, state_, label)); }
void Canvas::setTool(Tool tool) { finish(); if (tool_ != tool) hasPaintAnchor_ = false; tool_ = tool; setCursor(tool == Pan ? Qt::OpenHandCursor : Qt::CrossCursor); update(); }
void Canvas::setGridVisible(bool visible) { if (state_.gridVisible == visible) return;state_.gridVisible=visible;emit stateChanged();update(); }
void Canvas::setRayStep(double degrees) { if (qFuzzyCompare(state_.rayStepDegrees, degrees)) return;state_.rayStepDegrees=degrees;emit stateChanged();update(); }
void Canvas::setRayGap(int gap) { if (state_.rayGap == gap) return;state_.rayGap=gap;emit stateChanged();update(); }
void Canvas::setRayStartOpacity(int opacity) { if (state_.rayStartOpacity == opacity) return;state_.rayStartOpacity=opacity;emit stateChanged();update(); }
void Canvas::setRayEndOpacity(int opacity) { if (state_.rayEndOpacity == opacity) return;state_.rayEndOpacity=opacity;emit stateChanged();update(); }
void Canvas::setRayFadeLength(int length) { if (state_.rayFadeLength == length) return;state_.rayFadeLength=length;emit stateChanged();update(); }
void Canvas::setRayAppearance(double stepDegrees, int gap, int startOpacity, int endOpacity, int fadeLength) {
    if (qFuzzyCompare(state_.rayStepDegrees,stepDegrees)&&state_.rayGap==gap&&state_.rayStartOpacity==startOpacity&&state_.rayEndOpacity==endOpacity&&state_.rayFadeLength==fadeLength) return;
    state_.rayStepDegrees=stepDegrees;state_.rayGap=gap;state_.rayStartOpacity=startOpacity;state_.rayEndOpacity=endOpacity;state_.rayFadeLength=fadeLength;emit stateChanged();update();
}
void Canvas::setHorizonColor(QColor color) { if (!color.isValid()||state_.horizonColor==color) return;state_.horizonColor=color;emit stateChanged();update(); }
void Canvas::setHorizonOpacity(int opacity) { if (state_.horizonOpacity==opacity) return;state_.horizonOpacity=opacity;emit stateChanged();update(); }
void Canvas::setHorizonWidth(double width) { if (qFuzzyCompare(state_.horizonWidth,width)) return;state_.horizonWidth=width;emit stateChanged();update(); }
void Canvas::selectPoint(int index) {
    const int next=state_.vanishingPoints.isEmpty()?-1:qBound(0,index,state_.vanishingPoints.size()-1);
    if (selectedPointIndex_==next) return;
    selectedPointIndex_=next;emit selectedPointChanged(next);update();
}
void Canvas::addVanishingPoint() {
    finish();DrawingState before=state_;VanishingPoint point;
    point.id=QStringLiteral("vp-")+QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (state_.vanishingPoints.isEmpty()) point.position=QPointF(state_.image.width()/2.0,state_.horizonY);
    else {
        const auto &source=state_.vanishingPoints[qBound(0,selectedPointIndex_,state_.vanishingPoints.size()-1)];
        point.position=QPointF(state_.image.width()-source.position.x(),state_.horizonY);
        point.attachmentType=QStringLiteral("construction");point.attachmentTargetId=QStringLiteral("horizon");
        static const QColor colors[]{QColor("#d06b4c"),QColor("#4b9b67"),QColor("#896ac1"),QColor("#c08a34")};
        point.color=colors[state_.vanishingPoints.size()%4];
    }
    state_.vanishingPoints.append(point);selectedPointIndex_=state_.vanishingPoints.size()-1;
    commit(before,tr("добавление точки схода"));emit selectedPointChanged(selectedPointIndex_);emit stateChanged();update();
}
void Canvas::removeSelectedVanishingPoint() {
    finish();if(selectedPointIndex_<0||selectedPointIndex_>=state_.vanishingPoints.size())return;
    DrawingState before=state_;state_.vanishingPoints.removeAt(selectedPointIndex_);
    selectedPointIndex_=state_.vanishingPoints.isEmpty()?-1:qMin(selectedPointIndex_,state_.vanishingPoints.size()-1);
    commit(before,tr("удаление точки схода"));emit selectedPointChanged(selectedPointIndex_);emit stateChanged();update();
}
void Canvas::setSelectedPointColor(QColor color) {
    if(!color.isValid()||selectedPointIndex_<0||selectedPointIndex_>=state_.vanishingPoints.size()||state_.vanishingPoints[selectedPointIndex_].color==color)return;
    state_.vanishingPoints[selectedPointIndex_].color=color;emit stateChanged();update();
}
void Canvas::setSelectedPointVisible(bool visible) {
    if(selectedPointIndex_<0||selectedPointIndex_>=state_.vanishingPoints.size()||state_.vanishingPoints[selectedPointIndex_].visible==visible)return;
    state_.vanishingPoints[selectedPointIndex_].visible=visible;emit stateChanged();update();
}
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
        const QLineF edges[] = {QLineF(paper.topLeft(),paper.topRight()),QLineF(paper.topRight(),paper.bottomRight()),
                                QLineF(paper.bottomRight(),paper.bottomLeft()),QLineF(paper.bottomLeft(),paper.topLeft())};
        const double horizonY = toView(QPointF(0, state_.horizonY)).y();
        QColor horizonColor = state_.horizonColor; horizonColor.setAlphaF(state_.horizonOpacity/100.0);
        QPen horizon(horizonColor, state_.horizonWidth); horizon.setCosmetic(true); p.setPen(horizon);
        p.drawLine(QPointF(0, horizonY), QPointF(width(), horizonY));
        for (int pointIndex=0;pointIndex<state_.vanishingPoints.size();++pointIndex) {
            const auto &point=state_.vanishingPoints[pointIndex];
            const QPointF vanishing=toView(point.position);
            if (point.visible) {
                double radius=0;
                const QPointF corners[]={paper.topLeft(),paper.topRight(),paper.bottomLeft(),paper.bottomRight(),rect().topLeft(),rect().topRight(),rect().bottomLeft(),rect().bottomRight()};
                for(const QPointF &corner:corners)radius=qMax(radius,QLineF(vanishing,corner).length());
                for (double degrees=0; degrees<360.0; degrees+=state_.rayStepDegrees) {
                    const double angle=degrees*3.14159265358979323846/180.0;
                    const QPointF direction(std::cos(angle),std::sin(angle));
                    const QPointF start=vanishing+direction*state_.rayGap;
                    const QPointF viewportEnd=vanishing+direction*(radius+2);
                    if(QLineF(vanishing,start).length()>=QLineF(vanishing,viewportEnd).length())continue;
                    const QLineF visibleRay(start,viewportEnd);QPointF intersection;
                    bool crossesCanvas=paper.contains(start);double exitDistance=-1;
                    for(const QLineF &edge:edges){
                        if(visibleRay.intersects(edge,&intersection)!=QLineF::BoundedIntersection)continue;
                        crossesCanvas=true;exitDistance=qMax(exitDistance,QPointF::dotProduct(intersection-vanishing,direction));
                    }
                    if(!crossesCanvas||exitDistance<state_.rayGap)continue;
                    const QPointF end=vanishing+direction*exitDistance;
                    QColor startColor=point.color;startColor.setAlphaF(state_.rayStartOpacity/100.0);
                    QColor endColor=point.color;endColor.setAlphaF(state_.rayEndOpacity/100.0);
                    QPen ray;
                    if(state_.rayFadeLength>0){QLinearGradient fade(start,start+direction*state_.rayFadeLength);fade.setColorAt(0,startColor);fade.setColorAt(1,endColor);ray=QPen(QBrush(fade),1);}
                    else ray=QPen(endColor,1);
                    ray.setCosmetic(true);p.setPen(ray);p.drawLine(start,end);
                }
            }
            p.setPen(QPen(point.color,pointIndex==selectedPointIndex_?3:2));p.setBrush(Qt::white);p.drawEllipse(vanishing,6,6);
            if(point.attachmentType==QStringLiteral("construction")&&point.attachmentTargetId==QStringLiteral("horizon")){
                p.setPen(Qt::NoPen);p.setBrush(point.color);p.drawEllipse(vanishing,2.5,2.5);
            }
            p.setPen(QColor("#355274"));p.drawText(vanishing+QPointF(11,-10),tr("Точка схода %1").arg(pointIndex+1));
        }
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
        const double horizonY=toView(QPointF(0,state_.horizonY)).y();
        int hit=-1;double distance=16;
        for(int i=0;i<state_.vanishingPoints.size();++i){const double candidate=QLineF(e->localPos(),toView(state_.vanishingPoints[i].position)).length();if(candidate<=distance){distance=candidate;hit=i;}}
        if(hit>=0){selectPoint(hit);before_=state_;movingPointIndex_=hit;movingPoint_=dragging_=true;return;}
        if (std::abs(e->localPos().y()-horizonY)<=8) { before_=state_;movingHorizon_=dragging_=true;return; }
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
    else if (movingPoint_&&movingPointIndex_>=0&&movingPointIndex_<state_.vanishingPoints.size()) {
        QPointF point=toImage(e->localPos());auto &vanishing=state_.vanishingPoints[movingPointIndex_];
        if(std::abs(e->localPos().y()-toView(QPointF(0,state_.horizonY)).y())<=10){point.setY(state_.horizonY);vanishing.attachmentType=QStringLiteral("construction");vanishing.attachmentTargetId=QStringLiteral("horizon");}
        else{vanishing.attachmentType.clear();vanishing.attachmentTargetId.clear();}
        vanishing.position=point;update();
    }
    else if (movingHorizon_) {
        const double nextY=toImage(e->localPos()).y(),delta=nextY-state_.horizonY;state_.horizonY=nextY;
        for(auto &point:state_.vanishingPoints)if(point.attachmentType==QStringLiteral("construction")&&point.attachmentTargetId==QStringLiteral("horizon"))point.position.ry()+=delta;
        update();
    }
    else if (!straightStroke_) { stroke(last_, hoverPoint_); last_ = hoverPoint_; }
}
void Canvas::finish() {
    if (!dragging_) return;
    if (!panning_ && (movingPoint_ ? state_.vanishingPoints != before_.vanishingPoints : movingHorizon_ ? state_.horizonY != before_.horizonY : state_.image != before_.image))
        commit(before_, movingPoint_ ? tr("точку схода") : movingHorizon_ ? tr("горизонт") : (tool_ == Eraser ? tr("ластик") : tr("штрих")));
    if (!panning_ && !movingPoint_ && !movingHorizon_ && isPaintTool()) { paintAnchor_=last_;hasPaintAnchor_=true; }
    before_ = DrawingState();
    dragging_ = panning_ = movingPoint_ = movingHorizon_ = horizonCarriesPoint_ = straightStroke_ = false;movingPointIndex_=-1;
    setCursor(tool_ == Pan ? Qt::OpenHandCursor : Qt::CrossCursor);
}
void Canvas::mouseReleaseEvent(QMouseEvent *) { finish(); }
void Canvas::wheelEvent(QWheelEvent *e) { setZoom(zoom_*std::pow(1.15, e->angleDelta().y()/120.0), e->position()); e->accept(); }
void Canvas::keyPressEvent(QKeyEvent *e) {
    if (e->key() == Qt::Key_Space) { space_ = true; setCursor(Qt::OpenHandCursor); e->accept(); }
    else if (e->key() == Qt::Key_Shift) { shiftPressed_=true;update();e->accept(); }
    else if (e->key() == Qt::Key_Control) { controlPressed_=true;update();e->accept(); }
    else if (e->key() == Qt::Key_Escape && dragging_) { if (!panning_) state_ = before_; dragging_ = panning_ = movingPoint_ = movingHorizon_ = horizonCarriesPoint_ = false;movingPointIndex_=-1; before_ = DrawingState(); update(); }
    else QWidget::keyPressEvent(e);
}
void Canvas::keyReleaseEvent(QKeyEvent *e) {
    if (e->key() == Qt::Key_Space) { space_ = false; setCursor(tool_ == Pan ? Qt::OpenHandCursor : Qt::CrossCursor); }
    else if (e->key() == Qt::Key_Shift) { shiftPressed_=false;update(); }
    else if (e->key() == Qt::Key_Control) { controlPressed_=false;update(); }
    else QWidget::keyReleaseEvent(e);
}
void Canvas::focusOutEvent(QFocusEvent *e) { finish(); space_ = shiftPressed_ = controlPressed_ = false; update(); QWidget::focusOutEvent(e); }
