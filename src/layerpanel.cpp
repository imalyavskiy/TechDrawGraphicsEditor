#include "layerpanel.h"

#include "canvas.h"
#include <QtWidgets>

namespace {
QIcon visibilityIcon(bool visible) {
    QPixmap image(20, 20);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor("#3d4b5a"), 1.5));
    if (visible) {
        QPainterPath eye;
        eye.moveTo(2, 10);
        eye.cubicTo(6, 4, 14, 4, 18, 10);
        eye.cubicTo(14, 16, 6, 16, 2, 10);
        painter.drawPath(eye);
        painter.setBrush(QColor("#3d4b5a"));
        painter.drawEllipse(QPointF(10, 10), 2.4, 2.4);
    } else {
        painter.drawLine(3, 3, 17, 17);
        painter.drawLine(4, 14, 16, 6);
    }
    return QIcon(image);
}

QIcon lockIcon(bool locked) {
    QPixmap image(20, 20);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor("#3d4b5a"), 1.5));
    painter.drawRoundedRect(QRectF(4, 9, 12, 8), 1.5, 1.5);
    QPainterPath shackle;
    shackle.moveTo(7, 9);
    shackle.lineTo(7, 7);
    shackle.cubicTo(7, 2.5, 13, 2.5, 13, 7);
    shackle.lineTo(13, locked ? 9 : 7);
    if (!locked)
        shackle.lineTo(16, 7);
    painter.drawPath(shackle);
    return QIcon(image);
}

QPixmap layerThumbnail(const LayerEntry &entry, QSize canvasSize) {
    const QSize target(48, 48);
    QImage image(target, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    const int tile = 6;
    for (int y = 0; y < target.height(); y += tile)
        for (int x = 0; x < target.width(); x += tile)
            painter.fillRect(x,
                             y,
                             tile,
                             tile,
                             ((x / tile) + (y / tile)) % 2 ? QColor("#d7d7d7") : QColor("#f1f1f1"));
    const LayerType *type = LayerTypeRegistry::instance().type(entry.typeId);
    if (type && type->renderer && entry.content && canvasSize.isValid()) {
        const QSizeF fitted = QSizeF(canvasSize).scaled(target, Qt::KeepAspectRatio);
        LayerRenderContext context;
        context.documentToDevice.translate((target.width() - fitted.width()) / 2.0,
                                            (target.height() - fitted.height()) / 2.0);
        context.documentToDevice.scale(fitted.width() / canvasSize.width(), fitted.height() / canvasSize.height());
        context.deviceClip = image.rect();
        context.scale = fitted.width() / canvasSize.width();
        context.quality = LayerRenderContext::Thumbnail;
        LayerEntry preview = entry;
        preview.opacity = 100;
        preview.visible = true;
        type->renderer->render(painter, preview, *preview.content, context);
    }
    painter.setPen(QColor("#aeb4bc"));
    painter.drawRect(image.rect().adjusted(0, 0, -1, -1));
    return QPixmap::fromImage(image);
}
} // namespace

LayerPanel::LayerPanel(Canvas *canvas, QWidget *parent) : QWidget(parent), canvas_(canvas) {
    setObjectName("layerPanel");
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(7, 7, 7, 7);
    layout->setSpacing(6);
    list_ = new QListWidget(this);
    list_->setObjectName("layersList");
    list_->setSelectionMode(QAbstractItemView::SingleSelection);
    list_->setSpacing(2);
    layout->addWidget(list_, 1);

    auto *buttons = new QHBoxLayout;
    buttons->setSpacing(2);
    add_ = new QToolButton(this);
    add_->setObjectName("addLayer");
    add_->setText(tr("+"));
    add_->setToolTip(tr("Добавить растровый слой"));
    remove_ = new QToolButton(this);
    remove_->setObjectName("removeLayer");
    remove_->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
    remove_->setToolTip(tr("Удалить выбранный слой"));
    duplicate_ = new QToolButton(this);
    duplicate_->setObjectName("duplicateLayer");
    duplicate_->setIcon(style()->standardIcon(QStyle::SP_FileDialogNewFolder));
    duplicate_->setToolTip(tr("Дублировать выбранный слой"));
    up_ = new QToolButton(this);
    up_->setObjectName("raiseLayer");
    up_->setIcon(style()->standardIcon(QStyle::SP_ArrowUp));
    up_->setToolTip(tr("Поднять слой"));
    down_ = new QToolButton(this);
    down_->setObjectName("lowerLayer");
    down_->setIcon(style()->standardIcon(QStyle::SP_ArrowDown));
    down_->setToolTip(tr("Опустить слой"));
    buttons->addWidget(add_);
    buttons->addWidget(remove_);
    buttons->addWidget(duplicate_);
    buttons->addStretch();
    buttons->addWidget(up_);
    buttons->addWidget(down_);
    layout->addLayout(buttons);

    auto *form = new QFormLayout;
    opacity_ = new QSpinBox(this);
    opacity_->setObjectName("layerOpacity");
    opacity_->setRange(0, 100);
    opacity_->setSuffix(tr(" %"));
    opacity_->setKeyboardTracking(false);
    form->addRow(tr("Непрозрачность"), opacity_);
    addTransparency_ = new QPushButton(tr("Добавить прозрачность"), this);
    addTransparency_->setObjectName("addLayerTransparency");
    form->addRow(addTransparency_);
    alphaLocked_ = new QCheckBox(tr("Фиксировать прозрачность"), this);
    alphaLocked_->setObjectName("layerAlphaLocked");
    form->addRow(alphaLocked_);
    layout->addLayout(form);

    connect(add_, &QToolButton::clicked, canvas_, &Canvas::addRasterLayer);
    connect(remove_, &QToolButton::clicked, canvas_, &Canvas::removeActiveLayer);
    connect(duplicate_, &QToolButton::clicked, canvas_, &Canvas::duplicateActiveLayer);
    connect(up_, &QToolButton::clicked, canvas_, &Canvas::moveActiveLayerUp);
    connect(down_, &QToolButton::clicked, canvas_, &Canvas::moveActiveLayerDown);
    connect(list_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *current) {
        if (!updating_ && current)
            canvas_->selectLayer(current->data(Qt::UserRole).toString());
    });
    connect(opacity_, &QSpinBox::editingFinished, this, [this] {
        if (!updating_)
            canvas_->setLayerOpacity(canvas_->state().layers.activeLayerId(), opacity_->value());
    });
    connect(addTransparency_, &QPushButton::clicked, this, [this] {
        canvas_->addLayerTransparency(canvas_->state().layers.activeLayerId());
    });
    connect(alphaLocked_, &QCheckBox::toggled, this, [this](bool locked) {
        if (!updating_)
            canvas_->setLayerAlphaLocked(canvas_->state().layers.activeLayerId(), locked);
    });
    connect(canvas_, &Canvas::layersChanged, this, &LayerPanel::updateFromState);
    updateFromState();
}

