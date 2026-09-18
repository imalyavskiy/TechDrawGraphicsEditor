#include "selftest.h"
#include "autohidedockwidget.h"
#include "layermodel.h"
#include "mainwindow.h"
#include <QtWidgets>
#include <private/qzipreader_p.h>
#include <private/qzipwriter_p.h>
#include <cmath>
#include <stdexcept>

namespace {
QString tracePath;
/// Записывает результат утверждения в трассировку и прерывает сценарий при ошибке.
void require(bool value, const char *message) {
    QFile trace(tracePath);
    if (trace.open(QIODevice::Append)) {
        trace.write(value ? "PASS " : "FAIL ");
        trace.write(message);
        trace.write("\n");
    }
    if (!value)
        throw std::runtime_error(message);
}
/// Доставляет Canvas синтетическое событие мыши с заданными кнопками и модификаторами.
void mouse(Canvas *canvas,
           QEvent::Type type,
           QPointF position,
           Qt::MouseButton button,
           Qt::MouseButtons buttons,
           Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QMouseEvent event(type, position, button, buttons, modifiers);
    QApplication::sendEvent(canvas, &event);
}
/// Имитирует перетаскивание между двумя точками в координатах изображения.
void drag(Canvas *canvas, QPointF a, QPointF b) {
    mouse(canvas, QEvent::MouseButtonPress, canvas->toView(a), Qt::LeftButton, Qt::LeftButton);
    mouse(canvas, QEvent::MouseMove, canvas->toView(b), Qt::NoButton, Qt::LeftButton);
    mouse(canvas, QEvent::MouseButtonRelease, canvas->toView(b), Qt::LeftButton, Qt::NoButton);
}
/// Имитирует щелчок по точке изображения с необязательными модификаторами клавиатуры.
void click(Canvas *canvas, QPointF point, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    mouse(canvas, QEvent::MouseButtonPress, canvas->toView(point), Qt::LeftButton, Qt::LeftButton, modifiers);
    mouse(canvas, QEvent::MouseButtonRelease, canvas->toView(point), Qt::LeftButton, Qt::NoButton, modifiers);
}
} // namespace

