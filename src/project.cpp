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
#include <private/qzipreader_p.h>
#include <private/qzipwriter_p.h>
#include <cmath>

namespace {
constexpr qint64 maxProjectBytes=512000000;
bool fail(QString *error, const QString &text) { if (error) *error = text; return false; }
bool encode(const QImage &image,QByteArray *png,QString *error) {
    QBuffer buffer(png);buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer,"PNG")) return fail(error,QStringLiteral("Не удалось создать PNG."));
    return true;
}
bool decode(QIODevice *device, QImage *image, QString *error) {
    QImageReader reader(device, "PNG");
    if (!Project::validSize(reader.size()))
        return fail(error, QStringLiteral("Размер изображения недопустим. Максимум: 8192 по стороне и 16 млн пикселей."));
    QImage result = reader.read();
    if (result.isNull()) return fail(error, reader.errorString());
    *image = result.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    return true;
}
QJsonObject perspectiveJson(const DrawingState &state) {
    return QJsonObject{{"x",state.vanishing.x()},{"y",state.vanishing.y()},{"horizonY",state.horizonY}};
}
bool parsePerspective(const QJsonValue &value,DrawingState *state,QString *error) {
    if (!value.isObject()) return fail(error,QStringLiteral("Отсутствуют параметры перспективы."));
    const auto perspective=value.toObject();
    if (!perspective.value("x").isDouble()||!perspective.value("y").isDouble())
        return fail(error,QStringLiteral("Некорректные параметры перспективы."));
    const double x=perspective.value("x").toDouble(),y=perspective.value("y").toDouble();
    const double horizonY=perspective.value("horizonY").toDouble(y);
    if (!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(horizonY)||std::abs(x)>1000000||std::abs(y)>1000000||std::abs(horizonY)>1000000)
        return fail(error,QStringLiteral("Параметры перспективы вне допустимого диапазона."));
    state->horizonY=horizonY;state->vanishing=QPointF(x,y);
    return true;
}
bool validState(const DrawingState &state) {
    return Project::validSize(state.image.size())&&!state.image.isNull()&&std::isfinite(state.vanishing.x())&&std::isfinite(state.horizonY)&&
        std::isfinite(state.vanishing.y())&&std::abs(state.vanishing.x())<=1000000&&std::abs(state.vanishing.y())<=1000000&&std::abs(state.horizonY)<=1000000&&
        std::isfinite(state.rayStepDegrees)&&state.rayStepDegrees>=1&&state.rayStepDegrees<=30&&state.gridColor.isValid()&&state.rayGap>=0&&state.rayGap<=200&&
        state.rayStartOpacity>=0&&state.rayStartOpacity<=100&&state.rayEndOpacity>=0&&state.rayEndOpacity<=100&&
        state.rayFadeLength>=0&&state.rayFadeLength<=500;
}
bool samePersistentState(const DrawingState &a,const DrawingState &b) {
    return a.image==b.image&&a.vanishing==b.vanishing&&qFuzzyCompare(a.horizonY+1,b.horizonY+1);
}
}

bool Project::validSize(QSize size) {
    return size.width() > 0 && size.height() > 0 && size.width() <= 8192 &&
           size.height() <= 8192 && qint64(size.width()) * size.height() <= 16000000;
}

bool Project::save(const QString &path,const DrawingState &state,QString *error) {
    DrawingHistory history;history.states.append(state);return save(path,history,error);
}

bool Project::save(const QString &path,const DrawingHistory &history,QString *error) {
    if (history.states.isEmpty()||history.states.size()>31||history.labels.size()!=history.states.size()-1||history.index<0||history.index>=history.states.size())
        return fail(error,QStringLiteral("Некорректная история документа."));
    const QSize size=history.states[history.index].image.size();
    for (const auto &state:history.states)
        if (!validState(state)||state.image.size()!=size) return fail(error,QStringLiteral("История содержит недопустимое состояние документа."));

    QByteArray currentPng;
    if (!encode(history.states[history.index].image,&currentPng,error)) return false;
    QVector<QPair<QString,QByteArray>> historyImages;
    QJsonArray states;
    QString previousImage;
    for (int i=0;i<history.states.size();++i) {
        QString imagePath;
        if (i==history.index) imagePath=QStringLiteral("drawing.png");
        else if (i>0&&history.states[i].image==history.states[i-1].image) imagePath=previousImage;
        else {
            imagePath=QStringLiteral("history/state-%1.png").arg(i,4,10,QChar('0'));
            QByteArray png;if (!encode(history.states[i].image,&png,error)) return false;
            historyImages.append(qMakePair(imagePath,png));
        }
        previousImage=imagePath;
        states.append(QJsonObject{{"image",imagePath},{"perspective",perspectiveJson(history.states[i])}});
    }
    QJsonArray labels;for (const auto &label:history.labels) labels.append(label);
    QJsonObject historyJson{{"index",history.index},{"states",states},{"labels",labels}};
    const DrawingState &current=history.states[history.index];
    QJsonObject metadata{{"format","Drawing"},{"version",3},{"width",size.width()},{"height",size.height()},
        {"image","drawing.png"},{"perspective",perspectiveJson(current)},{"history",historyJson}};

    QByteArray archive;QBuffer archiveBuffer(&archive);archiveBuffer.open(QIODevice::WriteOnly);
    {
        QZipWriter zip(&archiveBuffer);zip.setCompressionPolicy(QZipWriter::NeverCompress);
        zip.addFile("drawing.png",currentPng);
        for (const auto &item:historyImages) zip.addFile(item.first,item.second);
        zip.addFile("project.json",QJsonDocument(metadata).toJson());zip.close();
        if (zip.status()!=QZipWriter::NoError) return fail(error,QStringLiteral("Ошибка создания контейнера DRW."));
    }
    if (archive.size()>maxProjectBytes) return fail(error,QStringLiteral("Проект с историей превышает ограничение 512 МБ."));
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return fail(error,file.errorString());
    if (file.write(archive)!=archive.size()) { file.cancelWriting();return fail(error,file.errorString()); }
    if (!file.commit()) return fail(error,file.errorString());
    return true;
}

