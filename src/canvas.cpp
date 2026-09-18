#include "canvas.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QUuid>
#include <cmath>
#include <limits>

namespace {
constexpr int rulerSize = 28;
/// Подбирает читаемый шаг делений линейки из последовательности 1, 2, 5, 10.
double niceStep(double minimum) {
    if (!std::isfinite(minimum) || minimum <= 0)
        return 1;
    const double power = std::pow(10.0, std::floor(std::log10(minimum))), scaled = minimum / power;
    double nice = 1;
    if (scaled > 5)
        nice = 10;
    else if (scaled > 2)
        nice = 5;
    else if (scaled > 1)
        nice = 2;
    return nice * power;
}
/// Форматирует координату линейки, убирая отрицательный ноль и лишние знаки.
QString coordinateLabel(double value) {
    if (std::abs(value) < 0.0001)
        value = 0;
    return std::abs(value) >= 1000 ? QString::number(value, 'g', 4)
                                   : QString::number(value, 'f', std::abs(value) < 10 ? 1 : 0);
}
/// Обрезает односторонний луч прямоугольником, не добавляя противоположную ветвь за его началом.
bool clippedRay(QPointF origin, QPointF direction, const QRectF &rect, QLineF *result) {
    double minimum = 0;
    double maximum = std::numeric_limits<double>::infinity();
    const auto constrain = [&](double start, double delta, double low, double high) {
        if (std::abs(delta) < 0.0000001)
            return start >= low && start <= high;
        double first = (low - start) / delta;
        double second = (high - start) / delta;
        if (first > second)
            std::swap(first, second);
        minimum = qMax(minimum, first);
        maximum = qMin(maximum, second);
        return minimum <= maximum;
    };
    if (!constrain(origin.x(), direction.x(), rect.left(), rect.right()) ||
        !constrain(origin.y(), direction.y(), rect.top(), rect.bottom()) || !std::isfinite(maximum))
        return false;
    *result = QLineF(origin + direction * minimum, origin + direction * maximum);
    return result->length() > 0.0001;
}
/// Переносит несохраняемые параметры отображения перспективы между снимками истории.
void copyPerspectiveAppearance(const DrawingState &source, DrawingState *target) {
    target->gridVisible = source.gridVisible;
    target->rayStepDegrees = source.rayStepDegrees;
    target->rayAngleOffset = source.rayAngleOffset;
    target->rayPattern = source.rayPattern;
    target->rayWidth = source.rayWidth;
    target->rayGap = source.rayGap;
    target->rayStartOpacity = source.rayStartOpacity;
    target->rayEndOpacity = source.rayEndOpacity;
    target->rayFadeLength = source.rayFadeLength;
    target->horizonColor = source.horizonColor;
    target->horizonOpacity = source.horizonOpacity;
    target->horizonWidth = source.horizonWidth;
    target->horizonVisible = source.horizonVisible;
    target->verticalColor = source.verticalColor;
    target->verticalOpacity = source.verticalOpacity;
    target->verticalWidth = source.verticalWidth;
    target->verticalVisible = source.verticalVisible;
    target->axesVisible = source.axesVisible;
    target->markersVisible = source.markersVisible;
    target->horizonSymmetry = source.horizonSymmetry;
    target->verticalSymmetry = source.verticalSymmetry;
    for (auto &point : target->vanishingPoints) {
        for (const auto &existing : source.vanishingPoints) {
            if (existing.id != point.id)
                continue;
            point.color = existing.color;
            point.visible = existing.visible;
            break;
        }
    }
}
} // namespace

/// Хранит полные снимки до и после одной операции для стека Undo/Redo.
class StateCommand : public QUndoCommand {
public:
    /// Создаёт команду с владельцем холста, двумя снимками и пользовательской подписью.
    StateCommand(Canvas *canvas, DrawingState before, DrawingState after, const QString &label)
        : canvas_(canvas), before_(std::move(before)), after_(std::move(after)) {
        setText(label);
    }
    /// Восстанавливает снимок до операции.
    void undo() override {
        canvas_->apply(before_);
    }
    /// Применяет снимок после операции, включая первый вызов при добавлении команды.
    void redo() override {
        canvas_->apply(after_);
    }
    /// Возвращает снимок до операции для сериализации истории.
    const DrawingState &before() const {
        return before_;
    }
    /// Возвращает снимок после операции для сериализации истории.
    const DrawingState &after() const {
        return after_;
    }

private:
    Canvas *canvas_;
    DrawingState before_, after_;
};

Canvas::Canvas(QWidget *parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumSize(200, 160);
    setObjectName("drawingCanvas");
    DrawingState initial;
    QImage initialImage(1000, 620, QImage::Format_ARGB32_Premultiplied);
    initialImage.fill(Qt::white);
    initial.setSingleRasterImage(initialImage, tr("Фон"));
    initial.horizonY = 240;
    initial.verticalX = 500;
    initial.vanishingPoints.append({QStringLiteral("vp-1"),
                                    QPointF(650, 240),
                                    PerspectiveTarget::constructionType(),
                                    PerspectiveTarget::horizon()});
    initial.vanishingPoints.last().name = tr("Точка схода 1");
    setDocument(initial);
}

void Canvas::setDocument(const DrawingState &state, bool clean) {
    DrawingHistory history;
    history.states.append(state);
    setDocument(history, clean);
}

void Canvas::setDocument(const DrawingHistory &history, bool clean) {
    dragging_ = panning_ = movingPoint_ = movingHorizon_ = movingVertical_ = false;
    movingPointIndex_ = movingSymmetricPointIndex_ = -1;
    straightStroke_ = shiftPressed_ = controlPressed_ = false;
    hasPaintAnchor_ = hasHoverPoint_ = false;
    state_ = history.states.first();
    selectedGuideId_.clear();
    selectedPointIndex_ =
        state_.vanishingPoints.isEmpty() ? -1 : qBound(0, selectedPointIndex_, state_.vanishingPoints.size() - 1);
    undo_.clear();
    const int memoryLimit = qBound(1, int(128000000 / qMax(qint64(1), state_.layers.estimatedBytes())), 30);
    undo_.setUndoLimit(qMax(memoryLimit, history.labels.size()));
    for (int i = 0; i < history.labels.size(); ++i)
        undo_.push(new StateCommand(this, history.states[i], history.states[i + 1], history.labels[i]));
    undo_.setIndex(history.index);
    if (clean)
        undo_.setClean();
    else
        undo_.resetClean();
    pan_ = QPointF();
    emit layersChanged();
    emit stateChanged();
    emit selectedPointChanged(selectedPointIndex_);
    update();
}

DrawingHistory Canvas::history() const {
    DrawingHistory result;
    result.index = undo_.index();
    if (undo_.count() == 0) {
        result.states.append(state_);
        return result;
    }
    const auto *first = dynamic_cast<const StateCommand *>(undo_.command(0));
    if (!first) {
        result.states.append(state_);
        result.index = 0;
        return result;
    }
    result.states.append(first->before());
    for (int i = 0; i < undo_.count(); ++i) {
        const auto *command = dynamic_cast<const StateCommand *>(undo_.command(i));
        if (!command) {
            result.states.clear();
            result.labels.clear();
            result.states.append(state_);
            result.index = 0;
            return result;
        }
        result.states.append(command->after());
        result.labels.append(command->text());
    }
    return result;
}

void Canvas::selectLayer(const QString &id) {
    if (!state_.layers.setActiveLayerId(id))
        return;
    update();
    emit layersChanged();
    emit stateChanged();
}