namespace {
/// Передаёт результаты теста рисования и проекта последующим проверкам совместимости.
struct ProjectFixture {
    QImage renderedPixels;
    DrawingState loadedState;
    QString projectPath;
};

/// Возвращает итоговые пиксели снимка независимо от внутреннего числа и типов слоёв.
QImage pixels(const DrawingState &state) {
    return state.flattenedImage();
}

/// Получает изменяемый растр активного слоя через реестр типов, как это делает инструмент рисования.
QImage *editablePixels(DrawingState *state) {
    LayerEntry *entry = state->layers.activeEntry();
    LayerContent *content = state->layers.editableActiveContent(LayerCapability::RasterPainting);
    const LayerType *type = entry ? LayerTypeRegistry::instance().type(entry->typeId) : nullptr;
    return content && type && type->rasterEditor ? type->rasterEditor(*content) : nullptr;
}

/// Проверяет реестр типов, copy-on-write, композицию, миниатюру и кодек растрового слоя.
void testLayerArchitecture() {
    QImage source(24, 16, QImage::Format_ARGB32_Premultiplied);
    source.fill(QColor("#b94c42"));
    LayerStack layers = LayerStack::singleRaster(source, QStringLiteral("Background"), false, true);
    require(layers.entries().size() == 1 && layers.activeEntry() &&
                layers.activeEntry()->typeId == LayerTypes::raster(),
            "single raster layer stack was not created");
    const LayerType *type = LayerTypeRegistry::instance().type(LayerTypes::raster());
    require(type && type->renderer && type->codec && type->rasterEditor &&
                type->capabilities.testFlag(LayerCapability::RasterPainting),
            "raster layer type is not fully registered");

    LayerStack snapshot = layers;
    LayerContent *editable = layers.editableActiveContent(LayerCapability::RasterPainting);
    QImage *pixels = editable ? type->rasterEditor(*editable) : nullptr;
    require(pixels, "raster layer did not provide an editable target");
    pixels->setPixelColor(3, 4, QColor("#234f8c"));
    const auto *snapshotRaster = dynamic_cast<const RasterLayerContent *>(snapshot.activeEntry()->content.get());
    require(snapshotRaster && snapshotRaster->image.pixelColor(3, 4) == QColor("#b94c42"),
            "editing a layer changed a shared history snapshot");

    QImage composition = LayerCompositor::compose(layers, source.size());
    require(composition.pixelColor(3, 4) == QColor("#234f8c"),
            "layer compositor did not render edited raster content");
    QImage thumbnail = LayerCompositor::thumbnail(layers, source.size(), QSize(48, 48));
    require(thumbnail.size() == QSize(48, 48) && !thumbnail.isNull(), "layer compositor did not create a thumbnail");

    QJsonObject manifest;
    QHash<QString, QByteArray> resources;
    QString error;
    require(type->codec->encode(*layers.activeEntry()->content,
                                QStringLiteral("layers/test"),
                                &manifest,
                                &resources,
                                &error),
            qPrintable(error));
    auto decoded = type->codec->decode(
        manifest, [&resources](const QString &path) { return resources.value(path); }, source.size(), &error);
    require(decoded && decoded->equals(*layers.activeEntry()->content), "raster layer codec roundtrip failed");
}

/// Создаёт воспроизводимый снимок документа для всех групп интеграционных проверок.
DrawingState initialDrawingState() {
    DrawingState initial;
    QImage image(1000, 620, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    initial.setSingleRasterImage(image, QStringLiteral("Background"));
    initial.horizonY = 240;
    initial.verticalX = 500;
    initial.vanishingPoints.append({QStringLiteral("vp-1"), QPointF(650, 240), QString(), QString()});
    return initial;
}

/// Проверяет компоновку, меню, настройки и основные диалоги главного окна.
void testMainWindowUi(MainWindow &window, const QDir &out) {
    require(QCoreApplication::translate("TranslationProbe", "catalog-loaded") == QStringLiteral("catalog-loaded-ru"),
            "external translation catalog was not loaded");
    window.show();
    QApplication::processEvents();
    require(QCoreApplication::applicationName() == QStringLiteral("TechDraw") &&
                QApplication::applicationDisplayName() == QStringLiteral("Технический рисунок / Technical Draw") &&
                window.windowTitle().contains(QStringLiteral("Технический рисунок / Technical Draw")),
            "visible application branding must use the full product name");
    auto *aboutAction = window.findChild<QAction *>("aboutAction");
    bool aboutChecked = false;
    QTimer::singleShot(0, &window, [&] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        auto *icon = dialog ? dialog->findChild<QLabel *>("aboutIcon") : nullptr;
        auto *name = dialog ? dialog->findChild<QLabel *>("aboutName") : nullptr;
        aboutChecked = dialog && dialog->windowTitle() == QStringLiteral("О программе") && icon &&
                       !icon->pixmap(Qt::ReturnByValue).isNull() && name &&
                       name->text() == QStringLiteral("Технический рисунок\nTechnical Draw");
        if (dialog)
            dialog->reject();
    });
    require(aboutAction, "About action is missing");
    aboutAction->trigger();
    require(aboutChecked, "About dialog must contain the application icon and full product name");
    Canvas *canvas = window.canvas();
    auto *mainToolbar = window.findChild<QToolBar *>("mainToolbar");
    auto *toolsToolbar = window.findChild<QToolBar *>("toolsToolbar");
    auto *toolsDock = window.findChild<AutoHideDockWidget *>("toolsDock");
    auto *fileMenu = window.findChild<QMenu *>("fileMenu");
    auto *editMenu = window.findChild<QMenu *>("editMenu");
    auto *viewMenu = window.findChild<QMenu *>("viewMenu");
    auto *toolsMenu = window.findChild<QMenu *>("toolsMenu");
    require(mainToolbar && toolsToolbar && toolsDock && mainToolbar->toolButtonStyle() == Qt::ToolButtonIconOnly &&
                toolsToolbar->toolButtonStyle() == Qt::ToolButtonIconOnly &&
                toolsToolbar->orientation() == Qt::Horizontal,
            "toolbars must show icons only and drawing tools must be horizontal");
    auto *mainToolbarToggle = window.findChild<QAction *>("mainToolbarToggle");
    auto *toolsToolbarToggle = window.findChild<QAction *>("toolsToolbarToggle");
    require(fileMenu && editMenu && viewMenu && toolsMenu && mainToolbarToggle && toolsToolbarToggle &&
                viewMenu->actions().contains(mainToolbarToggle) && viewMenu->actions().contains(toolsToolbarToggle),
            "menus or toolbar visibility actions are missing");
    mainToolbarToggle->trigger();
    QApplication::processEvents();
    require(!mainToolbar->isVisible(), "main toolbar could not be hidden through View menu");
    mainToolbarToggle->trigger();
    toolsToolbarToggle->trigger();
    QApplication::processEvents();
    require(!toolsToolbar->isVisible(), "tools toolbar could not be hidden through View menu");
    toolsToolbarToggle->trigger();
    QApplication::processEvents();
    auto *toolsPin = window.findChild<QToolButton *>("toolsPanelPin");
    auto *toolsTitle = window.findChild<QLabel *>("toolsPanelTitle");
    auto *toolsStrip = window.findChild<QDockWidget *>("toolsAutoHideStrip");
    auto *toolsTab = window.findChild<QToolButton *>("toolsAutoHideTab");
    auto *toolsOverlay = window.findChild<QWidget *>("toolsAutoHideOverlay");
    require(toolsPin && toolsTitle && toolsStrip && toolsTab && toolsOverlay && toolsDock->isPinned() &&
                toolsPin->mapTo(&window, QPoint()).x() > toolsTitle->mapTo(&window, QPoint()).x(),
            "left panel pin or auto-hide controls are missing");
    toolsPin->click();
    QApplication::processEvents();
    require(!toolsDock->isPinned(), "left panel did not switch to auto-hide mode");
    require(!toolsToolbar->isVisible(), "unpinning the left panel did not remove it from the layout");
    require(toolsStrip->isVisible() && toolsTab->isVisible(),
            "unpinning the left panel did not reveal its edge tab");
    require(toolsStrip->geometry().right() < canvas->geometry().left(),
            "the left auto-hide tab must occupy a strip outside the canvas and its rulers");
    require(canvas->mapTo(&window, QPoint()).x() -
                    (toolsTab->mapTo(&window, QPoint()).x() + toolsTab->width()) >=
                3,
            "the left auto-hide tab must leave a small gap before the ruler");
    require(toolsToolbarToggle->isChecked(), "unpinning the left panel changed its View menu state");
    toolsToolbarToggle->trigger();
    QApplication::processEvents();
    require(!toolsStrip->isVisible(), "the View menu did not hide the unpinned left panel and its tab");
    toolsToolbarToggle->trigger();
    QApplication::processEvents();
    require(toolsStrip->isVisible() && toolsTab->isVisible(),
            "the View menu did not restore the unpinned left panel tab");
    toolsTab->click();
    QApplication::processEvents();
    require(toolsOverlay->isVisible() && toolsToolbar->isVisible(),
            "the left edge tab must reveal the drawing panel over the workspace");
    QMouseEvent outsideTools(QEvent::MouseButtonPress,
                             QPointF(2, 2),
                             Qt::LeftButton,
                             Qt::LeftButton,
                             Qt::NoModifier);
    QApplication::sendEvent(mainToolbar, &outsideTools);
    QApplication::processEvents();
    require(!toolsOverlay->isVisible() && toolsTab->isVisible(),
            "clicking outside an unpinned panel must hide it without hiding its tab");
    toolsTab->click();
    toolsPin->click();
    QApplication::processEvents();
    require(toolsDock->isPinned() && toolsToolbar->isVisible() && !toolsTab->isVisible(),
            "pinning the open left panel must restore it to the window layout");
    auto *newMenuAction = window.findChild<QAction *>("newAction");
    auto *openMenuAction = window.findChild<QAction *>("openAction");
    auto *saveMenuAction = window.findChild<QAction *>("saveAction");
    auto *undoMenuAction = window.findChild<QAction *>("undoAction");
    auto *redoMenuAction = window.findChild<QAction *>("redoAction");
    auto *frontMenuAction = window.findChild<QAction *>("frontColorAction");
    auto *backMenuAction = window.findChild<QAction *>("backColorAction");
    auto *swapMenuAction = window.findChild<QAction *>("swapColorsAction");
    require(newMenuAction && openMenuAction && saveMenuAction && undoMenuAction && redoMenuAction &&
                frontMenuAction && backMenuAction && swapMenuAction && fileMenu->actions().contains(newMenuAction) &&
                fileMenu->actions().contains(openMenuAction) && fileMenu->actions().contains(saveMenuAction) &&
                editMenu->actions().contains(undoMenuAction) && editMenu->actions().contains(redoMenuAction) &&
                toolsMenu->actions().contains(frontMenuAction) && toolsMenu->actions().contains(backMenuAction) &&
                toolsMenu->actions().contains(swapMenuAction),
            "main toolbar commands are not all available through menus");
    for (int i = 0; i < 5; ++i) {
        auto *toolAction = window.findChild<QAction *>(QString("tool%1").arg(i));
        require(toolAction && toolsMenu->actions().contains(toolAction), "drawing tool is missing from Tools menu");
    }
    auto *perspectiveAction = window.findChild<QAction *>("tool4");
    require(perspectiveAction && perspectiveAction->text() == QStringLiteral("Перспектива") &&
                mainToolbar->actions().contains(perspectiveAction) && !toolsToolbar->actions().contains(perspectiveAction),
            "perspective mode must be available from the main toolbar, not the drawing tools toolbar");
    auto *toolProperties = window.findChild<QWidget *>("toolPropertiesPanel");
    auto *toolPropertiesTitle = window.findChild<QLabel *>("toolPropertiesTitle");
    auto *widthControl = window.findChild<QSpinBox *>("strokeWidth");
    auto *opacityControl = window.findChild<QSpinBox *>("strokeOpacity");
    auto *hardnessControl = window.findChild<QSpinBox *>("strokeHardness");
    auto *spacingControl = window.findChild<QSpinBox *>("strokeSpacing");
    auto *strengthControl = window.findChild<QSpinBox *>("eraserStrength");
    auto *strokePreview = window.findChild<QWidget *>("strokePreview");
    require(toolProperties && toolPropertiesTitle && widthControl && opacityControl && hardnessControl &&
                spacingControl && strengthControl && strokePreview && toolsDock->isAncestorOf(toolProperties) &&
                toolPropertiesTitle->text() == QStringLiteral("Карандаш"),
            "the shared drawing tool properties panel is missing from below the tool selector");
    auto *frontButton = window.findChild<QPushButton *>("frontColor");
    auto *backButton = window.findChild<QPushButton *>("backColor");
    require(frontButton && backButton && frontButton->text().isEmpty() && backButton->text().isEmpty() &&
                !frontButton->toolTip().isEmpty() && !backButton->toolTip().isEmpty(),
            "color buttons must use tooltips instead of labels");
    auto *fitButton = window.findChild<QToolButton *>("fitButton");
    auto *actualButton = window.findChild<QToolButton *>("actualSizeButton");
    require(fitButton && actualButton && fitButton->toolButtonStyle() == Qt::ToolButtonIconOnly &&
                actualButton->toolButtonStyle() == Qt::ToolButtonIconOnly && !fitButton->toolTip().isEmpty() &&
                !actualButton->toolTip().isEmpty(),
            "status buttons must show icons and tooltips");
    auto *rayStep = window.findChild<QDoubleSpinBox *>("rayStep");
    auto *rayGap = window.findChild<QSpinBox *>("rayGap");
    auto *rayStartOpacity = window.findChild<QSpinBox *>("rayStartOpacity");
    auto *rayEndOpacity = window.findChild<QSpinBox *>("rayEndOpacity");
    auto *rayFadeLength = window.findChild<QSpinBox *>("rayFadeLength");
    auto *rayPattern = window.findChild<QComboBox *>("rayPattern");
    auto *rayWidth = window.findChild<QDoubleSpinBox *>("rayWidth");
    auto *rayAngleOffset = window.findChild<QDoubleSpinBox *>("rayAngleOffset");
    require(rayStep && rayGap && rayStartOpacity && rayEndOpacity && rayFadeLength && rayPattern && rayWidth &&
                rayAngleOffset && rayStep->maximum() == 30.0 && rayStep->value() == 10.0 && rayGap->value() == 12 &&
                rayStartOpacity->value() == 10 && rayEndOpacity->value() == 70 && rayFadeLength->value() == 50 &&
                rayPattern->count() == 4 && rayWidth->value() == 1 && rayAngleOffset->value() == 0,
            "perspective ray appearance controls are missing or invalid");
    auto *vanishingPointSettings = window.findChild<QWidget *>("vanishingPointSettings");
    auto *vanishingPointsList = window.findChild<QWidget *>("vanishingPointsList");
    auto *selectedPointSettings = window.findChild<QWidget *>("selectedVanishingPointSettings");
    auto *pointsListControl = window.findChild<QListWidget *>("vanishingPointsListControl");
    auto *addPoint = window.findChild<QPushButton *>("addVanishingPoint");
    auto *removePoint = window.findChild<QPushButton *>("removeVanishingPoint");
    auto *gridColor = window.findChild<QPushButton *>("gridColor");
    auto *savePerspectiveDefaults = window.findChild<QPushButton *>("savePerspectiveDefaults");
    auto *resetPerspectiveDefaults = window.findChild<QPushButton *>("resetPerspectiveDefaults");
    require(vanishingPointSettings &&
                vanishingPointSettings->property("title").toString() == QStringLiteral("Настройки точек схода") &&
                vanishingPointsList && selectedPointSettings && pointsListControl && addPoint &&
                addPoint->text() == QStringLiteral("Добавить") && removePoint && gridColor && savePerspectiveDefaults &&
                resetPerspectiveDefaults && vanishingPointSettings->isAncestorOf(rayStep) &&
                vanishingPointSettings->isAncestorOf(rayGap) && vanishingPointSettings->isAncestorOf(rayStartOpacity) &&
                vanishingPointSettings->isAncestorOf(rayEndOpacity) &&
                vanishingPointSettings->isAncestorOf(rayFadeLength) &&
                vanishingPointSettings->isAncestorOf(savePerspectiveDefaults) &&
                vanishingPointSettings->isAncestorOf(resetPerspectiveDefaults) &&
                vanishingPointsList->isAncestorOf(selectedPointSettings) &&
                selectedPointSettings->isAncestorOf(gridColor),
            "perspective panel must expose one universal point creation command and separate settings");
    auto *horizonSettings = window.findChild<QWidget *>("horizonSettings");
    auto *horizonColor = window.findChild<QPushButton *>("horizonColor");
    auto *horizonOpacity = window.findChild<QSpinBox *>("horizonOpacity");
    auto *horizonWidth = window.findChild<QDoubleSpinBox *>("horizonWidth");
    auto *horizonLocked = window.findChild<QCheckBox *>("horizonLocked");
    auto *horizonSymmetry = window.findChild<QCheckBox *>("horizonSymmetry");
    auto *selectedPointLocked = window.findChild<QCheckBox *>("selectedPointLocked");
    auto *verticalSettings = window.findChild<QWidget *>("mainVerticalSettings");
    auto *verticalColor = window.findChild<QPushButton *>("verticalColor");
    auto *verticalOpacity = window.findChild<QSpinBox *>("verticalOpacity");
    auto *verticalWidth = window.findChild<QDoubleSpinBox *>("verticalWidth");
    auto *verticalLocked = window.findChild<QCheckBox *>("verticalLocked");
    auto *verticalSymmetry = window.findChild<QCheckBox *>("verticalSymmetry");
    auto *verticalVisible = window.findChild<QCheckBox *>("verticalVisible");
    require(horizonSettings && horizonColor && horizonOpacity && horizonWidth && horizonLocked && horizonSymmetry &&
                verticalSettings && verticalColor && verticalOpacity && verticalWidth && verticalLocked &&
                verticalSymmetry && verticalVisible && selectedPointLocked &&
                horizonSettings->isAncestorOf(horizonColor) && horizonSettings->isAncestorOf(horizonSymmetry) &&
                verticalSettings->isAncestorOf(verticalColor) && verticalSettings->isAncestorOf(verticalOpacity) &&
                verticalSettings->isAncestorOf(verticalWidth) && verticalSettings->isAncestorOf(verticalLocked) &&
                verticalSettings->isAncestorOf(verticalSymmetry) &&
                selectedPointSettings->isAncestorOf(selectedPointLocked) && horizonOpacity->value() == 70 &&
                horizonWidth->value() == 1.0 && verticalOpacity->value() == 70 && verticalWidth->value() == 1.0 &&
                !horizonLocked->isChecked() && !verticalLocked->isChecked() && !horizonSymmetry->isChecked() &&
                !verticalSymmetry->isChecked() && !selectedPointLocked->isChecked(),
            "perspective construction controls are missing, misplaced, or invalid");
    auto *perspectivePanel = qobject_cast<QWidget *>(horizonSettings->parentWidget());
    auto *perspectiveLayout = perspectivePanel ? qobject_cast<QVBoxLayout *>(perspectivePanel->layout()) : nullptr;
    require(perspectiveLayout &&
                perspectiveLayout->indexOf(horizonSettings) < perspectiveLayout->indexOf(verticalSettings) &&
                perspectiveLayout->indexOf(verticalSettings) < perspectiveLayout->indexOf(vanishingPointSettings) &&
                perspectiveLayout->indexOf(vanishingPointSettings) < perspectiveLayout->indexOf(vanishingPointsList),
            "perspective blocks must be ordered as horizon, main vertical, point settings, and point list");
    auto *perspectiveDock = window.findChild<AutoHideDockWidget *>("perspectiveDock");
    auto *perspectivePanelToggle = window.findChild<QAction *>("perspectivePanelToggle");
    auto *perspectivePin = window.findChild<QToolButton *>("perspectivePanelPin");
    auto *perspectiveTitle = window.findChild<QLabel *>("perspectivePanelTitle");
    auto *perspectiveStrip = window.findChild<QDockWidget *>("perspectiveAutoHideStrip");
    auto *perspectiveTab = window.findChild<QToolButton *>("perspectiveAutoHideTab");
    auto *perspectiveOverlay = window.findChild<QWidget *>("perspectiveAutoHideOverlay");
    auto *horizonToggle = window.findChild<QToolButton *>("horizonSettingsToggle");
    auto *horizonContent = window.findChild<QWidget *>("horizonSettingsContent");
    auto *verticalToggle = window.findChild<QToolButton *>("mainVerticalSettingsToggle");
    auto *commonToggle = window.findChild<QToolButton *>("vanishingPointSettingsToggle");
    auto *pointsToggle = window.findChild<QToolButton *>("vanishingPointsListToggle");
    auto *selectedPointToggle = window.findChild<QToolButton *>("selectedVanishingPointSettingsToggle");
    auto *horizonFrame = qobject_cast<QFrame *>(horizonSettings);
    auto *verticalFrame = qobject_cast<QFrame *>(verticalSettings);
    auto *commonFrame = qobject_cast<QFrame *>(vanishingPointSettings);
    auto *pointsFrame = qobject_cast<QFrame *>(vanishingPointsList);
    auto *selectedPointFrame = qobject_cast<QFrame *>(selectedPointSettings);
    require(perspectiveDock && perspectivePanelToggle && viewMenu->actions().contains(perspectivePanelToggle) &&
                perspectivePin && perspectiveTitle && perspectiveStrip && perspectiveTab && perspectiveOverlay &&
                perspectivePin->mapTo(&window, QPoint()).x() < perspectiveTitle->mapTo(&window, QPoint()).x() &&
                perspectiveDock->windowTitle() == QStringLiteral("Перспектива") && horizonToggle &&
                horizonContent && verticalToggle && commonToggle && pointsToggle && selectedPointToggle &&
                horizonFrame && verticalFrame && commonFrame && pointsFrame && selectedPointFrame &&
                horizonFrame->frameShape() == QFrame::StyledPanel &&
                verticalFrame->frameShape() == QFrame::StyledPanel &&
                commonFrame->frameShape() == QFrame::StyledPanel && pointsFrame->frameShape() == QFrame::StyledPanel &&
                selectedPointFrame->frameShape() == QFrame::StyledPanel &&
                horizonToggle->arrowType() == Qt::DownArrow && horizonSettings->property("expanded").toBool(),
            "perspective rollout headers, borders, or panel title are invalid");
    perspectivePanelToggle->setChecked(true);
    QApplication::processEvents();
    perspectivePin->click();
    QApplication::processEvents();
    require(!perspectiveDock->isPinned() && perspectiveStrip->isVisible() && perspectiveTab->isVisible() &&
                perspectivePanelToggle->isChecked(),
            "unpinning the perspective panel must leave its checked edge tab visible");
    require(perspectiveStrip->geometry().left() > canvas->geometry().right(),
            "the right auto-hide tab must occupy a strip outside the canvas and its rulers");
    require(perspectiveTab->mapTo(&window, QPoint()).x() -
                    (canvas->mapTo(&window, QPoint()).x() + canvas->width()) >=
                3,
            "the right auto-hide tab must leave a small gap after the ruler");
    perspectiveTab->click();
    QApplication::processEvents();
    require(perspectiveOverlay->isVisible(), "the right edge tab must reveal the perspective panel");
    QMouseEvent outsidePerspective(QEvent::MouseButtonPress,
                                   QPointF(2, 2),
                                   Qt::LeftButton,
                                   Qt::LeftButton,
                                   Qt::NoModifier);
    QApplication::sendEvent(mainToolbar, &outsidePerspective);
    QApplication::processEvents();
    require(!perspectiveOverlay->isVisible() && perspectiveTab->isVisible(),
            "clicking outside the perspective panel must return it to its edge tab");
    perspectiveTab->click();
    perspectivePin->click();
    perspectivePanelToggle->setChecked(false);
    QApplication::processEvents();
    require(perspectiveDock->isPinned() && !perspectiveDock->isVisible() && !perspectiveTab->isVisible(),
            "the View menu must completely hide a pinned perspective panel");
    horizonToggle->click();
    QApplication::processEvents();
    require(horizonContent->isHidden() && horizonToggle->arrowType() == Qt::RightArrow &&
                !horizonSettings->property("expanded").toBool(),
            "collapsed rollout must hide its content and show a right arrow");
    horizonToggle->click();
    QApplication::processEvents();
    require(!horizonContent->isHidden() && horizonToggle->arrowType() == Qt::DownArrow &&
                horizonSettings->property("expanded").toBool(),
            "expanded rollout must restore its content and show a down arrow");
    require(pointsListControl->count() == 1 && canvas->state().vanishingPoints.size() == 1,
            "initial vanishing point list is invalid");
    auto *firstCard = pointsListControl->itemWidget(pointsListControl->item(0));
    auto *pointNumber = firstCard ? firstCard->findChild<QLabel *>("vanishingPointNumber") : nullptr;
    auto *pointName = firstCard ? firstCard->findChild<QLineEdit *>("vanishingPointName") : nullptr;
    auto *pointVisibility = firstCard ? firstCard->findChild<QToolButton *>("vanishingPointVisibility") : nullptr;
    auto *pointLock = firstCard ? firstCard->findChild<QToolButton *>("vanishingPointLock") : nullptr;
    auto *selectedPointVisible = window.findChild<QCheckBox *>("selectedPointVisible");
    require(firstCard && pointNumber && pointNumber->text() == QStringLiteral("1") && pointName &&
                pointName->text() == QStringLiteral("Точка схода 1") && pointVisibility &&
                pointVisibility->isChecked() && !pointVisibility->icon().isNull() && pointLock &&
                !pointLock->isChecked() && !pointLock->icon().isNull() && selectedPointVisible,
            "vanishing point card must contain its number, editable name, eye, and lock buttons");
    require(firstCard->grab().save(out.filePath("point-card.png")), "point card screenshot failed");
    const qint64 openEyeIcon = pointVisibility->icon().cacheKey();
    pointVisibility->click();
    require(!canvas->state().vanishingPoints[0].visible && !selectedPointVisible->isChecked() &&
                pointVisibility->icon().cacheKey() != openEyeIcon,
            "card eye button did not hide the point family or update its icon");
    pointVisibility->click();
    require(canvas->state().vanishingPoints[0].visible && selectedPointVisible->isChecked(),
            "card eye button did not restore the point family");
    const qint64 openLockIcon = pointLock->icon().cacheKey();
    pointLock->click();
    require(canvas->state().vanishingPoints[0].locked && selectedPointLocked->isChecked() &&
                pointLock->icon().cacheKey() != openLockIcon,
            "card lock button did not lock the point or update its icon");
    pointLock->click();
    require(!canvas->state().vanishingPoints[0].locked && !selectedPointLocked->isChecked(),
            "card lock button did not unlock the point");
    pointName->setText(QStringLiteral("Левая точка"));
    QMetaObject::invokeMethod(pointName, "editingFinished", Qt::DirectConnection);
    require(canvas->state().vanishingPoints[0].name == QStringLiteral("Левая точка"),
            "editing a card name did not update the point");
    canvas->undoStack()->undo();
    require(canvas->state().vanishingPoints[0].name == QStringLiteral("Точка схода 1"),
            "point name edit must be undoable");
    canvas->undoStack()->redo();
    require(canvas->state().vanishingPoints[0].name == QStringLiteral("Левая точка"), "point name redo failed");
    const QString firstPointId = canvas->state().vanishingPoints[0].id;
    addPoint->click();
    require(pointsListControl->count() == 2 && canvas->state().vanishingPoints.size() == 2 &&
                canvas->selectedPointIndex() == 1 && canvas->state().vanishingPoints[1].id != firstPointId,
            "adding and selecting a second vanishing point failed");
    removePoint->click();
    require(canvas->state().vanishingPoints.size() == 1, "removing a vanishing point failed");
    canvas->undoStack()->undo();
    require(canvas->state().vanishingPoints.size() == 2, "vanishing point deletion must be undoable");
    canvas->undoStack()->redo();
    require(canvas->state().vanishingPoints.size() == 1, "vanishing point deletion redo failed");
    canvas->undoStack()->setClean();
    auto *pointX = window.findChild<QDoubleSpinBox *>("vanishingPointX");
    auto *pointY = window.findChild<QDoubleSpinBox *>("vanishingPointY");
    auto *pointUnits = window.findChild<QComboBox *>("vanishingPointUnits");
    auto *horizonPosition = window.findChild<QDoubleSpinBox *>("horizonPosition");
    auto *horizonUnits = window.findChild<QComboBox *>("horizonUnits");
    auto *verticalPosition = window.findChild<QDoubleSpinBox *>("verticalPosition");
    auto *verticalUnits = window.findChild<QComboBox *>("verticalUnits");
    auto *pointAttachment = window.findChild<QComboBox *>("vanishingPointAttachment");
    auto *canvasPosition = window.findChild<QLabel *>("canvasPosition");
    require(pointX && pointY && pointUnits && horizonPosition && horizonUnits && verticalPosition && verticalUnits &&
                pointAttachment && pointAttachment->count() == 4 && canvasPosition && pointUnits->currentIndex() == 0 &&
                horizonUnits->currentIndex() == 0 && verticalUnits->currentIndex() == 0 &&
                verticalPosition->value() == 0 && std::abs(pointX->value() - 15.0) < 0.01 &&
                std::abs(pointY->value() - 11.29) < 0.02,
            "centered percentage coordinate controls or attachment choices are missing or incorrect");
    mouse(canvas, QEvent::MouseMove, canvas->toView(QPointF(600, 260)), Qt::NoButton, Qt::NoButton);
    require(!canvas->rulerPercent() && canvasPosition->text() == QStringLiteral("X: 100 px   Y: 50 px"),
            "canvas rulers must default to centered pixels while point fields use percent");
    const QPointF beforeUnitChange = canvas->state().vanishingPoints[0].position;
    pointUnits->setCurrentIndex(1);
    require(horizonUnits->currentIndex() == 1 && verticalUnits->currentIndex() == 1 && pointX->value() == 150 &&
                pointY->value() == 70 && horizonPosition->value() == 70 && verticalPosition->value() == 0 &&
                canvas->state().vanishingPoints[0].position == beforeUnitChange,
            "unit switching must preserve geometry and synchronize coordinate controls");
    require(!canvas->rulerPercent(), "changing vanishing point units must not change ruler units");
    auto *settingsAction = window.findChild<QAction *>("settingsAction");
    bool settingsDialogChecked = false;
    QTimer::singleShot(0, &window, [&] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        auto *tabs = dialog ? dialog->findChild<QTabWidget *>("settingsTabs") : nullptr;
        auto *rulerUnits = dialog ? dialog->findChild<QComboBox *>("rulerUnits") : nullptr;
        settingsDialogChecked = tabs && tabs->count() == 3 && tabs->tabText(0) == QStringLiteral("Вид") &&
                                tabs->tabText(1) == QStringLiteral("Система") &&
                                tabs->tabText(2) == QStringLiteral("Файлы") && rulerUnits &&
                                rulerUnits->currentIndex() == 0;
        if (rulerUnits)
            rulerUnits->setCurrentIndex(1);
        if (dialog)
            dialog->accept();
    });
    require(settingsAction, "settings action is missing");
    settingsAction->trigger();
    require(settingsDialogChecked && canvas->rulerPercent() && QSettings().value("view/rulers/percent").toBool() &&
                pointUnits->currentIndex() == 1,
            "settings dialog did not persist independent ruler units");
    pointX->setValue(-300);
    require(canvas->state().vanishingPoints[0].position == QPointF(200, 240),
            "numeric X coordinate did not update the attached point");
    pointAttachment->setCurrentIndex(0);
    pointY->setValue(100);
    require(canvas->state().vanishingPoints[0].position == QPointF(200, 210) &&
                canvas->state().vanishingPoints[0].attachmentTargetIds.isEmpty(),
            "free point numeric Y coordinate or attachment state is incorrect");
    horizonPosition->setValue(50);
    require(canvas->state().horizonY == 260 && canvas->state().vanishingPoints[0].position == QPointF(200, 210),
            "numeric horizon position must not move a free point");
    pointAttachment->setCurrentIndex(2);
    require(canvas->state().vanishingPoints[0].position == QPointF(500, 210) &&
                canvas->state().vanishingPoints[0].isAttachedTo(PerspectiveTarget::vertical()),
            "point did not attach to the main vertical");
    verticalPosition->setValue(60);
    require(canvas->state().verticalX == 560 && canvas->state().vanishingPoints[0].position == QPointF(560, 210),
            "numeric main vertical movement must carry its attached point");
    pointAttachment->setCurrentIndex(3);
    require(canvas->state().vanishingPoints[0].position == QPointF(560, 260) &&
                canvas->state().vanishingPoints[0].attachmentTargetIds.size() == 2 && !pointX->isEnabled() &&
                !pointY->isEnabled(),
            "intersection attachment must constrain both coordinates and disable their editors");
    pointAttachment->setCurrentIndex(0);
    canvas->undoStack()->setClean();
    require(!savePerspectiveDefaults->isEnabled(), "unchanged factory perspective defaults must not be saveable");
    auto *newAction = window.findChild<QAction *>("newAction");
    require(newAction, "new canvas action missing");
    QTimer::singleShot(0, &window, [] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (!dialog)
            return;
        auto *width = dialog->findChild<QSpinBox *>("newCanvasWidth");
        auto *height = dialog->findChild<QSpinBox *>("newCanvasHeight");
        if (width && height) {
            width->setValue(640);
            height->setValue(480);
            dialog->accept();
        }
    });
    newAction->trigger();
    require(window.canvas()->state().canvasSize == QSize(640, 480) &&
                QSettings().value("canvas/newWidth").toInt() == 640 &&
                QSettings().value("canvas/newHeight").toInt() == 480,
            "new canvas size was not remembered after creation");
    MainWindow rememberedSizeWindow;
    bool rememberedSizeShown = false;
    auto *rememberedNewAction = rememberedSizeWindow.findChild<QAction *>("newAction");
    QTimer::singleShot(0, &rememberedSizeWindow, [&] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (!dialog)
            return;
        auto *width = dialog->findChild<QSpinBox *>("newCanvasWidth");
        auto *height = dialog->findChild<QSpinBox *>("newCanvasHeight");
        rememberedSizeShown = width && height && width->value() == 640 && height->value() == 480;
        if (width && height) {
            width->setValue(800);
            height->setValue(600);
        }
        dialog->reject();
    });
    rememberedNewAction->trigger();
    require(rememberedSizeShown && QSettings().value("canvas/newWidth").toInt() == 640 &&
                QSettings().value("canvas/newHeight").toInt() == 480,
            "new canvas dialog did not restore size or cancel changed it");
    rememberedSizeWindow.close();
    canvas->undoStack()->setClean();
    const int beforeAppearanceChange = canvas->undoStack()->count();
    rayStep->setValue(7.5);
    rayGap->setValue(20);
    rayStartOpacity->setValue(15);
    rayEndOpacity->setValue(80);
    rayFadeLength->setValue(60);
    rayPattern->setCurrentIndex(1);
    rayWidth->setValue(2.5);
    rayAngleOffset->setValue(12);
    require(canvas->state().rayStepDegrees == 7.5 && canvas->state().rayGap == 20 &&
                canvas->state().rayStartOpacity == 15 && canvas->state().rayEndOpacity == 80 &&
                canvas->state().rayFadeLength == 60 && canvas->state().rayPattern == 1 &&
                canvas->state().rayWidth == 2.5 && canvas->state().rayAngleOffset == 12,
            "perspective controls did not update the document");
    require(canvas->undoStack()->count() == beforeAppearanceChange && canvas->undoStack()->isClean(),
            "perspective appearance must not enter project history or dirty the document");
    horizonOpacity->setValue(35);
    horizonWidth->setValue(3.5);
    QSettings().setValue("perspective/view/horizonColor", QStringLiteral("#d43b3b"));
    canvas->setHorizonColor(QColor("#d43b3b"));
    verticalOpacity->setValue(45);
    verticalWidth->setValue(2.5);
    QSettings().setValue("perspective/view/verticalColor", QStringLiteral("#9b4fc2"));
    canvas->setVerticalColor(QColor("#9b4fc2"));
    require(canvas->state().horizonColor == QColor("#d43b3b") && canvas->state().horizonOpacity == 35 &&
                canvas->state().horizonWidth == 3.5 && canvas->state().verticalColor == QColor("#9b4fc2") &&
                canvas->state().verticalOpacity == 45 && canvas->state().verticalWidth == 2.5 &&
                canvas->undoStack()->count() == beforeAppearanceChange && canvas->undoStack()->isClean(),
            "horizon and main vertical appearance must remain outside project history");
    require(savePerspectiveDefaults->isEnabled(), "changed perspective defaults must enable saving");
    savePerspectiveDefaults->click();
    QSettings savedDefaults;
    require(savedDefaults.value("perspective/common/rayStepDegrees").toDouble() == 7.5 &&
                savedDefaults.value("perspective/common/rayGap").toInt() == 20 &&
                savedDefaults.value("perspective/common/rayStartOpacity").toInt() == 15 &&
                savedDefaults.value("perspective/common/rayEndOpacity").toInt() == 80 &&
                savedDefaults.value("perspective/common/rayFadeLength").toInt() == 60 &&
                savedDefaults.value("perspective/common/rayPattern").toInt() == 1,
            "save common perspective settings did not persist all six values");
    require(!savePerspectiveDefaults->isEnabled(), "saving perspective defaults must disable the save button");
    rayGap->setValue(21);
    require(savePerspectiveDefaults->isEnabled(), "changing one saved perspective value must enable the save button");
    rayGap->setValue(20);
    require(!savePerspectiveDefaults->isEnabled(), "restoring saved perspective values must disable the save button");
    MainWindow defaultsWindow;
    auto *savedRayStep = defaultsWindow.findChild<QDoubleSpinBox *>("rayStep");
    auto *savedRayGap = defaultsWindow.findChild<QSpinBox *>("rayGap");
    auto *savedStartOpacity = defaultsWindow.findChild<QSpinBox *>("rayStartOpacity");
    auto *savedEndOpacity = defaultsWindow.findChild<QSpinBox *>("rayEndOpacity");
    auto *savedFadeLength = defaultsWindow.findChild<QSpinBox *>("rayFadeLength");
    auto *savedPattern = defaultsWindow.findChild<QComboBox *>("rayPattern");
    auto *resetSavedDefaults = defaultsWindow.findChild<QPushButton *>("resetPerspectiveDefaults");
    auto *saveSavedDefaults = defaultsWindow.findChild<QPushButton *>("savePerspectiveDefaults");
    require(savedRayStep && savedRayGap && savedStartOpacity && savedEndOpacity && savedFadeLength && savedPattern &&
                saveSavedDefaults && resetSavedDefaults && savedRayStep->value() == 7.5 && savedRayGap->value() == 20 &&
                savedStartOpacity->value() == 15 && savedEndOpacity->value() == 80 && savedFadeLength->value() == 60 &&
                savedPattern->currentIndex() == 1 && !saveSavedDefaults->isEnabled(),
            "saved common perspective settings were not applied to a new document");
    require(defaultsWindow.canvas()->state().horizonColor == QColor("#d43b3b") &&
                defaultsWindow.canvas()->state().horizonOpacity == 35 &&
                defaultsWindow.canvas()->state().horizonWidth == 3.5 &&
                defaultsWindow.canvas()->state().verticalColor == QColor("#9b4fc2") &&
                defaultsWindow.canvas()->state().verticalOpacity == 45 &&
                defaultsWindow.canvas()->state().verticalWidth == 2.5 &&
                defaultsWindow.canvas()->state().rayWidth == 2.5 &&
                defaultsWindow.canvas()->state().rayAngleOffset == 12,
            "application display settings did not persist between windows");
    const int beforeReset = defaultsWindow.canvas()->undoStack()->count();
    resetSavedDefaults->click();
    require(defaultsWindow.canvas()->state().rayStepDegrees == 10 && defaultsWindow.canvas()->state().rayGap == 12 &&
                defaultsWindow.canvas()->state().rayStartOpacity == 10 &&
                defaultsWindow.canvas()->state().rayEndOpacity == 70 &&
                defaultsWindow.canvas()->state().rayFadeLength == 50 &&
                defaultsWindow.canvas()->state().rayPattern == 0 &&
                defaultsWindow.canvas()->undoStack()->count() == beforeReset &&
                !QSettings().contains("perspective/common/rayStepDegrees") && !saveSavedDefaults->isEnabled(),
            "reset common perspective settings did not restore factory values outside project history");
    defaultsWindow.close();
    QSettings().remove("perspective/view");
}

