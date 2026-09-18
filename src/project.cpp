#include "project.h"
#include <QBuffer>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QImageReader>
#include <QImageWriter>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QPainter>
#include <private/qzipreader_p.h>
#include <private/qzipwriter_p.h>
#include <algorithm>
#include <cmath>

namespace {
constexpr qint64 maxProjectBytes = 512000000;
/// Записывает текст ошибки при наличии приёмника и возвращает `false` для цепочки проверок.
bool fail(QString *error, const QString &text) {
    if (error)
        *error = text;
    return false;
}
/// Кодирует изображение в PNG в памяти для помещения внутрь контейнера проекта.
bool encode(const QImage &image, QByteArray *png, QString *error) {
    QBuffer buffer(png);
    buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, "PNG"))
        return fail(error, QCoreApplication::translate("Project", "Не удалось создать PNG."));
    return true;
}
/// Декодирует PNG из устройства с проверкой размера до выделения полного растра.
bool decode(QIODevice *device, QImage *image, QString *error) {
    QImageReader reader(device, "PNG");
    if (!Project::validSize(reader.size()))
        return fail(error,
                    QCoreApplication::translate(
                        "Project", "Размер изображения недопустим. Максимум: 8192 по стороне и 16 млн пикселей."));
    QImage result = reader.read();
    if (result.isNull())
        return fail(error, reader.errorString());
    *image = result.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    return true;
}
/// Сериализует только сохраняемую геометрию перспективы, исключая параметры оформления.
QJsonObject perspectiveJson(const DrawingState &state) {
    QJsonArray points;
    for (const auto &point : state.vanishingPoints) {
        QJsonArray attachments;
        for (const auto &targetId : point.attachmentTargetIds)
            attachments.append(QJsonObject{{"type", PerspectiveTarget::constructionType()}, {"targetId", targetId}});
        points.append(QJsonObject{{"id", point.id},
                                  {"name", point.name},
                                  {"x", point.position.x()},
                                  {"y", point.position.y()},
                                  {"locked", point.locked},
                                  {"attachments", attachments}});
    }
    return QJsonObject{{"horizon", QJsonObject{{"y", state.horizonY}, {"locked", state.horizonLocked}}},
                       {"vertical", QJsonObject{{"x", state.verticalX}, {"locked", state.verticalLocked}}},
                       {"points", points}};
}
/// Сериализует документную геометрию направляющих без параметров представления.
QJsonArray guidesJson(const DrawingState &state) {
    QJsonArray result;
    for (const Guide &guide : state.guides) {
        QJsonObject object{{"id", guide.id}};
        if (guide.type == GuideType::Horizontal) {
            object["type"] = "horizontal";
            object["position"] = guide.position;
        } else if (guide.type == GuideType::Vertical) {
            object["type"] = "vertical";
            object["position"] = guide.position;
        } else {
            object["type"] = "perspective";
            object["vanishingPointId"] = guide.vanishingPointId;
            object["angleRadians"] = guide.angleRadians;
        }
        result.append(object);
    }
    return result;
}
/// Читает направляющие DRW 10 и оставляет пустой набор при миграции прежних форматов.
bool parseGuides(const QJsonValue &value, DrawingState *state, QString *error, int formatVersion) {
    state->guides.clear();
    if (formatVersion < 10)
        return true;
    if (!value.isArray())
        return fail(error, QCoreApplication::translate("Project", "Отсутствует список направляющих."));
    const QJsonArray array = value.toArray();
    if (array.size() > 1024)
        return fail(error, QCoreApplication::translate("Project", "Слишком много направляющих."));
    QSet<QString> ids;
    QSet<QString> pointIds;
    for (const VanishingPoint &point : state->vanishingPoints)
        pointIds.insert(point.id);
    for (const QJsonValue &value : array) {
        if (!value.isObject())
            return fail(error, QCoreApplication::translate("Project", "Некорректная направляющая."));
        const QJsonObject object = value.toObject();
        Guide guide;
        guide.id = object.value("id").toString();
        const QString type = object.value("type").toString();
        if (guide.id.isEmpty() || guide.id.size() > 80 || ids.contains(guide.id))
            return fail(error, QCoreApplication::translate("Project", "Некорректный идентификатор направляющей."));
        if (type == QStringLiteral("horizontal") || type == QStringLiteral("vertical")) {
            guide.type = type == QStringLiteral("horizontal") ? GuideType::Horizontal : GuideType::Vertical;
            guide.position = object.value("position").toDouble(qQNaN());
            if (!std::isfinite(guide.position) || std::abs(guide.position) > 1000000)
                return fail(error, QCoreApplication::translate("Project", "Положение направляющей вне диапазона."));
        } else if (type == QStringLiteral("perspective")) {
            guide.type = GuideType::Perspective;
            guide.vanishingPointId = object.value("vanishingPointId").toString();
            guide.angleRadians = object.value("angleRadians").toDouble(qQNaN());
            if (!pointIds.contains(guide.vanishingPointId) || !std::isfinite(guide.angleRadians) ||
                std::abs(guide.angleRadians) > 1000)
                return fail(error,
                            QCoreApplication::translate("Project", "Некорректная перспективная направляющая."));
        } else {
            return fail(error, QCoreApplication::translate("Project", "Неизвестный тип направляющей."));
        }
        ids.insert(guide.id);
        state->guides.append(guide);
    }
    return true;
}
/// Читает геометрию перспективы с миграцией схем версий 1–10 в актуальную модель.
bool parsePerspective(const QJsonValue &value, DrawingState *state, QString *error, int formatVersion) {
    if (!value.isObject())
        return fail(error, QCoreApplication::translate("Project", "Отсутствуют параметры перспективы."));
    const auto perspective = value.toObject();
    if (formatVersion >= 4) {
        if (!perspective.value("horizon").isObject() || !perspective.value("points").isArray())
            return fail(error, QCoreApplication::translate("Project", "Некорректная структура перспективы."));
        const auto horizon = perspective.value("horizon").toObject();
        const double horizonY = horizon.value("y").toDouble(qQNaN());
        if (!std::isfinite(horizonY) || std::abs(horizonY) > 1000000)
            return fail(error,
                        QCoreApplication::translate("Project", "Положение горизонта вне допустимого диапазона."));
        if (formatVersion >= 5 && !horizon.value("locked").isBool())
            return fail(error, QCoreApplication::translate("Project", "Некорректное состояние фиксации горизонта."));
        double verticalX = state->canvasSize.width() / 2.0;
        bool verticalLocked = false;
        if (formatVersion >= 6) {
            if (!perspective.value("vertical").isObject())
                return fail(error, QCoreApplication::translate("Project", "Отсутствует главная вертикаль."));
            const auto vertical = perspective.value("vertical").toObject();
            verticalX = vertical.value("x").toDouble(qQNaN());
            if (!std::isfinite(verticalX) || std::abs(verticalX) > 1000000)
                return fail(
                    error,
                    QCoreApplication::translate("Project", "Положение главной вертикали вне допустимого диапазона."));
            if (!vertical.value("locked").isBool())
                return fail(
                    error,
                    QCoreApplication::translate("Project", "Некорректное состояние фиксации главной вертикали."));
            verticalLocked = vertical.value("locked").toBool();
        }
        const auto points = perspective.value("points").toArray();
        if (points.size() > 32)
            return fail(error, QCoreApplication::translate("Project", "Слишком много точек схода."));
        QSet<QString> ids;
        QVector<VanishingPoint> parsed;
        for (int pointIndex = 0; pointIndex < points.size(); ++pointIndex) {
            const auto entry = points[pointIndex];
            if (!entry.isObject())
                return fail(error, QCoreApplication::translate("Project", "Некорректная точка схода."));
            const auto object = entry.toObject();
            VanishingPoint point;
            point.id = object.value("id").toString();
            point.position = QPointF(object.value("x").toDouble(qQNaN()), object.value("y").toDouble(qQNaN()));
            if (point.id.isEmpty() || point.id.size() > 80 || ids.contains(point.id) ||
                !std::isfinite(point.position.x()) || !std::isfinite(point.position.y()) ||
                std::abs(point.position.x()) > 1000000 || std::abs(point.position.y()) > 1000000)
                return fail(error, QCoreApplication::translate("Project", "Некорректная точка схода."));
            if (formatVersion >= 7 && !object.value("name").isString())
                return fail(error, QCoreApplication::translate("Project", "Некорректное название точки схода."));
            point.name = formatVersion >= 7
                             ? object.value("name").toString()
                             : QCoreApplication::translate("Project", "Точка схода %1").arg(pointIndex + 1);
            if (point.name.size() > 120)
                return fail(error, QCoreApplication::translate("Project", "Название точки схода слишком длинное."));
            if (formatVersion >= 5 && !object.value("locked").isBool())
                return fail(error,
                            QCoreApplication::translate("Project", "Некорректное состояние фиксации точки схода."));
            point.locked = formatVersion >= 5 && object.value("locked").toBool();
            ids.insert(point.id);
            if (formatVersion >= 8) {
                const auto attachments = object.value("attachments");
                if (!attachments.isArray())
                    return fail(error, QCoreApplication::translate("Project", "Некорректные привязки точки схода."));
                QSet<QString> targets;
                for (const auto &entry : attachments.toArray()) {
                    if (!entry.isObject())
                        return fail(error,
                                    QCoreApplication::translate("Project", "Некорректная привязка точки схода."));
                    const auto binding = entry.toObject();
                    const QString type = binding.value("type").toString(),
                                  targetId = binding.value("targetId").toString();
                    if (type != PerspectiveTarget::constructionType() ||
                        (targetId != PerspectiveTarget::horizon() && targetId != PerspectiveTarget::vertical()) ||
                        targets.contains(targetId))
                        return fail(error,
                                    QCoreApplication::translate(
                                        "Project", "Неизвестная или повторная цель привязки точки схода."));
                    targets.insert(targetId);
                    point.attachmentTargetIds.append(targetId);
                }
            } else {
                const auto attachment = object.value("attachment");
                if (!attachment.isNull() && !attachment.isUndefined()) {
                    if (!attachment.isObject())
                        return fail(error,
                                    QCoreApplication::translate("Project", "Некорректная привязка точки схода."));
                    const auto binding = attachment.toObject();
                    const QString type = binding.value("type").toString(),
                                  targetId = binding.value("targetId").toString();
                    const bool knownTarget = targetId == PerspectiveTarget::horizon() ||
                                             (formatVersion >= 6 && targetId == PerspectiveTarget::vertical());
                    if (type != PerspectiveTarget::constructionType() || !knownTarget)
                        return fail(error,
                                    QCoreApplication::translate("Project", "Неизвестная цель привязки точки схода."));
                    point.attachmentTargetIds.append(targetId);
                }
            }
            parsed.append(point);
        }
        state->horizonY = horizonY;
        state->horizonLocked = formatVersion >= 5 && horizon.value("locked").toBool();
        state->verticalX = verticalX;
        state->verticalLocked = verticalLocked;
        state->vanishingPoints = parsed;
        return true;
    }
    if (!perspective.value("x").isDouble() || !perspective.value("y").isDouble())
        return fail(error, QCoreApplication::translate("Project", "Некорректные параметры перспективы."));
    const double x = perspective.value("x").toDouble(), y = perspective.value("y").toDouble();
    const double horizonY = perspective.value("horizonY").toDouble(y);
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(horizonY) || std::abs(x) > 1000000 ||
        std::abs(y) > 1000000 || std::abs(horizonY) > 1000000)
        return fail(error, QCoreApplication::translate("Project", "Параметры перспективы вне допустимого диапазона."));
    VanishingPoint point;
    point.id = QStringLiteral("vp-1");
    point.position = QPointF(x, y);
    point.name = QCoreApplication::translate("Project", "Точка схода 1");
    if (qFuzzyCompare(y + 1, horizonY + 1))
        point.attachmentTargetIds.append(PerspectiveTarget::horizon());
    state->horizonY = horizonY;
    state->verticalX = state->canvasSize.width() / 2.0;
    state->vanishingPoints = {point};
    return true;
}
/// Проверяет полный снимок перед сохранением, включая пределы геометрии и оформления.
bool validState(const DrawingState &state) {
    if (!Project::validSize(state.canvasSize) || state.layers.entries().isEmpty() ||
        state.layers.entries().size() > 128 ||
        state.layers.activeEntry() == nullptr || !std::isfinite(state.horizonY) ||
        std::abs(state.horizonY) > 1000000 || !std::isfinite(state.verticalX) || std::abs(state.verticalX) > 1000000 ||
        state.vanishingPoints.size() > 32)
        return false;
    QSet<QString> layerIds;
    for (const auto &layer : state.layers.entries()) {
        if (layer.id.isEmpty() || layerIds.contains(layer.id) || layer.name.size() > 120 || layer.opacity < 0 ||
            layer.opacity > 100 || !std::isfinite(layer.offset.x()) || !std::isfinite(layer.offset.y()) ||
            !layer.content || layer.content->typeId() != layer.typeId ||
            !LayerTypeRegistry::instance().type(layer.typeId))
            return false;
        layerIds.insert(layer.id);
    }
    QSet<QString> ids;
    for (const auto &point : state.vanishingPoints) {
        if (point.id.isEmpty() || point.id.size() > 80 || point.name.size() > 120 || ids.contains(point.id) ||
            !std::isfinite(point.position.x()) || !std::isfinite(point.position.y()) ||
            std::abs(point.position.x()) > 1000000 || std::abs(point.position.y()) > 1000000 ||
            !point.color.isValid() || point.attachmentTargetIds.size() > 2)
            return false;
        ids.insert(point.id);
        QSet<QString> targets;
        for (const auto &targetId : point.attachmentTargetIds) {
            if ((targetId != PerspectiveTarget::horizon() && targetId != PerspectiveTarget::vertical()) ||
                targets.contains(targetId))
                return false;
            targets.insert(targetId);
        }
    }
    QSet<QString> guideIds;
    for (const Guide &guide : state.guides) {
        if (guide.id.isEmpty() || guide.id.size() > 80 || guideIds.contains(guide.id) ||
            !std::isfinite(guide.position) || !std::isfinite(guide.angleRadians) ||
            std::abs(guide.position) > 1000000 || std::abs(guide.angleRadians) > 1000)
            return false;
        if (guide.type == GuideType::Perspective) {
            if (guide.vanishingPointId.isEmpty() || !ids.contains(guide.vanishingPointId))
                return false;
        } else if ((guide.type != GuideType::Horizontal && guide.type != GuideType::Vertical) ||
                   !guide.vanishingPointId.isEmpty()) {
            return false;
        }
        guideIds.insert(guide.id);
    }
    return std::isfinite(state.rayStepDegrees) && state.rayStepDegrees >= 1 && state.rayStepDegrees <= 30 &&
           std::isfinite(state.rayAngleOffset) && state.rayAngleOffset >= -180 && state.rayAngleOffset <= 180 &&
           state.rayPattern >= 0 && state.rayPattern <= 3 && std::isfinite(state.rayWidth) && state.rayWidth >= 0.1 &&
           state.rayWidth <= 20 && state.rayGap >= 0 && state.rayGap <= 200 && state.rayStartOpacity >= 0 &&
           state.rayStartOpacity <= 100 && state.rayEndOpacity >= 0 && state.rayEndOpacity <= 100 &&
           state.rayFadeLength >= 0 && state.rayFadeLength <= 500 && state.horizonColor.isValid() &&
           state.horizonOpacity >= 0 && state.horizonOpacity <= 100 && std::isfinite(state.horizonWidth) &&
           state.horizonWidth >= 0.1 && state.horizonWidth <= 20 && state.verticalColor.isValid() &&
           state.verticalOpacity >= 0 && state.verticalOpacity <= 100 && std::isfinite(state.verticalWidth) &&
           state.verticalWidth >= 0.1 && state.verticalWidth <= 20;
}
/// Сравнивает сохраняемые данные с учётом появления стеков в DRW 9 и направляющих в DRW 10.
bool samePersistentState(const DrawingState &a, const DrawingState &b, int formatVersion) {
    if (a.canvasSize != b.canvasSize || (formatVersion >= 9 ? a.layers != b.layers
                                                            : a.flattenedImage() != b.flattenedImage()) ||
        !qFuzzyCompare(a.horizonY + 1, b.horizonY + 1) ||
        a.horizonLocked != b.horizonLocked ||
        !qFuzzyCompare(a.verticalX + 1, b.verticalX + 1) || a.verticalLocked != b.verticalLocked ||
        a.vanishingPoints.size() != b.vanishingPoints.size() ||
        (formatVersion >= 10 && a.guides != b.guides))
        return false;
    for (int i = 0; i < a.vanishingPoints.size(); ++i) {
        const auto &x = a.vanishingPoints[i], &y = b.vanishingPoints[i];
        if (x.id != y.id || x.name != y.name || x.position != y.position ||
            x.attachmentTargetIds != y.attachmentTargetIds || x.locked != y.locked)
            return false;
    }
    return true;
}

