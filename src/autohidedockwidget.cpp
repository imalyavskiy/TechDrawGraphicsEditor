#include "autohidedockwidget.h"

#include <QtWidgets>

namespace {
/// Узкая вкладка рисует название панели вдоль соответствующего края окна.
class EdgeTabButton final : public QToolButton {
public:
    EdgeTabButton(const QString &text, Qt::DockWidgetArea area, QWidget *parent)
        : QToolButton(parent), text_(text), area_(area) {
        setCursor(Qt::PointingHandCursor);
        setToolTip(text);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

    QSize sizeHint() const override {
        return QSize(fontMetrics().height() + 6, fontMetrics().horizontalAdvance(text_) + 8);
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        QColor background = palette().color(QPalette::Button);
        if (isDown())
            background = background.darker(108);
        else if (underMouse())
            background = background.lighter(106);
        painter.fillRect(rect(), background);
        painter.setPen(QPen(palette().color(QPalette::Mid), 1));
        painter.drawRect(rect().adjusted(0, 0, -1, -1));
        painter.setPen(palette().color(QPalette::ButtonText));
        painter.setRenderHint(QPainter::TextAntialiasing);
        painter.translate(width() / 2.0, height() / 2.0);
        painter.rotate(area_ == Qt::LeftDockWidgetArea ? -90.0 : 90.0);
        painter.drawText(QRectF(-height() / 2.0, -width() / 2.0, height(), width()), Qt::AlignCenter, text_);
    }

private:
    QString text_;
    Qt::DockWidgetArea area_;
};

/// Строит простую пиктограмму канцелярской кнопки без зависимости от темы ОС.
QIcon pinIcon(bool pinned) {
    QPixmap image(18, 18);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor("#44515f"), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.translate(9, 9);
    painter.rotate(pinned ? 0.0 : -45.0);
    painter.drawLine(-4, -5, 4, -5);
    painter.drawLine(-2, -5, -2, 1);
    painter.drawLine(2, -5, 2, 1);
    painter.drawLine(-4, 1, 4, 1);
    painter.drawLine(0, 1, 0, 7);
    return QIcon(image);
}
} // namespace

AutoHideDockWidget::AutoHideDockWidget(const QString &title,
                                       Qt::DockWidgetArea area,
                                       const QString &settingsKey,
                                       bool initiallyVisible,
                                       int defaultWidth,
                                       QMainWindow *owner)
    : QDockWidget(title, owner), owner_(owner), area_(area), settingsKey_(settingsKey),
      preferredWidth_(defaultWidth) {
    setAllowedAreas(area);
    setFeatures(QDockWidget::NoDockWidgetFeatures);
    auto *emptyTitleBar = new QWidget(this);
    emptyTitleBar->setFixedHeight(0);
    setTitleBarWidget(emptyTitleBar);

    pinnedHost_ = new QWidget(this);
    pinnedLayout_ = new QVBoxLayout(pinnedHost_);
    pinnedLayout_->setContentsMargins(0, 0, 0, 0);
    pinnedLayout_->setSpacing(0);
    QDockWidget::setWidget(pinnedHost_);

    panelFrame_ = new QFrame(pinnedHost_);
    panelFrame_->setObjectName(settingsKey_ + "PanelFrame");
    panelFrame_->setFrameShape(QFrame::StyledPanel);
    panelFrame_->setAutoFillBackground(true);
    panelLayout_ = new QVBoxLayout(panelFrame_);
    panelLayout_->setContentsMargins(0, 0, 0, 0);
    panelLayout_->setSpacing(0);
    auto *header = new QWidget(panelFrame_);
    header->setObjectName(settingsKey_ + "PanelHeader");
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(7, 4, 4, 4);
    auto *titleLabel = new QLabel(title, header);
    titleLabel->setObjectName(settingsKey_ + "PanelTitle");
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    pinButton_ = new QToolButton(header);
    pinButton_->setObjectName(settingsKey_ + "PanelPin");
    pinButton_->setCheckable(true);
    pinButton_->setAutoRaise(true);
    if (area_ == Qt::LeftDockWidgetArea) {
        headerLayout->addWidget(titleLabel);
        headerLayout->addStretch();
        headerLayout->addWidget(pinButton_);
    } else {
        headerLayout->addWidget(pinButton_);
        headerLayout->addWidget(titleLabel);
        headerLayout->addStretch();
    }
    panelLayout_->addWidget(header);
    pinnedLayout_->addWidget(panelFrame_);

    overlay_ = new QFrame(owner_);
    overlay_->setObjectName(settingsKey_ + "AutoHideOverlay");
    overlay_->setFrameShape(QFrame::StyledPanel);
    overlay_->setLineWidth(1);
    overlay_->setAutoFillBackground(true);
    overlayLayout_ = new QVBoxLayout(overlay_);
    overlayLayout_->setContentsMargins(0, 0, 0, 0);
    overlay_->hide();

    tabDock_ = new QDockWidget(owner_);
    tabDock_->setObjectName(settingsKey_ + "AutoHideStrip");
    tabDock_->setAllowedAreas(area_);
    tabDock_->setFeatures(QDockWidget::NoDockWidgetFeatures);
    auto *emptyTabTitleBar = new QWidget(tabDock_);
    emptyTabTitleBar->setFixedHeight(0);
    tabDock_->setTitleBarWidget(emptyTabTitleBar);
    auto *tabHost = new QWidget(tabDock_);
    auto *tabLayout = new QVBoxLayout(tabHost);
    tabLayout->setContentsMargins(area_ == Qt::LeftDockWidgetArea ? 0 : 4,
                                 2,
                                 area_ == Qt::LeftDockWidgetArea ? 4 : 0,
                                 0);
    tabLayout->setSpacing(0);
    edgeTab_ = new EdgeTabButton(title, area_, tabHost);
    edgeTab_->setObjectName(settingsKey_ + "AutoHideTab");
    tabLayout->addWidget(edgeTab_);
    tabLayout->addStretch();
    tabDock_->setWidget(tabHost);
    tabDock_->setFixedWidth(edgeTab_->sizeHint().width() + 6);
    owner_->addDockWidget(area_, tabDock_);
    tabDock_->hide();

    visibilityAction_ = new QAction(title, owner_);
    visibilityAction_->setCheckable(true);
    QSettings settings;
    pinned_ = settings.value("workspace/" + settingsKey_ + "/pinned", true).toBool();
    preferredWidth_ = settings.value("workspace/" + settingsKey_ + "/width", defaultWidth).toInt();
    visibilityAction_->setChecked(
        settings.value("workspace/" + settingsKey_ + "/visible", initiallyVisible).toBool());
    connect(pinButton_, &QToolButton::clicked, this, [this](bool checked) { setPinned(checked); });
    connect(edgeTab_, &QToolButton::clicked, this, &AutoHideDockWidget::showOverlay);
    connect(visibilityAction_, &QAction::toggled, this, [this](bool visible) {
        QSettings().setValue("workspace/" + settingsKey_ + "/visible", visible);
        applyVisibility();
    });
    owner_->installEventFilter(this);
    qApp->installEventFilter(this);
    updatePinButton();
    QTimer::singleShot(0, this, [this] {
        setPinned(pinned_);
        applyVisibility();
    });
}

void AutoHideDockWidget::setPanelWidget(QWidget *widget) {
    widget->setParent(panelFrame_);
    panelLayout_->addWidget(widget, 1);
}

QAction *AutoHideDockWidget::visibilityAction() const {
    return visibilityAction_;
}

void AutoHideDockWidget::reveal() {
    if (!visibilityAction_->isChecked())
        visibilityAction_->setChecked(true);
    if (pinned_) {
        show();
    } else {
        showOverlay();
    }
}

bool AutoHideDockWidget::isPinned() const {
    return pinned_;
}

bool AutoHideDockWidget::eventFilter(QObject *watched, QEvent *event) {
    if (watched == owner_ &&
        (event->type() == QEvent::Resize || event->type() == QEvent::Move || event->type() == QEvent::Show ||
         event->type() == QEvent::LayoutRequest)) {
        QTimer::singleShot(0, this, &AutoHideDockWidget::updateFloatingGeometry);
    }
    if (!pinned_ && overlay_->isVisible() && event->type() == QEvent::MouseButtonPress) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        QPoint globalPosition = mouseEvent->globalPos();
        if (auto *source = qobject_cast<QWidget *>(watched))
            globalPosition = source->mapToGlobal(mouseEvent->pos());
        const QRect overlayRect(overlay_->mapToGlobal(QPoint(0, 0)), overlay_->size());
        const QRect tabRect(edgeTab_->mapToGlobal(QPoint(0, 0)), edgeTab_->size());
        if (!overlayRect.contains(globalPosition) && !tabRect.contains(globalPosition))
            hideOverlay();
    }
    return QDockWidget::eventFilter(watched, event);
}

