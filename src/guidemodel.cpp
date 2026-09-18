#include "guidemodel.h"

#include <QLineF>
#include <cmath>

GuideProjection GuideGeometry::project(const Guide &guide,
                                       QPointF sample,
                                       const QHash<QString, QPointF> &vanishingPoints) {
    GuideProjection result;
    if (guide.type == GuideType::Horizontal) {
        result.point = QPointF(sample.x(), guide.position);
        result.direction = QPointF(1, 0);
    } else if (guide.type == GuideType::Vertical) {
        result.point = QPointF(guide.position, sample.y());
        result.direction = QPointF(0, 1);
    } else {
        const auto origin = vanishingPoints.constFind(guide.vanishingPointId);
        if (origin == vanishingPoints.cend() || !std::isfinite(guide.angleRadians))
            return result;
        const QPointF direction(std::cos(guide.angleRadians), std::sin(guide.angleRadians));
        const double along = qMax(0.0, QPointF::dotProduct(sample - origin.value(), direction));
        result.point = origin.value() + direction * along;
        result.direction = direction;
    }
    result.distance = QLineF(sample, result.point).length();
    result.valid = std::isfinite(result.distance);
    return result;
}

int GuideGeometry::nearest(const QVector<Guide> &guides,
                           QPointF sample,
                           const QHash<QString, QPointF> &vanishingPoints,
                           double maximumDistance,
                           GuideProjection *projection) {
    int nearestIndex = -1;
    GuideProjection nearestProjection;
    nearestProjection.distance = maximumDistance;
    for (int index = 0; index < guides.size(); ++index) {
        const GuideProjection candidate = project(guides[index], sample, vanishingPoints);
        if (!candidate.valid || candidate.distance > nearestProjection.distance)
            continue;
        nearestIndex = index;
        nearestProjection = candidate;
    }
    if (projection)
        *projection = nearestProjection;
    return nearestIndex;
}