/// Проверяет редактирование точек, осей, привязок, симметрии, линеек и шаблонов лучей.
void testPerspectiveGeometry(MainWindow &window, Canvas *canvas, const DrawingState &initial) {
    auto *horizonSymmetry = window.findChild<QCheckBox *>("horizonSymmetry");
    auto *verticalSymmetry = window.findChild<QCheckBox *>("verticalSymmetry");
    auto *horizonVisible = window.findChild<QCheckBox *>("horizonVisible");
    auto *axesVisible = window.findChild<QCheckBox *>("axesVisible");
    auto *markersVisible = window.findChild<QCheckBox *>("markersVisible");
    require(horizonVisible && axesVisible && markersVisible && horizonSymmetry && verticalSymmetry,
            "perspective visibility and axis symmetry controls are missing");
    DrawingState pair = initial;
    pair.vanishingPoints[0].attachmentTargetIds = QStringList{PerspectiveTarget::horizon()};
    pair.vanishingPoints.append({QStringLiteral("vp-2"),
                                 QPointF(350, 240),
                                 PerspectiveTarget::constructionType(),
                                 PerspectiveTarget::horizon()});
    canvas->setDocument(pair);
    canvas->setHorizonSymmetry(true);
    canvas->selectPoint(0);
    canvas->setSelectedPointPosition(QPointF(720, 240));
    require(canvas->state().vanishingPoints[1].position == QPointF(280, 240), "horizontal symmetry movement failed");
    DrawingState multiplePartners = initial;
    multiplePartners.gridVisible = true;
    multiplePartners.vanishingPoints[0].position = QPointF(700, 240);
    multiplePartners.vanishingPoints[0].attachmentTargetIds = QStringList{PerspectiveTarget::horizon()};
    multiplePartners.vanishingPoints.append({QStringLiteral("vp-nearest"),
                                             QPointF(310, 240),
                                             PerspectiveTarget::constructionType(),
                                             PerspectiveTarget::horizon()});
    multiplePartners.vanishingPoints.append({QStringLiteral("vp-other"),
                                             QPointF(360, 240),
                                             PerspectiveTarget::constructionType(),
                                             PerspectiveTarget::horizon()});
    Canvas multiplePartnerCanvas;
    multiplePartnerCanvas.resize(800, 600);
    multiplePartnerCanvas.setDocument(multiplePartners);
    multiplePartnerCanvas.setHorizonSymmetry(true);
    multiplePartnerCanvas.fit();
    multiplePartnerCanvas.setTool(Canvas::Perspective);
    mouse(&multiplePartnerCanvas,
          QEvent::MouseButtonPress,
          multiplePartnerCanvas.toView(QPointF(700, 240)),
          Qt::LeftButton,
          Qt::LeftButton);
    mouse(&multiplePartnerCanvas,
          QEvent::MouseMove,
          multiplePartnerCanvas.toView(QPointF(520, 240)),
          Qt::NoButton,
          Qt::LeftButton);
    mouse(&multiplePartnerCanvas,
          QEvent::MouseMove,
          multiplePartnerCanvas.toView(QPointF(800, 240)),
          Qt::NoButton,
          Qt::LeftButton);
    mouse(&multiplePartnerCanvas,
          QEvent::MouseButtonRelease,
          multiplePartnerCanvas.toView(QPointF(800, 240)),
          Qt::LeftButton,
          Qt::NoButton);
    require(multiplePartnerCanvas.state().vanishingPoints[1].position == QPointF(200, 240) &&
                multiplePartnerCanvas.state().vanishingPoints[2].position == QPointF(360, 240),
            "symmetry must keep the nearest reflected partner selected at drag start");
    DrawingState threePoint = pair;
    threePoint.vanishingPoints.append({QStringLiteral("vp-3"),
                                       QPointF(500, 80),
                                       PerspectiveTarget::constructionType(),
                                       PerspectiveTarget::vertical()});
    threePoint.gridVisible = true;
    Canvas threePointCanvas;
    threePointCanvas.resize(800, 600);
    threePointCanvas.setDocument(threePoint);
    threePointCanvas.fit();
    threePointCanvas.setTool(Canvas::Perspective);
    threePointCanvas.selectPoint(2);
    threePointCanvas.setSelectedPointPosition(QPointF(700, 60));
    require(threePointCanvas.state().vanishingPoints[2].position == QPointF(500, 60),
            "vertical attachment must constrain the third point while allowing upper and lower positions");
    drag(&threePointCanvas, QPointF(500, 400), QPointF(560, 400));
    require(threePointCanvas.state().verticalX == 560 &&
                threePointCanvas.state().vanishingPoints[2].position == QPointF(560, 60),
            "dragging the main vertical must carry the attached third point");
    threePointCanvas.undoStack()->undo();
    require(threePointCanvas.state().verticalX == 500 &&
                threePointCanvas.state().vanishingPoints[2].position == QPointF(500, 60),
            "main vertical movement undo failed");
    DrawingState emptyPoints = initial;
    emptyPoints.vanishingPoints.clear();
    Canvas placementCanvas;
    placementCanvas.setDocument(emptyPoints);
    placementCanvas.addVanishingPoint();
    placementCanvas.addVanishingPoint();
    placementCanvas.addVanishingPoint();
    require(placementCanvas.state().vanishingPoints.size() == 3 &&
                placementCanvas.state().vanishingPoints[0].position == QPointF(500, 310) &&
                placementCanvas.state().vanishingPoints[1].position == QPointF(460, 310) &&
                placementCanvas.state().vanishingPoints[2].position == QPointF(420, 310) &&
                placementCanvas.state().vanishingPoints[0].attachmentTargetIds.isEmpty() &&
                placementCanvas.state().vanishingPoints[1].attachmentTargetIds.isEmpty() &&
                placementCanvas.state().vanishingPoints[2].attachmentTargetIds.isEmpty(),
            "universal points must be created free at the center and then in successive positions to the left");
    DrawingState fourPoint = pair;
    fourPoint.gridVisible = true;
    fourPoint.vanishingPoints.append({QStringLiteral("vp-3"),
                                      QPointF(500, 40),
                                      PerspectiveTarget::constructionType(),
                                      PerspectiveTarget::vertical()});
    fourPoint.vanishingPoints.append({QStringLiteral("vp-4"),
                                      QPointF(500, 440),
                                      PerspectiveTarget::constructionType(),
                                      PerspectiveTarget::vertical()});
    Canvas fourPointCanvas;
    fourPointCanvas.resize(800, 600);
    fourPointCanvas.setDocument(fourPoint);
    fourPointCanvas.setHorizonSymmetry(true);
    fourPointCanvas.setVerticalSymmetry(true);
    require(fourPointCanvas.state().vanishingPoints.size() == 4,
            "four universal points must support two horizontal and two vertical attachments");
    fourPointCanvas.selectPoint(0);
    fourPointCanvas.setSelectedPointPosition(QPointF(720, 240));
    require(fourPointCanvas.state().vanishingPoints[1].position == QPointF(280, 240),
            "left and right pair must be symmetric around the main vertical");
    fourPointCanvas.selectPoint(2);
    fourPointCanvas.setSelectedPointPosition(QPointF(700, 40));
    require(fourPointCanvas.state().vanishingPoints[2].position == QPointF(500, 40) &&
                fourPointCanvas.state().vanishingPoints[3].position == QPointF(500, 440),
            "upper and lower pair must be symmetric around the horizon");
    fourPointCanvas.setHorizonY(260);
    require(fourPointCanvas.state().vanishingPoints[0].position.y() == 260 &&
                fourPointCanvas.state().vanishingPoints[1].position.y() == 260 &&
                fourPointCanvas.state().vanishingPoints[2].position.y() == 60 &&
                fourPointCanvas.state().vanishingPoints[3].position.y() == 460,
            "moving the horizon must preserve both symmetric pairs");
    fourPointCanvas.setVerticalX(540);
    require(fourPointCanvas.state().vanishingPoints[0].position.x() == 760 &&
                fourPointCanvas.state().vanishingPoints[1].position.x() == 320 &&
                fourPointCanvas.state().vanishingPoints[2].position.x() == 540 &&
                fourPointCanvas.state().vanishingPoints[3].position.x() == 540,
            "moving the main vertical must preserve both symmetric pairs");
    fourPointCanvas.selectPoint(2);
    fourPointCanvas.setSelectedPointVisible(false);
    require(!fourPointCanvas.state().vanishingPoints[2].visible && fourPointCanvas.state().vanishingPoints[0].visible &&
                fourPointCanvas.state().vanishingPoints[1].visible &&
                fourPointCanvas.state().vanishingPoints[3].visible,
            "each of four guide families must remain independently visible");
    DrawingState central = initial;
    central.gridVisible = true;
    Canvas centralCanvas;
    centralCanvas.resize(800, 600);
    centralCanvas.setDocument(central);
    centralCanvas.fit();
    centralCanvas.setTool(Canvas::Perspective);
    drag(&centralCanvas, QPointF(650, 240), QPointF(504, 244));
    const auto &centralPoint = centralCanvas.state().vanishingPoints[0];
    require(centralPoint.position == QPointF(500, 240) && centralPoint.isAttachedTo(PerspectiveTarget::horizon()) &&
                centralPoint.isAttachedTo(PerspectiveTarget::vertical()),
            "a point near the axes intersection must snap to both axes");
    centralCanvas.setHorizonY(275);
    require(centralCanvas.state().vanishingPoints[0].position == QPointF(500, 275),
            "moving the horizon must update the Y coordinate of a doubly attached point");
    centralCanvas.setVerticalX(530);
    require(centralCanvas.state().vanishingPoints[0].position == QPointF(530, 275),
            "moving the main vertical must update the X coordinate of a doubly attached point");
    centralCanvas.setSelectedPointPosition(QPointF(800, 40));
    require(centralCanvas.state().vanishingPoints[0].position == QPointF(530, 275),
            "numeric input must preserve both constraints of a doubly attached point");
    DrawingState cursorState = initial;
    cursorState.gridVisible = true;
    Canvas cursorCanvas;
    cursorCanvas.resize(700, 500);
    cursorCanvas.setDocument(cursorState);
    cursorCanvas.fit();
    cursorCanvas.setTool(Canvas::Perspective);
    const QPointF blankCursor = cursorCanvas.toView(QPointF(100, 100)),
                  pointCursor = cursorCanvas.toView(cursorState.vanishingPoints[0].position),
                  verticalCursor = cursorCanvas.toView(QPointF(cursorState.verticalX, 400)),
                  horizonCursor = cursorCanvas.toView(QPointF(100, cursorState.horizonY));
    mouse(&cursorCanvas, QEvent::MouseMove, blankCursor, Qt::NoButton, Qt::NoButton);
    require(cursorCanvas.cursor().shape() == Qt::CrossCursor,
            "perspective cursor must remain a cross away from draggable objects");
    mouse(&cursorCanvas, QEvent::MouseMove, pointCursor, Qt::NoButton, Qt::NoButton);
    require(cursorCanvas.cursor().shape() == Qt::OpenHandCursor, "hovering an unlocked point must show an open hand");
    mouse(&cursorCanvas, QEvent::MouseButtonPress, pointCursor, Qt::LeftButton, Qt::LeftButton);
    require(cursorCanvas.cursor().shape() == Qt::ClosedHandCursor, "dragging a point must show a closed hand");
    mouse(&cursorCanvas, QEvent::MouseButtonRelease, pointCursor, Qt::LeftButton, Qt::NoButton);
    require(cursorCanvas.cursor().shape() == Qt::OpenHandCursor,
            "releasing a draggable point must restore the open hand");
    cursorCanvas.setSelectedPointLocked(true);
    mouse(&cursorCanvas, QEvent::MouseMove, pointCursor, Qt::NoButton, Qt::NoButton);
    require(cursorCanvas.cursor().shape() == Qt::ForbiddenCursor,
            "hovering a locked point must show the forbidden cursor");
    mouse(&cursorCanvas, QEvent::MouseMove, verticalCursor, Qt::NoButton, Qt::NoButton);
    require(cursorCanvas.cursor().shape() == Qt::OpenHandCursor, "hovering the main vertical must show an open hand");
    mouse(&cursorCanvas, QEvent::MouseButtonPress, verticalCursor, Qt::LeftButton, Qt::LeftButton);
    require(cursorCanvas.cursor().shape() == Qt::ClosedHandCursor,
            "dragging the main vertical must show a closed hand");
    mouse(&cursorCanvas, QEvent::MouseButtonRelease, verticalCursor, Qt::LeftButton, Qt::NoButton);
    cursorCanvas.setHorizonLocked(true);
    mouse(&cursorCanvas, QEvent::MouseMove, horizonCursor, Qt::NoButton, Qt::NoButton);
    require(cursorCanvas.cursor().shape() == Qt::ForbiddenCursor,
            "hovering a locked horizon must show the forbidden cursor");
    Canvas rulerCanvas;
    rulerCanvas.resize(500, 400);
    rulerCanvas.setDocument(initial);
    rulerCanvas.setGridVisible(false);
    rulerCanvas.fit();
    QEvent leave(QEvent::Leave);
    QApplication::sendEvent(&rulerCanvas, &leave);
    QImage rulersOnly(rulerCanvas.size(), QImage::Format_ARGB32_Premultiplied);
    rulersOnly.fill(Qt::transparent);
    rulerCanvas.render(&rulersOnly);
    const QColor rulerBackground("#f4f5f7");
    int topRuler = 0, bottomRuler = 0, leftRuler = 0, rightRuler = 0;
    for (int x = 0; x < 500; ++x) {
        if (rulersOnly.pixelColor(x, 10) == rulerBackground)
            ++topRuler;
        if (rulersOnly.pixelColor(x, 390) == rulerBackground)
            ++bottomRuler;
    }
    for (int y = 28; y < 372; ++y) {
        if (rulersOnly.pixelColor(10, y) == rulerBackground)
            ++leftRuler;
        if (rulersOnly.pixelColor(490, y) == rulerBackground)
            ++rightRuler;
    }
    require(topRuler > 250 && bottomRuler > 250 && leftRuler > 170 && rightRuler > 170,
            "four fixed viewport rulers were not rendered");
    const QImage rulerPixels = pixels(rulerCanvas.state());
    mouse(&rulerCanvas, QEvent::MouseMove, QPointF(250, 200), Qt::NoButton, Qt::NoButton);
    QImage cursorProjection(rulerCanvas.size(), QImage::Format_ARGB32_Premultiplied);
    cursorProjection.fill(Qt::transparent);
    rulerCanvas.render(&cursorProjection);
    int changedPixels = 0, topProjection = 0, bottomProjection = 0, leftProjection = 0, rightProjection = 0,
        cursorLabels = 0;
    for (int y = 0; y < cursorProjection.height(); ++y)
        if (cursorProjection.pixelColor(250, y) != rulersOnly.pixelColor(250, y)) {
            ++changedPixels;
            if (y < 28)
                ++topProjection;
            if (y >= cursorProjection.height() - 28)
                ++bottomProjection;
        }
    for (int x = 0; x < cursorProjection.width(); ++x)
        if (cursorProjection.pixelColor(x, 200) != rulersOnly.pixelColor(x, 200)) {
            ++changedPixels;
            if (x < 28)
                ++leftProjection;
            if (x >= cursorProjection.width() - 28)
                ++rightProjection;
        }
    for (int y = 0; y < cursorProjection.height(); ++y)
        for (int x = 0; x < cursorProjection.width(); ++x)
            if (cursorProjection.pixelColor(x, y) == QColor("#fff4b5"))
                ++cursorLabels;
    require(changedPixels > 20 && topProjection && bottomProjection && leftProjection && rightProjection &&
                cursorLabels == 0 && pixels(rulerCanvas.state()) == rulerPixels && rulerCanvas.undoStack()->isClean(),
            "dashed cursor projections must cross all rulers without numeric badges or document edits");
    DrawingState lockState = initial;
    lockState.gridVisible = true;
    lockState.vanishingPoints[0].attachmentTargetIds = QStringList{PerspectiveTarget::horizon()};
    Canvas lockCanvas;
    lockCanvas.resize(700, 500);
    lockCanvas.setDocument(lockState);
    lockCanvas.fit();
    lockCanvas.setTool(Canvas::Perspective);
    lockCanvas.setSelectedPointLocked(true);
    require(lockCanvas.state().vanishingPoints[0].locked, "vanishing point lock was not enabled");
    lockCanvas.undoStack()->undo();
    require(!lockCanvas.state().vanishingPoints[0].locked, "vanishing point lock undo failed");
    lockCanvas.undoStack()->redo();
    lockCanvas.setHorizonLocked(true);
    require(lockCanvas.state().horizonLocked, "horizon lock was not enabled");
    lockCanvas.undoStack()->undo();
    require(!lockCanvas.state().horizonLocked, "horizon lock undo failed");
    lockCanvas.undoStack()->redo();
    lockCanvas.setVerticalLocked(true);
    require(lockCanvas.state().verticalLocked, "main vertical lock was not enabled");
    lockCanvas.undoStack()->undo();
    require(!lockCanvas.state().verticalLocked, "main vertical lock undo failed");
    lockCanvas.undoStack()->redo();
    const QPointF fixedPoint = lockCanvas.state().vanishingPoints[0].position;
    const double fixedHorizon = lockCanvas.state().horizonY, fixedVertical = lockCanvas.state().verticalX;
    drag(&lockCanvas, fixedPoint, fixedPoint + QPointF(80, 40));
    drag(&lockCanvas, QPointF(100, fixedHorizon), QPointF(100, fixedHorizon + 60));
    drag(&lockCanvas, QPointF(fixedVertical, 400), QPointF(fixedVertical + 60, 400));
    require(lockCanvas.state().vanishingPoints[0].position == fixedPoint &&
                lockCanvas.state().horizonY == fixedHorizon && lockCanvas.state().verticalX == fixedVertical,
            "locked perspective objects must ignore direct dragging");
    lockCanvas.setSelectedPointPosition(fixedPoint + QPointF(40, 0));
    require(lockCanvas.state().vanishingPoints[0].position == fixedPoint + QPointF(40, 0),
            "numeric input must move a locked vanishing point");
    lockCanvas.setHorizonY(fixedHorizon + 30);
    require(lockCanvas.state().horizonY == fixedHorizon + 30 &&
                lockCanvas.state().vanishingPoints[0].position.y() == fixedPoint.y() + 30,
            "numeric horizon movement must remain available and carry an attached locked point");
    lockCanvas.setVerticalX(fixedVertical + 30);
    require(lockCanvas.state().verticalX == fixedVertical + 30,
            "numeric main vertical movement must remain available while locked");
}

