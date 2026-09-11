#pragma once
#include <QImage>
#include <QPointF>
#include <QColor>
#include <QStringList>
#include <QVector>

struct VanishingPoint {
    QString id;
    QPointF position;
    QString attachmentType;
    QString attachmentTargetId;
    QColor color = QColor("#628ed1");
    bool visible = true;
    bool locked = false;

    bool operator==(const VanishingPoint &other) const {
        return id == other.id && position == other.position &&
               attachmentType == other.attachmentType && attachmentTargetId == other.attachmentTargetId &&
               color == other.color && visible == other.visible && locked == other.locked;
    }
    bool operator!=(const VanishingPoint &other) const { return !(*this == other); }
};

struct DrawingState {
    QImage image;
    QVector<VanishingPoint> vanishingPoints;
    double horizonY = 0;
    bool horizonLocked = false;
    bool gridVisible = false;
    double rayStepDegrees = 10.0;
    double rayAngleOffset = 0;
    int rayPattern = 0;
    double rayWidth = 1.0;
    int rayGap = 12;
    int rayStartOpacity = 10;
    int rayEndOpacity = 70;
    int rayFadeLength = 50;
    QColor horizonColor = QColor("#628ed1");
    int horizonOpacity = 70;
    double horizonWidth = 1.0;
    bool horizonVisible = true;
    bool axesVisible = false;
    bool markersVisible = true;
    bool symmetricPoints = false;
};

struct DrawingHistory {
    QVector<DrawingState> states;
    QStringList labels;
    int index = 0;
};

namespace Project {
bool validSize(QSize size);
bool save(const QString &path, const DrawingState &state, QString *error);
bool save(const QString &path, const DrawingHistory &history, QString *error);
bool load(const QString &path, DrawingState *state, QString *error);
bool load(const QString &path, DrawingHistory *history, QString *error);
bool loadPng(const QString &path, QImage *image, QString *error);
bool exportPng(const QString &path, const QImage &image, QString *error);
}