void Canvas::addRasterLayer() {
    const DrawingState before = state_;
    const int number = state_.layers.entries().size() + 1;
    if (state_.layers.addRaster(state_.canvasSize, tr("Слой %1").arg(number)).isEmpty())
        return;
    commit(before, tr("добавление слоя"));
    emit layersChanged();
    emit stateChanged();
}

void Canvas::removeActiveLayer() {
    const DrawingState before = state_;
    if (!state_.layers.removeActive())
        return;
    commit(before, tr("удаление слоя"));
    emit layersChanged();
    emit stateChanged();
}

void Canvas::duplicateActiveLayer() {
    const LayerEntry *active = state_.layers.activeEntry();
    if (!active)
        return;
    const DrawingState before = state_;
    if (state_.layers.duplicateActive(tr("%1 — копия").arg(active->name)).isEmpty())
        return;
    commit(before, tr("дублирование слоя"));
    emit layersChanged();
    emit stateChanged();
}

void Canvas::moveActiveLayerUp() {
    const DrawingState before = state_;
    if (!state_.layers.moveActive(1))
        return;
    commit(before, tr("порядок слоёв"));
    emit layersChanged();
    emit stateChanged();
}

void Canvas::moveActiveLayerDown() {
    const DrawingState before = state_;
    if (!state_.layers.moveActive(-1))
        return;
    commit(before, tr("порядок слоёв"));
    emit layersChanged();
    emit stateChanged();
}

void Canvas::renameLayer(const QString &id, const QString &name) {
    const QString trimmed = name.trimmed().left(120);
    LayerEntry *entry = editableLayerEntry(id);
    if (!entry || trimmed.isEmpty() || entry->name == trimmed)
        return;
    const DrawingState before = state_;
    entry->name = trimmed;
    commit(before, tr("название слоя"));
    emit layersChanged();
    emit stateChanged();
}

void Canvas::setLayerVisible(const QString &id, bool visible) {
    LayerEntry *entry = editableLayerEntry(id);
    if (!entry || entry->visible == visible)
        return;
    const DrawingState before = state_;
    entry->visible = visible;
    commit(before, visible ? tr("показ слоя") : tr("скрытие слоя"));
    emit layersChanged();
    emit stateChanged();
}

void Canvas::setLayerLocked(const QString &id, bool locked) {
    LayerEntry *entry = editableLayerEntry(id);
    if (!entry || entry->locked == locked)
        return;
    const DrawingState before = state_;
    entry->locked = locked;
    commit(before, tr("фиксацию слоя"));
    emit layersChanged();
    emit stateChanged();
}

void Canvas::setLayerOpacity(const QString &id, int opacity) {
    LayerEntry *entry = editableLayerEntry(id);
    opacity = qBound(0, opacity, 100);
    if (!entry || entry->opacity == opacity)
        return;
    const DrawingState before = state_;
    entry->opacity = opacity;
    commit(before, tr("непрозрачность слоя"));
    emit layersChanged();
    emit stateChanged();
}

void Canvas::setLayerOffset(const QString &id, QPointF offset) {
    LayerEntry *entry = editableLayerEntry(id);
    if (!entry || !std::isfinite(offset.x()) || !std::isfinite(offset.y()) || std::abs(offset.x()) > 1000000 ||
        std::abs(offset.y()) > 1000000 || entry->offset == offset)
        return;
    const DrawingState before = state_;
    entry->offset = offset;
    commit(before, tr("смещение слоя"));
    emit layersChanged();
    emit stateChanged();
}

void Canvas::addLayerTransparency(const QString &id) {
    LayerEntry *entry = editableLayerEntry(id);
    if (!entry || entry->typeId != LayerTypes::raster() || !entry->content)
        return;
    auto *content = dynamic_cast<RasterLayerContent *>(entry->content.get());
    if (!content || content->transparencyAvailable)
        return;
    const DrawingState before = state_;
    if (!entry->content.unique()) {
        entry->content = entry->content->clone();
        content = dynamic_cast<RasterLayerContent *>(entry->content.get());
    }
    content->transparencyAvailable = true;
    content->alphaLocked = false;
    commit(before, tr("добавление прозрачности слоя"));
    emit layersChanged();
    emit stateChanged();
}

void Canvas::setLayerAlphaLocked(const QString &id, bool locked) {
    LayerEntry *entry = editableLayerEntry(id);
    if (!entry || entry->typeId != LayerTypes::raster() || !entry->content)
        return;
    auto *content = dynamic_cast<RasterLayerContent *>(entry->content.get());
    if (!content || !content->transparencyAvailable || content->alphaLocked == locked)
        return;
    const DrawingState before = state_;
    if (!entry->content.unique()) {
        entry->content = entry->content->clone();
        content = dynamic_cast<RasterLayerContent *>(entry->content.get());
    }
    content->alphaLocked = locked;
    commit(before, tr("фиксацию прозрачности слоя"));
    emit layersChanged();
    emit stateChanged();
}

void Canvas::apply(const DrawingState &state) {
    DrawingState next = state;
    copyPerspectiveAppearance(state_, &next);
    state_ = std::move(next);
    selectedPointIndex_ =
        state_.vanishingPoints.isEmpty() ? -1 : qBound(0, selectedPointIndex_, state_.vanishingPoints.size() - 1);
    emit stateChanged();
    emit layersChanged();
    emit selectedPointChanged(selectedPointIndex_);
    update();
}