/// Проверяет рисующие инструменты, историю, форматы DRW/PNG и возвращает созданные данные.
ProjectFixture testDrawingAndProject(Canvas *canvas, const DrawingState &initial, const QDir &out) {
    QEvent leave(QEvent::Leave);
    canvas->setDocument(initial);
    canvas->fit();
    require(canvas->undoStack()->isClean(), "initial document must be clean");
    canvas->setStrokeWidth(9);
    canvas->setFront(QColor("#26364a"));
    drag(canvas, QPointF(120, 100), QPointF(280, 100));
    require(pixels(canvas->state()).pixelColor(200, 100) == QColor("#26364a"), "pencil stroke did not reach image");
    require(!canvas->undoStack()->isClean(), "stroke must make document dirty");
    canvas->undoStack()->undo();
    require(pixels(canvas->state()) == pixels(initial), "undo must restore pixels");
    require(canvas->undoStack()->isClean(), "undo back to saved image must be clean");
    canvas->undoStack()->redo();
    require(pixels(canvas->state()) != pixels(initial), "redo must restore stroke");
    canvas->setBack(QColor("#e8cf9b"));
    canvas->setTool(Canvas::Eraser);
    drag(canvas, QPointF(190, 95), QPointF(190, 105));
    require(pixels(canvas->state()).pixelColor(190, 100) == QColor("#e8cf9b"), "eraser must use Back");
    canvas->setDocument(initial);
    canvas->setTool(Canvas::Pencil);
    canvas->setFront(QColor("#26364a"));
    canvas->setStrokeWidth(3);
    click(canvas, {100, 200});
    click(canvas, {200, 200}, Qt::ShiftModifier);
    click(canvas, {200, 260}, Qt::ShiftModifier);
    require(pixels(canvas->state()).pixelColor(150, 200) == QColor("#26364a") &&
                pixels(canvas->state()).pixelColor(200, 230) == QColor("#26364a"),
            "shift clicks must create connected segments");
    canvas->undoStack()->undo();
    require(pixels(canvas->state()).pixelColor(150, 200) == QColor("#26364a") &&
                pixels(canvas->state()).pixelColor(200, 230) == QColor(Qt::white),
            "each connected segment must be separately undoable");
    canvas->undoStack()->redo();
    click(canvas, {300, 300});
    click(canvas, {390, 310}, Qt::ShiftModifier | Qt::ControlModifier);
    require(pixels(canvas->state()).pixelColor(350, 300) == QColor("#26364a"), "ctrl shift must constrain segment angle");
    canvas->setDocument(initial);
    canvas->setTool(Canvas::Brush);
    canvas->setFront(QColor("#6c3f88"));
    canvas->setStrokeWidth(11);
    drag(canvas, {110, 180}, {210, 180});
    require(pixels(canvas->state()).pixelColor(160, 180) == QColor("#6c3f88"), "brush stroke did not reach image");
    DrawingToolSettings softBrush;
    softBrush.width = 20;
    softBrush.opacity = 50;
    softBrush.hardness = 0;
    softBrush.spacing = 50;
    canvas->setStrokeSettings(softBrush);
    click(canvas, {260, 180});
    const QColor softCenter = pixels(canvas->state()).pixelColor(260, 180);
    const QColor softEdge = pixels(canvas->state()).pixelColor(269, 180);
    require(softCenter != QColor(Qt::white) && softCenter != QColor("#6c3f88") && softEdge != softCenter,
            "brush opacity and soft hardness must affect the round stamp");
    softBrush.opacity = 100;
    softBrush.hardness = 100;
    softBrush.spacing = 50;
    canvas->setStrokeSettings(softBrush);
    drag(canvas, {260, 220}, {360, 220});
    require(pixels(canvas->state()).pixelColor(310, 220) == QColor("#6c3f88"),
            "stamp interpolation must keep a fast brush stroke continuous");
    canvas->setBack(QColor("#e8cf9b"));
    canvas->setTool(Canvas::Eraser);
    DrawingToolSettings eraserSettings;
    eraserSettings.width = 11;
    eraserSettings.hardness = 100;
    eraserSettings.strength = 100;
    canvas->setStrokeSettings(eraserSettings);
    click(canvas, {110, 180});
    click(canvas, {210, 180}, Qt::ShiftModifier);
    require(pixels(canvas->state()).pixelColor(160, 180) == QColor("#e8cf9b"), "eraser straight segment must use Back");
    const int undoIndex = canvas->undoStack()->index();
    const QImage renderedPixels = pixels(canvas->state());
    canvas->setZoom(1.7, QPointF(100, 150));
    require(pixels(canvas->state()) == renderedPixels && canvas->undoStack()->index() == undoIndex,
            "zoom must not edit document");
    QPointF point(100, 100);
    require(QLineF(canvas->toImage(canvas->toView(point)), point).length() < 0.001, "view coordinate roundtrip failed");
    canvas->setTool(Canvas::Pan);
    drag(canvas, QPointF(100, 100), QPointF(150, 130));
    require(pixels(canvas->state()) == renderedPixels && canvas->undoStack()->index() == undoIndex,
            "pan must not edit document");
    canvas->fit();
    canvas->setGridVisible(true);
    canvas->setTool(Canvas::Perspective);
    drag(canvas, QPointF(650, 240), QPointF(600, 210));
    require(QLineF(canvas->state().vanishingPoints[0].position, QPointF(600, 210)).length() < 0.01,
            "perspective point must move freely away from the horizon");
    require(pixels(canvas->state()) == renderedPixels, "perspective must not alter pixels");
    QApplication::sendEvent(canvas, &leave);
    QImage freePointView(canvas->size(), QImage::Format_ARGB32_Premultiplied);
    freePointView.fill(Qt::transparent);
    canvas->render(&freePointView);
    const QPointF freePoint = canvas->toView(canvas->state().vanishingPoints[0].position);
    require(freePointView.pixelColor(qRound(freePoint.x()), qRound(freePoint.y())) == QColor(Qt::white),
            "free vanishing point must not show the snap indicator");
    drag(canvas, QPointF(600, 210), QPointF(550, 245));
    require(canvas->state().vanishingPoints[0].position == QPointF(550, 240),
            "perspective point must snap near the horizon");
    drag(canvas, QPointF(100, 240), QPointF(100, 300));
    require(canvas->state().horizonY == 300 && canvas->state().vanishingPoints[0].position == QPointF(550, 300),
            "moving the horizon must carry a snapped vanishing point vertically");
    QApplication::sendEvent(canvas, &leave);
    QImage gridView(canvas->size(), QImage::Format_ARGB32_Premultiplied);
    gridView.fill(Qt::transparent);
    canvas->render(&gridView);
    const QPointF snappedPoint = canvas->toView(canvas->state().vanishingPoints[0].position);
    require(gridView.pixelColor(qRound(snappedPoint.x()), qRound(snappedPoint.y())) ==
                canvas->state().vanishingPoints[0].color,
            "snapped vanishing point must show its colored center");
    const QRectF paper(canvas->toView(QPointF()), QSizeF(pixels(canvas->state()).size()) * canvas->zoom());
    const QPoint outsideHorizon(qRound(paper.right() + 12),
                                qRound(canvas->toView(QPointF(0, canvas->state().horizonY)).y()));
    bool horizonOutsideCanvas = false;
    for (int y = outsideHorizon.y() - 2; y <= outsideHorizon.y() + 2; ++y)
        for (int x = outsideHorizon.x() - 2; x <= outsideHorizon.x() + 2; ++x)
            if (gridView.valid(x, y) && gridView.pixelColor(x, y) != QColor("#dce0e5"))
                horizonOutsideCanvas = true;
    require(outsideHorizon.x() < canvas->width() && horizonOutsideCanvas,
            "horizon must remain visible outside the canvas");
    canvas->undoStack()->undo();
    require(canvas->state().horizonY == 240 && canvas->state().vanishingPoints[0].position == QPointF(550, 240),
            "horizon undo failed");
    canvas->undoStack()->undo();
    require(canvas->state().vanishingPoints[0].position == QPointF(600, 210), "snapping undo failed");
    canvas->undoStack()->undo();
    require(canvas->state().vanishingPoints[0].position == QPointF(650, 240), "free perspective point undo failed");
    canvas->undoStack()->redo();
    canvas->undoStack()->undo();
    require(canvas->undoStack()->canUndo() && canvas->undoStack()->canRedo(),
            "history fixture must have undo and redo operations");
    const DrawingHistory savedHistory = canvas->history();
    QString error;
    const auto projectPath = out.filePath("roundtrip.drw");
    require(Project::save(projectPath, savedHistory, &error), qPrintable(error));
    DrawingHistory loadedHistory;
    require(Project::load(projectPath, &loadedHistory, &error), qPrintable(error));
    require(loadedHistory.states.size() == savedHistory.states.size() && loadedHistory.labels == savedHistory.labels &&
                loadedHistory.index == savedHistory.index,
            "DRW history index or labels mismatch");
    Canvas restored;
    restored.setDocument(loadedHistory, true);
    const DrawingState restoredCurrent = restored.state();
    const QString undoLabel = restored.undoStack()->undoText();
    const QString redoLabel = restored.undoStack()->redoText();
    require(restored.undoStack()->isClean() && restored.undoStack()->canUndo() && restored.undoStack()->canRedo() &&
                !undoLabel.isEmpty() && !redoLabel.isEmpty(),
            "restored history must preserve clean index and both directions");
    restored.undoStack()->undo();
    require(restored.state().vanishingPoints != restoredCurrent.vanishingPoints ||
                restored.state().gridVisible != restoredCurrent.gridVisible ||
                pixels(restored.state()) != pixels(restoredCurrent),
            "restored undo did not change state");
    restored.undoStack()->redo();
    require(pixels(restored.state()) == pixels(restoredCurrent) &&
                restored.state().vanishingPoints == restoredCurrent.vanishingPoints && restored.undoStack()->isClean(),
            "restored redo did not return to saved state");
    DrawingState loaded;
    require(Project::load(projectPath, &loaded, &error), qPrintable(error));
    require(pixels(loaded) == pixels(canvas->state()) && loaded.vanishingPoints == canvas->state().vanishingPoints &&
                loaded.horizonY == canvas->state().horizonY && loaded.verticalX == canvas->state().verticalX &&
                !loaded.gridVisible,
            "DRW geometry roundtrip mismatch");
    DrawingState styled = initial;
    styled.gridVisible = true;
    styled.verticalX = 555;
    styled.verticalLocked = true;
    styled.vanishingPoints[0].name = QStringLiteral("Главная точка");
    styled.vanishingPoints[0].position = QPointF(555, 240);
    styled.vanishingPoints[0].attachmentTargetIds =
        QStringList{PerspectiveTarget::horizon(), PerspectiveTarget::vertical()};
    styled.vanishingPoints[0].locked = true;
    styled.horizonLocked = true;
    styled.rayStepDegrees = 7.5;
    styled.rayGap = 20;
    styled.rayStartOpacity = 15;
    styled.rayEndOpacity = 80;
    styled.rayFadeLength = 60;
    styled.horizonColor = QColor("#d43b3b");
    styled.horizonOpacity = 100;
    styled.horizonWidth = 3;
    styled.verticalColor = QColor("#d09040");
    styled.verticalOpacity = 100;
    styled.verticalWidth = 4;
    require(Project::save(out.filePath("styled-guides.drw"), styled, &error) &&
                Project::load(out.filePath("styled-guides.drw"), &loaded, &error),
            qPrintable(error));
    require(loaded.vanishingPoints[0].name == QStringLiteral("Главная точка") &&
                loaded.vanishingPoints[0].position == QPointF(555, 240) &&
                loaded.vanishingPoints[0].isAttachedTo(PerspectiveTarget::horizon()) &&
                loaded.vanishingPoints[0].isAttachedTo(PerspectiveTarget::vertical()) &&
                loaded.vanishingPoints[0].locked && loaded.horizonY == 240 && loaded.horizonLocked &&
                loaded.verticalX == 555 && loaded.verticalLocked && !loaded.gridVisible &&
                loaded.vanishingPoints[0].color == QColor("#628ed1") && loaded.rayStepDegrees == 10 &&
                loaded.rayGap == 12 && loaded.rayStartOpacity == 10 && loaded.rayEndOpacity == 70 &&
                loaded.rayFadeLength == 50 && loaded.horizonColor == QColor("#628ed1") && loaded.horizonOpacity == 70 &&
                loaded.horizonWidth == 1 && loaded.verticalColor == QColor("#9b6bc0") && loaded.verticalOpacity == 70 &&
                loaded.verticalWidth == 1,
            "DRW must restore point names, perspective geometry, dual attachments, and locks without stored "
            "appearance");
    QZipReader styledArchive(out.filePath("styled-guides.drw"));
    const auto styledMetadata = QJsonDocument::fromJson(styledArchive.fileData("project.json")).object();
    const auto storedPerspective = styledMetadata.value("perspective").toObject();
    const auto storedPoint = storedPerspective.value("points").toArray().first().toObject();
    const auto storedAttachments = storedPoint.value("attachments").toArray();
    require(styledMetadata.value("version").toInt() == Project::CurrentFormatVersion &&
                storedPoint.value("name").toString() == QStringLiteral("Главная точка") &&
                storedPoint.value("locked").toBool() && storedAttachments.size() == 2 &&
                storedAttachments[0].toObject().value("targetId").toString() == PerspectiveTarget::horizon() &&
                storedAttachments[1].toObject().value("targetId").toString() == PerspectiveTarget::vertical() &&
                storedPerspective.value("horizon").toObject().value("locked").toBool() &&
                storedPerspective.value("vertical").toObject().value("x").toDouble() == 555 &&
                storedPerspective.value("vertical").toObject().value("locked").toBool(),
            "DRW must contain point names, multiple point attachments, construction axes, and perspective locks");
    auto downgradeToVersion6 = [](QJsonObject perspective) {
        QJsonArray points = perspective.value("points").toArray();
        for (int index = 0; index < points.size(); ++index) {
            QJsonObject point = points[index].toObject();
            point.remove("name");
            const auto attachments = point.take("attachments").toArray();
            point["attachment"] = attachments.isEmpty() ? QJsonValue::Null : attachments.first();
            points[index] = point;
        }
        perspective["points"] = points;
        return perspective;
    };
    QJsonObject version6Metadata = styledMetadata;
    version6Metadata["version"] = 6;
    version6Metadata["perspective"] = downgradeToVersion6(version6Metadata.value("perspective").toObject());
    QJsonObject version6History = version6Metadata.value("history").toObject();
    QJsonArray version6States = version6History.value("states").toArray();
    for (int index = 0; index < version6States.size(); ++index) {
        QJsonObject state = version6States[index].toObject();
        state["perspective"] = downgradeToVersion6(state.value("perspective").toObject());
        version6States[index] = state;
    }
    version6History["states"] = version6States;
    version6Metadata["history"] = version6History;
    QZipWriter version6Archive(out.filePath("legacy-v6.drw"));
    version6Archive.addFile("drawing.png", styledArchive.fileData("drawing.png"));
    version6Archive.addFile("project.json", QJsonDocument(version6Metadata).toJson());
    version6Archive.close();
    DrawingState version6State;
    require(Project::load(out.filePath("legacy-v6.drw"), &version6State, &error) &&
                version6State.vanishingPoints.first().name == QStringLiteral("Точка схода 1") &&
                version6State.vanishingPoints.first().isAttachedTo(PerspectiveTarget::horizon()),
            "version 6 projects must receive default point names and restore their single attachment");
    Canvas styledCanvas;
    QImage styledImage(200, 200, QImage::Format_ARGB32_Premultiplied);
    styledImage.fill(Qt::white);
    styled.setSingleRasterImage(styledImage, QStringLiteral("Background"));
    styled.vanishingPoints[0].position = QPointF(100, 100);
    styled.vanishingPoints[0].attachmentTargetIds.clear();
    styled.horizonY = 100;
    styled.verticalX = 120;
    styled.rayStepDegrees = 30;
    styled.rayGap = 20;
    styled.rayStartOpacity = 10;
    styled.rayEndOpacity = 70;
    styled.rayFadeLength = 50;
    styledCanvas.resize(400, 400);
    styledCanvas.setDocument(styled);
    QImage styledView(styledCanvas.size(), QImage::Format_ARGB32_Premultiplied);
    styledView.fill(Qt::transparent);
    styledCanvas.render(&styledView);
    const QPointF styledVanishing = styledCanvas.toView(styled.vanishingPoints[0].position);
    const QColor gapPixel = styledView.pixelColor(qRound(styledVanishing.x()), qRound(styledVanishing.y() + 12));
    const QColor startPixel = styledView.pixelColor(qRound(styledVanishing.x()), qRound(styledVanishing.y() + 22));
    const QColor endPixel = styledView.pixelColor(qRound(styledVanishing.x()), qRound(styledVanishing.y() + 80));
    auto darkness = [](const QColor &color) { return 765 - color.red() - color.green() - color.blue(); };
    require(gapPixel == QColor(Qt::white) && darkness(endPixel) > darkness(startPixel),
            "perspective ray gap or opacity ramp was not rendered");
    const QRectF styledPaper(styledCanvas.toView(QPointF()), QSizeF(styled.canvasSize) * styledCanvas.zoom());
    const QColor independentHorizon = styledView.pixelColor(
        qRound(styledPaper.right() + 20), qRound(styledCanvas.toView(QPointF(0, styled.horizonY)).y()));
    require(independentHorizon == styled.horizonColor, "horizon must use its own color, opacity, and line width");
    const QColor independentVertical = styledView.pixelColor(
        qRound(styledCanvas.toView(QPointF(styled.verticalX, 0)).x()), qRound(styledPaper.bottom() + 20));
    require(independentVertical == styled.verticalColor,
            "main vertical must continue outside the canvas with its own appearance");
    require(styledView.pixelColor(qRound(styledVanishing.x()), qRound(styledPaper.bottom() + 12)) == QColor("#dce0e5"),
            "perspective ray must stop after crossing the canvas");
    DrawingState outside = styled;
    outside.vanishingPoints[0].position = QPointF(-50, 0);
    outside.horizonY = 0;
    outside.rayStepDegrees = 30;
    outside.rayGap = 0;
    outside.rayStartOpacity = 100;
    outside.rayEndOpacity = 100;
    outside.rayFadeLength = 0;
    styledCanvas.setDocument(outside);
    QImage filteredView(styledCanvas.size(), QImage::Format_ARGB32_Premultiplied);
    filteredView.fill(Qt::transparent);
    styledCanvas.render(&filteredView);
    const QPointF outsideVanishing = styledCanvas.toView(outside.vanishingPoints[0].position);
    auto hasGuidePixel = [&](QPointF point) {
        for (int y = qRound(point.y()) - 2; y <= qRound(point.y()) + 2; ++y)
            for (int x = qRound(point.x()) - 2; x <= qRound(point.x()) + 2; ++x)
                if (filteredView.valid(x, y) && filteredView.pixelColor(x, y) != QColor("#dce0e5"))
                    return true;
        return false;
    };
    require(hasGuidePixel(outsideVanishing + QPointF(52, 30)) && !hasGuidePixel(outsideVanishing + QPointF(0, 50)) &&
                !hasGuidePixel(outsideVanishing + QPointF(260, 150)),
            "perspective rays must approach and cross the canvas, stop at its far edge, and hide when they miss it");
    QSettings applicationAppearance;
    applicationAppearance.setValue("perspective/common/rayStepDegrees", 6.5);
    applicationAppearance.setValue("perspective/common/rayGap", 18);
    applicationAppearance.setValue("perspective/common/rayStartOpacity", 16);
    applicationAppearance.setValue("perspective/common/rayEndOpacity", 76);
    applicationAppearance.setValue("perspective/common/rayFadeLength", 56);
    MainWindow pathWindow;
    require(pathWindow.openPath(projectPath), "path settings fixture did not open");
    require(pathWindow.canvas()->state().vanishingPoints == canvas->state().vanishingPoints &&
                pathWindow.canvas()->state().horizonY == canvas->state().horizonY &&
                pathWindow.canvas()->state().verticalX == canvas->state().verticalX &&
                pathWindow.canvas()->state().rayStepDegrees == 6.5 && pathWindow.canvas()->state().rayGap == 18 &&
                pathWindow.canvas()->state().rayStartOpacity == 16 &&
                pathWindow.canvas()->state().rayEndOpacity == 76 && pathWindow.canvas()->state().rayFadeLength == 56,
            "opening DRW must combine project geometry with application appearance settings");
    require(QDir(QSettings().value("files/openDirectory").toString()) == QDir(out.absolutePath()) &&
                !QSettings().contains("files/saveDirectory"),
            "open and save directories must be independent");
    auto *pathSaveAction = pathWindow.findChild<QAction *>("saveAction");
    require(pathSaveAction, "save action missing");
    pathSaveAction->trigger();
    QApplication::processEvents();
    require(QDir(QSettings().value("files/saveDirectory").toString()) == QDir(out.absolutePath()),
            "successful save did not remember its directory");
    pathWindow.close();
    QSettings().remove("perspective/common");
    QStringList recentPaths;
    for (int i = 0; i < 6; ++i) {
        const QString path = out.filePath(QString("recent-%1.drw").arg(i));
        require(Project::save(path, loaded, &error), qPrintable(error));
        recentPaths.append(QFileInfo(path).absoluteFilePath());
    }
    MainWindow recentWindow;
    for (const auto &path : recentPaths)
        require(recentWindow.openPath(path), "recent file fixture did not open");
    auto *recentMenu = recentWindow.findChild<QMenu *>("recentFilesMenu");
    require(recentMenu && recentMenu->actions().size() == 5, "recent files menu must contain five entries");
    const QStringList storedRecent = QSettings().value("files/recentFiles").toStringList();
    require(storedRecent.size() == 5 && storedRecent.first() == recentPaths.last() &&
                !storedRecent.contains(recentPaths.first()) &&
                recentMenu->actions().first()->data().toString() == recentPaths.last(),
            "recent files order, limit, or menu data is wrong");
    MainWindow persistedRecentWindow;
    auto *persistedRecentMenu = persistedRecentWindow.findChild<QMenu *>("recentFilesMenu");
    require(persistedRecentMenu && persistedRecentMenu->actions().size() == 5 &&
                persistedRecentMenu->actions().first()->data().toString() == recentPaths.last(),
            "recent files did not persist between windows");
    recentWindow.close();
    persistedRecentWindow.close();
    return {renderedPixels, loaded, projectPath};
}

