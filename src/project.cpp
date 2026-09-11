#include "project.h"
#include <QBuffer>
#include <QFile>
#include <QImageReader>
#include <QImageWriter>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <private/qzipreader_p.h>
#include <private/qzipwriter_p.h>
#include <cmath>

namespace {
bool fail(QString *error, const QString &text) { if (error) *error = text; return false; }
bool decode(QIODevice *device, QImage *image, QString *error) {
    QImageReader reader(device, "PNG");
    if (!Project::validSize(reader.size()))
        return fail(error, QStringLiteral("Размер изображения недопустим. Максимум: 8192 по стороне и 16 млн пикселей."));
    QImage result = reader.read();
    if (result.isNull()) return fail(error, reader.errorString());
    *image = result.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    return true;
}
}

bool Project::validSize(QSize size) {
    return size.width() > 0 && size.height() > 0 && size.width() <= 8192 &&
           size.height() <= 8192 && qint64(size.width()) * size.height() <= 16000000;
}

bool Project::save(const QString &path, const DrawingState &state, QString *error) {
    if (!validSize(state.image.size()) || state.image.isNull()) return fail(error, QStringLiteral("Нет допустимого изображения."));
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    if (!state.image.save(&buffer, "PNG")) return fail(error, QStringLiteral("Не удалось создать PNG."));
    QJsonObject perspective{{"visible", state.gridVisible}, {"x", state.vanishing.x()},
        {"y", state.vanishing.y()}, {"rays", state.rays}, {"color", state.gridColor.name(QColor::HexArgb)}};
    QJsonObject metadata{{"format", "Drawing"}, {"version", 1}, {"width", state.image.width()},
        {"height", state.image.height()}, {"image", "drawing.png"}, {"perspective", perspective}};
    QByteArray archive;
    QBuffer archiveBuffer(&archive);
    archiveBuffer.open(QIODevice::WriteOnly);
    {
        QZipWriter zip(&archiveBuffer);
        zip.setCompressionPolicy(QZipWriter::NeverCompress);
        zip.addFile("drawing.png", png);
        zip.addFile("project.json", QJsonDocument(metadata).toJson());
        zip.close();
        if (zip.status() != QZipWriter::NoError) return fail(error, QStringLiteral("Ошибка создания контейнера DRW."));
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return fail(error, file.errorString());
    if (file.write(archive) != archive.size()) { file.cancelWriting(); return fail(error, file.errorString()); }
    if (!file.commit()) return fail(error, file.errorString());
    return true;
}

bool Project::load(const QString &path, DrawingState *state, QString *error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return fail(error, file.errorString());
    if (file.size() > 100000000) return fail(error, QStringLiteral("Файл слишком велик для прототипа (100 МБ)."));
    QZipReader zip(&file);
    const auto files = zip.fileInfoList();
    int pngCount = 0, jsonCount = 0;
    for (const auto &entry : files) {
        if (entry.filePath == "drawing.png") {
            if (!entry.isFile || entry.size < 1 || entry.size > 80000000) return fail(error, QStringLiteral("Недопустимый PNG в проекте."));
            ++pngCount;
        }
        if (entry.filePath == "project.json") {
            if (!entry.isFile || entry.size < 1 || entry.size > 65536) return fail(error, QStringLiteral("Недопустимые метаданные проекта."));
            ++jsonCount;
        }
    }
    if (zip.status() != QZipReader::NoError || pngCount != 1 || jsonCount != 1)
        return fail(error, QStringLiteral("Повреждённый DRW: нужны drawing.png и project.json."));
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(zip.fileData("project.json"), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) return fail(error, QStringLiteral("Не удалось прочитать project.json."));
    const auto object = document.object();
    if (object.value("format").toString() != "Drawing" || object.value("version").toDouble(-1) != 1 || object.value("image").toString() != "drawing.png")
        return fail(error, QStringLiteral("Неизвестный формат или версия DRW."));
    const double width = object.value("width").toDouble(-1), height = object.value("height").toDouble(-1);
    if (width < 1 || height < 1 || width > 8192 || height > 8192 || width != std::floor(width) || height != std::floor(height) || !validSize(QSize(int(width), int(height))))
        return fail(error, QStringLiteral("Некорректный размер холста в проекте."));
    DrawingState result;
    QByteArray png = zip.fileData("drawing.png");
    QBuffer buffer(&png);
    buffer.open(QIODevice::ReadOnly);
    if (!decode(&buffer, &result.image, error)) return false;
    if (result.image.size() != QSize(int(width), int(height))) return fail(error, QStringLiteral("Размеры PNG и project.json не совпадают."));
    const auto value = object.value("perspective");
    if (!value.isObject()) return fail(error, QStringLiteral("Отсутствуют параметры перспективы."));
    const auto perspective = value.toObject();
    if (!perspective.value("x").isDouble() || !perspective.value("y").isDouble() || !perspective.value("visible").isBool())
        return fail(error, QStringLiteral("Некорректные параметры перспективы."));
    const double x = perspective.value("x").toDouble(), y = perspective.value("y").toDouble();
    const double rays = perspective.value("rays").toDouble(-1);
    result.gridColor = QColor(perspective.value("color").toString());
    if (!std::isfinite(x) || !std::isfinite(y) || std::abs(x) > 1000000 || std::abs(y) > 1000000 || rays < 4 || rays > 64 || rays != std::floor(rays) || !result.gridColor.isValid())
        return fail(error, QStringLiteral("Параметры перспективы вне допустимого диапазона."));
    result.vanishing = QPointF(x, y);
    result.rays = int(rays);
    result.gridVisible = perspective.value("visible").toBool();
    *state = result;
    return true;
}

bool Project::loadPng(const QString &path, QImage *image, QString *error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return fail(error, file.errorString());
    return decode(&file, image, error);
}

bool Project::exportPng(const QString &path, const QImage &image, QString *error) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return fail(error, file.errorString());
    if (!image.save(&file, "PNG")) { file.cancelWriting(); return fail(error, QStringLiteral("Не удалось записать PNG.")); }
    if (!file.commit()) return fail(error, file.errorString());
    return true;
}
