#pragma once
#include "project.h"
#include <QWidget>
#include <QUndoStack>

class QPainter;

class Canvas : public QWidget {
    Q_OBJECT
public:
    enum Tool { Pencil, Brush, Eraser, Pan, Perspective };
    explicit Canvas(QWidget *parent = nullptr);
    const DrawingState &state() const {
        return state_;
    }
    QUndoStack *undoStack() {
        return &undo_;
    }
    void setDocument(const DrawingState &state, bool clean = true);
    void setDocument(const DrawingHistory &history, bool clean = true);
    DrawingHistory history() const;
    void setTool(Tool tool);
    Tool tool() const {
        return tool_;
    }
    void setFront(QColor color) {
        front_ = color;
    }
    void setBack(QColor color) {
        back_ = color;
    }
    void setStrokeWidth(int width) {
        width_ = width;
    }
    void setGridVisible(bool visible);
    void setRayStep(double degrees);
    void setRayAngleOffset(double degrees);
    void setRayPattern(int pattern);
    void setRayWidth(double width);
    void setRayGap(int gap);
    void setRayStartOpacity(int opacity);
    void setRayEndOpacity(int opacity);
    void setRayFadeLength(int length);
    void
    setRayAppearance(double stepDegrees, int gap, int startOpacity, int endOpacity, int fadeLength, int pattern = 0);
    void setHorizonColor(QColor color);
    void setHorizonOpacity(int opacity);
    void setHorizonWidth(double width);
    void setHorizonY(double imageY);
    void setHorizonLocked(bool locked);
    void setHorizonVisible(bool visible);
    void setVerticalColor(QColor color);
    void setVerticalOpacity(int opacity);
    void setVerticalWidth(double width);
    void setVerticalX(double imageX);
    void setVerticalLocked(bool locked);
    void setVerticalVisible(bool visible);
    void setAxesVisible(bool visible);
    void setMarkersVisible(bool visible);
    void setHorizonSymmetry(bool enabled);
    void setVerticalSymmetry(bool enabled);
    void setRulerPercent(bool percent);
    bool rulerPercent() const {
        return rulerPercent_;
    }
    int selectedPointIndex() const {
        return selectedPointIndex_;
    }
    void selectPoint(int index);
    void addVanishingPoint();
    void removeSelectedVanishingPoint();
    void setSelectedPointColor(QColor color);
    void setSelectedPointName(const QString &name);
    void setSelectedPointVisible(bool visible);
    void setSelectedPointPosition(QPointF position);
    void setSelectedPointAttachment(const QString &targetId);
    void setSelectedPointLocked(bool locked);
    void setZoom(double zoom, QPointF anchor = QPointF(-1, -1));
    double zoom() const {
        return zoom_;
    }
    void fit();
    QPointF toImage(QPointF point) const;
    QPointF toView(QPointF point) const;
    void apply(const DrawingState &state);
signals:
    void stateChanged();
    void viewChanged();
    void positionChanged(QPointF position);
    void selectedPointChanged(int index);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void keyReleaseEvent(QKeyEvent *) override;
    void focusOutEvent(QFocusEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    DrawingState state_;
    DrawingState before_;
    QUndoStack undo_;
    Tool tool_ = Pencil;
    QColor front_ = QColor("#2c3441");
    QColor back_ = Qt::white;
    int width_ = 3;
    double zoom_ = 1.0;
    QPointF pan_;
    QPointF last_;
    QPointF paintAnchor_;
    QPointF hoverPoint_;
    bool dragging_ = false;
    bool panning_ = false;
    bool movingPoint_ = false;
    bool movingHorizon_ = false;
    bool movingVertical_ = false;
    bool space_ = false;
    int selectedPointIndex_ = 0;
    int movingPointIndex_ = -1;
    int movingSymmetricPointIndex_ = -1;
    bool straightStroke_ = false;
    bool shiftPressed_ = false;
    bool controlPressed_ = false;
    bool hasPaintAnchor_ = false;
    bool hasHoverPoint_ = false;
    bool rulerPercent_ = false;
    bool cursorInViewport_ = false;
    QPointF cursorView_;
    void stroke(QPointF start, QPointF end);
    void commit(const DrawingState &before, const QString &label);
    void finish();
    bool isPaintTool() const;
    QPointF constrainedPoint(QPointF point, bool constrainAngle) const;
    int symmetricPartnerIndex(int movedIndex) const;
    void updateSymmetricPoint(int movedIndex, int partnerIndex = -1);
    QVector<int> attachedPointIndices(const QString &targetId) const;
    void moveHorizon(double imageY);
    void moveVertical(double imageX);
    enum PerspectiveHit { NoPerspectiveHit = -1, VerticalHit = -2, HorizonHit = -3 };
    int perspectiveHit(QPointF viewPosition) const;
    void updatePerspectiveCursor(QPointF viewPosition);
    QRectF viewportRect() const;
    void drawRulers(QPainter &painter);
};
