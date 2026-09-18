#include "layermodel.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QImageReader>
#include <QPainter>
#include <QUuid>

namespace {
/// Записывает диагностическое сообщение и возвращает `false` из проверок кодека.
bool fail(QString *error, const QString &message) {
    if (error)
        *error = message;
    return false;
}

/// Выводит растровое содержимое с общими смещением, видимостью и непрозрачностью записи.
class RasterLayerRenderer final : public LayerRenderer {
public:
    /// Рисует QImage в координатах документа, не изменяя содержимое.
    void render(QPainter &painter,
                const LayerEntry &entry,
                const LayerContent &content,
                const LayerRenderContext &context) const override {
        const auto *raster = dynamic_cast<const RasterLayerContent *>(&content);
        if (!raster)
            return;
        painter.save();
        painter.resetTransform();
        painter.setClipRect(context.deviceClip);
        painter.setTransform(context.documentToDevice);
        painter.setOpacity(qBound(0, entry.opacity, 100) / 100.0);
        painter.drawImage(entry.offset, raster->image);
        painter.restore();
    }
};

/// Преобразует растровое содержимое в PNG-ресурс контейнера и обратно.
class RasterLayerCodec final : public LayerCodec {
public:
    /// Кодирует изображение и записывает свойства его альфа-канала в манифест.
    bool encode(const LayerContent &content,
                const QString &resourceRoot,
                QJsonObject *manifest,
                QHash<QString, QByteArray> *resources,
                QString *error) const override {
        const auto *raster = dynamic_cast<const RasterLayerContent *>(&content);
        if (!raster)
            return fail(error, QCoreApplication::translate("LayerModel", "Некорректное растровое содержимое слоя."));
        QByteArray png;
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        if (!raster->image.save(&buffer, "PNG"))
            return fail(error, QCoreApplication::translate("LayerModel", "Не удалось закодировать растровый слой."));
        const QString path = resourceRoot + QStringLiteral("/content.png");
        resources->insert(path, png);
        *manifest = QJsonObject{{"image", path},
                                {"transparencyAvailable", raster->transparencyAvailable},
                                {"alphaLocked", raster->alphaLocked}};
        return true;
    }

    std::shared_ptr<LayerContent> decode(const QJsonObject &manifest,
                                         const LayerResourceReader &resourceReader,
                                         QSize canvasSize,
                                         QString *error) const override {
        const QString path = manifest.value("image").toString();
        if (path.isEmpty() || !manifest.value("transparencyAvailable").isBool() ||
            !manifest.value("alphaLocked").isBool()) {
            fail(error, QCoreApplication::translate("LayerModel", "Некорректный манифест растрового слоя."));
            return {};
        }
        QByteArray png = resourceReader(path);
        QBuffer buffer(&png);
        buffer.open(QIODevice::ReadOnly);
        QImageReader reader(&buffer, "PNG");
        if (reader.size() != canvasSize) {
            fail(error, QCoreApplication::translate("LayerModel", "Размер растрового слоя не совпадает с холстом."));
            return {};
        }
        QImage image = reader.read();
        if (image.isNull()) {
            fail(error, reader.errorString());
            return {};
        }
        auto result = std::make_shared<RasterLayerContent>();
        result->image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        result->transparencyAvailable = manifest.value("transparencyAvailable").toBool();
        result->alphaLocked = manifest.value("alphaLocked").toBool();
        return result;
    }
};
} // namespace

const QString &LayerTypes::raster() {
    static const QString id = QStringLiteral("raster");
    return id;
}

bool LayerEntry::operator==(const LayerEntry &other) const {
    const bool contentEqual = content == other.content ||
                              (content && other.content && content->typeId() == other.content->typeId() &&
                               content->equals(*other.content));
    return id == other.id && typeId == other.typeId && name == other.name && visible == other.visible &&
           locked == other.locked && opacity == other.opacity && offset == other.offset && contentEqual;
}

LayerStack LayerStack::singleRaster(
    const QImage &image, const QString &name, bool transparencyAvailable, bool alphaLocked) {
    LayerStack result;
    LayerEntry entry;
    entry.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    entry.typeId = LayerTypes::raster();
    entry.name = name;
    auto content = std::make_shared<RasterLayerContent>();
    content->image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    content->transparencyAvailable = transparencyAvailable;
    content->alphaLocked = alphaLocked;
    entry.content = content;
    result.entries_.append(entry);
    result.activeLayerId_ = entry.id;
    return result;
}

const QVector<LayerEntry> &LayerStack::entries() const { return entries_; }
QVector<LayerEntry> &LayerStack::entries() { return entries_; }
const QString &LayerStack::activeLayerId() const { return activeLayerId_; }

bool LayerStack::setActiveLayerId(const QString &id) {
    for (const auto &entry : entries_)
        if (entry.id == id) {
            activeLayerId_ = id;
            return true;
        }
    return false;
}

const LayerEntry *LayerStack::activeEntry() const {
    for (const auto &entry : entries_)
        if (entry.id == activeLayerId_)
            return &entry;
    return nullptr;
}