LayerEntry *Canvas::editableLayerEntry(const QString &id) {
    for (auto &entry : state_.layers.entries())
        if (entry.id == id)
            return &entry;
    return nullptr;
}
void Canvas::commit(const DrawingState &before, const QString &label) {
    undo_.push(new StateCommand(this, before, state_, label));
}
void Canvas::setTool(Tool tool) {
    finish();
    if (tool_ != tool)
        hasPaintAnchor_ = false;
    tool_ = tool;
    setCursor(tool == Pan ? Qt::OpenHandCursor : Qt::CrossCursor);
    emit toolChanged(tool_);
    update();
}
void Canvas::setGuidesVisible(bool visible) {
    if (guidesVisible_ == visible)
        return;
    guidesVisible_ = visible;
    update();
}
void Canvas::addGuide(GuideType type, double position) {
    if ((type != GuideType::Horizontal && type != GuideType::Vertical) || !std::isfinite(position) ||
        std::abs(position) > 1000000)
        return;
    const DrawingState before = state_;
    Guide guide;
    guide.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    guide.type = type;
    guide.position = position;
    state_.guides.append(guide);
    selectedGuideId_ = guide.id;
    commit(before, tr("создание направляющей"));
    emit selectedGuideChanged(selectedGuideId_);
    emit stateChanged();
    update();
}
void Canvas::removeSelectedGuide() {
    if (selectedGuideId_.isEmpty())
        return;
    for (int index = 0; index < state_.guides.size(); ++index) {
        if (state_.guides[index].id != selectedGuideId_)
            continue;
        const DrawingState before = state_;
        state_.guides.removeAt(index);
        selectedGuideId_.clear();
        commit(before, tr("удаление направляющей"));
        emit selectedGuideChanged(selectedGuideId_);
        emit stateChanged();
        update();
        return;
    }
}
void Canvas::removeAllGuides() {
    if (state_.guides.isEmpty())
        return;
    const DrawingState before = state_;
    state_.guides.clear();
    selectedGuideId_.clear();
    commit(before, tr("удаление всех направляющих"));
    emit selectedGuideChanged(selectedGuideId_);
    emit stateChanged();
    update();
}
void Canvas::setGridVisible(bool visible) {
    if (state_.gridVisible == visible)
        return;
    state_.gridVisible = visible;
    emit stateChanged();
    update();
}
void Canvas::setRayStep(double degrees) {
    if (qFuzzyCompare(state_.rayStepDegrees, degrees))
        return;
    state_.rayStepDegrees = degrees;
    emit stateChanged();
    update();
}
void Canvas::setRayAngleOffset(double degrees) {
    if (!std::isfinite(degrees) || degrees < -180 || degrees > 180 ||
        qFuzzyCompare(state_.rayAngleOffset + 181, degrees + 181))
        return;
    state_.rayAngleOffset = degrees;
    emit stateChanged();
    update();
}
void Canvas::setRayPattern(int pattern) {
    pattern = qBound(0, pattern, 3);
    if (state_.rayPattern == pattern)
        return;
    state_.rayPattern = pattern;
    emit stateChanged();
    update();
}
void Canvas::setRayWidth(double width) {
    if (!std::isfinite(width) || width < 0.1 || width > 20 || qFuzzyCompare(state_.rayWidth, width))
        return;
    state_.rayWidth = width;
    emit stateChanged();
    update();
}
void Canvas::setRayGap(int gap) {
    if (state_.rayGap == gap)
        return;
    state_.rayGap = gap;
    emit stateChanged();
    update();
}
void Canvas::setRayStartOpacity(int opacity) {
    if (state_.rayStartOpacity == opacity)
        return;
    state_.rayStartOpacity = opacity;
    emit stateChanged();
    update();
}
void Canvas::setRayEndOpacity(int opacity) {
    if (state_.rayEndOpacity == opacity)
        return;
    state_.rayEndOpacity = opacity;
    emit stateChanged();
    update();
}
void Canvas::setRayFadeLength(int length) {
    if (state_.rayFadeLength == length)
        return;
    state_.rayFadeLength = length;
    emit stateChanged();
    update();
}
void Canvas::setRayAppearance(
    double stepDegrees, int gap, int startOpacity, int endOpacity, int fadeLength, int pattern) {
    if (qFuzzyCompare(state_.rayStepDegrees, stepDegrees) && state_.rayGap == gap &&
        state_.rayStartOpacity == startOpacity && state_.rayEndOpacity == endOpacity &&
        state_.rayFadeLength == fadeLength && state_.rayPattern == pattern)
        return;
    state_.rayStepDegrees = stepDegrees;
    state_.rayGap = gap;
    state_.rayStartOpacity = startOpacity;
    state_.rayEndOpacity = endOpacity;
    state_.rayFadeLength = fadeLength;
    state_.rayPattern = qBound(0, pattern, 3);
    emit stateChanged();
    update();
}
void Canvas::setHorizonColor(QColor color) {
    if (!color.isValid() || state_.horizonColor == color)
        return;
    state_.horizonColor = color;
    emit stateChanged();
    update();
}
void Canvas::setHorizonOpacity(int opacity) {
    if (state_.horizonOpacity == opacity)
        return;
    state_.horizonOpacity = opacity;
    emit stateChanged();
    update();
}
void Canvas::setHorizonWidth(double width) {
    if (qFuzzyCompare(state_.horizonWidth, width))
        return;
    state_.horizonWidth = width;
    emit stateChanged();
    update();
}
void Canvas::setHorizonY(double imageY) {
    if (!std::isfinite(imageY) || std::abs(imageY) > 1000000 || qFuzzyCompare(state_.horizonY + 1, imageY + 1))
        return;
    finish();
    DrawingState before = state_;
    moveHorizon(imageY);
    commit(before, tr("положение горизонта"));
    emit stateChanged();
    update();
}
void Canvas::setHorizonLocked(bool locked) {
    if (state_.horizonLocked == locked)
        return;
    finish();
    DrawingState before = state_;
    state_.horizonLocked = locked;
    commit(before, tr("фиксацию горизонта"));
    emit stateChanged();
    update();
}
void Canvas::setHorizonVisible(bool visible) {
    if (state_.horizonVisible == visible)
        return;
    state_.horizonVisible = visible;
    emit stateChanged();
    update();
}
void Canvas::setVerticalColor(QColor color) {
    if (!color.isValid() || state_.verticalColor == color)
        return;
    state_.verticalColor = color;
    emit stateChanged();
    update();
}
void Canvas::setVerticalOpacity(int opacity) {
    if (state_.verticalOpacity == opacity)
        return;
    state_.verticalOpacity = opacity;
    emit stateChanged();
    update();
}
void Canvas::setVerticalWidth(double width) {
    if (qFuzzyCompare(state_.verticalWidth, width))
        return;
    state_.verticalWidth = width;
    emit stateChanged();
    update();
}
void Canvas::setVerticalX(double imageX) {
    if (!std::isfinite(imageX) || std::abs(imageX) > 1000000 || qFuzzyCompare(state_.verticalX + 1, imageX + 1))
        return;
    finish();
    DrawingState before = state_;
    moveVertical(imageX);
    commit(before, tr("положение главной вертикали"));
    emit stateChanged();
    update();
}
void Canvas::setVerticalLocked(bool locked) {
    if (state_.verticalLocked == locked)
        return;
    finish();
    DrawingState before = state_;
    state_.verticalLocked = locked;
    commit(before, tr("фиксацию главной вертикали"));
    emit stateChanged();
    update();
}
void Canvas::setVerticalVisible(bool visible) {
    if (state_.verticalVisible == visible)
        return;
    state_.verticalVisible = visible;
    emit stateChanged();
    update();
}
void Canvas::setAxesVisible(bool visible) {
    if (state_.axesVisible == visible)
        return;
    state_.axesVisible = visible;
    emit stateChanged();
    update();
}
void Canvas::setMarkersVisible(bool visible) {
    if (state_.markersVisible == visible)
        return;
    state_.markersVisible = visible;
    emit stateChanged();
    update();
}
void Canvas::setHorizonSymmetry(bool enabled) {
    if (state_.horizonSymmetry == enabled)
        return;
    state_.horizonSymmetry = enabled;
    emit stateChanged();
    update();
}
void Canvas::setVerticalSymmetry(bool enabled) {
    if (state_.verticalSymmetry == enabled)
        return;
    state_.verticalSymmetry = enabled;
    emit stateChanged();
    update();
}
void Canvas::setRulerPercent(bool percent) {
    if (rulerPercent_ == percent)
        return;
    rulerPercent_ = percent;
    update();
}
void Canvas::selectPoint(int index) {
    const int next = state_.vanishingPoints.isEmpty() ? -1 : qBound(0, index, state_.vanishingPoints.size() - 1);
    if (selectedPointIndex_ == next)
        return;
    selectedPointIndex_ = next;
    emit selectedPointChanged(next);
    update();
}
void Canvas::addVanishingPoint() {
    finish();
    if (state_.vanishingPoints.size() >= 32)
        return;
    DrawingState before = state_;
    VanishingPoint point;
    point.id = QStringLiteral("vp-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    point.position = QPointF(state_.canvasSize.width() / 2.0, state_.canvasSize.height() / 2.0);
    constexpr double placementStep = 40.0;
    auto occupied = [this](QPointF candidate) {
        for (const auto &existing : state_.vanishingPoints)
            if (QLineF(candidate, existing.position).length() < 1.0)
                return true;
        return false;
    };
    while (occupied(point.position))
        point.position.rx() -= placementStep;
    static const QColor colors[]{
        QColor("#628ed1"), QColor("#d06b4c"), QColor("#4b9b67"), QColor("#896ac1"), QColor("#c08a34")};
    point.color = colors[state_.vanishingPoints.size() % 5];
    point.name = tr("Точка схода %1").arg(state_.vanishingPoints.size() + 1);
    state_.vanishingPoints.append(point);
    selectedPointIndex_ = state_.vanishingPoints.size() - 1;
    commit(before, tr("добавление точки схода"));
    emit selectedPointChanged(selectedPointIndex_);
    emit stateChanged();
    update();
}
void Canvas::removeSelectedVanishingPoint() {
    finish();
    if (selectedPointIndex_ < 0 || selectedPointIndex_ >= state_.vanishingPoints.size())
        return;
    DrawingState before = state_;
    state_.vanishingPoints.removeAt(selectedPointIndex_);
    selectedPointIndex_ =
        state_.vanishingPoints.isEmpty() ? -1 : qMin(selectedPointIndex_, state_.vanishingPoints.size() - 1);
    commit(before, tr("удаление точки схода"));
    emit selectedPointChanged(selectedPointIndex_);
    emit stateChanged();
    update();
}
void Canvas::setSelectedPointColor(QColor color) {
    if (!color.isValid() || selectedPointIndex_ < 0 || selectedPointIndex_ >= state_.vanishingPoints.size() ||
        state_.vanishingPoints[selectedPointIndex_].color == color)
        return;
    state_.vanishingPoints[selectedPointIndex_].color = color;
    emit stateChanged();
    update();
}
void Canvas::setSelectedPointName(const QString &name) {
    if (selectedPointIndex_ < 0 || selectedPointIndex_ >= state_.vanishingPoints.size() || name.size() > 120 ||
        state_.vanishingPoints[selectedPointIndex_].name == name)
        return;
    finish();
    DrawingState before = state_;
    state_.vanishingPoints[selectedPointIndex_].name = name;
    commit(before, tr("название точки схода"));
    emit stateChanged();
    update();
}
void Canvas::setSelectedPointVisible(bool visible) {
    if (selectedPointIndex_ < 0 || selectedPointIndex_ >= state_.vanishingPoints.size() ||
        state_.vanishingPoints[selectedPointIndex_].visible == visible)
        return;
    state_.vanishingPoints[selectedPointIndex_].visible = visible;
    emit stateChanged();
    update();
}
void Canvas::setSelectedPointPosition(QPointF position) {
    if (selectedPointIndex_ < 0 || selectedPointIndex_ >= state_.vanishingPoints.size() ||
        !std::isfinite(position.x()) || !std::isfinite(position.y()) || std::abs(position.x()) > 1000000 ||
        std::abs(position.y()) > 1000000)
        return;
    auto &point = state_.vanishingPoints[selectedPointIndex_];
    if (point.isAttachedTo(PerspectiveTarget::horizon()))
        position.setY(state_.horizonY);
    if (point.isAttachedTo(PerspectiveTarget::vertical()))
        position.setX(state_.verticalX);
    if (point.position == position)
        return;
    finish();
    DrawingState before = state_;
    state_.vanishingPoints[selectedPointIndex_].position = position;
    updateSymmetricPoint(selectedPointIndex_);
    commit(before, tr("координаты точки схода"));
    emit stateChanged();
    update();
}
int Canvas::symmetricPartnerIndex(int movedIndex) const {
    if (movedIndex < 0 || movedIndex >= state_.vanishingPoints.size())
        return -1;
    const auto &moved = state_.vanishingPoints[movedIndex];
    if (moved.attachmentTargetIds.size() != 1)
        return -1;
    const QString target = moved.attachmentTargetIds.first();
    if ((target == PerspectiveTarget::horizon() && !state_.horizonSymmetry) ||
        (target == PerspectiveTarget::vertical() && !state_.verticalSymmetry))
        return -1;
    const auto points = attachedPointIndices(target);
    if (points.size() < 2)
        return -1;
    const QPointF reflected = target == PerspectiveTarget::horizon()
                                  ? QPointF(2 * state_.verticalX - moved.position.x(), state_.horizonY)
                                  : QPointF(state_.verticalX, 2 * state_.horizonY - moved.position.y());
    int nearest = -1;
    double nearestDistance = std::numeric_limits<double>::max();
    for (const int index : points) {
        if (index == movedIndex)
            continue;
        const QPointF delta = state_.vanishingPoints[index].position - reflected;
        const double distance = QPointF::dotProduct(delta, delta);
        if (distance < nearestDistance) {
            nearestDistance = distance;
            nearest = index;
        }
    }
    return nearest;
}
void Canvas::updateSymmetricPoint(int movedIndex, int partnerIndex) {
    if (movedIndex < 0 || movedIndex >= state_.vanishingPoints.size())
        return;
    if (partnerIndex < 0)
        partnerIndex = symmetricPartnerIndex(movedIndex);
    if (partnerIndex < 0 || partnerIndex >= state_.vanishingPoints.size() || partnerIndex == movedIndex)
        return;
    const auto &moved = state_.vanishingPoints[movedIndex];
    auto &other = state_.vanishingPoints[partnerIndex];
    if (moved.attachmentTargetIds.size() != 1 || moved.attachmentTargetIds != other.attachmentTargetIds)
        return;
    if (moved.isAttachedTo(PerspectiveTarget::horizon()) && state_.horizonSymmetry)
        other.position = QPointF(2 * state_.verticalX - moved.position.x(), state_.horizonY);
    else if (moved.isAttachedTo(PerspectiveTarget::vertical()) && state_.verticalSymmetry)
        other.position = QPointF(state_.verticalX, 2 * state_.horizonY - moved.position.y());
}
QVector<int> Canvas::attachedPointIndices(const QString &targetId) const {
    QVector<int> result;
    for (int i = 0; i < state_.vanishingPoints.size(); ++i) {
        const auto &point = state_.vanishingPoints[i];
        if (point.attachmentTargetIds == QStringList{targetId})
            result.append(i);
    }
    return result;
}
void Canvas::setSelectedPointAttachment(const QString &targetId) {
    if (selectedPointIndex_ < 0 || selectedPointIndex_ >= state_.vanishingPoints.size())
        return;
    auto &point = state_.vanishingPoints[selectedPointIndex_];
    QStringList targets;
    if (targetId == PerspectiveTarget::horizon() || targetId == PerspectiveTarget::vertical())
        targets.append(targetId);
    else if (targetId == PerspectiveTarget::intersection())
        targets = QStringList{PerspectiveTarget::horizon(), PerspectiveTarget::vertical()};
    if (point.attachmentTargetIds == targets)
        return;
    finish();
    DrawingState before = state_;
    point.attachmentTargetIds = targets;
    if (point.isAttachedTo(PerspectiveTarget::horizon()))
        point.position.setY(state_.horizonY);
    if (point.isAttachedTo(PerspectiveTarget::vertical()))
        point.position.setX(state_.verticalX);
    updateSymmetricPoint(selectedPointIndex_);
    commit(before, tr("привязку точки схода"));
    emit stateChanged();
    update();
}
void Canvas::setSelectedPointLocked(bool locked) {
    if (selectedPointIndex_ < 0 || selectedPointIndex_ >= state_.vanishingPoints.size() ||
        state_.vanishingPoints[selectedPointIndex_].locked == locked)
        return;
    finish();
    DrawingState before = state_;
    state_.vanishingPoints[selectedPointIndex_].locked = locked;
    commit(before, tr("фиксацию точки схода"));
    emit stateChanged();
    update();
}
QRectF Canvas::viewportRect() const {
    return QRectF(rect()).adjusted(rulerSize, rulerSize, -rulerSize, -rulerSize);
}
QPointF Canvas::toImage(QPointF point) const {
    return (point - viewportRect().center() - pan_) / zoom_ +
           QPointF(state_.canvasSize.width() / 2.0, state_.canvasSize.height() / 2.0);
}
QPointF Canvas::toView(QPointF point) const {
    return (point - QPointF(state_.canvasSize.width() / 2.0, state_.canvasSize.height() / 2.0)) * zoom_ +
           viewportRect().center() + pan_;
}
void Canvas::setZoom(double zoom, QPointF anchor) {
    if (anchor.x() < 0)
        anchor = viewportRect().center();
    const QPointF imagePoint = toImage(anchor);
    zoom_ = qBound(0.05, zoom, 16.0);
    pan_ += anchor - toView(imagePoint);
    emit viewChanged();
    update();
}
void Canvas::fit() {
    pan_ = QPointF();
    const QRectF viewport = viewportRect();
    zoom_ = qBound(
        0.05,
        qMin((viewport.width() - 60.0) / state_.canvasSize.width(),
             (viewport.height() - 60.0) / state_.canvasSize.height()),
        16.0);
    emit viewChanged();
    update();
}

void Canvas::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.fillRect(rect(), QColor("#dce0e5"));
    p.save();
    p.setClipRect(viewportRect());
    const QRectF paper(toView(QPointF()), QSizeF(state_.canvasSize) * zoom_);
    p.fillRect(paper.translated(3, 4), QColor(0, 0, 0, 35));
    p.fillRect(paper, Qt::white);
    p.setRenderHint(QPainter::SmoothPixmapTransform, zoom_ < 1);
    LayerRenderContext layerContext;
    layerContext.documentToDevice.translate(toView(QPointF()).x(), toView(QPointF()).y());
    layerContext.documentToDevice.scale(zoom_, zoom_);
    layerContext.deviceClip = paper.intersected(viewportRect()).toAlignedRect();
    layerContext.scale = zoom_;
    layerContext.dpi = logicalDpiX();
    layerContext.quality = LayerRenderContext::Interactive;
    LayerCompositor::render(p, state_.layers, layerContext);
    if (guidesVisible_) {
        p.setRenderHint(QPainter::Antialiasing);
        QHash<QString, QPointF> vanishingPoints;
        for (const VanishingPoint &point : state_.vanishingPoints)
            vanishingPoints.insert(point.id, point.position);
        for (const Guide &guide : state_.guides) {
            QColor color = guide.id == selectedGuideId_ ? QColor("#ed8b24") : QColor("#2f86c7");
            QPen pen(color, guide.id == selectedGuideId_ ? 2 : 1, Qt::DashLine);
            pen.setCosmetic(true);
            p.setPen(pen);
            if (guide.type == GuideType::Horizontal) {
                const double y = toView(QPointF(0, guide.position)).y();
                p.drawLine(QPointF(viewportRect().left(), y), QPointF(viewportRect().right(), y));
            } else if (guide.type == GuideType::Vertical) {
                const double x = toView(QPointF(guide.position, 0)).x();
                p.drawLine(QPointF(x, viewportRect().top()), QPointF(x, viewportRect().bottom()));
            } else if (vanishingPoints.contains(guide.vanishingPointId)) {
                const QPointF origin = toView(vanishingPoints.value(guide.vanishingPointId));
                const QPointF direction(std::cos(guide.angleRadians), std::sin(guide.angleRadians));
                QLineF visible;
                if (clippedRay(origin, direction, viewportRect(), &visible))
                    p.drawLine(visible);
            }
        }
    }
    if (creatingGuide_) {
        QPen preview(QColor("#ed8b24"), 1, Qt::DashLine);
        preview.setCosmetic(true);
        p.setPen(preview);
        if (creatingGuideType_ == GuideType::Horizontal) {
            const double y = toView(QPointF(0, guidePreviewPosition_)).y();
            p.drawLine(QPointF(viewportRect().left(), y), QPointF(viewportRect().right(), y));
        } else {
            const double x = toView(QPointF(guidePreviewPosition_, 0)).x();
            p.drawLine(QPointF(x, viewportRect().top()), QPointF(x, viewportRect().bottom()));
        }
    }
    if (state_.gridVisible) {
        p.setRenderHint(QPainter::Antialiasing);
        const QLineF edges[] = {QLineF(paper.topLeft(), paper.topRight()),
                                QLineF(paper.topRight(), paper.bottomRight()),
                                QLineF(paper.bottomRight(), paper.bottomLeft()),
                                QLineF(paper.bottomLeft(), paper.topLeft())};
        if (state_.axesVisible) {
            QPen axis(QColor(70, 80, 92, 95), 1, Qt::DashLine);
            axis.setCosmetic(true);
            p.setPen(axis);
            const QPointF center =
                toView(QPointF(state_.canvasSize.width() / 2.0, state_.canvasSize.height() / 2.0));
            p.drawLine(QPointF(center.x(), 0), QPointF(center.x(), height()));
            p.drawLine(QPointF(0, center.y()), QPointF(width(), center.y()));
        }
        const double horizonY = toView(QPointF(0, state_.horizonY)).y();
        QColor horizonColor = state_.horizonColor;
        horizonColor.setAlphaF(state_.horizonOpacity / 100.0);
        QPen horizon(horizonColor, state_.horizonWidth);
        horizon.setCosmetic(true);
        p.setPen(horizon);
        if (state_.horizonVisible)
            p.drawLine(QPointF(0, horizonY), QPointF(width(), horizonY));
        const double verticalX = toView(QPointF(state_.verticalX, 0)).x();
        QColor verticalColor = state_.verticalColor;
        verticalColor.setAlphaF(state_.verticalOpacity / 100.0);
        QPen vertical(verticalColor, state_.verticalWidth);
        vertical.setCosmetic(true);
        p.setPen(vertical);
        if (state_.verticalVisible)
            p.drawLine(QPointF(verticalX, 0), QPointF(verticalX, height()));
        for (int pointIndex = 0; pointIndex < state_.vanishingPoints.size(); ++pointIndex) {
            const auto &point = state_.vanishingPoints[pointIndex];
            const QPointF vanishing = toView(point.position);
            if (point.visible) {
                double radius = 0;
                const QPointF corners[] = {paper.topLeft(),
                                           paper.topRight(),
                                           paper.bottomLeft(),
                                           paper.bottomRight(),
                                           rect().topLeft(),
                                           rect().topRight(),
                                           rect().bottomLeft(),
                                           rect().bottomRight()};
                for (const QPointF &corner : corners)
                    radius = qMax(radius, QLineF(vanishing, corner).length());
                for (double degrees = state_.rayAngleOffset; degrees < 360.0 + state_.rayAngleOffset;
                     degrees += state_.rayStepDegrees) {
                    const double angle = degrees * 3.14159265358979323846 / 180.0;
                    const QPointF direction(std::cos(angle), std::sin(angle));
                    const QPointF start = vanishing + direction * state_.rayGap;
                    const QPointF viewportEnd = vanishing + direction * (radius + 2);
                    if (QLineF(vanishing, start).length() >= QLineF(vanishing, viewportEnd).length())
                        continue;
                    const QLineF visibleRay(start, viewportEnd);
                    QPointF intersection;
                    bool crossesCanvas = paper.contains(start);
                    double exitDistance = -1;
                    for (const QLineF &edge : edges) {
                        if (visibleRay.intersects(edge, &intersection) != QLineF::BoundedIntersection)
                            continue;
                        crossesCanvas = true;
                        exitDistance = qMax(exitDistance, QPointF::dotProduct(intersection - vanishing, direction));
                    }
                    if (!crossesCanvas || exitDistance < state_.rayGap)
                        continue;
                    const QPointF end = vanishing + direction * exitDistance;
                    QColor startColor = point.color;
                    startColor.setAlphaF(state_.rayStartOpacity / 100.0);
                    QColor endColor = point.color;
                    endColor.setAlphaF(state_.rayEndOpacity / 100.0);
                    QPen ray;
                    if (state_.rayFadeLength > 0) {
                        QLinearGradient fade(start, start + direction * state_.rayFadeLength);
                        fade.setColorAt(0, startColor);
                        fade.setColorAt(1, endColor);
                        ray = QPen(QBrush(fade), state_.rayWidth);
                    } else
                        ray = QPen(endColor, state_.rayWidth);
                    static const Qt::PenStyle styles[]{Qt::SolidLine, Qt::DashLine, Qt::DotLine, Qt::DashDotLine};
                    ray.setStyle(styles[qBound(0, state_.rayPattern, 3)]);
                    ray.setCosmetic(true);
                    p.setPen(ray);
                    p.drawLine(start, end);
                }
            }
            if (state_.markersVisible) {
                p.setPen(QPen(point.color, pointIndex == selectedPointIndex_ ? 3 : 2));
                p.setBrush(Qt::white);
                p.drawEllipse(vanishing, 6, 6);
                if (!point.attachmentTargetIds.isEmpty()) {
                    p.setPen(Qt::NoPen);
                    p.setBrush(point.color);
                    p.drawEllipse(vanishing, 2.5, 2.5);
                }
                p.setPen(QColor("#355274"));
                p.drawText(vanishing + QPointF(11, -10), tr("Точка схода %1").arg(pointIndex + 1));
            }
        }
    }
    if (isPaintTool() && hasPaintAnchor_ && hasHoverPoint_ && shiftPressed_ && !dragging_) {
        const QPointF endpoint = constrainedPoint(hoverPoint_, controlPressed_);
        p.save();
        p.setClipRect(paper);
        p.setRenderHint(QPainter::Antialiasing);
        QPen preview(QColor(40, 52, 68, 170), 1, Qt::DashLine);
        preview.setCosmetic(true);
        p.setPen(preview);
        p.drawLine(toView(paintAnchor_), toView(endpoint));
        p.restore();
    }
    p.restore();
    drawRulers(p);
}

