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

/// Даёт закреплённой dock-панели одинаковую явную область изменения ширины с обеих сторон окна.
class DockResizeHandle final : public QWidget {
public:
    /// Связывает область захвата с изменяемой панелью, её главным окном и стороной размещения.
    DockResizeHandle(QDockWidget *dock, QMainWindow *owner, Qt::DockWidgetArea area, QWidget *parent)
        : QWidget(parent), dock_(dock), owner_(owner), area_(area) {
        setCursor(Qt::SizeHorCursor);
        setFixedWidth(6);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    }

protected:
    /// Показывает тонкую границу, на которой пользователь получает курсор горизонтального resize.
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setPen(palette().color(QPalette::Mid));
        painter.drawLine(width() / 2, 0, width() / 2, height());
    }

    /// Начинает жест и запоминает неизменную экранную координату указателя и исходную ширину панели.
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() != Qt::LeftButton)
            return;
        dragging_ = true;
        startScreenX_ = qRound(event->screenPos().x());
        startWidth_ = dock_->width();
        grabMouse();
        event->accept();
    }

    /// Преобразует экранное смещение указателя в требуемую ширину с учётом стороны панели.
    void mouseMoveEvent(QMouseEvent *event) override {
        if (!dragging_ || !(event->buttons() & Qt::LeftButton))
            return;
        const int delta = qRound(event->screenPos().x()) - startScreenX_;
        const int requestedWidth =
            qMax(dock_->minimumWidth(), startWidth_ + (area_ == Qt::LeftDockWidgetArea ? delta : -delta));
        owner_->resizeDocks({dock_}, {requestedWidth}, Qt::Horizontal);
        event->accept();
    }

    /// Завершает жест и освобождает захваченный указатель мыши.
    void mouseReleaseEvent(QMouseEvent *event) override {
        if (event->button() != Qt::LeftButton)
            return;
        dragging_ = false;
        if (mouseGrabber() == this)
            releaseMouse();
        event->accept();
    }

