#pragma once
#include <QImage>
#include <QPointF>
#include <QColor>
#include <QStringList>
#include <QVector>

struct VanishingPoint {
    VanishingPoint() = default;
    VanishingPoint(const QString &pointId,QPointF pointPosition,const QString &type=QString(),const QString &targetId=QString())
        : id(pointId),position(pointPosition) { if(type==QStringLiteral("construction")&&!targetId.isEmpty())attachmentTargetIds.append(targetId); }
    QString id;
    QPointF position;
    QStringList attachmentTargetIds;
    QColor color = QColor("#628ed1");
    bool visible = true;
    bool locked = false;
    QString name;

    bool isAttachedTo(const QString &targetId) const { return attachmentTargetIds.contains(targetId); }

    bool operator==(const VanishingPoint &other) const {
        return id == other.id && position == other.position &&
               attachmentTargetIds == other.attachmentTargetIds &&
               color == other.color && visible == other.visible && locked == other.locked && name == other.name;
    }
    bool operator!=(const VanishingPoint &other) const { return !(*this == other); }
};

struct DrawingState {
    QImage image;
    QVector<VanishingPoint> vanishingPoints;
    double horizonY = 0;
    bool horizonLocked = false;
    double verticalX = 0;
    bool verticalLocked = false;
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
    QColor verticalColor = QColor("#9b6bc0");
    int verticalOpacity = 70;
    double verticalWidth = 1.0;
    bool verticalVisible = true;
    bool axesVisible = false;
    bool markersVisible = true;
    bool horizonSymmetry = false;
    bool verticalSymmetry = false;
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