/// Проверяет единую панель и независимое сохранение параметров карандаша, кисти и ластика.
void testToolWidthPersistence() {
    MainWindow widthsWindow;
    auto *widthControl = widthsWindow.findChild<QSpinBox *>("strokeWidth");
    auto *opacityControl = widthsWindow.findChild<QSpinBox *>("strokeOpacity");
    auto *hardnessControl = widthsWindow.findChild<QSpinBox *>("strokeHardness");
    auto *spacingControl = widthsWindow.findChild<QSpinBox *>("strokeSpacing");
    auto *strengthControl = widthsWindow.findChild<QSpinBox *>("eraserStrength");
    auto *toolTitle = widthsWindow.findChild<QLabel *>("toolPropertiesTitle");
    auto *eraserMode = widthsWindow.findChild<QLabel *>("eraserMode");
    auto *pencilAction = widthsWindow.findChild<QAction *>("tool0");
    auto *brushAction = widthsWindow.findChild<QAction *>("tool1");
    auto *eraserAction = widthsWindow.findChild<QAction *>("tool2");
    auto *panAction = widthsWindow.findChild<QAction *>("tool3");
    require(widthControl && opacityControl && hardnessControl && spacingControl && strengthControl && toolTitle &&
                eraserMode && pencilAction && brushAction && eraserAction && panAction,
            "shared per-tool controls are missing");
    widthControl->setValue(4);
    opacityControl->setValue(80);
    spacingControl->setValue(12);
    brushAction->trigger();
    require(toolTitle->text() == QStringLiteral("Кисть") && widthControl->value() == 3 &&
                opacityControl->value() == 100 && hardnessControl->value() == 70 && spacingControl->value() == 15 &&
                !opacityControl->isHidden() && !hardnessControl->isHidden() && strengthControl->isHidden(),
            "brush must expose its own width, opacity, hardness, and spacing");
    widthControl->setValue(11);
    opacityControl->setValue(65);
    hardnessControl->setValue(40);
    spacingControl->setValue(20);
    eraserAction->trigger();
    require(toolTitle->text() == QStringLiteral("Ластик") && widthControl->value() == 3 &&
                hardnessControl->value() == 100 && spacingControl->value() == 15 && strengthControl->value() == 100 &&
                opacityControl->isHidden() && !hardnessControl->isHidden() && !strengthControl->isHidden() &&
                eraserMode->text() == QStringLiteral("Цветом Back"),
            "eraser must expose hardness and strength and report the current Back-color behavior");
    widthControl->setValue(17);
    hardnessControl->setValue(25);
    spacingControl->setValue(30);
    strengthControl->setValue(55);
    pencilAction->trigger();
    require(toolTitle->text() == QStringLiteral("Карандаш") && widthControl->value() == 4 &&
                opacityControl->value() == 80 && spacingControl->value() == 12 && hardnessControl->isHidden(),
            "pencil settings were not restored");
    brushAction->trigger();
    require(widthControl->value() == 11 && opacityControl->value() == 65 && hardnessControl->value() == 40 &&
                spacingControl->value() == 20,
            "brush settings were not restored");
    eraserAction->trigger();
    require(widthControl->value() == 17 && hardnessControl->value() == 25 && spacingControl->value() == 30 &&
                strengthControl->value() == 55,
            "eraser settings were not restored");
    panAction->trigger();
    require(!widthControl->isEnabled() && toolTitle->text() == QStringLiteral("Параметры рисования"),
            "drawing controls must be disabled for a non-paint mode");
    widthsWindow.close();
    MainWindow persistedWidthsWindow;
    widthControl = persistedWidthsWindow.findChild<QSpinBox *>("strokeWidth");
    opacityControl = persistedWidthsWindow.findChild<QSpinBox *>("strokeOpacity");
    hardnessControl = persistedWidthsWindow.findChild<QSpinBox *>("strokeHardness");
    spacingControl = persistedWidthsWindow.findChild<QSpinBox *>("strokeSpacing");
    strengthControl = persistedWidthsWindow.findChild<QSpinBox *>("eraserStrength");
    brushAction = persistedWidthsWindow.findChild<QAction *>("tool1");
    eraserAction = persistedWidthsWindow.findChild<QAction *>("tool2");
    require(widthControl && opacityControl && hardnessControl && spacingControl && strengthControl &&
                widthControl->value() == 4 && opacityControl->value() == 80 && spacingControl->value() == 12,
            "pencil settings did not persist");
    brushAction->trigger();
    require(widthControl->value() == 11 && opacityControl->value() == 65 && hardnessControl->value() == 40 &&
                spacingControl->value() == 20,
            "brush settings did not persist");
    eraserAction->trigger();
    require(widthControl->value() == 17 && hardnessControl->value() == 25 && spacingControl->value() == 30 &&
                strengthControl->value() == 55,
            "eraser settings did not persist");
    persistedWidthsWindow.close();
}

