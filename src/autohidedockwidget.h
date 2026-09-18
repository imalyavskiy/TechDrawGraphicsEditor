#pragma once

#include <QDockWidget>

class QAction;
class QFrame;
class QMainWindow;
class QToolButton;
class QVBoxLayout;

/// Боковая панель, которую можно закрепить либо свернуть во вкладку у края окна.
class AutoHideDockWidget final : public QDockWidget {
    Q_OBJECT

public:
    /// Создаёт панель для одной стороны окна и восстанавливает её постоянное состояние.
    AutoHideDockWidget(const QString &title,
                       Qt::DockWidgetArea area,
                       const QString &settingsKey,
                       bool initiallyVisible,
                       int defaultWidth,
                       QMainWindow *owner);
    /// Помещает пользовательское содержимое под общим заголовком с кнопкой закрепления.
    void setPanelWidget(QWidget *widget);
    /// Возвращает независимую команду полной видимости для меню «Вид».
    QAction *visibilityAction() const;
    /// Показывает закреплённую панель либо временно раскрывает откреплённую.
    void reveal();
    /// Сообщает, находится ли панель в постоянной компоновке окна.
    bool isPinned() const;

protected:
    /// Следит за геометрией окна и скрывает временную панель после щелчка снаружи.
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QMainWindow *owner_ = nullptr;
    Qt::DockWidgetArea area_ = Qt::LeftDockWidgetArea;
    QString settingsKey_;
    int minimumPanelWidth_ = 220;
    int preferredWidth_ = 260;
    bool pinned_ = true;
    QWidget *pinnedHost_ = nullptr;
    QVBoxLayout *pinnedLayout_ = nullptr;
    QFrame *panelFrame_ = nullptr;
    QVBoxLayout *panelLayout_ = nullptr;
    QToolButton *pinButton_ = nullptr;
    QFrame *overlay_ = nullptr;
    QVBoxLayout *overlayLayout_ = nullptr;
    QDockWidget *tabDock_ = nullptr;
    QToolButton *edgeTab_ = nullptr;
    QAction *visibilityAction_ = nullptr;

    /// Переключает размещение панели между dock-областью и временным слоем.
    void setPinned(bool pinned);
    /// Применяет состояние команды меню к dock-панели и крайней вкладке.
    void applyVisibility();
    /// Переносит общую рамку панели в указанный контейнер.
    void movePanelTo(QWidget *host, QVBoxLayout *layout);
    /// Показывает временную панель поверх центральной рабочей области.
    void showOverlay();
    /// Скрывает временную панель, сохраняя доступной её крайнюю вкладку.
    void hideOverlay();
    /// Пересчитывает положение временной панели и вкладки после изменения окна.
    void updateFloatingGeometry();
    /// Обновляет пиктограмму и подсказку кнопки закрепления.
    void updatePinButton();
};