void LayerPanel::updateFromState() {
    updating_ = true;
    list_->clear();
    const LayerStack &layers = canvas_->state().layers;
    for (int index = layers.entries().size() - 1; index >= 0; --index) {
        const LayerEntry &entry = layers.entries()[index];
        auto *item = new QListWidgetItem(list_);
        item->setData(Qt::UserRole, entry.id);
        auto *row = new QWidget(list_);
        row->setObjectName("layerRow");
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(3, 3, 3, 3);
        rowLayout->setSpacing(4);
        auto *preview = new QLabel(row);
        preview->setObjectName("layerThumbnail");
        preview->setPixmap(layerThumbnail(entry, canvas_->state().canvasSize));
        preview->setFixedSize(48, 48);
        auto *text = new QVBoxLayout;
        auto *name = new QLineEdit(entry.name, row);
        name->setObjectName("layerName");
        name->setProperty("layerId", entry.id);
        auto *type = new QLabel(entry.typeId == LayerTypes::raster() ? tr("Растровый слой") : entry.typeId, row);
        type->setObjectName("layerType");
        QFont typeFont = type->font();
        typeFont.setPointSizeF(qMax(7.0, typeFont.pointSizeF() - 1));
        type->setFont(typeFont);
        text->addWidget(name);
        text->addWidget(type);
        auto *visible = new QToolButton(row);
        visible->setObjectName("layerVisible");
        visible->setCheckable(true);
        visible->setChecked(entry.visible);
        visible->setIcon(visibilityIcon(entry.visible));
        visible->setToolTip(entry.visible ? tr("Скрыть слой") : tr("Показать слой"));
        auto *locked = new QToolButton(row);
        locked->setObjectName("layerLocked");
        locked->setCheckable(true);
        locked->setChecked(entry.locked);
        locked->setIcon(lockIcon(entry.locked));
        locked->setToolTip(entry.locked ? tr("Разблокировать слой") : tr("Заблокировать слой"));
        rowLayout->addWidget(preview);
        rowLayout->addLayout(text, 1);
        rowLayout->addWidget(visible);
        rowLayout->addWidget(locked);
        item->setSizeHint(QSize(200, 58));
        list_->setItemWidget(item, row);
        if (entry.id == layers.activeLayerId())
            list_->setCurrentItem(item);
        connect(name, &QLineEdit::editingFinished, this, [this, name, id = entry.id] {
            canvas_->renameLayer(id, name->text());
        });
        connect(visible, &QToolButton::toggled, this, [this, visible, id = entry.id](bool checked) {
            visible->setIcon(visibilityIcon(checked));
            canvas_->setLayerVisible(id, checked);
        });
        connect(locked, &QToolButton::toggled, this, [this, locked, id = entry.id](bool checked) {
            locked->setIcon(lockIcon(checked));
            canvas_->setLayerLocked(id, checked);
        });
    }
    const LayerEntry *active = layers.activeEntry();
    opacity_->setEnabled(active);
    opacity_->setValue(active ? active->opacity : 100);
    remove_->setEnabled(layers.entries().size() > 1);
    duplicate_->setEnabled(active);
    int activeIndex = -1;
    for (int index = 0; index < layers.entries().size(); ++index)
        if (layers.entries()[index].id == layers.activeLayerId())
            activeIndex = index;
    up_->setEnabled(activeIndex >= 0 && activeIndex < layers.entries().size() - 1);
    down_->setEnabled(activeIndex > 0);
    const auto *raster = active && active->typeId == LayerTypes::raster()
                             ? dynamic_cast<const RasterLayerContent *>(active->content.get())
                             : nullptr;
    addTransparency_->setVisible(raster && !raster->transparencyAvailable);
    alphaLocked_->setVisible(raster && raster->transparencyAvailable);
    alphaLocked_->setChecked(raster && raster->alphaLocked);
    updating_ = false;
}
