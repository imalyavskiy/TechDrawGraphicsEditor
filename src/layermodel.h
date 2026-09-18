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

/// Перечисляет необязательные операции, которые тип слоя предоставляет инструментам и интерфейсу.
enum class LayerCapability {
    RasterPainting = 0x01,
    Transparency = 0x02,
    Thumbnail = 0x04
};
Q_DECLARE_FLAGS(LayerCapabilities, LayerCapability)
Q_DECLARE_OPERATORS_FOR_FLAGS(LayerCapabilities)

class LayerContent {
public:
    /// Обеспечивает корректное разрушение содержимого через общий указатель.
    virtual ~LayerContent() = default;
    /// Возвращает устойчивый строковый идентификатор зарегистрированного типа.
    virtual QString typeId() const = 0;
    /// Создаёт отделённую копию для изменения снимка по правилу copy-on-write.
    virtual std::shared_ptr<LayerContent> clone() const = 0;
    /// Сравнивает данные двух экземпляров одного типа для истории и проверки изменений.
    virtual bool equals(const LayerContent &other) const = 0;
    /// Оценивает объём данных для ограничения истории документа.
    virtual qint64 estimatedBytes() const = 0;
};

/// Хранит общие свойства одного слоя независимо от конкретного типа его содержимого.
struct LayerEntry {
    QString id;
    QString typeId;
    QString name;
    bool visible = true;
    bool locked = false;
    int opacity = 100;
    QPointF offset;
    std::shared_ptr<LayerContent> content;

    /// Сравнивает общие свойства и делегирует сравнение данных объектам содержимого.
    bool operator==(const LayerEntry &other) const;
    /// Проверяет отличие общих свойств либо данных содержимого.
    bool operator!=(const LayerEntry &other) const { return !(*this == other); }
};

/// Представляет упорядоченный снизу вверх стек слоёв и устойчивый выбор активного слоя.
class LayerStack {
public:
    /// Создаёт совместимый однослойный документ из прежнего растрового изображения.
    static LayerStack singleRaster(const QImage &image,
                                   const QString &name,
                                   bool transparencyAvailable,
                                   bool alphaLocked);
    /// Возвращает неизменяемую последовательность слоёв в порядке композиции.
    const QVector<LayerEntry> &entries() const;
    /// Возвращает последовательность для структурных операций модели документа.
    QVector<LayerEntry> &entries();
    /// Возвращает устойчивый идентификатор активного слоя.
    const QString &activeLayerId() const;
    /// Выбирает существующий слой и отклоняет неизвестный идентификатор.
    bool setActiveLayerId(const QString &id);
    /// Находит активную запись без возможности изменения.
    const LayerEntry *activeEntry() const;
    /// Находит активную запись для изменения её общих свойств.
    LayerEntry *activeEntry();
    /// Проверяет доступность операции и отделяет содержимое от других снимков перед изменением.
    LayerContent *editableActiveContent(LayerCapability capability);
    /// Оценивает суммарный объём содержимого всех записей.
    qint64 estimatedBytes() const;
    /// Сравнивает порядок, выбор и все записи двух стеков.
    bool operator==(const LayerStack &other) const;
    /// Проверяет отличие структуры либо содержимого стеков.
    bool operator!=(const LayerStack &other) const { return !(*this == other); }

private:
    QVector<LayerEntry> entries_;
    QString activeLayerId_;
};

/// Описывает независимые от QWidget параметры одного прохода рендеринга слоёв.
struct LayerRenderContext {
    /// Позволяет типу слоя выбирать качество для экрана, экспорта или миниатюры.
    enum Quality { Interactive, Final, Thumbnail };
    QTransform documentToDevice;
    QRect deviceClip;
    qreal scale = 1.0;
    qreal dpi = 96.0;
    Quality quality = Interactive;
};