void AutoHideDockWidget::setPinned(bool pinned) {
    if (!pinned_ && pinned)
        hideOverlay();
    if (pinned_ && !pinned && isVisible())
        preferredWidth_ = qMax(180, width());
    pinned_ = pinned;
    QSettings settings;
    settings.setValue("workspace/" + settingsKey_ + "/pinned", pinned_);
    settings.setValue("workspace/" + settingsKey_ + "/width", preferredWidth_);
    if (pinned_)
        movePanelTo(pinnedHost_, pinnedLayout_);
    else
        movePanelTo(overlay_, overlayLayout_);
    updatePinButton();
    applyVisibility();
    QTimer::singleShot(0, this, &AutoHideDockWidget::updateFloatingGeometry);
}

void AutoHideDockWidget::applyVisibility() {
    const bool enabled = visibilityAction_->isChecked();
    if (pinned_) {
        overlay_->hide();
        tabDock_->hide();
        QDockWidget::setVisible(enabled);
    } else {
        QDockWidget::hide();
        overlay_->hide();
        tabDock_->setVisible(enabled);
    }
    QTimer::singleShot(0, this, &AutoHideDockWidget::updateFloatingGeometry);
}

void AutoHideDockWidget::movePanelTo(QWidget *host, QVBoxLayout *layout) {
    pinnedLayout_->removeWidget(panelFrame_);
    overlayLayout_->removeWidget(panelFrame_);
    panelFrame_->setParent(host);
    layout->addWidget(panelFrame_);
    panelFrame_->show();
}

void AutoHideDockWidget::showOverlay() {
    if (pinned_ || !visibilityAction_->isChecked())
        return;
    updateFloatingGeometry();
    overlay_->show();
    overlay_->raise();
    edgeTab_->raise();
}

void AutoHideDockWidget::hideOverlay() {
    overlay_->hide();
}

void AutoHideDockWidget::updateFloatingGeometry() {
    if (!owner_->centralWidget())
        return;
    const QRect central = owner_->centralWidget()->geometry();
    if (!central.isValid())
        return;
    const int width = qBound(220, preferredWidth_, qMax(220, central.width() - 48));
    const int overlayX = area_ == Qt::LeftDockWidgetArea ? central.left() : central.right() - width + 1;
    overlay_->setGeometry(overlayX, central.top(), width, central.height());
    if (overlay_->isVisible())
        overlay_->raise();
}

void AutoHideDockWidget::updatePinButton() {
    pinButton_->setChecked(pinned_);
    pinButton_->setIcon(pinIcon(pinned_));
    pinButton_->setToolTip(pinned_ ? tr("Открепить панель") : tr("Закрепить панель"));
    pinButton_->setAccessibleName(pinButton_->toolTip());
}