bool Project::load(const QString &path,DrawingState *state,QString *error) {
    DrawingHistory history;if (!load(path,&history,error)) return false;*state=history.states[history.index];return true;
}

bool Project::load(const QString &path,DrawingHistory *history,QString *error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return fail(error,file.errorString());
    if (file.size()>maxProjectBytes) return fail(error,QStringLiteral("Файл слишком велик для прототипа (512 МБ)."));
    QZipReader zip(&file);const auto files=zip.fileInfoList();
    QHash<QString,int> counts;QHash<QString,qint64> sizes;qint64 unpacked=0;
    for (const auto &entry:files) if (entry.isFile) { ++counts[entry.filePath];sizes[entry.filePath]=entry.size;unpacked+=entry.size; }
    if (zip.status()!=QZipReader::NoError||unpacked>maxProjectBytes||counts.value("drawing.png")!=1||counts.value("project.json")!=1||
        sizes.value("drawing.png")<1||sizes.value("drawing.png")>80000000||sizes.value("project.json")<1||sizes.value("project.json")>1048576)
        return fail(error,QStringLiteral("Повреждённый DRW: нужны допустимые drawing.png и project.json."));

    QJsonParseError parseError;const auto document=QJsonDocument::fromJson(zip.fileData("project.json"),&parseError);
    if (parseError.error!=QJsonParseError::NoError||!document.isObject()) return fail(error,QStringLiteral("Не удалось прочитать project.json."));
    const auto object=document.object();const double versionValue=object.value("version").toDouble(-1);
    if (object.value("format").toString()!="Drawing"||(versionValue!=1&&versionValue!=2&&versionValue!=3)||object.value("image").toString()!="drawing.png")
        return fail(error,QStringLiteral("Неизвестный формат или версия DRW."));
    const double width=object.value("width").toDouble(-1),height=object.value("height").toDouble(-1);
    if (width<1||height<1||width>8192||height>8192||width!=std::floor(width)||height!=std::floor(height)||!validSize(QSize(int(width),int(height))))
        return fail(error,QStringLiteral("Некорректный размер холста в проекте."));
    const QSize expectedSize{int(width),int(height)};

    QHash<QString,QImage> images;
    auto loadImage=[&](const QString &imagePath,QImage *image)->bool {
        static const QRegularExpression historyName(QStringLiteral("^history/state-[0-9]{4}\\.png$"));
        if (imagePath!="drawing.png"&&!historyName.match(imagePath).hasMatch()) return fail(error,QStringLiteral("Некорректная ссылка на изображение истории."));
        if (images.contains(imagePath)) { *image=images.value(imagePath);return true; }
        if (counts.value(imagePath)!=1||sizes.value(imagePath)<1||sizes.value(imagePath)>80000000) return fail(error,QStringLiteral("Отсутствует изображение истории."));
        QByteArray png=zip.fileData(imagePath);QBuffer buffer(&png);buffer.open(QIODevice::ReadOnly);QImage decoded;
        if (!decode(&buffer,&decoded,error)) return false;
        if (decoded.size()!=expectedSize) return fail(error,QStringLiteral("Размер изображения истории не совпадает с проектом."));
        images.insert(imagePath,decoded);*image=decoded;return true;
    };

    DrawingState current;
    if (!loadImage("drawing.png",&current.image)||!parsePerspective(object.value("perspective"),&current,error)) return false;
    DrawingHistory result;
    if (versionValue==1) { result.states.append(current);*history=result;return true; }

    const auto historyValue=object.value("history");
    if (!historyValue.isObject()) return fail(error,QStringLiteral("Отсутствует история документа."));
    const auto historyObject=historyValue.toObject();const auto states=historyObject.value("states").toArray();const auto labels=historyObject.value("labels").toArray();
    const double indexValue=historyObject.value("index").toDouble(-1);
    if (states.isEmpty()||states.size()>31||labels.size()!=states.size()-1||indexValue<0||indexValue>=states.size()||indexValue!=std::floor(indexValue))
        return fail(error,QStringLiteral("Некорректное оглавление истории документа."));
    for (const auto &value:states) {
        if (!value.isObject()) return fail(error,QStringLiteral("Некорректное состояние истории."));
        const auto stateObject=value.toObject();DrawingState state;
        if (!loadImage(stateObject.value("image").toString(),&state.image)||!parsePerspective(stateObject.value("perspective"),&state,error)) return false;
        result.states.append(state);
    }
    for (const auto &value:labels) {
        if (!value.isString()||value.toString().size()>200) return fail(error,QStringLiteral("Некорректная подпись операции истории."));
        result.labels.append(value.toString());
    }
    result.index=int(indexValue);
    if (!samePersistentState(result.states[result.index],current)) return fail(error,QStringLiteral("Текущее состояние не совпадает с историей документа."));
    *history=result;return true;
}

bool Project::loadPng(const QString &path,QImage *image,QString *error) {
    QFile file(path);if (!file.open(QIODevice::ReadOnly)) return fail(error,file.errorString());return decode(&file,image,error);
}

bool Project::exportPng(const QString &path,const QImage &image,QString *error) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return fail(error,file.errorString());
    if (!image.save(&file,"PNG")) { file.cancelWriting();return fail(error,QStringLiteral("Не удалось записать PNG.")); }
    if (!file.commit()) return fail(error,file.errorString());
    return true;
}
