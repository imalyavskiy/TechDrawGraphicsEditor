#pragma once
#include "project.h"
#include <QWidget>
#include <QUndoStack>

class Canvas : public QWidget {
    Q_OBJECT
public:
    enum Tool { Pencil, Brush, Eraser, Pan, Perspective };
    explicit Canvas(QWidget *parent = nullptr);
    const DrawingState &state() const { return state_; }
    QUndoStack *undoStack() { return &undo_; }
    void setDocument(const DrawingState &state, bool clean = true);
    void setDocument(const DrawingHistory &history, bool clean = true);
    DrawingHistory history() const;
    void setTool(Tool tool);
    Tool tool() const { return tool_; }
    void setFront(QColor color) { front_ = color; }
    void setBack(QColor color) { back_ = color; }
    void setStrokeWidth(int width) { width_ = width; }
    void setGridVisible(bool visible);
    void setRayStep(double degrees);
    void setRayGap(int gap);
    void setRayStartOpacity(int opacity);
    void setRayEndOpacity(int opacity);
    void setRayFadeLength(int length);
    void setRayAppearance(double stepDegrees, int gap, int startOpacity, int endOpacity, int fadeLength);
    void setHorizonColor(QColor color);
    void setHorizonOpacity(int opacity);
    void setHorizonWidth(double width);
    void setHorizonY(double imageY);
    int selectedPointIndex() const { return selectedPointIndex_; }
    void selectPoint(int index);
    void addVanishingPoint();
    void removeSelectedVanishingPoint();
    void setSelectedPointColor(QColor color);
    void setSelectedPointVisible(bool visible);
    void setSelectedPointPosition(QPointF position);
    void setSelectedPointAttachedToHorizon(bool attached);
    void setZoom(double zoom, QPointF anchor = QPointF(-1, -1));
    double zoom() const { return zoom_; }
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
private:
    DrawingState state_, before_;
    QUndoStack undo_;
    Tool tool_ = Pencil;
    QColor front_ = QColor("#2c3441"), back_ = Qt::white;
    int width_ = 3;
    double zoom_ = 1.0;
    QPointF pan_, last_, paintAnchor_, hoverPoint_;
    bool dragging_ = false, panning_ = false, movingPoint_ = false, movingHorizon_ = false, space_ = false;
    int selectedPointIndex_ = 0;
    int movingPointIndex_ = -1;
    bool horizonCarriesPoint_ = false;
    bool straightStroke_ = false, shiftPressed_ = false, controlPressed_ = false;
    bool hasPaintAnchor_ = false, hasHoverPoint_ = false;
    void stroke(QPointF a, QPointF b);
    void commit(const DrawingState &before, const QString &label);
    void finish();
    bool isPaintTool() const;
    QPointF constrainedPoint(QPointF point, bool constrainAngle) const;
};
