#pragma once
#include "layermodel.h"
#include <QByteArray>
#include <QImage>
#include <QPointF>
#include <QColor>
#include <QStringList>
#include <QVector>

namespace PerspectiveTarget {
/// Возвращает тип сохраняемой привязки к элементу перспективной оснастки.
inline const QString &constructionType() {
    static const QString value = QStringLiteral("construction");
    return value;
}
/// Возвращает устойчивый идентификатор линии горизонта.
inline const QString &horizon() {
    static const QString value = QStringLiteral("horizon");
    return value;
}
/// Возвращает устойчивый идентификатор главной вертикали.
inline const QString &vertical() {
    static const QString value = QStringLiteral("vertical");
    return value;
}
/// Возвращает составной идентификатор пересечения обеих осей для элементов интерфейса.
inline const QString &intersection() {
    static const QString value = QStringLiteral("intersection");
    return value;
}
} // namespace PerspectiveTarget

/// Описывает одну точку схода, её геометрию, привязки и параметры отображения.
struct VanishingPoint {
    /// Создаёт пустую точку для последующего заполнения при чтении проекта.
    VanishingPoint() = default;
    /// Создаёт точку и при необходимости добавляет одну совместимую привязку старого формата.
    VanishingPoint(const QString &pointId,
                   QPointF pointPosition,
                   const QString &type = QString(),
                   const QString &targetId = QString())
        : id(pointId), position(pointPosition) {
        if (type == PerspectiveTarget::constructionType() && !targetId.isEmpty())
            attachmentTargetIds.append(targetId);
    }
    QString id;
    QPointF position;
    QStringList attachmentTargetIds;
    QColor color = QColor("#628ed1");
    bool visible = true;
    bool locked = false;
    QString name;

    /// Проверяет наличие привязки к указанной оси без учёта других привязок точки.
    bool isAttachedTo(const QString &targetId) const {
        return attachmentTargetIds.contains(targetId);
    }

    /// Сравнивает сохраняемую геометрию и текущие параметры отображения двух точек.
    bool operator==(const VanishingPoint &other) const {
        return id == other.id && position == other.position && attachmentTargetIds == other.attachmentTargetIds &&
               color == other.color && visible == other.visible && locked == other.locked && name == other.name;
    }
    /// Проверяет отличие хотя бы одного свойства точки.
    bool operator!=(const VanishingPoint &other) const {
        return !(*this == other);
    }
};

/// Содержит полный снимок документа: размер холста, стек слоёв, геометрию перспективы и её оформление.
struct DrawingState {
    QSize canvasSize;
    LayerStack layers;
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

    /// Заменяет содержимое снимка одним растровым слоем для новых документов и старых форматов проекта.
    void setSingleRasterImage(const QImage &image,
                              const QString &name,
                              bool transparencyAvailable = false,
                              bool alphaLocked = true) {
        canvasSize = image.size();
        layers = LayerStack::singleRaster(image, name, transparencyAvailable, alphaLocked);
    }
    /// Собирает видимые слои в изображение размера холста для экспорта и совместимого сохранения.
    QImage flattenedImage() const { return LayerCompositor::compose(layers, canvasSize); }
};

/// Представляет сериализуемую историю снимков и позицию активного состояния.
struct DrawingHistory {
    QVector<DrawingState> states;
    QStringList labels;
    int index = 0;
};

namespace Project {
inline constexpr int CurrentFormatVersion = 8;

/// Проверяет размер растра по ограничениям стороны и общего числа пикселей.
bool validSize(QSize size);
/// Сохраняет один снимок документа в контейнер `.drw` без дополнительной истории.
bool save(const QString &path, const DrawingState &state, QString *error);
/// Сохраняет историю документа в атомарно заменяемый контейнер `.drw`.
bool save(const QString &path, const DrawingHistory &history, QString *error);
/// Загружает только активный снимок из `.drw`, включая проекты прежних версий.
bool load(const QString &path, DrawingState *state, QString *error);
/// Загружает состояние и доступную историю Undo/Redo из `.drw`.
bool load(const QString &path, DrawingHistory *history, QString *error);
/// Читает PNG с проверкой установленных ограничений размера.
bool loadPng(const QString &path, QImage *image, QString *error);
/// Атомарно записывает только растр документа в PNG без служебной оснастки.
bool exportPng(const QString &path, const QImage &image, QString *error);
/// Атомарно экспортирует растр в PNG, JPEG или BMP с заданным качеством кодирования.
bool exportImage(const QString &path, const QImage &image, const QByteArray &format, int quality, QString *error);
} // namespace Project