/// Интерфейс неизменяющего рендерера одного зарегистрированного типа содержимого.
class LayerRenderer {
public:
    /// Обеспечивает корректное разрушение реализации через общий указатель.
    virtual ~LayerRenderer() = default;
    /// Рисует одну запись в переданный painter с учётом общего контекста.
    virtual void render(QPainter &painter,
                        const LayerEntry &entry,
                        const LayerContent &content,
                        const LayerRenderContext &context) const = 0;
};

/// Функция чтения именованного ресурса из контейнера проекта.
using LayerResourceReader = std::function<QByteArray(const QString &path)>;

/// Интерфейс сериализации данных конкретного типа без знания формата в общем коде проекта.
class LayerCodec {
public:
    /// Обеспечивает корректное разрушение реализации через общий указатель.
    virtual ~LayerCodec() = default;
    /// Записывает манифест типа и его именованные двоичные ресурсы.
    virtual bool encode(const LayerContent &content,
                        const QString &resourceRoot,
                        QJsonObject *manifest,
                        QHash<QString, QByteArray> *resources,
                        QString *error) const = 0;
    /// Восстанавливает содержимое из манифеста и ресурсов контейнера.
    virtual std::shared_ptr<LayerContent> decode(const QJsonObject &manifest,
                                                 const LayerResourceReader &resourceReader,
                                                 QSize canvasSize,
                                                 QString *error) const = 0;
};

/// Собирает фабрику, возможности, рендерер, кодек и необязательный доступ к растровому редактору типа.
struct LayerType {
    QString id;
    LayerCapabilities capabilities;
    std::function<std::shared_ptr<LayerContent>()> factory;
    std::shared_ptr<const LayerRenderer> renderer;
    std::shared_ptr<const LayerCodec> codec;
    std::function<QImage *(LayerContent &)> rasterEditor;
};

/// Реестр связывает строковый идентификатор типа со всеми его операциями расширения.
class LayerTypeRegistry {
public:
    /// Возвращает единый реестр со встроенным растровым типом.
    static LayerTypeRegistry &instance();
    /// Регистрирует полностью описанный новый тип без замены существующего.
    bool registerType(const LayerType &type);
    /// Находит описание типа либо возвращает `nullptr`.
    const LayerType *type(const QString &id) const;
    /// Проверяет заявленную возможность типа указанной записи.
    bool supports(const LayerEntry &entry, LayerCapability capability) const;

private:
    /// Создаёт реестр и регистрирует обязательный растровый тип.
    LayerTypeRegistry();
    QHash<QString, LayerType> types_;
};

/// Выполняет общий проход видимых слоёв для экрана, экспорта и миниатюр.
class LayerCompositor {
public:
    /// Рисует видимые записи снизу вверх через зарегистрированные рендереры.
    static void render(QPainter &painter, const LayerStack &layers, const LayerRenderContext &context);
    /// Создаёт сведённое изображение документа заданного размера.
    static QImage compose(const LayerStack &layers, QSize canvasSize, const QColor &background = Qt::transparent);
    /// Создаёт вписанную миниатюру через тот же проход композиции.
    static QImage thumbnail(const LayerStack &layers, QSize canvasSize, QSize targetSize);
};

/// Первое пользовательское содержимое слоя хранит растр и правила изменения его альфа-канала.
class RasterLayerContent final : public LayerContent {
public:
    QImage image;
    bool transparencyAvailable = false;
    bool alphaLocked = true;

    /// Возвращает идентификатор встроенного растрового типа.
    QString typeId() const override;
    /// Создаёт копию с неявно разделяемым QImage для copy-on-write.
    std::shared_ptr<LayerContent> clone() const override;
    /// Сравнивает пиксели и свойства прозрачности.
    bool equals(const LayerContent &other) const override;
    /// Возвращает размер буфера изображения как оценку памяти.
    qint64 estimatedBytes() const override;
};

namespace LayerTypes {
/// Возвращает устойчивый идентификатор встроенного растрового типа.
const QString &raster();
}
