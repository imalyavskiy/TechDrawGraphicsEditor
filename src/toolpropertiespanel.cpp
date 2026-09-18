#include "toolpropertiespanel.h"

#include "drawingtoolsettings.h"
#include "rolloutsection.h"

#include <QtWidgets>

/// Рисует фактический круглый наконечник активного инструмента в панели параметров.
class StrokePreview final : public QWidget {
public:
    /// Создаёт квадратную область предпросмотра фиксированного минимального размера.
    explicit StrokePreview(QWidget *parent = nullptr) : QWidget(parent) {
        setObjectName("strokePreview");
        setMinimumHeight(72);
    }

    /// Заменяет параметры и цвет отображаемого наконечника.
    void setPreview(const DrawingToolSettings &settings, const QColor &color, bool softEdge) {
        settings_ = settings;
        color_ = color;
        softEdge_ = softEdge;
        update();
    }

protected:
    /// Рисует шахматный фон и круг с жёстким либо плавным краем.
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.fillRect(rect(), QColor("#ffffff"));
        const int cell = 8;
        for (int y = 0; y < height(); y += cell)
            for (int x = 0; x < width(); x += cell)
                if (((x / cell) + (y / cell)) % 2)
                    painter.fillRect(QRect(x, y, cell, cell), QColor("#e8ebef"));
        painter.setRenderHint(QPainter::Antialiasing);
        const qreal radius = qBound(3.0, settings_.width / 2.0, qMin(width(), height()) / 2.0 - 8.0);
        const QPointF center(width() / 2.0, height() / 2.0);
        QColor solid = color_;
        solid.setAlphaF(settings_.opacity / 100.0);
        painter.setPen(Qt::NoPen);
        if (softEdge_ && settings_.hardness < 100) {
            QRadialGradient gradient(center, radius);
            const qreal hardStop = qBound(0.0, settings_.hardness / 100.0, 0.999);
            gradient.setColorAt(0, solid);
            gradient.setColorAt(hardStop, solid);
            QColor transparent = solid;
            transparent.setAlpha(0);
            gradient.setColorAt(1, transparent);
            painter.setBrush(gradient);
        } else {
            painter.setBrush(solid);
        }
        painter.drawEllipse(center, radius, radius);
        painter.setPen(QColor("#8e949d"));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(rect().adjusted(0, 0, -1, -1));
    }

private:
    DrawingToolSettings settings_;
    QColor color_ = Qt::black;
    bool softEdge_ = false;
};