/// Рекурсивно заменяет временные ссылки кодека каноническими путями ресурсов архива.
QJsonValue replaceResourcePaths(const QJsonValue &value, const QHash<QString, QString> &paths) {
    if (value.isString())
        return paths.value(value.toString(), value.toString());
    if (value.isArray()) {
        QJsonArray result;
        for (const auto &item : value.toArray())
            result.append(replaceResourcePaths(item, paths));
        return result;
    }
    if (value.isObject()) {
        QJsonObject result;
        const auto object = value.toObject();
        for (auto iterator = object.constBegin(); iterator != object.constEnd(); ++iterator)
            result.insert(iterator.key(), replaceResourcePaths(iterator.value(), paths));
        return result;
    }
    return value;
}

/// Сериализует общие свойства стека и дедуплицирует ресурсы кодеков по SHA-256.
bool serializeLayers(const LayerStack &layers,
                     int stateIndex,
                     QJsonObject *result,
                     QHash<QString, QByteArray> *resources,
                     QString *error) {
    QJsonArray entries;
    for (int layerIndex = 0; layerIndex < layers.entries().size(); ++layerIndex) {
        const LayerEntry &entry = layers.entries()[layerIndex];
        const LayerType *type = LayerTypeRegistry::instance().type(entry.typeId);
        if (!type || !type->codec || !entry.content)
            return fail(error, QCoreApplication::translate("Project", "Неизвестный тип слоя: %1").arg(entry.typeId));
        QJsonObject contentManifest;
        QHash<QString, QByteArray> localResources;
        const QString temporaryRoot = QStringLiteral("state-%1/layer-%2").arg(stateIndex).arg(layerIndex);
        if (!type->codec->encode(*entry.content, temporaryRoot, &contentManifest, &localResources, error))
            return false;
        QHash<QString, QString> canonicalPaths;
        for (auto iterator = localResources.constBegin(); iterator != localResources.constEnd(); ++iterator) {
            QString suffix = QFileInfo(iterator.key()).suffix().toLower();
            if (suffix.isEmpty() || suffix.size() > 8)
                suffix = QStringLiteral("bin");
            const QByteArray digest = QCryptographicHash::hash(iterator.value(), QCryptographicHash::Sha256).toHex();
            const QString canonical = QStringLiteral("resources/%1.%2").arg(QString::fromLatin1(digest), suffix);
            if (resources->contains(canonical) && resources->value(canonical) != iterator.value())
                return fail(error, QCoreApplication::translate("Project", "Конфликт ресурсов содержимого слоя."));
            resources->insert(canonical, iterator.value());
            canonicalPaths.insert(iterator.key(), canonical);
        }
        contentManifest = replaceResourcePaths(contentManifest, canonicalPaths).toObject();
        entries.append(QJsonObject{{"id", entry.id},
                                   {"type", entry.typeId},
                                   {"name", entry.name},
                                   {"visible", entry.visible},
                                   {"locked", entry.locked},
                                   {"opacity", entry.opacity},
                                   {"offset", QJsonObject{{"x", entry.offset.x()}, {"y", entry.offset.y()}}},
                                   {"content", contentManifest}});
    }
    *result = QJsonObject{{"activeLayerId", layers.activeLayerId()}, {"entries", entries}};
    return true;
}