private:
    QDockWidget *dock_ = nullptr;
    QMainWindow *owner_ = nullptr;
    Qt::DockWidgetArea area_ = Qt::LeftDockWidgetArea;
    bool dragging_ = false;
    int startScreenX_ = 0;
    int startWidth_ = 0;
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
                                       QMainWindow *owner,
                                       int edgeOrder)
    : QDockWidget(title, owner), owner_(owner), area_(area), settingsKey_(settingsKey),
      edgeOrder_(edgeOrder), minimumPanelWidth_(defaultWidth), preferredWidth_(defaultWidth) {
    setAllowedAreas(area);
    setFeatures(QDockWidget::NoDockWidgetFeatures);
    setMinimumWidth(minimumPanelWidth_);
    setMaximumWidth(QWIDGETSIZE_MAX);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    auto *emptyTitleBar = new QWidget(this);
    emptyTitleBar->setFixedHeight(0);
    setTitleBarWidget(emptyTitleBar);

    pinnedHost_ = new QWidget(this);
    auto *pinnedColumns = new QHBoxLayout(pinnedHost_);
    pinnedColumns->setContentsMargins(0, 0, 0, 0);
    pinnedColumns->setSpacing(0);
    auto *pinnedPanelHost = new QWidget(pinnedHost_);
    pinnedLayout_ = new QVBoxLayout(pinnedPanelHost);
    pinnedLayout_->setContentsMargins(0, 0, 0, 0);
    pinnedLayout_->setSpacing(0);
    auto *resizeHandle = new DockResizeHandle(this, owner_, area_, pinnedHost_);
    resizeHandle->setObjectName(settingsKey_ + "ResizeHandle");
    if (area_ == Qt::LeftDockWidgetArea) {
        pinnedColumns->addWidget(pinnedPanelHost, 1);
        pinnedColumns->addWidget(resizeHandle);
    } else {
        pinnedColumns->addWidget(resizeHandle);
        pinnedColumns->addWidget(pinnedPanelHost, 1);
    }
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

    edgeTab_ = new EdgeTabButton(title, area_, owner_);
    edgeTab_->setObjectName(settingsKey_ + "AutoHideTab");
    edgeTab_->setProperty("edgeOrder", edgeOrder_);
    edgeTab_->hide();

    visibilityAction_ = new QAction(title, owner_);
    visibilityAction_->setCheckable(true);
    QSettings settings;
    pinned_ = settings.value("workspace/" + settingsKey_ + "/pinned", true).toBool();
    preferredWidth_ = qMax(minimumPanelWidth_,
                           settings.value("workspace/" + settingsKey_ + "/width", defaultWidth).toInt());
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

void AutoHideDockWidget::attachEdgeTab() {
    if (tabStrip_)
        return;
    const QString stripName = area_ == Qt::LeftDockWidgetArea ? "leftAutoHideStrip" : "rightAutoHideStrip";
    tabStrip_ = owner_->findChild<QToolBar *>(stripName, Qt::FindDirectChildrenOnly);
    QVBoxLayout *tabLayout = nullptr;
    if (!tabStrip_) {
        tabStrip_ = new QToolBar(owner_);
        tabStrip_->setObjectName(stripName);
        tabStrip_->setAllowedAreas(area_ == Qt::LeftDockWidgetArea ? Qt::LeftToolBarArea : Qt::RightToolBarArea);
        tabStrip_->setMovable(false);
        tabStrip_->setFloatable(false);
        tabStrip_->setOrientation(Qt::Vertical);
        tabStrip_->setContentsMargins(0, 0, 0, 0);
        auto *tabHost = new QWidget(tabStrip_);
        tabHost->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        tabLayout = new QVBoxLayout(tabHost);
        tabLayout->setContentsMargins(area_ == Qt::LeftDockWidgetArea ? 0 : 4,
                                     2,
                                     area_ == Qt::LeftDockWidgetArea ? 4 : 0,
                                     0);
        tabLayout->setSpacing(0);
        tabLayout->addStretch();
        tabStrip_->addWidget(tabHost);
        tabStrip_->setFixedWidth(edgeTab_->sizeHint().width() + 6);
        owner_->addToolBar(area_ == Qt::LeftDockWidgetArea ? Qt::LeftToolBarArea : Qt::RightToolBarArea,
                           tabStrip_);
    } else {
        QWidget *tabHost = tabStrip_->widgetForAction(tabStrip_->actions().constFirst());
        tabLayout = qobject_cast<QVBoxLayout *>(tabHost->layout());
    }
    edgeTab_->setParent(tabLayout->parentWidget());
    int insertIndex = 0;
    while (insertIndex < tabLayout->count() - 1) {
        QWidget *existing = tabLayout->itemAt(insertIndex)->widget();
        if (existing && existing->property("edgeOrder").toInt() > edgeOrder_)
            break;
        ++insertIndex;
    }
    tabLayout->insertWidget(insertIndex, edgeTab_);
    applyVisibility();
}

void AutoHideDockWidget::collapseToTab() {
    if (!visibilityAction_->isChecked())
        return;
    if (pinned_)
        setPinned(false);
    else
        hideOverlay();
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
        preferredWidth_ = qMax(minimumPanelWidth_, width());
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
        edgeTab_->hide();
        QDockWidget::setVisible(enabled);
    } else {
        QDockWidget::hide();
        overlay_->hide();
        edgeTab_->setVisible(enabled);
    }
    updateEdgeStripVisibility();
    QTimer::singleShot(0, this, [this] {
        const auto siblings = owner_->findChildren<AutoHideDockWidget *>(QString(), Qt::FindDirectChildrenOnly);
        for (AutoHideDockWidget *sibling : siblings)
            if (sibling->area_ == area_)
                sibling->updateFloatingGeometry();
    });
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
    const int width = qBound(minimumPanelWidth_,
                             preferredWidth_,
                             qMax(minimumPanelWidth_, owner_->width() - 48));
    const int innerEdge = area_ == Qt::LeftDockWidgetArea
                              ? (tabStrip_ && tabStrip_->isVisible() ? tabStrip_->geometry().right() + 1
                                                                    : central.left())
                              : (tabStrip_ && tabStrip_->isVisible() ? tabStrip_->geometry().left() - 1
                                                                    : central.right());
    const int overlayX = area_ == Qt::LeftDockWidgetArea ? innerEdge : innerEdge - width + 1;
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

void AutoHideDockWidget::updateEdgeStripVisibility() {
    if (!tabStrip_)
        return;
    bool anyVisible = false;
    const auto tabs = tabStrip_->findChildren<EdgeTabButton *>();
    for (const EdgeTabButton *tab : tabs)
        anyVisible = anyVisible || !tab->isHidden();
    tabStrip_->setVisible(anyVisible);
}
