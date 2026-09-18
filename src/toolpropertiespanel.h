#pragma once

#include <QColor>
#include <QWidget>

class DrawingToolSettingsModel;
class QLabel;
class QPushButton;
class QSpinBox;
class QComboBox;
class RolloutSection;
class StrokePreview;

/// Показывает параметры выбранного рисующего или вспомогательного инструмента.
class ToolPropertiesPanel final : public QWidget {
    Q_OBJECT

public:
    /// Создаёт элементы панели и связывает их с единой моделью настроек.
    explicit ToolPropertiesPanel(DrawingToolSettingsModel *model, QWidget *parent = nullptr);
    /// Обновляет показываемые образцы цветов Front и Back.
    void setColors(const QColor &front, const QColor &back);
    /// Передаёт фокус полю ширины для команды меню параметров инструмента.
    void focusWidth();
    /// Выбирает набор видимых секций по текущему инструменту Canvas.
    void setCanvasTool(int tool);
    /// Синхронизирует комбинированный список с целью перемещения без повторного сигнала.
    void setMoveTarget(int target);

signals:
    /// Запрашивает открытие общего диалога выбора цвета Front.
    void frontColorRequested();
    /// Запрашивает открытие общего диалога выбора цвета Back.
    void backColorRequested();
    /// Запрашивает обмен общих цветов Front и Back.
    void swapColorsRequested();
    /// Передаёт выбранную пользователем цель универсального инструмента перемещения.
    void moveTargetRequested(int target);

private:
    DrawingToolSettingsModel *model_ = nullptr;
    QLabel *title_ = nullptr;
    QLabel *unavailable_ = nullptr;
    QLabel *opacityLabel_ = nullptr;
    QLabel *hardnessLabel_ = nullptr;
    QLabel *strengthLabel_ = nullptr;
    QLabel *resultLabel_ = nullptr;
    QLabel *eraserMode_ = nullptr;
    QSpinBox *width_ = nullptr;
    QSpinBox *opacity_ = nullptr;
    QSpinBox *hardness_ = nullptr;
    QSpinBox *spacing_ = nullptr;
    QSpinBox *strength_ = nullptr;
    QPushButton *front_ = nullptr;
    QPushButton *back_ = nullptr;
    StrokePreview *preview_ = nullptr;
    RolloutSection *mainSection_ = nullptr;
    RolloutSection *detailsSection_ = nullptr;
    RolloutSection *moveSection_ = nullptr;
    QComboBox *moveTarget_ = nullptr;
    QColor frontColor_;
    QColor backColor_;
    int canvasTool_ = 0;

    /// Синхронизирует все видимые поля с активным инструментом и контекстом цели.
    void refresh();
    /// Обновляет значок кнопки одним цветным образцом.
    static void updateColorButton(QPushButton *button, const QColor &color);
};
