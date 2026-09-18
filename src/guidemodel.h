#pragma once

#include <QHash>
#include <QPointF>
#include <QString>
#include <QVector>
#include <limits>

/// Перечисляет геометрию прямолинейных направляющих первого варианта.
enum class GuideType { Horizontal, Vertical, Perspective };

/// Представляет устойчивый идентификатор направляющей внутри документа.
using GuideId = QString;

/// Хранит документную геометрию одной направляющей без параметров отображения и временного выбора.
struct Guide {
    /// Устойчиво связывает объект между историей, сохранением и пользовательскими командами.
    GuideId id;
    /// Определяет допустимую геометрию и способ редактирования направляющей.
    GuideType type = GuideType::Horizontal;
    /// Хранит Y горизонтальной либо X вертикальной направляющей в координатах документа.
    double position = 0;
    /// Ссылается на начало перспективного луча; для обычных направляющих остаётся пустым.
    QString vanishingPointId;
    /// Задаёт направление перспективного луча в радианах от положительной оси X.
    double angleRadians = 0;

    /// Сравнивает устойчивую геометрию двух направляющих для истории и проверки проекта.
    bool operator==(const Guide &other) const {
        return id == other.id && type == other.type && qFuzzyCompare(position + 1, other.position + 1) &&
               vanishingPointId == other.vanishingPointId &&
               qFuzzyCompare(angleRadians + 1, other.angleRadians + 1);
    }
    /// Проверяет наличие отличий в идентификаторе, типе или геометрии.
    bool operator!=(const Guide &other) const { return !(*this == other); }
};

/// Описывает ближайшую точку и касательное направление общей траектории.
struct GuideProjection {
    QPointF point;
    QPointF direction;
    double distance = std::numeric_limits<double>::infinity();
    bool valid = false;
};

namespace GuideGeometry {
/// Проецирует точку документа на направляющую, ограничивая перспективную геометрию одним лучом.
GuideProjection project(const Guide &guide,
                        QPointF sample,
                        const QHash<QString, QPointF> &vanishingPoints = {});
/// Находит ближайшую направляющую в заданном радиусе через единый контракт всех поддержанных типов.
int nearest(const QVector<Guide> &guides,
            QPointF sample,
            const QHash<QString, QPointF> &vanishingPoints,
            double maximumDistance,
            GuideProjection *projection = nullptr);
} // namespace GuideGeometry