void Canvas::drawRulers(QPainter &p) {
    const QRectF viewport = viewportRect();
    const QColor rulerBackground("#f4f5f7"), border("#9da4ae"), ink("#4c5664");
    p.fillRect(QRectF(0, 0, width(), rulerSize), rulerBackground);
    p.fillRect(QRectF(0, height() - rulerSize, width(), rulerSize), rulerBackground);
    p.fillRect(QRectF(0, rulerSize, rulerSize, height() - 2 * rulerSize), rulerBackground);
    p.fillRect(QRectF(width() - rulerSize, rulerSize, rulerSize, height() - 2 * rulerSize), rulerBackground);
    p.setPen(QPen(border, 1));
    p.drawRect(viewport);
    QFont font = p.font();
    font.setPixelSize(9);
    p.setFont(font);
    p.setPen(ink);
    const double horizontalUnitPixels = rulerPercent_ ? state_.canvasSize.width() / 100.0 : 1.0;
    const double verticalUnitPixels = rulerPercent_ ? state_.canvasSize.height() / 100.0 : 1.0;
    const double horizontalStep = niceStep(72.0 / (zoom_ * horizontalUnitPixels));
    const double verticalStep = niceStep(54.0 / (zoom_ * verticalUnitPixels));
    const double leftValue =
        (toImage(viewport.topLeft()).x() - state_.canvasSize.width() / 2.0) / horizontalUnitPixels;
    const double rightValue =
        (toImage(viewport.topRight()).x() - state_.canvasSize.width() / 2.0) / horizontalUnitPixels;
    for (double value = std::ceil(qMin(leftValue, rightValue) / horizontalStep) * horizontalStep;
         value <= qMax(leftValue, rightValue) + horizontalStep * 0.01;
         value += horizontalStep) {
        const double x = toView(QPointF(state_.canvasSize.width() / 2.0 + value * horizontalUnitPixels, 0)).x();
        if (x < viewport.left() - 1 || x > viewport.right() + 1)
            continue;
        p.drawLine(QPointF(x, viewport.top()), QPointF(x, viewport.top() - 7));
        p.drawLine(QPointF(x, viewport.bottom()), QPointF(x, viewport.bottom() + 7));
        const QString label = coordinateLabel(value);
        p.drawText(QRectF(x + 2, 2, 70, rulerSize - 9), Qt::AlignLeft | Qt::AlignVCenter, label);
        p.drawText(QRectF(x + 2, height() - rulerSize + 7, 70, rulerSize - 9), Qt::AlignLeft | Qt::AlignVCenter, label);
    }
    const double topValue =
        (state_.canvasSize.height() / 2.0 - toImage(viewport.topLeft()).y()) / verticalUnitPixels;
    const double bottomValue =
        (state_.canvasSize.height() / 2.0 - toImage(viewport.bottomLeft()).y()) / verticalUnitPixels;
    for (double value = std::ceil(qMin(topValue, bottomValue) / verticalStep) * verticalStep;
         value <= qMax(topValue, bottomValue) + verticalStep * 0.01;
         value += verticalStep) {
        const double y = toView(QPointF(0, state_.canvasSize.height() / 2.0 - value * verticalUnitPixels)).y();
        if (y < viewport.top() - 1 || y > viewport.bottom() + 1)
            continue;
        p.drawLine(QPointF(viewport.left(), y), QPointF(viewport.left() - 7, y));
        p.drawLine(QPointF(viewport.right(), y), QPointF(viewport.right() + 7, y));
        const QString label = coordinateLabel(value);
        p.drawText(QRectF(1, y - 8, rulerSize - 9, 16), Qt::AlignRight | Qt::AlignVCenter, label);
        p.drawText(QRectF(width() - rulerSize + 8, y - 8, rulerSize - 9, 16), Qt::AlignLeft | Qt::AlignVCenter, label);
    }
    if (!cursorInViewport_)
        return;
    QPen guide(QColor(49, 83, 130, 170), 1, Qt::DashLine);
    guide.setCosmetic(true);
    p.setPen(guide);
    p.drawLine(QPointF(cursorView_.x(), 0), QPointF(cursorView_.x(), height()));
    p.drawLine(QPointF(0, cursorView_.y()), QPointF(width(), cursorView_.y()));
}