/// Проверяет манифест стека и делегирует чтение содержимого кодеку зарегистрированного типа.
bool deserializeLayers(const QJsonValue &value,
                       QSize canvasSize,
                       const LayerResourceReader &resourceReader,
                       LayerStack *result,
                       QString *error) {
    if (!value.isObject())
        return fail(error, QCoreApplication::translate("Project", "Отсутствует стек слоёв."));
    const QJsonObject object = value.toObject();
    const QJsonArray entries = object.value("entries").toArray();
    const QString activeId = object.value("activeLayerId").toString();
    if (entries.isEmpty() || entries.size() > 128 || activeId.isEmpty())
        return fail(error, QCoreApplication::translate("Project", "Некорректный стек слоёв."));
    LayerStack stack;
    QSet<QString> ids;
    for (const auto &value : entries) {
        if (!value.isObject())
            return fail(error, QCoreApplication::translate("Project", "Некорректная запись слоя."));
        const QJsonObject object = value.toObject();
        LayerEntry entry;
        entry.id = object.value("id").toString();
        entry.typeId = object.value("type").toString();
        entry.name = object.value("name").toString();
        const QJsonObject offset = object.value("offset").toObject();
        entry.offset = QPointF(offset.value("x").toDouble(qQNaN()), offset.value("y").toDouble(qQNaN()));
        const double opacity = object.value("opacity").toDouble(-1);
        if (entry.id.isEmpty() || entry.id.size() > 120 || ids.contains(entry.id) || entry.typeId.isEmpty() ||
            entry.typeId.size() > 80 || entry.name.size() > 120 || !object.value("visible").isBool() ||
            !object.value("locked").isBool() || opacity < 0 || opacity > 100 || opacity != std::floor(opacity) ||
            !offset.contains("x") || !offset.contains("y") || !std::isfinite(entry.offset.x()) ||
            !std::isfinite(entry.offset.y()) || std::abs(entry.offset.x()) > 1000000 ||
            std::abs(entry.offset.y()) > 1000000 || !object.value("content").isObject())
            return fail(error, QCoreApplication::translate("Project", "Некорректные свойства слоя."));
        const LayerType *type = LayerTypeRegistry::instance().type(entry.typeId);
        if (!type || !type->codec)
            return fail(error, QCoreApplication::translate("Project", "Неизвестный тип слоя: %1").arg(entry.typeId));
        entry.visible = object.value("visible").toBool();
        entry.locked = object.value("locked").toBool();
        entry.opacity = int(opacity);
        entry.content = type->codec->decode(object.value("content").toObject(), resourceReader, canvasSize, error);
        if (!entry.content || entry.content->typeId() != entry.typeId)
            return false;
        ids.insert(entry.id);
        stack.entries().append(entry);
    }
    if (!stack.setActiveLayerId(activeId))
        return fail(error, QCoreApplication::translate("Project", "Активный слой отсутствует в стеке."));
    *result = std::move(stack);
    return true;
}
} // namespace