void testExportValidationAndScreenshots(
    MainWindow &window, Canvas *canvas, const DrawingState &initial, const QDir &out, const ProjectFixture &fixture) {
    QString error;
    DrawingState loaded = fixture.loadedState;
    DrawingHistory loadedHistory;
    const QImage &expectedPixels = fixture.renderedPixels;
    const QString &projectPath = fixture.projectPath;
    require(Project::exportPng(out.filePath("export.png"), pixels(canvas->state()), &error), qPrintable(error));
    QImage png;
    require(Project::loadPng(out.filePath("export.png"), &png, &error), qPrintable(error));
    require(png == expectedPixels, "grid leaked into exported PNG");
    QImage alphaExport(32, 24, QImage::Format_ARGB32_Premultiplied);
    alphaExport.fill(Qt::transparent);
    alphaExport.setPixelColor(16, 12, QColor(40, 80, 120, 128));
    require(Project::exportImage(out.filePath("export.jpg"), alphaExport, "JPEG", 87, &error), qPrintable(error));
    require(Project::exportImage(out.filePath("export.bmp"), alphaExport, "BMP", -1, &error), qPrintable(error));
    const QImage jpeg(out.filePath("export.jpg")), bmp(out.filePath("export.bmp"));
    require(!jpeg.isNull() && !bmp.isNull() && jpeg.size() == alphaExport.size() && bmp.size() == alphaExport.size() &&
                !jpeg.hasAlphaChannel() && !bmp.hasAlphaChannel() && jpeg.pixelColor(0, 0).lightness() > 240 &&
                bmp.pixelColor(0, 0) == QColor(Qt::white),
            "JPEG and BMP export must flatten transparency onto white and remain readable");
    require(!Project::exportImage(out.filePath("export.invalid"), alphaExport, "GIF", -1, &error),
            "an unsupported export format must be rejected");
    DrawingState transparent = loaded;
    QImage *transparentPixels = editablePixels(&transparent);
    require(transparentPixels, "transparent test layer is not editable");
    transparentPixels->fill(Qt::transparent);
    transparentPixels->setPixelColor(7, 8, QColor(40, 80, 120, 128));
    require(Project::save(out.filePath("alpha.drw"), transparent, &error), qPrintable(error));
    require(Project::load(out.filePath("alpha.drw"), &loaded, &error), qPrintable(error));
    require(pixels(loaded) == pixels(transparent), "alpha roundtrip failed");
    QFile bad(out.filePath("bad.drw"));
    bad.open(QIODevice::WriteOnly);
    bad.write("not a zip");
    bad.close();
    DrawingState unchanged = loaded;
    require(!Project::load(bad.fileName(), &loaded, &error), "bad project accepted");
    require(pixels(loaded) == pixels(unchanged), "failed load modified destination");
    require(!Project::save(out.filePath("missing/fail.drw"), loaded, &error), "save to missing directory should fail");
    QZipWriter invalid(out.filePath("version.drw"));
    invalid.addFile("drawing.png", QByteArray("bad"));
    invalid.addFile("project.json", QByteArray("{\"format\":\"Drawing\",\"version\":99,\"image\":\"drawing.png\"}"));
    invalid.close();
    require(!Project::load(out.filePath("version.drw"), &loaded, &error), "unknown project version accepted");
    QByteArray legacyPng;
    QBuffer legacyBuffer(&legacyPng);
    legacyBuffer.open(QIODevice::WriteOnly);
    require(pixels(initial).save(&legacyBuffer, "PNG"), "legacy PNG encoding failed");
    QJsonObject legacyPerspective{
        {"visible", false}, {"x", 650.0}, {"y", 240.0}, {"rays", 16}, {"color", QStringLiteral("#ff628ed1")}};
    QJsonObject legacyMetadata{{"format", "Drawing"},
                               {"version", 1},
                               {"width", 1000},
                               {"height", 620},
                               {"image", "drawing.png"},
                               {"perspective", legacyPerspective}};
    QZipWriter legacy(out.filePath("legacy-v1.drw"));
    legacy.addFile("drawing.png", legacyPng);
    legacy.addFile("project.json", QJsonDocument(legacyMetadata).toJson());
    legacy.close();
    DrawingHistory legacyHistory;
    require(Project::load(out.filePath("legacy-v1.drw"), &legacyHistory, &error) && legacyHistory.states.size() == 1 &&
                legacyHistory.index == 0 && legacyHistory.labels.isEmpty() &&
                legacyHistory.states.first().vanishingPoints.first().name == QStringLiteral("Точка схода 1") &&
                legacyHistory.states.first().rayStepDegrees == 10 && legacyHistory.states.first().horizonY == 240 &&
                legacyHistory.states.first().verticalX == 500,
            "version 1 project geometry compatibility failed");
    require(!Project::validSize(QSize(8192, 8192)) && !Project::validSize(QSize(-1, 10)),
            "invalid canvas size accepted");
    if (QGuiApplication::platformName() != "offscreen") {
        bool cancelClicked = false;
        QTimer::singleShot(0, &window, [&] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (box) {
                for (auto *button : box->buttons())
                    if (button->text() == QStringLiteral("Отмена")) {
                        cancelClicked = true;
                        button->click();
                        return;
                    }
                box->reject();
            }
        });
        require(!window.close() && cancelClicked && window.isVisible(), "cancel must prevent closing dirty document");
        canvas->undoStack()->setClean();
        require(window.openPath(projectPath) && canvas->undoStack()->canUndo() && canvas->undoStack()->canRedo(),
                "opening saved project did not restore history");
        canvas->setTool(Canvas::Pencil);
        drag(canvas, {40, 40}, {90, 40});
        const QImage beforeFailedOpen = pixels(canvas->state());
        QTimer::singleShot(0, &window, [] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (box)
                box->accept();
        });
        require(!window.openPath(bad.fileName()) && pixels(canvas->state()) == beforeFailedOpen &&
                    !canvas->undoStack()->isClean(),
                "failed open must preserve dirty document");
        bool saveClicked = false;
        QTimer::singleShot(0, &window, [&] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (box) {
                for (auto *button : box->buttons())
                    if (button->text() == QStringLiteral("Сохранить")) {
                        saveClicked = true;
                        button->click();
                        return;
                    }
                box->reject();
            }
        });
        require(window.close() && saveClicked, "save before closing failed");
        require(Project::load(projectPath, &loaded, &error) && pixels(loaded) == beforeFailedOpen,
                "save on close lost changes");
        require(Project::load(projectPath, &loadedHistory, &error) &&
                    loadedHistory.index == loadedHistory.labels.size() && loadedHistory.labels.size() > 1,
                "save on close lost undo history or retained discarded redo branch");
        window.show();
        QApplication::processEvents();
    }
    canvas->setDocument(initial);
    canvas->setTool(Canvas::Pencil);
    canvas->setFront(QColor("#526070"));
    canvas->setStrokeWidth(2);
    canvas->fit();
    const QVector<QPointF> house{{230, 450}, {230, 265}, {400, 173}, {563, 270}, {563, 451}, {230, 450}};
    for (int i = 1; i < house.size(); ++i)
        drag(canvas, house[i - 1], house[i]);
    drag(canvas, {230, 265}, {400, 352});
    drag(canvas, {400, 352}, {563, 270});
    drag(canvas, {400, 173}, {400, 352});
    drag(canvas, {400, 352}, {563, 451});
    drag(canvas, {442, 380}, {442, 316});
    drag(canvas, {442, 316}, {488, 294});
    drag(canvas, {488, 294}, {488, 409});
    drag(canvas, {183, 472}, {600, 472});
    require(Project::save(out.filePath("Набросок.drw"), canvas->state(), &error), qPrintable(error));
    QApplication::processEvents();
    require(window.grab().save(out.filePath("editor.png")), "screenshot failed");
    window.resize(720, 480);
    QApplication::processEvents();
    require(window.grab().save(out.filePath("editor-small.png")), "small screenshot failed");
    window.resize(1200, 800);
    QApplication::processEvents();
    auto *action = window.findChild<QAction *>("tool4");
    require(action, "perspective action missing");
    action->trigger();
    QApplication::processEvents();
    require(window.grab().save(out.filePath("perspective.png")), "perspective screenshot failed");
    canvas->undoStack()->setClean();
    window.close();
}