void Canvas::stroke(QPointF start, QPointF end) {
    const double length = QLineF(start, end).length();
    const double step = qMax(1.0, strokeSettings_.width * strokeSettings_.spacing / 100.0);
    if (length <= 0.0001) {
        stamp(start);
        distanceToNextStamp_ = step;
        return;
    }
    double distance = qMax(0.0, distanceToNextStamp_);
    while (distance <= length) {
        stamp(start + (end - start) * (distance / length));
        distance += step;
    }
    distanceToNextStamp_ = distance - length;
    update();
}

void Canvas::stamp(QPointF center) {
    LayerEntry *entry = state_.layers.activeEntry();
    LayerContent *content = state_.layers.editableActiveContent(LayerCapability::RasterPainting);
    const LayerType *type = entry ? LayerTypeRegistry::instance().type(entry->typeId) : nullptr;
    QImage *image = content && type && type->rasterEditor ? type->rasterEditor(*content) : nullptr;
    if (!image)
        return;
    QPainter painter(image);
    const auto *raster = dynamic_cast<const RasterLayerContent *>(content);
    const bool eraseToTransparency = tool_ == Eraser && raster && raster->transparencyAvailable &&
                                     !raster->alphaLocked;
    if (eraseToTransparency)
        painter.setCompositionMode(QPainter::CompositionMode_DestinationOut);
    const bool softEdge = tool_ != Pencil && strokeSettings_.hardness < 100;
    painter.setRenderHint(QPainter::Antialiasing, tool_ != Pencil);
    painter.setPen(Qt::NoPen);
    QColor color = eraseToTransparency ? QColor(Qt::black) : tool_ == Eraser ? back_ : front_;
    const int amount = tool_ == Eraser ? strokeSettings_.strength : strokeSettings_.opacity;
    color.setAlphaF(amount / 100.0);
    const double radius = strokeSettings_.width / 2.0;
    center -= entry->offset;
    if (softEdge) {
        QRadialGradient gradient(center, radius);
        const double hardStop = qBound(0.0, strokeSettings_.hardness / 100.0, 0.999);
        gradient.setColorAt(0, color);
        gradient.setColorAt(hardStop, color);
        QColor transparent = color;
        transparent.setAlpha(0);
        gradient.setColorAt(1, transparent);
        painter.setBrush(gradient);
    } else {
        painter.setBrush(color);
    }
    painter.drawEllipse(center, radius, radius);
}
bool Canvas::isPaintTool() const {
    return tool_ == Pencil || tool_ == Brush || tool_ == Eraser;
}
QPointF Canvas::constrainedPoint(QPointF point, bool constrainAngle) const {
    if (!constrainAngle || !hasPaintAnchor_)
        return point;
    const QPointF delta = point - paintAnchor_;
    const double radius = std::hypot(delta.x(), delta.y());
    if (radius == 0)
        return point;
    constexpr double step = 3.14159265358979323846 / 12.0;
    const double angle = std::round(std::atan2(delta.y(), delta.x()) / step) * step;
    return paintAnchor_ + QPointF(std::cos(angle) * radius, std::sin(angle) * radius);
}
int Canvas::perspectiveHit(QPointF viewPosition) const {
    if (!state_.gridVisible || !viewportRect().contains(viewPosition))
        return NoPerspectiveHit;
    int pointHit = NoPerspectiveHit;
    double distance = 16;
    if (state_.markersVisible)
        for (int i = 0; i < state_.vanishingPoints.size(); ++i) {
            const double candidate = QLineF(viewPosition, toView(state_.vanishingPoints[i].position)).length();
            if (candidate <= distance) {
                distance = candidate;
                pointHit = i;
            }
        }
    if (pointHit >= 0)
        return pointHit;
    if (state_.verticalVisible && std::abs(viewPosition.x() - toView(QPointF(state_.verticalX, 0)).x()) <= 8)
        return VerticalHit;
    if (state_.horizonVisible && std::abs(viewPosition.y() - toView(QPointF(0, state_.horizonY)).y()) <= 8)
        return HorizonHit;
    return NoPerspectiveHit;
}
void Canvas::updatePerspectiveCursor(QPointF viewPosition) {
    if (tool_ != Perspective || dragging_)
        return;
    const int hit = perspectiveHit(viewPosition);
    bool movable = false;
    if (hit >= 0)
        movable = !state_.vanishingPoints[hit].locked;
    else if (hit == VerticalHit)
        movable = !state_.verticalLocked;
    else if (hit == HorizonHit)
        movable = !state_.horizonLocked;
    setCursor(movable ? Qt::OpenHandCursor : hit != NoPerspectiveHit ? Qt::ForbiddenCursor : Qt::CrossCursor);
}
void Canvas::mousePressEvent(QMouseEvent *e) {
    if (e->button() != Qt::LeftButton && e->button() != Qt::MiddleButton)
        return;
    GuideType guideType;
    if (e->button() == Qt::LeftButton && rulerGuideType(e->localPos(), &guideType)) {
        setFocus();
        before_ = state_;
        creatingGuide_ = dragging_ = true;
        creatingGuideType_ = guideType;
        const QPointF image = toImage(e->localPos());
        guidePreviewPosition_ = guideType == GuideType::Horizontal ? image.y() : image.x();
        setCursor(Qt::ClosedHandCursor);
        update();
        return;
    }
    if (e->button() == Qt::LeftButton && !viewportRect().contains(e->localPos()))
        return;
    setFocus();
    panning_ = e->button() == Qt::MiddleButton || tool_ == Pan || space_;
    if (panning_) {
        dragging_ = true;
        last_ = e->localPos();
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    if (tool_ == Perspective) {
        const int hit = perspectiveHit(e->localPos());
        if (hit >= 0) {
            selectPoint(hit);
            if (state_.vanishingPoints[hit].locked)
                return;
            before_ = state_;
            movingPointIndex_ = hit;
            // Партнёр выбирается один раз при захвате и не перескакивает между точками во время движения.
            movingSymmetricPointIndex_ = symmetricPartnerIndex(hit);
            movingPoint_ = dragging_ = true;
            setCursor(Qt::ClosedHandCursor);
            return;
        }
        if (hit == VerticalHit) {
            if (state_.verticalLocked)
                return;
            before_ = state_;
            movingVertical_ = dragging_ = true;
            setCursor(Qt::ClosedHandCursor);
            return;
        }
        if (hit == HorizonHit) {
            if (state_.horizonLocked)
                return;
            before_ = state_;
            movingHorizon_ = dragging_ = true;
            setCursor(Qt::ClosedHandCursor);
            return;
        }
        return;
    }
    QPointF point = toImage(e->localPos());
    if (!QRectF(QPointF(), state_.canvasSize).contains(point))
        return;
    before_ = state_;
    dragging_ = true;
    distanceToNextStamp_ = 0;
    const bool straight = (e->modifiers() & Qt::ShiftModifier) || shiftPressed_;
    if (straight && hasPaintAnchor_) {
        point = constrainedPoint(point, (e->modifiers() & Qt::ControlModifier) || controlPressed_);
        straightStroke_ = true;
        last_ = point;
        stroke(paintAnchor_, point);
    } else {
        straightStroke_ = false;
        last_ = point;
        stroke(point, point);
    }
}
void Canvas::mouseMoveEvent(QMouseEvent *e) {
    cursorView_ = e->localPos();
    cursorInViewport_ = viewportRect().contains(cursorView_);
    hoverPoint_ = toImage(cursorView_);
    hasHoverPoint_ = QRectF(QPointF(), state_.canvasSize).contains(hoverPoint_);
    emit positionChanged(hoverPoint_);
    if (!dragging_) {
        updatePerspectiveCursor(e->localPos());
        update();
        return;
    }
    if (creatingGuide_) {
        const QPointF image = toImage(e->localPos());
        guidePreviewPosition_ = creatingGuideType_ == GuideType::Horizontal ? image.y() : image.x();
        update();
    } else if (panning_) {
        pan_ += e->localPos() - last_;
        last_ = e->localPos();
        update();
    } else if (movingPoint_ && movingPointIndex_ >= 0 && movingPointIndex_ < state_.vanishingPoints.size()) {
        QPointF point = toImage(e->localPos());
        auto &vanishing = state_.vanishingPoints[movingPointIndex_];
        const double horizonDistance = std::abs(e->localPos().y() - toView(QPointF(0, state_.horizonY)).y()),
                     verticalDistance = std::abs(e->localPos().x() - toView(QPointF(state_.verticalX, 0)).x());
        QStringList targets;
        if (horizonDistance <= 10)
            targets.append(PerspectiveTarget::horizon());
        if (verticalDistance <= 10)
            targets.append(PerspectiveTarget::vertical());
        vanishing.attachmentTargetIds = targets;
        if (vanishing.isAttachedTo(PerspectiveTarget::horizon()))
            point.setY(state_.horizonY);
        if (vanishing.isAttachedTo(PerspectiveTarget::vertical()))
            point.setX(state_.verticalX);
        vanishing.position = point;
        updateSymmetricPoint(movingPointIndex_, movingSymmetricPointIndex_);
        update();
    } else if (movingHorizon_) {
        moveHorizon(toImage(e->localPos()).y());
        update();
    } else if (movingVertical_) {
        moveVertical(toImage(e->localPos()).x());
        update();
    } else if (!straightStroke_) {
        stroke(last_, hoverPoint_);
        last_ = hoverPoint_;
    }
}

void Canvas::moveHorizon(double imageY) {
    const double delta = imageY - state_.horizonY;
    state_.horizonY = imageY;
    // Двойная привязка меняет только координату своей оси; вторую координату сохраняет другая ось.
    for (auto &point : state_.vanishingPoints)
        if (point.isAttachedTo(PerspectiveTarget::horizon()))
            point.position.setY(imageY);
    if (state_.verticalSymmetry && attachedPointIndices(PerspectiveTarget::vertical()).size() >= 2)
        for (auto &point : state_.vanishingPoints)
            if (point.attachmentTargetIds == QStringList{PerspectiveTarget::vertical()})
                point.position.ry() += delta;
}

void Canvas::moveVertical(double imageX) {
    const double delta = imageX - state_.verticalX;
    state_.verticalX = imageX;
    for (auto &point : state_.vanishingPoints)
        if (point.isAttachedTo(PerspectiveTarget::vertical()))
            point.position.setX(imageX);
    if (state_.horizonSymmetry && attachedPointIndices(PerspectiveTarget::horizon()).size() >= 2)
        for (auto &point : state_.vanishingPoints)
            if (point.attachmentTargetIds == QStringList{PerspectiveTarget::horizon()})
                point.position.rx() += delta;
}

void Canvas::finish() {
    if (!dragging_)
        return;
    // Независимо от числа промежуточных mouseMove весь жест становится одной командой истории.
    if (!panning_ && (movingPoint_      ? state_.vanishingPoints != before_.vanishingPoints
                      : movingHorizon_  ? state_.horizonY != before_.horizonY
                      : movingVertical_ ? state_.verticalX != before_.verticalX
                                        : state_.layers != before_.layers))
        commit(before_,
               movingPoint_      ? tr("точку схода")
               : movingHorizon_  ? tr("горизонт")
               : movingVertical_ ? tr("главную вертикаль")
                                 : (tool_ == Eraser ? tr("ластик") : tr("штрих")));
    if (!panning_ && !movingPoint_ && !movingHorizon_ && !movingVertical_ && isPaintTool()) {
        paintAnchor_ = last_;
        hasPaintAnchor_ = true;
    }
    before_ = DrawingState();
    dragging_ = panning_ = movingPoint_ = movingHorizon_ = movingVertical_ = straightStroke_ = creatingGuide_ = false;
    movingPointIndex_ = movingSymmetricPointIndex_ = -1;
    setCursor(tool_ == Pan ? Qt::OpenHandCursor : Qt::CrossCursor);
}
void Canvas::mouseReleaseEvent(QMouseEvent *e) {
    if (creatingGuide_) {
        finishGuideCreation(e->localPos());
        return;
    }
    finish();
    updatePerspectiveCursor(e->localPos());
}
void Canvas::wheelEvent(QWheelEvent *e) {
    setZoom(zoom_ * std::pow(1.15, e->angleDelta().y() / 120.0), e->position());
    e->accept();
}
void Canvas::keyPressEvent(QKeyEvent *e) {
    if (e->key() == Qt::Key_Space) {
        space_ = true;
        setCursor(Qt::OpenHandCursor);
        e->accept();
    } else if (e->key() == Qt::Key_Shift) {
        shiftPressed_ = true;
        update();
        e->accept();
    } else if (e->key() == Qt::Key_Control) {
        controlPressed_ = true;
        update();
        e->accept();
    } else if (e->key() == Qt::Key_Escape && dragging_) {
        if (!panning_)
            state_ = before_;
        dragging_ = panning_ = movingPoint_ = movingHorizon_ = movingVertical_ = creatingGuide_ = false;
        movingPointIndex_ = -1;
        before_ = DrawingState();
        update();
    } else
        QWidget::keyPressEvent(e);
}
void Canvas::keyReleaseEvent(QKeyEvent *e) {
    if (e->key() == Qt::Key_Space) {
        space_ = false;
        setCursor(tool_ == Pan ? Qt::OpenHandCursor : Qt::CrossCursor);
    } else if (e->key() == Qt::Key_Shift) {
        shiftPressed_ = false;
        update();
    } else if (e->key() == Qt::Key_Control) {
        controlPressed_ = false;
        update();
    } else
        QWidget::keyReleaseEvent(e);
}
void Canvas::focusOutEvent(QFocusEvent *e) {
    finish();
    space_ = shiftPressed_ = controlPressed_ = false;
    update();
    QWidget::focusOutEvent(e);
}
void Canvas::leaveEvent(QEvent *e) {
    cursorInViewport_ = false;
    hasHoverPoint_ = false;
    if (tool_ == Perspective && !dragging_)
        setCursor(Qt::CrossCursor);
    update();
    QWidget::leaveEvent(e);
}

bool Canvas::rulerGuideType(QPointF viewPosition, GuideType *type) const {
    const QRectF viewport = viewportRect();
    if (viewPosition.x() >= viewport.left() && viewPosition.x() <= viewport.right() &&
        (viewPosition.y() < viewport.top() || viewPosition.y() > viewport.bottom())) {
        *type = GuideType::Horizontal;
        return true;
    }
    if (viewPosition.y() >= viewport.top() && viewPosition.y() <= viewport.bottom() &&
        (viewPosition.x() < viewport.left() || viewPosition.x() > viewport.right())) {
        *type = GuideType::Vertical;
        return true;
    }
    return false;
}

void Canvas::finishGuideCreation(QPointF viewPosition) {
    const QPointF image = toImage(viewPosition);
    const bool accepted = viewportRect().contains(viewPosition) && QRectF(QPointF(), state_.canvasSize).contains(image);
    dragging_ = creatingGuide_ = false;
    if (accepted) {
        Guide guide;
        guide.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        guide.type = creatingGuideType_;
        guide.position = creatingGuideType_ == GuideType::Horizontal ? image.y() : image.x();
        state_.guides.append(guide);
        selectedGuideId_ = guide.id;
        commit(before_, tr("создание направляющей"));
        emit selectedGuideChanged(selectedGuideId_);
        emit stateChanged();
        setTool(Pan);
    } else {
        setCursor(tool_ == Pan ? Qt::OpenHandCursor : Qt::CrossCursor);
    }
    before_ = DrawingState();
    update();
}
