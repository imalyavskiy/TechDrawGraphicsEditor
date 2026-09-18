#pragma once

#include <QObject>
#include <QString>
#include <array>

/// Набор параметров одного рисующего инструмента, не входящий в файл проекта.
struct DrawingToolSettings {
    int width = 3;
    int opacity = 100;
    int hardness = 100;
    int spacing = 15;
    int strength = 100;
};

/// Описывает возможности текущей цели рисования без зависимости панели от реализации слоя.
struct DrawingTargetContext {
    bool editable = true;
    bool supportsTransparency = false;
    bool alphaLocked = true;
    QString unavailableReason;
};

/// Хранит параметры трёх рисующих инструментов и сообщает интерфейсу об их изменениях.
class DrawingToolSettingsModel final : public QObject {
    Q_OBJECT

public:
    /// Порядок значений совпадает с первыми тремя значениями Canvas::Tool.
    enum PaintTool { Pencil = 0, Brush = 1, Eraser = 2, PaintToolCount = 3 };

    /// Загружает проверенные значения инструментов из QSettings.
    explicit DrawingToolSettingsModel(QObject *parent = nullptr);

    /// Возвращает параметры указанного инструмента либо карандаша для неверного индекса.
    const DrawingToolSettings &settingsFor(int tool) const;
    /// Возвращает индекс активного рисующего инструмента или -1 для вспомогательного режима.
    int activeTool() const;
    /// Меняет активный инструмент, не изменяя его сохранённые параметры.
    void setActiveTool(int tool);
    /// Возвращает текущий контекст цели рисования.
    const DrawingTargetContext &targetContext() const;
    /// Заменяет контекст цели, который на этапе 8 будет формироваться активным слоем.
    void setTargetContext(const DrawingTargetContext &context);
    /// Задаёт ширину активного инструмента в пикселях.
    void setWidth(int value);
    /// Задаёт непрозрачность отпечатка активного карандаша или кисти.
    void setOpacity(int value);
    /// Задаёт жёсткость края активной кисти или ластика.
    void setHardness(int value);
    /// Задаёт расстояние между отпечатками в процентах диаметра.
    void setSpacing(int value);
    /// Задаёт силу активного ластика.
    void setStrength(int value);

signals:
    /// Сообщает о выборе другого рисующего инструмента или вспомогательного режима.
    void activeToolChanged();
    /// Сообщает об изменении параметров активного инструмента.
    void settingsChanged();
    /// Сообщает об изменении возможностей текущей цели рисования.
    void targetContextChanged();

private:
    std::array<DrawingToolSettings, PaintToolCount> settings_;
    DrawingTargetContext targetContext_;
    int activeTool_ = Pencil;

    /// Проверяет индекс рисующего инструмента.
    static bool validTool(int tool);
    /// Сохраняет одно поле активного инструмента и посылает сигнал только при изменении.
    void updateValue(int DrawingToolSettings::*field, int value, int minimum, int maximum, const QString &name);
};
