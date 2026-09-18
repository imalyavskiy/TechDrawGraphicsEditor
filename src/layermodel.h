#pragma once

#include <QHash>
#include <QImage>
#include <QJsonObject>
#include <QPointF>
#include <QSharedPointer>
#include <QSize>
#include <QString>
#include <QTransform>
#include <QVector>
#include <functional>
#include <memory>

class QPainter;

enum class LayerCapability {
    RasterPainting = 0x01,
    Transparency = 0x02,
    Thumbnail = 0x04
};
Q_DECLARE_FLAGS(LayerCapabilities, LayerCapability)
Q_DECLARE_OPERATORS_FOR_FLAGS(LayerCapabilities)

class LayerContent {
public:
    virtual ~LayerContent() = default;
    virtual QString typeId() const = 0;
    virtual std::shared_ptr<LayerContent> clone() const = 0;
    virtual bool equals(const LayerContent &other) const = 0;
    virtual qint64 estimatedBytes() const = 0;
};

struct LayerEntry {
    QString id;
    QString typeId;
    QString name;
    bool visible = true;
    bool locked = false;
    int opacity = 100;
    QPointF offset;
    std::shared_ptr<LayerContent> content;

    bool operator==(const LayerEntry &other) const;
    bool operator!=(const LayerEntry &other) const { return !(*this == other); }
};

class LayerStack {
public:
    static LayerStack singleRaster(const QImage &image,
                                   const QString &name,
                                   bool transparencyAvailable,
                                   bool alphaLocked);
    const QVector<LayerEntry> &entries() const;
    QVector<LayerEntry> &entries();
    const QString &activeLayerId() const;
    bool setActiveLayerId(const QString &id);
    const LayerEntry *activeEntry() const;
    LayerEntry *activeEntry();
    LayerContent *editableActiveContent(LayerCapability capability);
    qint64 estimatedBytes() const;
    bool operator==(const LayerStack &other) const;
    bool operator!=(const LayerStack &other) const { return !(*this == other); }

private:
    QVector<LayerEntry> entries_;
    QString activeLayerId_;
};

struct LayerRenderContext {
    enum Quality { Interactive, Final, Thumbnail };
    QTransform documentToDevice;
    QRect deviceClip;
    qreal scale = 1.0;
    qreal dpi = 96.0;
    Quality quality = Interactive;
};

class LayerRenderer {
public:
    virtual ~LayerRenderer() = default;
    virtual void render(QPainter &painter,
                        const LayerEntry &entry,
                        const LayerContent &content,
                        const LayerRenderContext &context) const = 0;
};

using LayerResourceReader = std::function<QByteArray(const QString &path)>;

class LayerCodec {
public:
    virtual ~LayerCodec() = default;
    virtual bool encode(const LayerContent &content,
                        const QString &resourceRoot,
                        QJsonObject *manifest,
                        QHash<QString, QByteArray> *resources,
                        QString *error) const = 0;
    virtual std::shared_ptr<LayerContent> decode(const QJsonObject &manifest,
                                                 const LayerResourceReader &resourceReader,
                                                 QSize canvasSize,
                                                 QString *error) const = 0;
};

struct LayerType {
    QString id;
    LayerCapabilities capabilities;
    std::function<std::shared_ptr<LayerContent>()> factory;
    std::shared_ptr<const LayerRenderer> renderer;
    std::shared_ptr<const LayerCodec> codec;
    std::function<QImage *(LayerContent &)> rasterEditor;
};

class LayerTypeRegistry {
public:
    static LayerTypeRegistry &instance();
    bool registerType(const LayerType &type);
    const LayerType *type(const QString &id) const;
    bool supports(const LayerEntry &entry, LayerCapability capability) const;

private:
    LayerTypeRegistry();
    QHash<QString, LayerType> types_;
};

class LayerCompositor {
public:
    static void render(QPainter &painter, const LayerStack &layers, const LayerRenderContext &context);
    static QImage compose(const LayerStack &layers, QSize canvasSize, const QColor &background = Qt::transparent);
    static QImage thumbnail(const LayerStack &layers, QSize canvasSize, QSize targetSize);
};

class RasterLayerContent final : public LayerContent {
public:
    QImage image;
    bool transparencyAvailable = false;
    bool alphaLocked = true;

    QString typeId() const override;
    std::shared_ptr<LayerContent> clone() const override;
    bool equals(const LayerContent &other) const override;
    qint64 estimatedBytes() const override;
};

namespace LayerTypes {
const QString &raster();
}