LayerEntry *LayerStack::activeEntry() {
    for (auto &entry : entries_)
        if (entry.id == activeLayerId_)
            return &entry;
    return nullptr;
}

LayerContent *LayerStack::editableActiveContent(LayerCapability capability) {
    LayerEntry *entry = activeEntry();
    if (!entry || entry->locked || !entry->visible || !entry->content ||
        !LayerTypeRegistry::instance().supports(*entry, capability))
        return nullptr;
    if (!entry->content.unique())
        entry->content = entry->content->clone();
    return entry->content.get();
}

qint64 LayerStack::estimatedBytes() const {
    qint64 result = 0;
    for (const auto &entry : entries_)
        if (entry.content)
            result += entry.content->estimatedBytes();
    return result;
}

bool LayerStack::operator==(const LayerStack &other) const {
    return activeLayerId_ == other.activeLayerId_ && entries_ == other.entries_;
}

LayerTypeRegistry &LayerTypeRegistry::instance() {
    static LayerTypeRegistry registry;
    return registry;
}

LayerTypeRegistry::LayerTypeRegistry() {
    LayerType raster;
    raster.id = LayerTypes::raster();
    raster.capabilities =
        LayerCapability::RasterPainting | LayerCapability::Transparency | LayerCapability::Thumbnail;
    raster.factory = [] { return std::make_shared<RasterLayerContent>(); };
    raster.renderer = std::make_shared<RasterLayerRenderer>();
    raster.codec = std::make_shared<RasterLayerCodec>();
    raster.rasterReader = [](const LayerContent &content) {
        const auto *rasterContent = dynamic_cast<const RasterLayerContent *>(&content);
        return rasterContent ? &rasterContent->image : nullptr;
    };
    raster.rasterEditor = [](LayerContent &content) {
        auto *rasterContent = dynamic_cast<RasterLayerContent *>(&content);
        return rasterContent ? &rasterContent->image : nullptr;
    };
    registerType(raster);
}

bool LayerTypeRegistry::registerType(const LayerType &type) {
    if (type.id.isEmpty() || !type.factory || !type.renderer || !type.codec || types_.contains(type.id))
        return false;
    types_.insert(type.id, type);
    return true;
}

const LayerType *LayerTypeRegistry::type(const QString &id) const {
    const auto iterator = types_.constFind(id);
    return iterator == types_.constEnd() ? nullptr : &iterator.value();
}

bool LayerTypeRegistry::supports(const LayerEntry &entry, LayerCapability capability) const {
    const LayerType *descriptor = type(entry.typeId);
    return descriptor && descriptor->capabilities.testFlag(capability);
}

void LayerCompositor::render(QPainter &painter, const LayerStack &layers, const LayerRenderContext &context) {
    for (const auto &entry : layers.entries()) {
        if (!entry.visible || entry.opacity <= 0 || !entry.content)
            continue;
        const LayerType *descriptor = LayerTypeRegistry::instance().type(entry.typeId);
        if (descriptor && descriptor->renderer)
            descriptor->renderer->render(painter, entry, *entry.content, context);
    }
}

QImage LayerCompositor::compose(const LayerStack &layers, QSize canvasSize, const QColor &background) {
    QImage result(canvasSize, QImage::Format_ARGB32_Premultiplied);
    result.fill(background);
    QPainter painter(&result);
    LayerRenderContext context;
    context.documentToDevice = QTransform();
    context.deviceClip = result.rect();
    context.quality = LayerRenderContext::Final;
    render(painter, layers, context);
    return result;
}

QImage LayerCompositor::thumbnail(const LayerStack &layers, QSize canvasSize, QSize targetSize) {
    if (!targetSize.isValid())
        return {};
    QImage result(targetSize, QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);
    QPainter painter(&result);
    const QSizeF fitted = QSizeF(canvasSize).scaled(targetSize, Qt::KeepAspectRatio);
    const QPointF topLeft((targetSize.width() - fitted.width()) / 2.0,
                          (targetSize.height() - fitted.height()) / 2.0);
    LayerRenderContext context;
    context.documentToDevice.translate(topLeft.x(), topLeft.y());
    context.documentToDevice.scale(fitted.width() / canvasSize.width(), fitted.height() / canvasSize.height());
    context.deviceClip = result.rect();
    context.scale = fitted.width() / canvasSize.width();
    context.quality = LayerRenderContext::Thumbnail;
    render(painter, layers, context);
    return result;
}

QString RasterLayerContent::typeId() const { return LayerTypes::raster(); }

std::shared_ptr<LayerContent> RasterLayerContent::clone() const {
    return std::make_shared<RasterLayerContent>(*this);
}

bool RasterLayerContent::equals(const LayerContent &other) const {
    const auto *raster = dynamic_cast<const RasterLayerContent *>(&other);
    return raster && image == raster->image && transparencyAvailable == raster->transparencyAvailable &&
           alphaLocked == raster->alphaLocked;
}

qint64 RasterLayerContent::estimatedBytes() const { return image.sizeInBytes(); }
    /// Декодирует проверенный PNG ожидаемого размера и восстанавливает свойства прозрачности.
