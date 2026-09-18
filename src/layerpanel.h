#pragma once

#include <QWidget>

class Canvas;
class QCheckBox;
class QListWidget;
class QPushButton;
class QSpinBox;
class QToolButton;

/// Показывает стек слоёв и передаёт пользовательские операции модели документа Canvas.
class LayerPanel final : public QWidget {
    Q_OBJECT

public:
    /// Создаёт панель для указанного холста и синхронизирует её с текущим стеком.
    explicit LayerPanel(Canvas *canvas, QWidget *parent = nullptr);

private:
    Canvas *canvas_ = nullptr;
    QListWidget *list_ = nullptr;
    QSpinBox *opacity_ = nullptr;
    QPushButton *addTransparency_ = nullptr;
    QCheckBox *alphaLocked_ = nullptr;
    QToolButton *add_ = nullptr;
    QToolButton *remove_ = nullptr;
    QToolButton *duplicate_ = nullptr;
    QToolButton *up_ = nullptr;
    QToolButton *down_ = nullptr;
    bool updating_ = false;

    /// Полностью обновляет строки и свойства активного слоя после изменения модели или истории.
    void updateFromState();
};