ToolPropertiesPanel::ToolPropertiesPanel(DrawingToolSettingsModel *model, QWidget *parent)
    : QWidget(parent), model_(model) {
    setObjectName("toolPropertiesPanel");
    setMinimumWidth(220);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(3, 4, 3, 4);
    outer->setSpacing(5);
    title_ = new QLabel(this);
    title_->setObjectName("toolPropertiesTitle");
    QFont titleFont = title_->font();
    titleFont.setBold(true);
    title_->setFont(titleFont);
    unavailable_ = new QLabel(this);
    unavailable_->setObjectName("drawingTargetStatus");
    unavailable_->setWordWrap(true);
    unavailable_->setStyleSheet("color:#a33b32;");
    outer->addWidget(title_);
    outer->addWidget(unavailable_);

    moveSection_ = new RolloutSection(tr("Цель перемещения"), "moveTargetSettings", this);
    auto *moveForm = new QFormLayout(moveSection_->contentWidget());
    moveForm->setContentsMargins(7, 6, 7, 7);
    moveTarget_ = new QComboBox(moveSection_);
    moveTarget_->setObjectName("moveTarget");
    moveTarget_->addItems({tr("Направляющие"), tr("Выбранный слой")});
    moveForm->addRow(tr("Перемещать"), moveTarget_);
    outer->addWidget(moveSection_);

    mainSection_ = new RolloutSection(tr("Основные параметры"), "strokeMainSettings", this);
    auto *mainForm = new QFormLayout(mainSection_->contentWidget());
    mainForm->setContentsMargins(7, 6, 7, 7);
    width_ = new QSpinBox(mainSection_);
    width_->setObjectName("strokeWidth");
    width_->setRange(1, 200);
    width_->setSuffix(tr(" px"));
    width_->setKeyboardTracking(false);
    mainForm->addRow(tr("Ширина"), width_);
    opacity_ = new QSpinBox(mainSection_);
    opacity_->setObjectName("strokeOpacity");
    opacity_->setRange(1, 100);
    opacity_->setSuffix(tr(" %"));
    opacity_->setKeyboardTracking(false);
    opacityLabel_ = new QLabel(tr("Непрозрачность"), mainSection_);
    mainForm->addRow(opacityLabel_, opacity_);
    auto *colors = new QWidget(mainSection_);
    auto *colorsLayout = new QHBoxLayout(colors);
    colorsLayout->setContentsMargins(0, 0, 0, 0);
    front_ = new QPushButton(colors);
    front_->setObjectName("frontColor");
    front_->setToolTip(tr("Основной цвет (Front)"));
    back_ = new QPushButton(colors);
    back_->setObjectName("backColor");
    back_->setToolTip(tr("Фоновый цвет и цвет ластика (Back)"));
    auto *swap = new QToolButton(colors);
    swap->setObjectName("swapColorsButton");
    swap->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    swap->setToolTip(tr("Поменять цвета местами (X)"));
    colorsLayout->addWidget(front_);
    colorsLayout->addWidget(back_);
    colorsLayout->addWidget(swap);
    colorsLayout->addStretch();
    mainForm->addRow(tr("Цвет"), colors);
    outer->addWidget(mainSection_);

    detailsSection_ = new RolloutSection(tr("Наконечник"), "strokeTipSettings", this);
    auto *details = new QFormLayout(detailsSection_->contentWidget());
    details->setContentsMargins(7, 6, 7, 7);
    preview_ = new StrokePreview(detailsSection_);
    details->addRow(preview_);
    hardness_ = new QSpinBox(detailsSection_);
    hardness_->setObjectName("strokeHardness");
    hardness_->setRange(0, 100);
    hardness_->setSuffix(tr(" %"));
    hardness_->setKeyboardTracking(false);
    hardnessLabel_ = new QLabel(tr("Жёсткость"), detailsSection_);
    details->addRow(hardnessLabel_, hardness_);
    spacing_ = new QSpinBox(detailsSection_);
    spacing_->setObjectName("strokeSpacing");
    spacing_->setRange(1, 100);
    spacing_->setSuffix(tr(" %"));
    spacing_->setKeyboardTracking(false);
    spacing_->setToolTip(tr("Расстояние между отпечатками в процентах диаметра"));
    details->addRow(tr("Шаг"), spacing_);
    strength_ = new QSpinBox(detailsSection_);
    strength_->setObjectName("eraserStrength");
    strength_->setRange(1, 100);
    strength_->setSuffix(tr(" %"));
    strength_->setKeyboardTracking(false);
    strengthLabel_ = new QLabel(tr("Сила"), detailsSection_);
    details->addRow(strengthLabel_, strength_);
    eraserMode_ = new QLabel(detailsSection_);
    eraserMode_->setObjectName("eraserMode");
    eraserMode_->setWordWrap(true);
    resultLabel_ = new QLabel(tr("Результат"), detailsSection_);
    details->addRow(resultLabel_, eraserMode_);
    outer->addWidget(detailsSection_);
    outer->addStretch();

    connect(width_, qOverload<int>(&QSpinBox::valueChanged), model_, &DrawingToolSettingsModel::setWidth);
    connect(opacity_, qOverload<int>(&QSpinBox::valueChanged), model_, &DrawingToolSettingsModel::setOpacity);
    connect(hardness_, qOverload<int>(&QSpinBox::valueChanged), model_, &DrawingToolSettingsModel::setHardness);
    connect(spacing_, qOverload<int>(&QSpinBox::valueChanged), model_, &DrawingToolSettingsModel::setSpacing);
    connect(strength_, qOverload<int>(&QSpinBox::valueChanged), model_, &DrawingToolSettingsModel::setStrength);
    connect(moveTarget_, qOverload<int>(&QComboBox::currentIndexChanged), this,
            &ToolPropertiesPanel::moveTargetRequested);
    connect(front_, &QPushButton::clicked, this, &ToolPropertiesPanel::frontColorRequested);
    connect(back_, &QPushButton::clicked, this, &ToolPropertiesPanel::backColorRequested);
    connect(swap, &QToolButton::clicked, this, &ToolPropertiesPanel::swapColorsRequested);
    connect(model_, &DrawingToolSettingsModel::activeToolChanged, this, &ToolPropertiesPanel::refresh);
    connect(model_, &DrawingToolSettingsModel::settingsChanged, this, &ToolPropertiesPanel::refresh);
    connect(model_, &DrawingToolSettingsModel::targetContextChanged, this, &ToolPropertiesPanel::refresh);
    refresh();
}