bool Project::validSize(QSize size) {
    return size.width() > 0 && size.height() > 0 && size.width() <= 8192 && size.height() <= 8192 &&
           qint64(size.width()) * size.height() <= 16000000;
}
bool Project::save(const QString &path, const DrawingState &state, QString *error) {
    DrawingHistory history;
    history.states.append(state);
    return save(path, history, error);
}

bool Project::save(const QString &path, const DrawingHistory &history, QString *error) {
    if (history.states.isEmpty() || history.states.size() > 31 || history.labels.size() != history.states.size() - 1 ||
        history.index < 0 || history.index >= history.states.size())
        return fail(error, QCoreApplication::translate("Project", "Некорректная история документа."));
    const QSize size = history.states[history.index].canvasSize;
    for (const auto &state : history.states)
        if (!validState(state) || state.canvasSize != size)
            return fail(error,
                        QCoreApplication::translate("Project", "История содержит недопустимое состояние документа."));

    QByteArray currentPng;
    if (!encode(history.states[history.index].flattenedImage(), &currentPng, error))
        return false;
    QHash<QString, QByteArray> resources;
    QJsonArray states;
    for (int i = 0; i < history.states.size(); ++i) {
        QJsonObject layers;
        if (!serializeLayers(history.states[i].layers, i, &layers, &resources, error))
            return false;
        states.append(QJsonObject{{"layers", layers},
                                  {"perspective", perspectiveJson(history.states[i])},
                                  {"guides", guidesJson(history.states[i])}});
    }
    QJsonArray labels;
    for (const auto &label : history.labels)
        labels.append(label);
    QJsonObject historyJson{{"index", history.index}, {"states", states}, {"labels", labels}};
    const DrawingState &current = history.states[history.index];
    QJsonObject metadata{{"format", "Drawing"},
                         {"version", Project::CurrentFormatVersion},
                         {"width", size.width()},
                         {"height", size.height()},
                         {"image", "drawing.png"},
                         {"layers", states[history.index].toObject().value("layers")},
                         {"perspective", perspectiveJson(current)},
                         {"guides", guidesJson(current)},
                         {"history", historyJson}};

    QByteArray archive;
    QBuffer archiveBuffer(&archive);
    archiveBuffer.open(QIODevice::WriteOnly);
    {
        QZipWriter zip(&archiveBuffer);
        zip.setCompressionPolicy(QZipWriter::NeverCompress);
        zip.addFile("drawing.png", currentPng);
        QStringList resourcePaths = resources.keys();
        std::sort(resourcePaths.begin(), resourcePaths.end());
        for (const auto &resourcePath : resourcePaths)
            zip.addFile(resourcePath, resources.value(resourcePath));
        zip.addFile("project.json", QJsonDocument(metadata).toJson());
        zip.close();
        if (zip.status() != QZipWriter::NoError)
            return fail(error, QCoreApplication::translate("Project", "Ошибка создания контейнера DRW."));
    }
    if (archive.size() > maxProjectBytes)
        return fail(error, QCoreApplication::translate("Project", "Проект с историей превышает ограничение 512 МБ."));
    // QSaveFile оставляет прежний проект целым, если запись или финальная атомарная замена не удалась.
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return fail(error, file.errorString());
    if (file.write(archive) != archive.size()) {
        file.cancelWriting();
        return fail(error, file.errorString());
    }
    if (!file.commit())
        return fail(error, file.errorString());
    return true;
}

