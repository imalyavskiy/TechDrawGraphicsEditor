#pragma once

#include <QHash>
#include <QPointF>
#include <QString>
#include <QVector>
#include <limits>

enum class GuideType { Horizontal, Vertical, Perspective };

using GuideId = QString;

struct Guide {
    GuideId id;
    GuideType type = GuideType::Horizontal;
    double position = 0;
    QString vanishingPointId;
    double angleRadians = 0;

    bool operator==(const Guide &other) const {
        return id == other.id && type == other.type && qFuzzyCompare(position + 1, other.position + 1) &&
               vanishingPointId == other.vanishingPointId &&
               qFuzzyCompare(angleRadians + 1, other.angleRadians + 1);
    }
    bool operator!=(const Guide &other) const { return !(*this == other); }
};

struct GuideProjection {
    QPointF point;
    QPointF direction;
    double distance = std::numeric_limits<double>::infinity();
    bool valid = false;
};

namespace GuideGeometry {
GuideProjection project(const Guide &guide,
                        QPointF sample,
                        const QHash<QString, QPointF> &vanishingPoints = {});
int nearest(const QVector<Guide> &guides,
            QPointF sample,
            const QHash<QString, QPointF> &vanishingPoints,
            double maximumDistance,
            GuideProjection *projection = nullptr);
} // namespace GuideGeometry