void ToolPropertiesPanel::setColors(const QColor &front, const QColor &back) {
    frontColor_ = front;
    backColor_ = back;
    updateColorButton(front_, frontColor_);
    updateColorButton(back_, backColor_);
    refresh();
}

void ToolPropertiesPanel::focusWidth() {
    width_->setFocus(Qt::ShortcutFocusReason);
    width_->selectAll();
}

void ToolPropertiesPanel::setCanvasTool(int tool) {
    if (canvasTool_ == tool)
        return;
    canvasTool_ = tool;
    refresh();
}

void ToolPropertiesPanel::setMoveTarget(int target) {
    QSignalBlocker blocker(moveTarget_);
    moveTarget_->setCurrentIndex(qBound(0, target, 1));
}

void ToolPropertiesPanel::refresh() {
    const int tool = model_->activeTool();
    const bool paints = tool >= DrawingToolSettingsModel::Pencil && tool < DrawingToolSettingsModel::PaintToolCount;
    const bool move = canvasTool_ == 3;
    const auto &context = model_->targetContext();
    const bool enabled = paints && context.editable;
    setEnabled(move || enabled);
    title_->setEnabled(true);
    unavailable_->setEnabled(true);
    moveSection_->setVisible(move);
    mainSection_->setVisible(paints && !move);
    detailsSection_->setVisible(paints && !move);
    if (move) {
        title_->setText(tr("Перемещение"));
        unavailable_->hide();
        return;
    }
    if (!paints) {
        title_->setText(tr("Параметры рисования"));
        unavailable_->setText(tr("Выбран вспомогательный режим"));
        unavailable_->show();
        return;
    }

    static const QStringList names{tr("Карандаш"), tr("Кисть"), tr("Ластик")};
    title_->setText(names[tool]);
    unavailable_->setText(context.editable ? QString() : context.unavailableReason);
    unavailable_->setVisible(!context.editable);
    const auto &settings = model_->settingsFor(tool);
    const QSignalBlocker widthBlock(width_), opacityBlock(opacity_), hardnessBlock(hardness_), spacingBlock(spacing_),
        strengthBlock(strength_);
    width_->setValue(settings.width);
    opacity_->setValue(settings.opacity);
    hardness_->setValue(settings.hardness);
    spacing_->setValue(settings.spacing);
    strength_->setValue(settings.strength);

    const bool eraser = tool == DrawingToolSettingsModel::Eraser;
    const bool softTip = tool != DrawingToolSettingsModel::Pencil;
    opacityLabel_->setVisible(!eraser);
    opacity_->setVisible(!eraser);
    hardnessLabel_->setVisible(softTip);
    hardness_->setVisible(softTip);
    strengthLabel_->setVisible(eraser);
    strength_->setVisible(eraser);
    resultLabel_->setVisible(eraser);
    eraserMode_->setVisible(eraser);
    front_->setVisible(!eraser);
    back_->setVisible(eraser);
    if (eraser)
        eraserMode_->setText(context.supportsTransparency && !context.alphaLocked ? tr("До прозрачности")
                                                                                 : tr("Цветом Back"));
    preview_->setPreview(settings, eraser ? backColor_ : frontColor_, softTip);
}

void ToolPropertiesPanel::updateColorButton(QPushButton *button, const QColor &color) {
    QPixmap swatch(28, 22);
    swatch.fill(color);
    QPainter painter(&swatch);
    painter.setPen(QColor("#8e949d"));
    painter.drawRect(0, 0, swatch.width() - 1, swatch.height() - 1);
    button->setIcon(QIcon(swatch));
    button->setIconSize(swatch.size());
    button->setFixedWidth(36);
}
