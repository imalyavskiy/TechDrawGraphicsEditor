#pragma once

#include <QDockWidget>

class QAction;
class QFrame;
class QMainWindow;
class QToolButton;
class QVBoxLayout;

class AutoHideDockWidget final : public QDockWidget {
    Q_OBJECT

public:
    AutoHideDockWidget(const QString &title,
                       Qt::DockWidgetArea area,
                       const QString &settingsKey,
                       bool initiallyVisible,
                       int defaultWidth,
                       QMainWindow *owner);
    void setPanelWidget(QWidget *widget);
    QAction *visibilityAction() const;
    void reveal();
    bool isPinned() const;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QMainWindow *owner_ = nullptr;
    Qt::DockWidgetArea area_ = Qt::LeftDockWidgetArea;
    QString settingsKey_;
    int preferredWidth_ = 260;
    bool pinned_ = true;
    QWidget *pinnedHost_ = nullptr;
    QVBoxLayout *pinnedLayout_ = nullptr;
    QFrame *panelFrame_ = nullptr;
    QVBoxLayout *panelLayout_ = nullptr;
    QToolButton *pinButton_ = nullptr;
    QFrame *overlay_ = nullptr;
    QVBoxLayout *overlayLayout_ = nullptr;
    QToolButton *edgeTab_ = nullptr;
    QAction *visibilityAction_ = nullptr;

    void setPinned(bool pinned);
    void applyVisibility();
    void movePanelTo(QWidget *host, QVBoxLayout *layout);
    void showOverlay();
    void hideOverlay();
    void updateFloatingGeometry();
    void updatePinButton();
};
