#include "canvas.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QUuid>
#include <cmath>
#include <limits>

namespace {
constexpr int rulerSize=28;
double niceStep(double minimum){
    if(!std::isfinite(minimum)||minimum<=0)return 1;
    const double power=std::pow(10.0,std::floor(std::log10(minimum))),scaled=minimum/power;double nice=1;if(scaled>5)nice=10;else if(scaled>2)nice=5;else if(scaled>1)nice=2;return nice*power;
}
QString coordinateLabel(double value){if(std::abs(value)<0.0001)value=0;return std::abs(value)>=1000?QString::number(value,'g',4):QString::number(value,'f',std::abs(value)<10?1:0);}
void copyPerspectiveAppearance(const DrawingState &source, DrawingState *target) {
    target->gridVisible=source.gridVisible;target->rayStepDegrees=source.rayStepDegrees;target->rayAngleOffset=source.rayAngleOffset;target->rayPattern=source.rayPattern;target->rayWidth=source.rayWidth;
    target->rayGap=source.rayGap;target->rayStartOpacity=source.rayStartOpacity;target->rayEndOpacity=source.rayEndOpacity;target->rayFadeLength=source.rayFadeLength;
    target->horizonColor=source.horizonColor;target->horizonOpacity=source.horizonOpacity;target->horizonWidth=source.horizonWidth;target->horizonVisible=source.horizonVisible;
    target->verticalColor=source.verticalColor;target->verticalOpacity=source.verticalOpacity;target->verticalWidth=source.verticalWidth;target->verticalVisible=source.verticalVisible;target->axesVisible=source.axesVisible;target->markersVisible=source.markersVisible;target->horizonSymmetry=source.horizonSymmetry;target->verticalSymmetry=source.verticalSymmetry;
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
    initial.verticalX = 500;
    initial.vanishingPoints.append({QStringLiteral("vp-1"), QPointF(650, 240), QStringLiteral("construction"), QStringLiteral("horizon")});
    initial.vanishingPoints.last().name=tr("Точка схода 1");
    setDocument(initial);
}

void Canvas::setDocument(const DrawingState &state, bool clean) {
    DrawingHistory history;
    history.states.append(state);
    setDocument(history,clean);
}

void Canvas::setDocument(const DrawingHistory &history, bool clean) {
    dragging_ = panning_ = movingPoint_ = movingHorizon_ = movingVertical_ = horizonCarriesPoint_ = false;
    movingPointIndex_ = movingSymmetricPointIndex_ = -1;
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
void Canvas::setRayAngleOffset(double degrees) { if(!std::isfinite(degrees)||degrees<-180||degrees>180||qFuzzyCompare(state_.rayAngleOffset+181,degrees+181))return;state_.rayAngleOffset=degrees;emit stateChanged();update(); }
void Canvas::setRayPattern(int pattern) { pattern=qBound(0,pattern,3);if(state_.rayPattern==pattern)return;state_.rayPattern=pattern;emit stateChanged();update(); }
void Canvas::setRayWidth(double width) { if(!std::isfinite(width)||width<0.1||width>20||qFuzzyCompare(state_.rayWidth,width))return;state_.rayWidth=width;emit stateChanged();update(); }
void Canvas::setRayGap(int gap) { if (state_.rayGap == gap) return;state_.rayGap=gap;emit stateChanged();update(); }
void Canvas::setRayStartOpacity(int opacity) { if (state_.rayStartOpacity == opacity) return;state_.rayStartOpacity=opacity;emit stateChanged();update(); }
void Canvas::setRayEndOpacity(int opacity) { if (state_.rayEndOpacity == opacity) return;state_.rayEndOpacity=opacity;emit stateChanged();update(); }
void Canvas::setRayFadeLength(int length) { if (state_.rayFadeLength == length) return;state_.rayFadeLength=length;emit stateChanged();update(); }
void Canvas::setRayAppearance(double stepDegrees, int gap, int startOpacity, int endOpacity, int fadeLength, int pattern) {
    if (qFuzzyCompare(state_.rayStepDegrees,stepDegrees)&&state_.rayGap==gap&&state_.rayStartOpacity==startOpacity&&state_.rayEndOpacity==endOpacity&&state_.rayFadeLength==fadeLength&&state_.rayPattern==pattern) return;
    state_.rayStepDegrees=stepDegrees;state_.rayGap=gap;state_.rayStartOpacity=startOpacity;state_.rayEndOpacity=endOpacity;state_.rayFadeLength=fadeLength;state_.rayPattern=qBound(0,pattern,3);emit stateChanged();update();
}
void Canvas::setHorizonColor(QColor color) { if (!color.isValid()||state_.horizonColor==color) return;state_.horizonColor=color;emit stateChanged();update(); }
void Canvas::setHorizonOpacity(int opacity) { if (state_.horizonOpacity==opacity) return;state_.horizonOpacity=opacity;emit stateChanged();update(); }
void Canvas::setHorizonWidth(double width) { if (qFuzzyCompare(state_.horizonWidth,width)) return;state_.horizonWidth=width;emit stateChanged();update(); }
void Canvas::setHorizonY(double imageY) {
    if(!std::isfinite(imageY)||std::abs(imageY)>1000000||qFuzzyCompare(state_.horizonY+1,imageY+1))return;
    finish();DrawingState before=state_;const double delta=imageY-state_.horizonY;state_.horizonY=imageY;
    for(auto &point:state_.vanishingPoints)if(point.attachmentType==QStringLiteral("construction")&&point.attachmentTargetId==QStringLiteral("horizon"))point.position.ry()+=delta;
    if(state_.verticalSymmetry&&attachedPointIndices(QStringLiteral("vertical")).size()>=2)for(auto &point:state_.vanishingPoints)if(point.attachmentType==QStringLiteral("construction")&&point.attachmentTargetId==QStringLiteral("vertical"))point.position.ry()+=delta;
    commit(before,tr("положение горизонта"));emit stateChanged();update();
}
void Canvas::setHorizonLocked(bool locked){if(state_.horizonLocked==locked)return;finish();DrawingState before=state_;state_.horizonLocked=locked;commit(before,tr("фиксацию горизонта"));emit stateChanged();update();}
void Canvas::setHorizonVisible(bool visible){if(state_.horizonVisible==visible)return;state_.horizonVisible=visible;emit stateChanged();update();}
void Canvas::setVerticalColor(QColor color){if(!color.isValid()||state_.verticalColor==color)return;state_.verticalColor=color;emit stateChanged();update();}
void Canvas::setVerticalOpacity(int opacity){if(state_.verticalOpacity==opacity)return;state_.verticalOpacity=opacity;emit stateChanged();update();}
void Canvas::setVerticalWidth(double width){if(qFuzzyCompare(state_.verticalWidth,width))return;state_.verticalWidth=width;emit stateChanged();update();}
void Canvas::setVerticalX(double imageX){
    if(!std::isfinite(imageX)||std::abs(imageX)>1000000||qFuzzyCompare(state_.verticalX+1,imageX+1))return;
    finish();DrawingState before=state_;const double delta=imageX-state_.verticalX;state_.verticalX=imageX;
    for(auto &point:state_.vanishingPoints)if(point.attachmentType==QStringLiteral("construction")&&point.attachmentTargetId==QStringLiteral("vertical"))point.position.rx()+=delta;
    if(state_.horizonSymmetry&&attachedPointIndices(QStringLiteral("horizon")).size()>=2)for(auto &point:state_.vanishingPoints)if(point.attachmentType==QStringLiteral("construction")&&point.attachmentTargetId==QStringLiteral("horizon"))point.position.rx()+=delta;
    commit(before,tr("положение главной вертикали"));emit stateChanged();update();
}
void Canvas::setVerticalLocked(bool locked){if(state_.verticalLocked==locked)return;finish();DrawingState before=state_;state_.verticalLocked=locked;commit(before,tr("фиксацию главной вертикали"));emit stateChanged();update();}
void Canvas::setVerticalVisible(bool visible){if(state_.verticalVisible==visible)return;state_.verticalVisible=visible;emit stateChanged();update();}
void Canvas::setAxesVisible(bool visible){if(state_.axesVisible==visible)return;state_.axesVisible=visible;emit stateChanged();update();}
void Canvas::setMarkersVisible(bool visible){if(state_.markersVisible==visible)return;state_.markersVisible=visible;emit stateChanged();update();}
void Canvas::setHorizonSymmetry(bool enabled){if(state_.horizonSymmetry==enabled)return;state_.horizonSymmetry=enabled;emit stateChanged();update();}
void Canvas::setVerticalSymmetry(bool enabled){if(state_.verticalSymmetry==enabled)return;state_.verticalSymmetry=enabled;emit stateChanged();update();}
void Canvas::setRulerPercent(bool percent){if(rulerPercent_==percent)return;rulerPercent_=percent;update();}
void Canvas::selectPoint(int index) {
    const int next=state_.vanishingPoints.isEmpty()?-1:qBound(0,index,state_.vanishingPoints.size()-1);
    if (selectedPointIndex_==next) return;
    selectedPointIndex_=next;emit selectedPointChanged(next);update();
}
void Canvas::addVanishingPoint() {
    finish();if(state_.vanishingPoints.size()>=32)return;DrawingState before=state_;VanishingPoint point;
    point.id=QStringLiteral("vp-")+QUuid::createUuid().toString(QUuid::WithoutBraces);
    point.position=QPointF(state_.image.width()/2.0,state_.image.height()/2.0);constexpr double placementStep=40.0;
    auto occupied=[this](QPointF candidate){for(const auto &existing:state_.vanishingPoints)if(QLineF(candidate,existing.position).length()<1.0)return true;return false;};while(occupied(point.position))point.position.rx()-=placementStep;
    static const QColor colors[]{QColor("#628ed1"),QColor("#d06b4c"),QColor("#4b9b67"),QColor("#896ac1"),QColor("#c08a34")};point.color=colors[state_.vanishingPoints.size()%5];point.name=tr("Точка схода %1").arg(state_.vanishingPoints.size()+1);state_.vanishingPoints.append(point);selectedPointIndex_=state_.vanishingPoints.size()-1;
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
void Canvas::setSelectedPointName(const QString &name) {
    if(selectedPointIndex_<0||selectedPointIndex_>=state_.vanishingPoints.size()||name.size()>120||state_.vanishingPoints[selectedPointIndex_].name==name)return;
    finish();DrawingState before=state_;state_.vanishingPoints[selectedPointIndex_].name=name;commit(before,tr("название точки схода"));emit stateChanged();update();
}
void Canvas::setSelectedPointVisible(bool visible) {
    if(selectedPointIndex_<0||selectedPointIndex_>=state_.vanishingPoints.size()||state_.vanishingPoints[selectedPointIndex_].visible==visible)return;
    state_.vanishingPoints[selectedPointIndex_].visible=visible;emit stateChanged();update();
}
void Canvas::setSelectedPointPosition(QPointF position) {
    if(selectedPointIndex_<0||selectedPointIndex_>=state_.vanishingPoints.size()||!std::isfinite(position.x())||!std::isfinite(position.y())||std::abs(position.x())>1000000||std::abs(position.y())>1000000)return;
    auto &point=state_.vanishingPoints[selectedPointIndex_];if(point.attachmentType==QStringLiteral("construction")&&point.attachmentTargetId==QStringLiteral("horizon"))position.setY(state_.horizonY);else if(point.attachmentType==QStringLiteral("construction")&&point.attachmentTargetId==QStringLiteral("vertical"))position.setX(state_.verticalX);
    if(point.position==position)return;
    finish();DrawingState before=state_;state_.vanishingPoints[selectedPointIndex_].position=position;updateSymmetricPoint(selectedPointIndex_);commit(before,tr("координаты точки схода"));emit stateChanged();update();
}
int Canvas::symmetricPartnerIndex(int movedIndex) const{
    if(movedIndex<0||movedIndex>=state_.vanishingPoints.size())return -1;
    const auto &moved=state_.vanishingPoints[movedIndex];
    if(moved.attachmentType!=QStringLiteral("construction"))return -1;
    const QString target=moved.attachmentTargetId;
    if((target==QStringLiteral("horizon")&&!state_.horizonSymmetry)||(target==QStringLiteral("vertical")&&!state_.verticalSymmetry))return -1;
    const auto points=attachedPointIndices(target);if(points.size()<2)return -1;
    const QPointF reflected=target==QStringLiteral("horizon")?QPointF(2*state_.verticalX-moved.position.x(),state_.horizonY):QPointF(state_.verticalX,2*state_.horizonY-moved.position.y());
    int nearest=-1;double nearestDistance=std::numeric_limits<double>::max();for(const int index:points){if(index==movedIndex)continue;const QPointF delta=state_.vanishingPoints[index].position-reflected;const double distance=QPointF::dotProduct(delta,delta);if(distance<nearestDistance){nearestDistance=distance;nearest=index;}}
    return nearest;
}
void Canvas::updateSymmetricPoint(int movedIndex,int partnerIndex){
    if(movedIndex<0||movedIndex>=state_.vanishingPoints.size())return;
    if(partnerIndex<0)partnerIndex=symmetricPartnerIndex(movedIndex);
    if(partnerIndex<0||partnerIndex>=state_.vanishingPoints.size()||partnerIndex==movedIndex)return;
    const auto &moved=state_.vanishingPoints[movedIndex];auto &other=state_.vanishingPoints[partnerIndex];if(moved.attachmentType!=QStringLiteral("construction")||other.attachmentType!=QStringLiteral("construction")||moved.attachmentTargetId!=other.attachmentTargetId)return;
    if(moved.attachmentTargetId==QStringLiteral("horizon")&&state_.horizonSymmetry)other.position=QPointF(2*state_.verticalX-moved.position.x(),state_.horizonY);
    else if(moved.attachmentTargetId==QStringLiteral("vertical")&&state_.verticalSymmetry)other.position=QPointF(state_.verticalX,2*state_.horizonY-moved.position.y());
}
QVector<int> Canvas::attachedPointIndices(const QString &targetId) const{QVector<int> result;for(int i=0;i<state_.vanishingPoints.size();++i){const auto &point=state_.vanishingPoints[i];if(point.attachmentType==QStringLiteral("construction")&&point.attachmentTargetId==targetId)result.append(i);}return result;}
void Canvas::setSelectedPointAttachedToHorizon(bool attached) {
    setSelectedPointAttachment(attached?QStringLiteral("horizon"):QString());
}
void Canvas::setSelectedPointAttachment(const QString &targetId) {
    if(selectedPointIndex_<0||selectedPointIndex_>=state_.vanishingPoints.size())return;
    auto &point=state_.vanishingPoints[selectedPointIndex_];
    const QString target=targetId==QStringLiteral("horizon")||targetId==QStringLiteral("vertical")?targetId:QString();
    const QString current=point.attachmentType==QStringLiteral("construction")?point.attachmentTargetId:QString();if(current==target)return;
    finish();DrawingState before=state_;
    if(!target.isEmpty()){point.attachmentType=QStringLiteral("construction");point.attachmentTargetId=target;if(target==QStringLiteral("horizon"))point.position.setY(state_.horizonY);else point.position.setX(state_.verticalX);updateSymmetricPoint(selectedPointIndex_);}
    else{point.attachmentType.clear();point.attachmentTargetId.clear();}
    commit(before,tr("привязку точки схода"));emit stateChanged();update();
}
void Canvas::setSelectedPointLocked(bool locked){
    if(selectedPointIndex_<0||selectedPointIndex_>=state_.vanishingPoints.size()||state_.vanishingPoints[selectedPointIndex_].locked==locked)return;
    finish();DrawingState before=state_;state_.vanishingPoints[selectedPointIndex_].locked=locked;commit(before,tr("фиксацию точки схода"));emit stateChanged();update();
}
QRectF Canvas::viewportRect() const{return QRectF(rect()).adjusted(rulerSize,rulerSize,-rulerSize,-rulerSize);}
QPointF Canvas::toImage(QPointF p) const { return (p - viewportRect().center() - pan_) / zoom_ + QPointF(state_.image.width()/2.0, state_.image.height()/2.0); }
QPointF Canvas::toView(QPointF p) const { return (p - QPointF(state_.image.width()/2.0, state_.image.height()/2.0))*zoom_ + viewportRect().center() + pan_; }
void Canvas::setZoom(double zoom, QPointF anchor) {
    if (anchor.x() < 0) anchor = viewportRect().center();
    const QPointF imagePoint = toImage(anchor);
    zoom_ = qBound(0.05, zoom, 16.0);
    pan_ += anchor - toView(imagePoint);
    emit viewChanged(); update();
}
void Canvas::fit() { pan_ = QPointF();const QRectF viewport=viewportRect();zoom_ = qBound(0.05, qMin((viewport.width()-60.0)/state_.image.width(), (viewport.height()-60.0)/state_.image.height()), 16.0); emit viewChanged(); update(); }

void Canvas::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.fillRect(rect(), QColor("#dce0e5"));
    p.save();p.setClipRect(viewportRect());
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
        if(state_.axesVisible){QPen axis(QColor(70,80,92,95),1,Qt::DashLine);axis.setCosmetic(true);p.setPen(axis);const QPointF center=toView(QPointF(state_.image.width()/2.0,state_.image.height()/2.0));p.drawLine(QPointF(center.x(),0),QPointF(center.x(),height()));p.drawLine(QPointF(0,center.y()),QPointF(width(),center.y()));}
        const double horizonY = toView(QPointF(0, state_.horizonY)).y();
        QColor horizonColor = state_.horizonColor; horizonColor.setAlphaF(state_.horizonOpacity/100.0);
        QPen horizon(horizonColor, state_.horizonWidth); horizon.setCosmetic(true); p.setPen(horizon);
        if(state_.horizonVisible)p.drawLine(QPointF(0, horizonY), QPointF(width(), horizonY));
        const double verticalX=toView(QPointF(state_.verticalX,0)).x();QColor verticalColor=state_.verticalColor;verticalColor.setAlphaF(state_.verticalOpacity/100.0);
        QPen vertical(verticalColor,state_.verticalWidth);vertical.setCosmetic(true);p.setPen(vertical);if(state_.verticalVisible)p.drawLine(QPointF(verticalX,0),QPointF(verticalX,height()));
        for (int pointIndex=0;pointIndex<state_.vanishingPoints.size();++pointIndex) {
            const auto &point=state_.vanishingPoints[pointIndex];
            const QPointF vanishing=toView(point.position);
            if (point.visible) {
                double radius=0;
                const QPointF corners[]={paper.topLeft(),paper.topRight(),paper.bottomLeft(),paper.bottomRight(),rect().topLeft(),rect().topRight(),rect().bottomLeft(),rect().bottomRight()};
                for(const QPointF &corner:corners)radius=qMax(radius,QLineF(vanishing,corner).length());
                for (double degrees=state_.rayAngleOffset; degrees<360.0+state_.rayAngleOffset; degrees+=state_.rayStepDegrees) {
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
                    if(state_.rayFadeLength>0){QLinearGradient fade(start,start+direction*state_.rayFadeLength);fade.setColorAt(0,startColor);fade.setColorAt(1,endColor);ray=QPen(QBrush(fade),state_.rayWidth);}
                    else ray=QPen(endColor,state_.rayWidth);
                    static const Qt::PenStyle styles[]{Qt::SolidLine,Qt::DashLine,Qt::DotLine,Qt::DashDotLine};ray.setStyle(styles[qBound(0,state_.rayPattern,3)]);ray.setCosmetic(true);p.setPen(ray);p.drawLine(start,end);
                }
            }
            if(state_.markersVisible){
                p.setPen(QPen(point.color,pointIndex==selectedPointIndex_?3:2));p.setBrush(Qt::white);p.drawEllipse(vanishing,6,6);
                if(point.attachmentType==QStringLiteral("construction")&&(point.attachmentTargetId==QStringLiteral("horizon")||point.attachmentTargetId==QStringLiteral("vertical"))){p.setPen(Qt::NoPen);p.setBrush(point.color);p.drawEllipse(vanishing,2.5,2.5);}
                p.setPen(QColor("#355274"));p.drawText(vanishing+QPointF(11,-10),tr("Точка схода %1").arg(pointIndex+1));
            }
        }
    }
    if (isPaintTool() && hasPaintAnchor_ && hasHoverPoint_ && shiftPressed_ && !dragging_) {
        const QPointF endpoint=constrainedPoint(hoverPoint_,controlPressed_);
        p.save();p.setClipRect(paper);p.setRenderHint(QPainter::Antialiasing);
        QPen preview(QColor(40,52,68,170),1,Qt::DashLine);preview.setCosmetic(true);p.setPen(preview);
        p.drawLine(toView(paintAnchor_),toView(endpoint));p.restore();
    }
    p.restore();drawRulers(p);
}

void Canvas::drawRulers(QPainter &p){
    const QRectF viewport=viewportRect();const QColor rulerBackground("#f4f5f7"),border("#9da4ae"),ink("#4c5664");
    p.fillRect(QRectF(0,0,width(),rulerSize),rulerBackground);p.fillRect(QRectF(0,height()-rulerSize,width(),rulerSize),rulerBackground);p.fillRect(QRectF(0,rulerSize,rulerSize,height()-2*rulerSize),rulerBackground);p.fillRect(QRectF(width()-rulerSize,rulerSize,rulerSize,height()-2*rulerSize),rulerBackground);
    p.setPen(QPen(border,1));p.drawRect(viewport);
    QFont font=p.font();font.setPixelSize(9);p.setFont(font);p.setPen(ink);
    const double horizontalUnitPixels=rulerPercent_?state_.image.width()/100.0:1.0;
    const double verticalUnitPixels=rulerPercent_?state_.image.height()/100.0:1.0;
    const double horizontalStep=niceStep(72.0/(zoom_*horizontalUnitPixels));
    const double verticalStep=niceStep(54.0/(zoom_*verticalUnitPixels));
    const double leftValue=(toImage(viewport.topLeft()).x()-state_.image.width()/2.0)/horizontalUnitPixels;
    const double rightValue=(toImage(viewport.topRight()).x()-state_.image.width()/2.0)/horizontalUnitPixels;
    for(double value=std::ceil(qMin(leftValue,rightValue)/horizontalStep)*horizontalStep;value<=qMax(leftValue,rightValue)+horizontalStep*0.01;value+=horizontalStep){
        const double x=toView(QPointF(state_.image.width()/2.0+value*horizontalUnitPixels,0)).x();if(x<viewport.left()-1||x>viewport.right()+1)continue;
        p.drawLine(QPointF(x,viewport.top()),QPointF(x,viewport.top()-7));p.drawLine(QPointF(x,viewport.bottom()),QPointF(x,viewport.bottom()+7));const QString label=coordinateLabel(value);p.drawText(QRectF(x+2,2,70,rulerSize-9),Qt::AlignLeft|Qt::AlignVCenter,label);p.drawText(QRectF(x+2,height()-rulerSize+7,70,rulerSize-9),Qt::AlignLeft|Qt::AlignVCenter,label);
    }
    const double topValue=(state_.image.height()/2.0-toImage(viewport.topLeft()).y())/verticalUnitPixels;
    const double bottomValue=(state_.image.height()/2.0-toImage(viewport.bottomLeft()).y())/verticalUnitPixels;
    for(double value=std::ceil(qMin(topValue,bottomValue)/verticalStep)*verticalStep;value<=qMax(topValue,bottomValue)+verticalStep*0.01;value+=verticalStep){
        const double y=toView(QPointF(0,state_.image.height()/2.0-value*verticalUnitPixels)).y();if(y<viewport.top()-1||y>viewport.bottom()+1)continue;
        p.drawLine(QPointF(viewport.left(),y),QPointF(viewport.left()-7,y));p.drawLine(QPointF(viewport.right(),y),QPointF(viewport.right()+7,y));const QString label=coordinateLabel(value);p.drawText(QRectF(1,y-8,rulerSize-9,16),Qt::AlignRight|Qt::AlignVCenter,label);p.drawText(QRectF(width()-rulerSize+8,y-8,rulerSize-9,16),Qt::AlignLeft|Qt::AlignVCenter,label);
    }
    if(!cursorInViewport_)return;
    QPen guide(QColor(49,83,130,170),1,Qt::DashLine);guide.setCosmetic(true);p.setPen(guide);p.drawLine(cursorView_,QPointF(cursorView_.x(),viewport.top()));p.drawLine(cursorView_,QPointF(cursorView_.x(),viewport.bottom()));p.drawLine(cursorView_,QPointF(viewport.left(),cursorView_.y()));p.drawLine(cursorView_,QPointF(viewport.right(),cursorView_.y()));
    const QPointF image=toImage(cursorView_);const QString suffix=rulerPercent_?QStringLiteral("%"):QStringLiteral(" px");const QString xLabel=coordinateLabel((image.x()-state_.image.width()/2.0)/horizontalUnitPixels)+suffix;const QString yLabel=coordinateLabel((state_.image.height()/2.0-image.y())/verticalUnitPixels)+suffix;
    p.setPen(QColor("#23405f"));p.setBrush(QColor("#fff4b5"));const int xWidth=qMax(44,p.fontMetrics().horizontalAdvance(xLabel)+8),yWidth=qMax(44,p.fontMetrics().horizontalAdvance(yLabel)+8);
    QRectF topBox(cursorView_.x()-xWidth/2.0,2,xWidth,rulerSize-5),bottomBox(cursorView_.x()-xWidth/2.0,height()-rulerSize+3,xWidth,rulerSize-5);
    p.drawRect(topBox);p.drawRect(bottomBox);p.drawText(topBox,Qt::AlignCenter,xLabel);p.drawText(bottomBox,Qt::AlignCenter,xLabel);
    p.save();p.setClipRect(QRectF(0,rulerSize,rulerSize,height()-2*rulerSize));p.drawRect(QRectF(1,cursorView_.y()-9,yWidth,18));p.drawText(QRectF(2,cursorView_.y()-9,yWidth-2,18),Qt::AlignCenter,yLabel);p.restore();
    p.save();p.setClipRect(QRectF(width()-rulerSize,rulerSize,rulerSize,height()-2*rulerSize));p.drawRect(QRectF(width()-yWidth-1,cursorView_.y()-9,yWidth,18));p.drawText(QRectF(width()-yWidth,cursorView_.y()-9,yWidth-2,18),Qt::AlignCenter,yLabel);p.restore();
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
int Canvas::perspectiveHit(QPointF viewPosition) const{
    if(!state_.gridVisible||!viewportRect().contains(viewPosition))return NoPerspectiveHit;
    int pointHit=NoPerspectiveHit;double distance=16;
    if(state_.markersVisible)for(int i=0;i<state_.vanishingPoints.size();++i){const double candidate=QLineF(viewPosition,toView(state_.vanishingPoints[i].position)).length();if(candidate<=distance){distance=candidate;pointHit=i;}}
    if(pointHit>=0)return pointHit;
    if(state_.verticalVisible&&std::abs(viewPosition.x()-toView(QPointF(state_.verticalX,0)).x())<=8)return VerticalHit;
    if(state_.horizonVisible&&std::abs(viewPosition.y()-toView(QPointF(0,state_.horizonY)).y())<=8)return HorizonHit;
    return NoPerspectiveHit;
}
void Canvas::updatePerspectiveCursor(QPointF viewPosition){
    if(tool_!=Perspective||dragging_)return;
    const int hit=perspectiveHit(viewPosition);bool movable=false;
    if(hit>=0)movable=!state_.vanishingPoints[hit].locked;else if(hit==VerticalHit)movable=!state_.verticalLocked;else if(hit==HorizonHit)movable=!state_.horizonLocked;
    setCursor(movable?Qt::OpenHandCursor:hit!=NoPerspectiveHit?Qt::ForbiddenCursor:Qt::CrossCursor);
}
void Canvas::mousePressEvent(QMouseEvent *e) {
    if (e->button() != Qt::LeftButton && e->button() != Qt::MiddleButton) return;
    if(e->button()==Qt::LeftButton&&!viewportRect().contains(e->localPos()))return;
    setFocus();
    panning_ = e->button() == Qt::MiddleButton || tool_ == Pan || space_;
    if (panning_) { dragging_ = true; last_ = e->localPos(); setCursor(Qt::ClosedHandCursor); return; }
    if (tool_ == Perspective) {
        const int hit=perspectiveHit(e->localPos());
        if(hit>=0){selectPoint(hit);if(state_.vanishingPoints[hit].locked)return;before_=state_;movingPointIndex_=hit;movingSymmetricPointIndex_=symmetricPartnerIndex(hit);movingPoint_=dragging_=true;setCursor(Qt::ClosedHandCursor);return;}
        if(hit==VerticalHit){if(state_.verticalLocked)return;before_=state_;movingVertical_=dragging_=true;setCursor(Qt::ClosedHandCursor);return;}
        if(hit==HorizonHit){if(state_.horizonLocked)return;before_=state_;movingHorizon_=dragging_=true;setCursor(Qt::ClosedHandCursor);return;}
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
    cursorView_=e->localPos();cursorInViewport_=viewportRect().contains(cursorView_);hoverPoint_=toImage(cursorView_);hasHoverPoint_=QRectF(QPointF(),state_.image.size()).contains(hoverPoint_);
    emit positionChanged(hoverPoint_);
    if (!dragging_) { updatePerspectiveCursor(e->localPos());update(); return; }
    if (panning_) { pan_ += e->localPos() - last_; last_ = e->localPos(); update(); }
    else if (movingPoint_&&movingPointIndex_>=0&&movingPointIndex_<state_.vanishingPoints.size()) {
        QPointF point=toImage(e->localPos());auto &vanishing=state_.vanishingPoints[movingPointIndex_];
        const double horizonDistance=std::abs(e->localPos().y()-toView(QPointF(0,state_.horizonY)).y()),verticalDistance=std::abs(e->localPos().x()-toView(QPointF(state_.verticalX,0)).x());
        QString target;const QString current=vanishing.attachmentType==QStringLiteral("construction")?vanishing.attachmentTargetId:QString();
        if(current==QStringLiteral("horizon")&&horizonDistance<=10)target=current;else if(current==QStringLiteral("vertical")&&verticalDistance<=10)target=current;else if(horizonDistance<=10||verticalDistance<=10)target=horizonDistance<=verticalDistance?QStringLiteral("horizon"):QStringLiteral("vertical");
        if(target==QStringLiteral("horizon")){point.setY(state_.horizonY);vanishing.attachmentType=QStringLiteral("construction");vanishing.attachmentTargetId=target;}
        else if(target==QStringLiteral("vertical")){point.setX(state_.verticalX);vanishing.attachmentType=QStringLiteral("construction");vanishing.attachmentTargetId=target;}
        else{vanishing.attachmentType.clear();vanishing.attachmentTargetId.clear();}
        vanishing.position=point;updateSymmetricPoint(movingPointIndex_,movingSymmetricPointIndex_);update();
    }
    else if (movingHorizon_) {
        const double nextY=toImage(e->localPos()).y(),delta=nextY-state_.horizonY;state_.horizonY=nextY;
        for(auto &point:state_.vanishingPoints)if(point.attachmentType==QStringLiteral("construction")&&point.attachmentTargetId==QStringLiteral("horizon"))point.position.ry()+=delta;
        if(state_.verticalSymmetry&&attachedPointIndices(QStringLiteral("vertical")).size()>=2)for(auto &point:state_.vanishingPoints)if(point.attachmentType==QStringLiteral("construction")&&point.attachmentTargetId==QStringLiteral("vertical"))point.position.ry()+=delta;
        update();
    }
    else if(movingVertical_){
        const double nextX=toImage(e->localPos()).x(),delta=nextX-state_.verticalX;state_.verticalX=nextX;
        for(auto &point:state_.vanishingPoints)if(point.attachmentType==QStringLiteral("construction")&&point.attachmentTargetId==QStringLiteral("vertical"))point.position.rx()+=delta;
        if(state_.horizonSymmetry&&attachedPointIndices(QStringLiteral("horizon")).size()>=2)for(auto &point:state_.vanishingPoints)if(point.attachmentType==QStringLiteral("construction")&&point.attachmentTargetId==QStringLiteral("horizon"))point.position.rx()+=delta;
        update();
    }
    else if (!straightStroke_) { stroke(last_, hoverPoint_); last_ = hoverPoint_; }
}
void Canvas::finish() {
    if (!dragging_) return;
    if (!panning_ && (movingPoint_ ? state_.vanishingPoints != before_.vanishingPoints : movingHorizon_ ? state_.horizonY != before_.horizonY : movingVertical_ ? state_.verticalX != before_.verticalX : state_.image != before_.image))
        commit(before_, movingPoint_ ? tr("точку схода") : movingHorizon_ ? tr("горизонт") : movingVertical_ ? tr("главную вертикаль") : (tool_ == Eraser ? tr("ластик") : tr("штрих")));
    if (!panning_ && !movingPoint_ && !movingHorizon_ && !movingVertical_ && isPaintTool()) { paintAnchor_=last_;hasPaintAnchor_=true; }
    before_ = DrawingState();
    dragging_ = panning_ = movingPoint_ = movingHorizon_ = movingVertical_ = horizonCarriesPoint_ = straightStroke_ = false;movingPointIndex_=movingSymmetricPointIndex_=-1;
    setCursor(tool_ == Pan ? Qt::OpenHandCursor : Qt::CrossCursor);
}
void Canvas::mouseReleaseEvent(QMouseEvent *e) { finish();updatePerspectiveCursor(e->localPos()); }
void Canvas::wheelEvent(QWheelEvent *e) { setZoom(zoom_*std::pow(1.15, e->angleDelta().y()/120.0), e->position()); e->accept(); }
void Canvas::keyPressEvent(QKeyEvent *e) {
    if (e->key() == Qt::Key_Space) { space_ = true; setCursor(Qt::OpenHandCursor); e->accept(); }
    else if (e->key() == Qt::Key_Shift) { shiftPressed_=true;update();e->accept(); }
    else if (e->key() == Qt::Key_Control) { controlPressed_=true;update();e->accept(); }
    else if (e->key() == Qt::Key_Escape && dragging_) { if (!panning_) state_ = before_; dragging_ = panning_ = movingPoint_ = movingHorizon_ = movingVertical_ = horizonCarriesPoint_ = false;movingPointIndex_=-1; before_ = DrawingState(); update(); }
    else QWidget::keyPressEvent(e);
}
void Canvas::keyReleaseEvent(QKeyEvent *e) {
    if (e->key() == Qt::Key_Space) { space_ = false; setCursor(tool_ == Pan ? Qt::OpenHandCursor : Qt::CrossCursor); }
    else if (e->key() == Qt::Key_Shift) { shiftPressed_=false;update(); }
    else if (e->key() == Qt::Key_Control) { controlPressed_=false;update(); }
    else QWidget::keyReleaseEvent(e);
}
void Canvas::focusOutEvent(QFocusEvent *e) { finish(); space_ = shiftPressed_ = controlPressed_ = false; update(); QWidget::focusOutEvent(e); }
void Canvas::leaveEvent(QEvent *e){cursorInViewport_=false;hasHoverPoint_=false;if(tool_==Perspective&&!dragging_)setCursor(Qt::CrossCursor);update();QWidget::leaveEvent(e);}
