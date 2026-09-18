#include "project.h"
#include <QBuffer>
#include <QFile>
#include <QHash>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <private/qzipreader_p.h>
#include <private/qzipwriter_p.h>
#include <cmath>

namespace {
constexpr qint64 maxProjectBytes = 512000000;
bool fail(QString *error, const QString &text) {
    if (error)
        *error = text;
    return false;
}
bool encode(const QImage &image, QByteArray *png, QString *error) {
    QBuffer buffer(png);
    buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, "PNG"))
        return fail(error, QStringLiteral("Не удалось создать PNG."));
    return true;
}
bool decode(QIODevice *device, QImage *image, QString *error) {
    QImageReader reader(device, "PNG");
    if (!Project::validSize(reader.size()))
        return fail(error,
                    QStringLiteral("Размер изображения недопустим. Максимум: 8192 по стороне и 16 млн пикселей."));
    QImage result = reader.read();
    if (result.isNull())
        return fail(error, reader.errorString());
    *image = result.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    return true;
}
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
bool parsePerspective(const QJsonValue &value, DrawingState *state, QString *error, int formatVersion) {
    if (!value.isObject())
        return fail(error, QStringLiteral("Отсутствуют параметры перспективы."));
    const auto perspective = value.toObject();
    if (formatVersion >= 4) {
        if (!perspective.value("horizon").isObject() || !perspective.value("points").isArray())
            return fail(error, QStringLiteral("Некорректная структура перспективы."));
        const auto horizon = perspective.value("horizon").toObject();
        const double horizonY = horizon.value("y").toDouble(qQNaN());
        if (!std::isfinite(horizonY) || std::abs(horizonY) > 1000000)
            return fail(error, QStringLiteral("Положение горизонта вне допустимого диапазона."));
        if (formatVersion >= 5 && !horizon.value("locked").isBool())
            return fail(error, QStringLiteral("Некорректное состояние фиксации горизонта."));
        double verticalX = state->image.width() / 2.0;
        bool verticalLocked = false;
        if (formatVersion >= 6) {
            if (!perspective.value("vertical").isObject())
                return fail(error, QStringLiteral("Отсутствует главная вертикаль."));
            const auto vertical = perspective.value("vertical").toObject();
            verticalX = vertical.value("x").toDouble(qQNaN());
            if (!std::isfinite(verticalX) || std::abs(verticalX) > 1000000)
                return fail(error, QStringLiteral("Положение главной вертикали вне допустимого диапазона."));
            if (!vertical.value("locked").isBool())
                return fail(error, QStringLiteral("Некорректное состояние фиксации главной вертикали."));
            verticalLocked = vertical.value("locked").toBool();
        }
        const auto points = perspective.value("points").toArray();
        if (points.size() > 32)
            return fail(error, QStringLiteral("Слишком много точек схода."));
        QSet<QString> ids;
        QVector<VanishingPoint> parsed;
        for (int pointIndex = 0; pointIndex < points.size(); ++pointIndex) {
            const auto entry = points[pointIndex];
            if (!entry.isObject())
                return fail(error, QStringLiteral("Некорректная точка схода."));
            const auto object = entry.toObject();
            VanishingPoint point;
            point.id = object.value("id").toString();
            point.position = QPointF(object.value("x").toDouble(qQNaN()), object.value("y").toDouble(qQNaN()));
            if (point.id.isEmpty() || point.id.size() > 80 || ids.contains(point.id) ||
                !std::isfinite(point.position.x()) || !std::isfinite(point.position.y()) ||
                std::abs(point.position.x()) > 1000000 || std::abs(point.position.y()) > 1000000)
                return fail(error, QStringLiteral("Некорректная точка схода."));
            if (formatVersion >= 7 && !object.value("name").isString())
                return fail(error, QStringLiteral("Некорректное название точки схода."));
            point.name = formatVersion >= 7 ? object.value("name").toString()
                                            : QStringLiteral("Точка схода %1").arg(pointIndex + 1);
            if (point.name.size() > 120)
                return fail(error, QStringLiteral("Название точки схода слишком длинное."));
            if (formatVersion >= 5 && !object.value("locked").isBool())
                return fail(error, QStringLiteral("Некорректное состояние фиксации точки схода."));
            point.locked = formatVersion >= 5 && object.value("locked").toBool();
            ids.insert(point.id);
            if (formatVersion >= Project::CurrentFormatVersion) {
                const auto attachments = object.value("attachments");
                if (!attachments.isArray())
                    return fail(error, QStringLiteral("Некорректные привязки точки схода."));
                QSet<QString> targets;
                for (const auto &entry : attachments.toArray()) {
                    if (!entry.isObject())
                        return fail(error, QStringLiteral("Некорректная привязка точки схода."));
                    const auto binding = entry.toObject();
                    const QString type = binding.value("type").toString(),
                                  targetId = binding.value("targetId").toString();
                    if (type != PerspectiveTarget::constructionType() ||
                        (targetId != PerspectiveTarget::horizon() && targetId != PerspectiveTarget::vertical()) ||
                        targets.contains(targetId))
                        return fail(error, QStringLiteral("Неизвестная или повторная цель привязки точки схода."));
                    targets.insert(targetId);
                    point.attachmentTargetIds.append(targetId);
                }
            } else {
                const auto attachment = object.value("attachment");
                if (!attachment.isNull() && !attachment.isUndefined()) {
                    if (!attachment.isObject())
                        return fail(error, QStringLiteral("Некорректная привязка точки схода."));
                    const auto binding = attachment.toObject();
                    const QString type = binding.value("type").toString(),
                                  targetId = binding.value("targetId").toString();
                    const bool knownTarget = targetId == PerspectiveTarget::horizon() ||
                                             (formatVersion >= 6 && targetId == PerspectiveTarget::vertical());
                    if (type != PerspectiveTarget::constructionType() || !knownTarget)
                        return fail(error, QStringLiteral("Неизвестная цель привязки точки схода."));
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
        return fail(error, QStringLiteral("Некорректные параметры перспективы."));
    const double x = perspective.value("x").toDouble(), y = perspective.value("y").toDouble();
    const double horizonY = perspective.value("horizonY").toDouble(y);
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(horizonY) || std::abs(x) > 1000000 ||
        std::abs(y) > 1000000 || std::abs(horizonY) > 1000000)
        return fail(error, QStringLiteral("Параметры перспективы вне допустимого диапазона."));
    VanishingPoint point;
    point.id = QStringLiteral("vp-1");
    point.position = QPointF(x, y);
    point.name = QStringLiteral("Точка схода 1");
    if (qFuzzyCompare(y + 1, horizonY + 1))
        point.attachmentTargetIds.append(PerspectiveTarget::horizon());
    state->horizonY = horizonY;
    state->verticalX = state->image.width() / 2.0;
    state->vanishingPoints = {point};
    return true;
}
bool validState(const DrawingState &state) {
    if (!Project::validSize(state.image.size()) || state.image.isNull() || !std::isfinite(state.horizonY) ||
        std::abs(state.horizonY) > 1000000 || !std::isfinite(state.verticalX) || std::abs(state.verticalX) > 1000000 ||
        state.vanishingPoints.size() > 32)
        return false;
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
bool samePersistentState(const DrawingState &a, const DrawingState &b) {
    if (a.image != b.image || !qFuzzyCompare(a.horizonY + 1, b.horizonY + 1) || a.horizonLocked != b.horizonLocked ||
        !qFuzzyCompare(a.verticalX + 1, b.verticalX + 1) || a.verticalLocked != b.verticalLocked ||
        a.vanishingPoints.size() != b.vanishingPoints.size())
        return false;
    for (int i = 0; i < a.vanishingPoints.size(); ++i) {
        const auto &x = a.vanishingPoints[i], &y = b.vanishingPoints[i];
        if (x.id != y.id || x.name != y.name || x.position != y.position ||
            x.attachmentTargetIds != y.attachmentTargetIds || x.locked != y.locked)
            return false;
    }
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
        return fail(error, QStringLiteral("Некорректная история документа."));
    const QSize size = history.states[history.index].image.size();
    for (const auto &state : history.states)
        if (!validState(state) || state.image.size() != size)
            return fail(error, QStringLiteral("История содержит недопустимое состояние документа."));

    QByteArray currentPng;
    if (!encode(history.states[history.index].image, &currentPng, error))
        return false;
    QVector<QPair<QString, QByteArray>> historyImages;
    QJsonArray states;
    QString previousImage;
    for (int i = 0; i < history.states.size(); ++i) {
        QString imagePath;
        if (i == history.index)
            imagePath = QStringLiteral("drawing.png");
        else if (i > 0 && history.states[i].image == history.states[i - 1].image)
            imagePath = previousImage;
        else {
            imagePath = QStringLiteral("history/state-%1.png").arg(i, 4, 10, QChar('0'));
            QByteArray png;
            if (!encode(history.states[i].image, &png, error))
                return false;
            historyImages.append(qMakePair(imagePath, png));
        }
        previousImage = imagePath;
        states.append(QJsonObject{{"image", imagePath}, {"perspective", perspectiveJson(history.states[i])}});
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
                         {"perspective", perspectiveJson(current)},
                         {"history", historyJson}};

    QByteArray archive;
    QBuffer archiveBuffer(&archive);
    archiveBuffer.open(QIODevice::WriteOnly);
    {
        QZipWriter zip(&archiveBuffer);
        zip.setCompressionPolicy(QZipWriter::NeverCompress);
        zip.addFile("drawing.png", currentPng);
        for (const auto &item : historyImages)
            zip.addFile(item.first, item.second);
        zip.addFile("project.json", QJsonDocument(metadata).toJson());
        zip.close();
        if (zip.status() != QZipWriter::NoError)
            return fail(error, QStringLiteral("Ошибка создания контейнера DRW."));
    }
    if (archive.size() > maxProjectBytes)
        return fail(error, QStringLiteral("Проект с историей превышает ограничение 512 МБ."));
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
        return fail(error, QStringLiteral("Файл слишком велик для прототипа (512 МБ)."));
    QZipReader zip(&file);
    const auto files = zip.fileInfoList();
    QHash<QString, int> counts;
    QHash<QString, qint64> sizes;
    qint64 unpacked = 0;
    for (const auto &entry : files)
        if (entry.isFile) {
            ++counts[entry.filePath];
            sizes[entry.filePath] = entry.size;
            unpacked += entry.size;
        }
    if (zip.status() != QZipReader::NoError || unpacked > maxProjectBytes || counts.value("drawing.png") != 1 ||
        counts.value("project.json") != 1 || sizes.value("drawing.png") < 1 || sizes.value("drawing.png") > 80000000 ||
        sizes.value("project.json") < 1 || sizes.value("project.json") > 1048576)
        return fail(error, QStringLiteral("Повреждённый DRW: нужны допустимые drawing.png и project.json."));

    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(zip.fileData("project.json"), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return fail(error, QStringLiteral("Не удалось прочитать project.json."));
    const auto object = document.object();
    const double versionValue = object.value("version").toDouble(-1);
    if (object.value("format").toString() != "Drawing" ||
        (versionValue != 1 && versionValue != 2 && versionValue != 3 && versionValue != 4 && versionValue != 5 &&
         versionValue != 6 && versionValue != 7 && versionValue != Project::CurrentFormatVersion) ||
        object.value("image").toString() != "drawing.png")
        return fail(error, QStringLiteral("Неизвестный формат или версия DRW."));
    const double width = object.value("width").toDouble(-1), height = object.value("height").toDouble(-1);
    if (width < 1 || height < 1 || width > 8192 || height > 8192 || width != std::floor(width) ||
        height != std::floor(height) || !validSize(QSize(int(width), int(height))))
        return fail(error, QStringLiteral("Некорректный размер холста в проекте."));
    const QSize expectedSize{int(width), int(height)};

    QHash<QString, QImage> images;
    auto loadImage = [&](const QString &imagePath, QImage *image) -> bool {
        static const QRegularExpression historyName(QStringLiteral("^history/state-[0-9]{4}\\.png$"));
        if (imagePath != "drawing.png" && !historyName.match(imagePath).hasMatch())
            return fail(error, QStringLiteral("Некорректная ссылка на изображение истории."));
        if (images.contains(imagePath)) {
            *image = images.value(imagePath);
            return true;
        }
        if (counts.value(imagePath) != 1 || sizes.value(imagePath) < 1 || sizes.value(imagePath) > 80000000)
            return fail(error, QStringLiteral("Отсутствует изображение истории."));
        QByteArray png = zip.fileData(imagePath);
        QBuffer buffer(&png);
        buffer.open(QIODevice::ReadOnly);
        QImage decoded;
        if (!decode(&buffer, &decoded, error))
            return false;
        if (decoded.size() != expectedSize)
            return fail(error, QStringLiteral("Размер изображения истории не совпадает с проектом."));
        images.insert(imagePath, decoded);
        *image = decoded;
        return true;
    };

    DrawingState current;
    if (!loadImage("drawing.png", &current.image) ||
        !parsePerspective(object.value("perspective"), &current, error, int(versionValue)))
        return false;
    DrawingHistory result;
    if (versionValue == 1) {
        result.states.append(current);
        *history = result;
        return true;
    }

    const auto historyValue = object.value("history");
    if (!historyValue.isObject())
        return fail(error, QStringLiteral("Отсутствует история документа."));
    const auto historyObject = historyValue.toObject();
    const auto states = historyObject.value("states").toArray();
    const auto labels = historyObject.value("labels").toArray();
    const double indexValue = historyObject.value("index").toDouble(-1);
    if (states.isEmpty() || states.size() > 31 || labels.size() != states.size() - 1 || indexValue < 0 ||
        indexValue >= states.size() || indexValue != std::floor(indexValue))
        return fail(error, QStringLiteral("Некорректное оглавление истории документа."));
    for (const auto &value : states) {
        if (!value.isObject())
            return fail(error, QStringLiteral("Некорректное состояние истории."));
        const auto stateObject = value.toObject();
        DrawingState state;
        if (!loadImage(stateObject.value("image").toString(), &state.image) ||
            !parsePerspective(stateObject.value("perspective"), &state, error, int(versionValue)))
            return false;
        result.states.append(state);
    }
    for (const auto &value : labels) {
        if (!value.isString() || value.toString().size() > 200)
            return fail(error, QStringLiteral("Некорректная подпись операции истории."));
        result.labels.append(value.toString());
    }
    result.index = int(indexValue);
    if (!samePersistentState(result.states[result.index], current))
        return fail(error, QStringLiteral("Текущее состояние не совпадает с историей документа."));
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
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return fail(error, file.errorString());
    if (!image.save(&file, "PNG")) {
        file.cancelWriting();
        return fail(error, QStringLiteral("Не удалось записать PNG."));
    }
    if (!file.commit())
        return fail(error, file.errorString());
    return true;
}