bool Project::load(const QString &path, DrawingState *state, QString *error) {
    DrawingHistory history;
    if (!load(path, &history, error))
        return false;
    *state = history.states[history.index];
    return true;
}

bool Project::load(const QString &path, DrawingHistory *history, QString *error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return fail(error, file.errorString());
    if (file.size() > maxProjectBytes)
        return fail(error, QCoreApplication::translate("Project", "Файл слишком велик для прототипа (512 МБ)."));
    QZipReader zip(&file);
    const auto files = zip.fileInfoList();
    QHash<QString, int> counts;
    QHash<QString, qint64> sizes;
    qint64 unpacked = 0;
    // Метаданные ZIP проверяются до распаковки содержимого, чтобы ограничить память на повреждённых файлах.
    for (const auto &entry : files)
        if (entry.isFile) {
            ++counts[entry.filePath];
            sizes[entry.filePath] = entry.size;
            unpacked += entry.size;
        }
    if (zip.status() != QZipReader::NoError || unpacked > maxProjectBytes || counts.value("drawing.png") != 1 ||
        counts.value("project.json") != 1 || sizes.value("drawing.png") < 1 || sizes.value("drawing.png") > 80000000 ||
        sizes.value("project.json") < 1 || sizes.value("project.json") > 1048576)
        return fail(
            error,
            QCoreApplication::translate("Project", "Повреждённый DRW: нужны допустимые drawing.png и project.json."));

    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(zip.fileData("project.json"), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return fail(error, QCoreApplication::translate("Project", "Не удалось прочитать project.json."));
    const auto object = document.object();
    const double versionValue = object.value("version").toDouble(-1);
    if (object.value("format").toString() != "Drawing" || versionValue < 1 ||
        versionValue > Project::CurrentFormatVersion || versionValue != std::floor(versionValue) ||
        object.value("image").toString() != "drawing.png")
        return fail(error, QCoreApplication::translate("Project", "Неизвестный формат или версия DRW."));
    const double width = object.value("width").toDouble(-1), height = object.value("height").toDouble(-1);
    if (width < 1 || height < 1 || width > 8192 || height > 8192 || width != std::floor(width) ||
        height != std::floor(height) || !validSize(QSize(int(width), int(height))))
        return fail(error, QCoreApplication::translate("Project", "Некорректный размер холста в проекте."));
    const QSize expectedSize{int(width), int(height)};

    QHash<QString, QImage> images;
    auto loadImage = [&](const QString &imagePath, QImage *image) -> bool {
        static const QRegularExpression historyName(QStringLiteral("^history/state-[0-9]{4}\\.png$"));
        if (imagePath != "drawing.png" && !historyName.match(imagePath).hasMatch())
            return fail(error, QCoreApplication::translate("Project", "Некорректная ссылка на изображение истории."));
        if (images.contains(imagePath)) {
            *image = images.value(imagePath);
            return true;
        }
        if (counts.value(imagePath) != 1 || sizes.value(imagePath) < 1 || sizes.value(imagePath) > 80000000)
            return fail(error, QCoreApplication::translate("Project", "Отсутствует изображение истории."));
        QByteArray png = zip.fileData(imagePath);
        QBuffer buffer(&png);
        buffer.open(QIODevice::ReadOnly);
        QImage decoded;
        if (!decode(&buffer, &decoded, error))
            return false;
        if (decoded.size() != expectedSize)
            return fail(error,
                        QCoreApplication::translate("Project", "Размер изображения истории не совпадает с проектом."));
        images.insert(imagePath, decoded);
        *image = decoded;
        return true;
    };

    bool resourceReadFailed = false;
    auto readResource = [&](const QString &resourcePath) -> QByteArray {
        static const QRegularExpression resourceName(
            QStringLiteral("^resources/[0-9a-f]{64}\\.[a-z0-9]{1,8}$"));
        if (!resourceName.match(resourcePath).hasMatch() || counts.value(resourcePath) != 1 ||
            sizes.value(resourcePath) < 1 || sizes.value(resourcePath) > 80000000) {
            resourceReadFailed = true;
            fail(error, QCoreApplication::translate("Project", "Отсутствует или повреждён ресурс слоя."));
            return {};
        }
        return zip.fileData(resourcePath);
    };

    DrawingState current;
    current.canvasSize = expectedSize;
    if (versionValue >= 9) {
        if (!deserializeLayers(object.value("layers"), expectedSize, readResource, &current.layers, error) ||
            resourceReadFailed)
            return false;
    } else {
        QImage currentImage;
        if (!loadImage("drawing.png", &currentImage))
            return false;
        current.setSingleRasterImage(
            currentImage, QCoreApplication::translate("Project", "Фон"), currentImage.hasAlphaChannel(), true);
    }
    if (!parsePerspective(object.value("perspective"), &current, error, int(versionValue)))
        return false;
    if (!parseGuides(object.value("guides"), &current, error, int(versionValue)))
        return false;
    DrawingHistory result;
    // Версия 1 предшествует сохраняемой истории и поэтому разворачивается в единственный снимок.
    if (versionValue == 1) {
        result.states.append(current);
        *history = result;
        return true;
    }

    const auto historyValue = object.value("history");
    if (!historyValue.isObject())
        return fail(error, QCoreApplication::translate("Project", "Отсутствует история документа."));
    const auto historyObject = historyValue.toObject();
    const auto states = historyObject.value("states").toArray();
    const auto labels = historyObject.value("labels").toArray();
    const double indexValue = historyObject.value("index").toDouble(-1);
    if (states.isEmpty() || states.size() > 31 || labels.size() != states.size() - 1 || indexValue < 0 ||
        indexValue >= states.size() || indexValue != std::floor(indexValue))
        return fail(error, QCoreApplication::translate("Project", "Некорректное оглавление истории документа."));
    for (const auto &value : states) {
        if (!value.isObject())
            return fail(error, QCoreApplication::translate("Project", "Некорректное состояние истории."));
        const auto stateObject = value.toObject();
        DrawingState state;
        state.canvasSize = expectedSize;
        if (versionValue >= 9) {
            resourceReadFailed = false;
            if (!deserializeLayers(stateObject.value("layers"), expectedSize, readResource, &state.layers, error) ||
                resourceReadFailed)
                return false;
        } else {
            QImage stateImage;
            if (!loadImage(stateObject.value("image").toString(), &stateImage))
                return false;
            state.setSingleRasterImage(
                stateImage, QCoreApplication::translate("Project", "Фон"), stateImage.hasAlphaChannel(), true);
        }
        if (!parsePerspective(stateObject.value("perspective"), &state, error, int(versionValue)))
            return false;
        if (!parseGuides(stateObject.value("guides"), &state, error, int(versionValue)))
            return false;
        result.states.append(state);
    }
    for (const auto &value : labels) {
        if (!value.isString() || value.toString().size() > 200)
            return fail(error, QCoreApplication::translate("Project", "Некорректная подпись операции истории."));
        result.labels.append(value.toString());
    }
    result.index = int(indexValue);
    if (!samePersistentState(result.states[result.index], current, int(versionValue)))
        return fail(error,
                    QCoreApplication::translate("Project", "Текущее состояние не совпадает с историей документа."));
    *history = result;
    return true;
}

bool Project::loadPng(const QString &path, QImage *image, QString *error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return fail(error, file.errorString());
    return decode(&file, image, error);
}

bool Project::exportPng(const QString &path, const QImage &image, QString *error) {
    return exportImage(path, image, QByteArrayLiteral("PNG"), -1, error);
}

bool Project::exportImage(
    const QString &path, const QImage &image, const QByteArray &format, int quality, QString *error) {
    const QByteArray normalized = format.toUpper();
    if (normalized != QByteArrayLiteral("PNG") && normalized != QByteArrayLiteral("JPEG") &&
        normalized != QByteArrayLiteral("BMP"))
        return fail(error, QCoreApplication::translate("Project", "Неподдерживаемый формат экспорта."));
    QImage output = image;
    if (normalized != QByteArrayLiteral("PNG") && image.hasAlphaChannel()) {
        output = QImage(image.size(), QImage::Format_RGB32);
        output.fill(Qt::white);
        QPainter painter(&output);
        painter.drawImage(QPoint(), image);
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return fail(error, file.errorString());
    QImageWriter writer(&file, normalized);
    if (quality >= 0)
        writer.setQuality(qBound(0, quality, 100));
    if (!writer.write(output)) {
        file.cancelWriting();
        return fail(error,
                    QCoreApplication::translate("Project", "Не удалось записать изображение: %1")
                        .arg(writer.errorString()));
    }
    if (!file.commit())
        return fail(error, file.errorString());
    return true;
}