/// Записывает итоговый маркер успешного выполнения для внешнего сценария с тайм-аутом.
void writeReport(const QDir &out) {
    QFile report(out.filePath("result.txt"));
    report.open(QIODevice::WriteOnly);
    report.write("PASS: pencil, brush, eraser, connected segments, angle constraint, persistent undo/redo history, "
                 "DRW v1 compatibility, pan/zoom, perspective, DRW/PNG, alpha, invalid input, screenshots\n");
    report.write(QGuiApplication::platformName() == "offscreen"
                     ? "SKIP: native dialog tests (run with -platform windows)\n"
                     : "PASS: close cancel/save, failed open preserves document\n");
}
} // namespace

/// Выполняет все группы самопроверки в изолированном каталоге и возвращает код процесса.
int runSelfTests(const QString &outputDirectory) {
    try {
        if (QGuiApplication::platformName() == "offscreen")
            QApplication::setStyle("Fusion");
        QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR") + "/Fonts/segoeui.ttf");
        QDir().mkpath(outputDirectory);
        QDir out(outputDirectory);
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, out.filePath("settings"));
        QSettings().clear();
        tracePath = out.filePath("trace.txt");
        {
            QFile trace(tracePath);
            trace.open(QIODevice::WriteOnly);
        }

        QTimer watchdog;
        watchdog.setSingleShot(true);
        watchdog.setInterval(12000);
        QObject::connect(&watchdog, &QTimer::timeout, [] {
            QFile trace(tracePath);
            trace.open(QIODevice::Append);
            trace.write("TIMEOUT\n");
            for (auto *widget : QApplication::topLevelWidgets()) {
                trace.write(widget->metaObject()->className());
                trace.write(" ");
                trace.write(widget->windowTitle().toUtf8());
                trace.write("\n");
            }
            trace.close();
            std::exit(2);
        });
        watchdog.start();

        testLayerArchitecture();

        MainWindow window;
        testMainWindowUi(window, out);
        Canvas *canvas = window.canvas();
        const DrawingState initial = initialDrawingState();
        testPerspectiveGeometry(window, canvas, initial);
        const ProjectFixture fixture = testDrawingAndProject(canvas, initial, out);
        testToolWidthPersistence();
        testExportValidationAndScreenshots(window, canvas, initial, out, fixture);
        writeReport(out);
        return 0;
    } catch (const std::exception &error) {
        qCritical() << "SELF_TEST_FAILED:" << error.what();
        return 1;
    }
}
