#pragma once
#include "drawingtoolsettings.h"
#include "project.h"
#include <QWidget>
#include <QUndoStack>

class QPainter;

/// Интерактивный виджет документа, который рисует растр и оснастку и ведёт историю изменений.
class Canvas : public QWidget {
    Q_OBJECT
public:
    /// Перечисляет режимы ввода: три растровых инструмента, перемещение объектов и редактирование перспективы.
    enum Tool { Pencil, Brush, Eraser, Move, Perspective };
    /// Определяет, какой класс объектов захватывает универсальный инструмент перемещения.
    enum MoveTarget { GuidesTarget, ActiveLayerTarget };
    /// Создаёт холст с начальным документом и пустой историей Undo/Redo.
    explicit Canvas(QWidget *parent = nullptr);
    /// Возвращает активный снимок документа без копирования.
    const DrawingState &state() const {
        return state_;
    }
    /// Возвращает стек команд, используемый действиями Undo/Redo главного окна.
    QUndoStack *undoStack() {
        return &undo_;
    }
    /// Выбирает слой для рисования и панели свойств без создания команды истории.
    void selectLayer(const QString &id);
    /// Создаёт прозрачный растровый слой над активным.
    void addRasterLayer();
    /// Удаляет активный слой, сохраняя в документе хотя бы один слой.
    void removeActiveLayer();
    /// Дублирует активный слой над исходным.
    void duplicateActiveLayer();
    /// Перемещает активный слой на один уровень вверх в композиции.
    void moveActiveLayerUp();
    /// Перемещает активный слой на один уровень вниз в композиции.
    void moveActiveLayerDown();
    /// Переименовывает указанный слой.
    void renameLayer(const QString &id, const QString &name);
    /// Показывает или скрывает указанный слой.
    void setLayerVisible(const QString &id, bool visible);
    /// Фиксирует или освобождает указанный слой для редактирования.
    void setLayerLocked(const QString &id, bool locked);
    /// Задаёт непрозрачность указанного слоя в процентах.
    void setLayerOpacity(const QString &id, int opacity);
    /// Задаёт смещение содержимого слоя в координатах документа.
    void setLayerOffset(const QString &id, QPointF offset);
    /// Добавляет растровому слою доступную прозрачность.
    void addLayerTransparency(const QString &id);
    /// Фиксирует или разрешает изменение альфа-канала растрового слоя.
    void setLayerAlphaLocked(const QString &id, bool locked);
    /// Заменяет документ одним снимком и при необходимости отмечает историю чистой.
    void setDocument(const DrawingState &state, bool clean = true);
    /// Восстанавливает документ вместе с сериализованной историей и её текущей позицией.
    void setDocument(const DrawingHistory &history, bool clean = true);
    /// Формирует сериализуемое представление текущего стека Undo/Redo.
    DrawingHistory history() const;
    /// Выбирает активный инструмент и сбрасывает незавершённый жест прежнего инструмента.
    void setTool(Tool tool);
    /// Возвращает активный инструмент ввода.
    Tool tool() const {
        return tool_;
    }
    /// Выбирает направляющие либо активный слой как явную цель инструмента перемещения.
    void setMoveTarget(MoveTarget target);
    /// Возвращает текущую цель универсального инструмента перемещения.
    MoveTarget moveTarget() const { return moveTarget_; }
    /// Временно показывает или скрывает все документные направляющие без изменения истории.
    void setGuidesVisible(bool visible);
    /// Возвращает текущее состояние отображения направляющих.
    bool guidesVisible() const { return guidesVisible_; }
    /// Включает или отключает единый режим прилипания инструментов к направляющим.
    void setSnapToGuides(bool enabled);
    /// Возвращает состояние единого режима прилипания к направляющим.
    bool snapToGuides() const { return snapToGuides_; }
    void setPerspectiveGuideCreationEnabled(bool enabled);
    bool perspectiveGuideCreationEnabled() const { return perspectiveGuideCreationEnabled_; }
    void setPerspectiveGuideAngleThreshold(double degrees);
    double perspectiveGuideAngleThreshold() const { return perspectiveGuideAngleThreshold_; }
    /// Возвращает устойчивый идентификатор выбранной направляющей либо пустую строку.
    const GuideId &selectedGuideId() const { return selectedGuideId_; }
    /// Создаёт обычную направляющую в точной координате документа и добавляет команду истории.
    void addGuide(GuideType type, double position);
    /// Удаляет выбранную направляющую одной отменяемой командой.
    void removeSelectedGuide();
    /// Удаляет весь набор направляющих одной отменяемой командой.
    void removeAllGuides();
    /// Устанавливает основной цвет карандаша и кисти.
    void setFront(QColor color) {
        front_ = color;
    }
    /// Устанавливает фоновый цвет, которым рисует ластик.
    void setBack(QColor color) {
        back_ = color;
    }
    /// Устанавливает все параметры следующего растрового штриха.
    void setStrokeSettings(const DrawingToolSettings &settings) {
        strokeSettings_ = settings;
    }
    /// Устанавливает только ширину штриха для совместимости управляющего кода и тестов.
    void setStrokeWidth(int width) {
        strokeSettings_.width = qBound(1, width, 200);
    }
    /// Включает или скрывает семейства опорных лучей.
    void setGridVisible(bool visible);
    /// Задаёт угловой шаг лучей в градусах.
    void setRayStep(double degrees);
    /// Задаёт угловое смещение первого луча в градусах.
    void setRayAngleOffset(double degrees);
    /// Выбирает шаблон пера для всех опорных лучей.
    void setRayPattern(int pattern);
    /// Задаёт экранную толщину опорных лучей.
    void setRayWidth(double width);
    /// Задаёт экранный отступ начала луча от маркера точки схода.
    void setRayGap(int gap);
    /// Задаёт начальную непрозрачность луча в процентах.
    void setRayStartOpacity(int opacity);
    /// Задаёт итоговую непрозрачность луча в процентах.
    void setRayEndOpacity(int opacity);
    /// Задаёт экранную длину перехода между двумя значениями непрозрачности.
    void setRayFadeLength(int length);
    /// Одной операцией применяет сохраняемый набор общего оформления опорных лучей.
    void
    setRayAppearance(double stepDegrees, int gap, int startOpacity, int endOpacity, int fadeLength, int pattern = 0);
    /// Задаёт цвет линии горизонта.
    void setHorizonColor(QColor color);
    /// Задаёт непрозрачность линии горизонта в процентах.
    void setHorizonOpacity(int opacity);
    /// Задаёт экранную ширину линии горизонта.
    void setHorizonWidth(double width);
    /// Перемещает горизонт по вертикали в координатах изображения и создаёт команду истории.
    void setHorizonY(double imageY);
    /// Запрещает или разрешает перетаскивание горизонта и сохраняет изменение в истории.
    void setHorizonLocked(bool locked);
    /// Включает или скрывает горизонт без изменения данных проекта.
    void setHorizonVisible(bool visible);
    /// Задаёт цвет главной вертикали.
    void setVerticalColor(QColor color);
    /// Задаёт непрозрачность главной вертикали в процентах.
    void setVerticalOpacity(int opacity);
    /// Задаёт экранную ширину главной вертикали.
    void setVerticalWidth(double width);
    /// Перемещает главную вертикаль по горизонтали в координатах изображения и создаёт команду истории.
    void setVerticalX(double imageX);
    /// Запрещает или разрешает перетаскивание главной вертикали и сохраняет изменение в истории.
    void setVerticalLocked(bool locked);
    /// Включает или скрывает главную вертикаль без изменения данных проекта.
    void setVerticalVisible(bool visible);
    /// Включает или скрывает координатные оси оснастки.
    void setAxesVisible(bool visible);
    /// Включает или скрывает управляющие маркеры точек схода.
    void setMarkersVisible(bool visible);
    /// Включает зеркальное движение пар точек, прикреплённых к горизонту.
    void setHorizonSymmetry(bool enabled);
    /// Включает зеркальное движение пар точек, прикреплённых к главной вертикали.
    void setVerticalSymmetry(bool enabled);
    /// Переключает линейки между пикселями и процентами полного размера холста.
    void setRulerPercent(bool percent);
    /// Возвращает текущий режим единиц линеек.
    bool rulerPercent() const {
        return rulerPercent_;
    }
    /// Возвращает индекс выбранной точки или отрицательное значение при пустом списке.
    int selectedPointIndex() const {
        return selectedPointIndex_;
    }
    /// Выбирает точку по индексу, ограничивая значение текущим размером коллекции.
    void selectPoint(int index);
    /// Добавляет свободную точку в первую незанятую позицию от центра влево.
    void addVanishingPoint();
    /// Удаляет выбранную точку, сохраняя хотя бы одну точку в документе.
    void removeSelectedVanishingPoint();
    /// Меняет цвет семейства лучей выбранной точки как настройку отображения.
    void setSelectedPointColor(QColor color);
    /// Переименовывает выбранную точку и создаёт команду истории.
    void setSelectedPointName(const QString &name);
    /// Включает или скрывает семейство лучей выбранной точки.
    void setSelectedPointVisible(bool visible);
    /// Точно задаёт положение выбранной точки в координатах изображения.
    void setSelectedPointPosition(QPointF position);
    /// Заменяет набор привязок выбранной точки состоянием, заданным элементом интерфейса.
    void setSelectedPointAttachment(const QString &targetId);
    /// Запрещает или разрешает перетаскивание выбранной точки.
    void setSelectedPointLocked(bool locked);
    /// Меняет масштаб вокруг экранной опорной точки и сохраняет её положение под курсором.
    void setZoom(double zoom, QPointF anchor = QPointF(-1, -1));
    /// Возвращает текущий коэффициент масштаба изображения.
    double zoom() const {
        return zoom_;
    }
    /// Вписывает холст в доступную область просмотра.
    void fit();
    /// Преобразует точку виджета в координаты изображения.
    QPointF toImage(QPointF point) const;
    /// Преобразует точку изображения в координаты виджета.
    QPointF toView(QPointF point) const;
    /// Применяет снимок из команды истории, сохраняя несериализуемые параметры вида.
    void apply(const DrawingState &state);
signals:
    /// Сообщает об изменении данных документа или его оформления.
    void stateChanged();
    /// Сообщает об изменении масштаба либо панорамирования.
    void viewChanged();
    /// Передаёт положение курсора в координатах изображения.
    void positionChanged(QPointF position);
    /// Сообщает об изменении выбора точки схода.
    void selectedPointChanged(int index);
    /// Сообщает панели и инструментам об изменении структуры, выбора или свойств слоёв.
    void layersChanged();
    /// Сообщает об автоматической смене инструмента после завершённого жеста на холсте.
    void toolChanged(Canvas::Tool tool);
    /// Сообщает панели свойств об автоматической смене цели перемещения.
    void moveTargetChanged(Canvas::MoveTarget target);
    void perspectiveGuideCreationChanged(bool enabled);
    /// Сообщает об изменении выбора направляющей для меню и будущей панели свойств.
    void selectedGuideChanged(const GuideId &id);

protected:
    /// Рисует растр, перспективную оснастку, маркеры и линейки.
    void paintEvent(QPaintEvent *) override;
    /// Начинает рисование, панорамирование или перетаскивание объекта перспективы.
    void mousePressEvent(QMouseEvent *) override;
    /// Продолжает активный жест и обновляет состояние наведения.
    void mouseMoveEvent(QMouseEvent *) override;
    /// Завершает активный жест мыши и фиксирует единственную команду истории.
    void mouseReleaseEvent(QMouseEvent *) override;
    /// Масштабирует холст колесом вокруг положения курсора.
    void wheelEvent(QWheelEvent *) override;
    /// Обрабатывает временное панорамирование и модификаторы прямого отрезка.
    void keyPressEvent(QKeyEvent *) override;
    /// Завершает временные режимы после отпускания клавиши.
    void keyReleaseEvent(QKeyEvent *) override;
    /// Сбрасывает временные модификаторы при потере фокуса.
    void focusOutEvent(QFocusEvent *) override;
    /// Скрывает экранные проекции курсора после выхода из виджета.
    void leaveEvent(QEvent *) override;

private:
    DrawingState state_;
    DrawingState before_;
    QUndoStack undo_;
    Tool tool_ = Pencil;
    MoveTarget moveTarget_ = GuidesTarget;
    QColor front_ = QColor("#2c3441");
    QColor back_ = Qt::white;
    DrawingToolSettings strokeSettings_;
    double distanceToNextStamp_ = 0;
    double zoom_ = 1.0;
    QPointF pan_;
    QPointF last_;
    QPointF paintAnchor_;
    QPointF hoverPoint_;
    bool dragging_ = false;
    bool panning_ = false;
    bool movingPoint_ = false;
    bool movingHorizon_ = false;
    bool movingVertical_ = false;
    bool movingGuides_ = false;
    bool movingLayer_ = false;
    bool space_ = false;
    int selectedPointIndex_ = 0;
    int movingPointIndex_ = -1;
    int movingSymmetricPointIndex_ = -1;
    bool straightStroke_ = false;
    bool shiftPressed_ = false;
    bool controlPressed_ = false;
    bool hasPaintAnchor_ = false;
    bool hasHoverPoint_ = false;
    bool rulerPercent_ = false;
    bool cursorInViewport_ = false;
    QPointF cursorView_;
    bool guidesVisible_ = true;
    bool snapToGuides_ = true;
    bool creatingGuide_ = false;
    bool perspectiveGuideCreationEnabled_ = false;
    bool creatingPerspectiveGuide_ = false;
    GuideType creatingGuideType_ = GuideType::Horizontal;
    double guidePreviewPosition_ = 0;
    double perspectiveGuideAngleThreshold_ = 12;
    QPointF perspectiveGuideSource_;
    QPointF perspectiveGuidePointer_;
    QString perspectiveGuideCandidateId_;
    GuideId selectedGuideId_;
    GuideId hoveredGuideId_;
    QVector<GuideId> movingGuideIds_;
    QString movingLayerId_;
    QPointF moveStartImage_;
    bool deleteMovedGuides_ = false;
    /// Рисует один растровый отрезок выбранным инструментом между двумя точками изображения.
    void stroke(QPointF start, QPointF end);
    /// Накладывает один круглый отпечаток активного инструмента на растровое изображение.
    void stamp(QPointF center);
    /// Добавляет снимок в Undo/Redo, если состояние действительно изменилось.
    void commit(const DrawingState &before, const QString &label);
    /// Завершает текущий жест и восстанавливает обычное состояние курсора.
    void finish();
    /// Проверяет, относится ли активный режим к растровым инструментам.
    bool isPaintTool() const;
    /// При необходимости привязывает направление отрезка к ближайшему углу, кратному 15 градусам.
    QPointF constrainedPoint(QPointF point, bool constrainAngle) const;
    /// Находит ближайшего к зеркальной позиции партнёра перемещаемой точки на активной оси.
    int symmetricPartnerIndex(int movedIndex) const;
    /// Перемещает выбранного партнёра в зеркальную позицию относительно пересечения осей.
    void updateSymmetricPoint(int movedIndex, int partnerIndex = -1);
    /// Возвращает индексы всех точек, прикреплённых к указанной оси.
    QVector<int> attachedPointIndices(const QString &targetId) const;
    /// Перемещает горизонт и только Y-координаты прикреплённых к нему точек.
    void moveHorizon(double imageY);
    /// Перемещает главную вертикаль и только X-координаты прикреплённых к ней точек.
    void moveVertical(double imageX);
    /// Кодирует результат проверки попадания в ось или точку перспективной оснастки.
    enum PerspectiveHit { NoPerspectiveHit = -1, VerticalHit = -2, HorizonHit = -3 };
    /// Возвращает код объекта оснастки под заданной экранной позицией.
    int perspectiveHit(QPointF viewPosition) const;
    /// Выбирает курсор по доступности и состоянию фиксации объекта под указателем.
    void updatePerspectiveCursor(QPointF viewPosition);
    /// Возвращает ближайшую направляющую либо пару обычных направляющих в точке их пересечения.
    QVector<int> guideHits(QPointF viewPosition) const;
    /// Показывает курсор захвата или запрета согласно явной цели инструмента перемещения.
    void updateMoveCursor(QPointF viewPosition);
    /// Возвращает прямоугольник внутри четырёх линеек, доступный для холста и оснастки.
    QRectF viewportRect() const;
    /// Рисует четыре линейки и проекции текущего положения курсора.
    void drawRulers(QPainter &painter);
    /// Находит изменяемую запись слоя по идентификатору.
    LayerEntry *editableLayerEntry(const QString &id);
    /// Определяет, начинается ли жест на одной из четырёх линеек, и возвращает тип направляющей.
    bool rulerGuideType(QPointF viewPosition, GuideType *type) const;
    /// Создаёт направляющую при отпускании над документом либо отменяет жест за его пределами.
    void finishGuideCreation(QPointF viewPosition);
    void finishPerspectiveGuideCreation();
};
