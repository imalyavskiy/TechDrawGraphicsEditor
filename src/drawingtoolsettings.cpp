#include "drawingtoolsettings.h"

#include <QSettings>
#include <QtGlobal>

namespace {
/// Возвращает устойчивую часть ключа настроек для указанного рисующего инструмента.
QString toolKey(int tool) {
    static const QStringList keys{QStringLiteral("pencil"), QStringLiteral("brush"), QStringLiteral("eraser")};
    return keys[qBound(0, tool, keys.size() - 1)];
}

/// Возвращает прежний ключ ширины для совместимости с уже сохранёнными настройками.
QString legacyWidthKey(int tool) {
    static const QStringList keys{
        QStringLiteral("tools/pencilWidth"), QStringLiteral("tools/brushWidth"), QStringLiteral("tools/eraserWidth")};
    return keys[qBound(0, tool, keys.size() - 1)];
}
} // namespace

DrawingToolSettingsModel::DrawingToolSettingsModel(QObject *parent) : QObject(parent) {
    QSettings stored;
    const std::array<int, PaintToolCount> defaultHardness{100, 70, 100};
    for (int tool = 0; tool < PaintToolCount; ++tool) {
        const QString prefix = QStringLiteral("tools/%1/").arg(toolKey(tool));
        auto &settings = settings_[tool];
        settings.width = qBound(1, stored.value(prefix + "width", stored.value(legacyWidthKey(tool), 3)).toInt(), 200);
        settings.opacity = qBound(1, stored.value(prefix + "opacity", 100).toInt(), 100);
        settings.hardness = qBound(0, stored.value(prefix + "hardness", defaultHardness[tool]).toInt(), 100);
        settings.spacing = qBound(1, stored.value(prefix + "spacing", 15).toInt(), 100);
        settings.strength = qBound(1, stored.value(prefix + "strength", 100).toInt(), 100);
    }
    settings_[Pencil].hardness = 100;
}

const DrawingToolSettings &DrawingToolSettingsModel::settingsFor(int tool) const {
    return settings_[validTool(tool) ? tool : Pencil];
}

int DrawingToolSettingsModel::activeTool() const {
    return activeTool_;
}

void DrawingToolSettingsModel::setActiveTool(int tool) {
    const int next = validTool(tool) ? tool : -1;
    if (activeTool_ == next)
        return;
    activeTool_ = next;
    emit activeToolChanged();
}

const DrawingTargetContext &DrawingToolSettingsModel::targetContext() const {
    return targetContext_;
}

void DrawingToolSettingsModel::setTargetContext(const DrawingTargetContext &context) {
    if (targetContext_.editable == context.editable &&
        targetContext_.supportsTransparency == context.supportsTransparency &&
        targetContext_.alphaLocked == context.alphaLocked &&
        targetContext_.unavailableReason == context.unavailableReason)
        return;
    targetContext_ = context;
    emit targetContextChanged();
}

void DrawingToolSettingsModel::setWidth(int value) {
    updateValue(&DrawingToolSettings::width, value, 1, 200, QStringLiteral("width"));
}

void DrawingToolSettingsModel::setOpacity(int value) {
    updateValue(&DrawingToolSettings::opacity, value, 1, 100, QStringLiteral("opacity"));
}

void DrawingToolSettingsModel::setHardness(int value) {
    if (activeTool_ == Pencil)
        value = 100;
    updateValue(&DrawingToolSettings::hardness, value, 0, 100, QStringLiteral("hardness"));
}

void DrawingToolSettingsModel::setSpacing(int value) {
    updateValue(&DrawingToolSettings::spacing, value, 1, 100, QStringLiteral("spacing"));
}

void DrawingToolSettingsModel::setStrength(int value) {
    updateValue(&DrawingToolSettings::strength, value, 1, 100, QStringLiteral("strength"));
}

bool DrawingToolSettingsModel::validTool(int tool) {
    return tool >= Pencil && tool < PaintToolCount;
}

void DrawingToolSettingsModel::updateValue(
    int DrawingToolSettings::*field, int value, int minimum, int maximum, const QString &name) {
    if (!validTool(activeTool_))
        return;
    value = qBound(minimum, value, maximum);
    auto &settings = settings_[activeTool_];
    if (settings.*field == value)
        return;
    settings.*field = value;
    const QString prefix = QStringLiteral("tools/%1/").arg(toolKey(activeTool_));
    QSettings().setValue(prefix + name, value);
    if (name == QStringLiteral("width"))
        QSettings().setValue(legacyWidthKey(activeTool_), value);
    emit settingsChanged();
}
