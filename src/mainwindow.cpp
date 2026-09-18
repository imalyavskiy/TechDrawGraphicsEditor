#include "mainwindow.h"
#include "autohidedockwidget.h"
#include "rolloutsection.h"
#include "drawingtoolsettings.h"
#include "layerpanel.h"
#include "toolpropertiespanel.h"
#include <QtWidgets>

namespace {
/// Возвращает полное название, видимое пользователю и доступное для перевода.
QString productName() {
    return QCoreApplication::translate("MainWindow", "Технический рисунок / Technical Draw");
}

/// Строит пиктограмму одного из пяти инструментов рисования и навигации.
QIcon toolIcon(int kind) {
    QPixmap image(24, 24);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor("#364152"), 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    if (kind == 0) {
        painter.drawPolygon(QPolygonF(
            QVector<QPointF>{QPointF(5, 15), QPointF(15, 5), QPointF(19, 9), QPointF(9, 19), QPointF(4, 20)}));
        painter.drawLine(13, 7, 17, 11);
    } else if (kind == 1) {
        painter.drawLine(6, 19, 17, 8);
        painter.drawLine(14, 5, 20, 11);
        painter.drawLine(17, 8, 14, 5);
        painter.setBrush(QColor("#364152"));
        painter.drawEllipse(QRectF(3, 16, 7, 5));
    } else if (kind == 2) {
        painter.drawPolygon(QPolygonF(
            QVector<QPointF>{QPointF(4, 14), QPointF(13, 5), QPointF(20, 12), QPointF(12, 20), QPointF(9, 20)}));
        painter.drawLine(8, 10, 16, 17);
        painter.drawLine(11, 20, 21, 20);
    } else if (kind == 3) {
        painter.drawLine(12, 3, 12, 21);
        painter.drawLine(3, 12, 21, 12);
        painter.drawLine(12, 3, 9, 7);
        painter.drawLine(12, 3, 15, 7);
        painter.drawLine(12, 21, 9, 17);
        painter.drawLine(12, 21, 15, 17);
        painter.drawLine(3, 12, 7, 9);
        painter.drawLine(3, 12, 7, 15);
        painter.drawLine(21, 12, 17, 9);
        painter.drawLine(21, 12, 17, 15);
    } else {
        painter.drawEllipse(QPointF(12, 10), 3, 3);
        painter.drawLine(12, 1, 12, 6);
        painter.drawLine(12, 14, 12, 22);
        painter.drawLine(2, 10, 8, 10);
        painter.drawLine(16, 10, 22, 10);
        painter.drawLine(4, 22, 10, 13);
        painter.drawLine(20, 22, 14, 13);
    }
    return QIcon(image);
}
/// Строит пиктограмму команды масштаба один к одному.
QIcon actualSizeIcon() {
    QPixmap image(28, 20);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setPen(QColor("#364152"));
    QFont font = painter.font();
    font.setBold(true);
    font.setPixelSize(11);
    painter.setFont(font);
    painter.drawText(image.rect(), Qt::AlignCenter, QCoreApplication::translate("MainWindow", "1:1"));
    return QIcon(image);
}
/// Строит пиктограмму общего переключателя прилипания к направляющим.
QIcon snapIcon() {
    QPixmap image(24, 24);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor("#364152"), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawArc(QRectF(5, 3, 14, 16), 0, 180 * 16);
    painter.drawLine(5, 11, 5, 17);
    painter.drawLine(19, 11, 19, 17);
    painter.setPen(QPen(QColor("#2f86c7"), 2));
    painter.drawLine(3, 19, 9, 19);
    painter.drawLine(15, 19, 21, 19);
    return QIcon(image);
}
/// Строит открытый или закрытый глаз для переключателя видимости точки.
QIcon eyeIcon(bool open) {
    QPixmap image(20, 20);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor("#364152"), 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    QPainterPath lid;
    lid.moveTo(2, 10);
    lid.cubicTo(6, open ? 4 : 7, 14, open ? 4 : 7, 18, 10);
    if (open) {
        lid.cubicTo(14, 16, 6, 16, 2, 10);
        painter.drawPath(lid);
        painter.setBrush(QColor("#364152"));
        painter.drawEllipse(QPointF(10, 10), 2.2, 2.2);
    } else {
        painter.drawPath(lid);
        painter.drawLine(5, 13, 4, 15);
        painter.drawLine(10, 14, 10, 16);
        painter.drawLine(15, 13, 16, 15);
    }
    return QIcon(image);
}
/// Строит открытый или закрытый замок для переключателя фиксации точки.
QIcon lockIcon(bool locked) {
    QPixmap image(20, 20);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor("#364152"), 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(QColor("#edf0f4"));
    painter.drawRoundedRect(QRectF(4, 9, 12, 9), 2, 2);
    painter.setBrush(Qt::NoBrush);
    QPainterPath shackle;
    if (locked) {
        shackle.moveTo(6.5, 9);
        shackle.lineTo(6.5, 7);
        shackle.cubicTo(6.5, 2.5, 13.5, 2.5, 13.5, 7);
        shackle.lineTo(13.5, 9);
    } else {
        shackle.moveTo(7, 9);
        shackle.lineTo(7, 7);
        shackle.cubicTo(7, 2.5, 14, 2.5, 14, 7);
        shackle.lineTo(16, 7);
    }
    painter.drawPath(shackle);
    painter.drawLine(10, 12, 10, 15);
    return QIcon(image);
}
/// Обновляет цветной образец кнопки без текстовой подписи.
void colorSwatch(QPushButton *button, QColor color) {
    QPixmap swatch(22, 22);
    swatch.fill(color);
    QPainter painter(&swatch);
    painter.setPen(QColor("#8e949d"));
    painter.drawRect(0, 0, 21, 21);
    button->setIcon(QIcon(swatch));
    button->setIconSize(QSize(22, 22));
}
/// Добавляет расширение к пути, если оно отсутствует без учёта регистра.
QString withSuffix(QString path, const QString &suffix) {
    if (!path.endsWith(suffix, Qt::CaseInsensitive))
        path += suffix;
    return path;
}
/// Возвращает доступный пользовательский каталог документов либо домашний каталог.
QString defaultDirectory() {
    const QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return documents.isEmpty() ? QDir::homePath() : documents;
}
/// Читает сохранённый каталог и заменяет недоступное значение каталогом по умолчанию.
QString rememberedDirectory(const QString &key) {
    const QString path = QSettings().value(key).toString();
    return QDir(path).exists() ? path : defaultDirectory();
}
/// Сохраняет родительский каталог успешно использованного файла.
void rememberDirectory(const QString &key, const QString &filePath) {
    QSettings().setValue(key, QFileInfo(filePath).absolutePath());
}
/// Формирует предлагаемый путь файла в каталоге, связанном с указанной операцией.
QString suggestedFile(const QString &key, const QString &name) {
    return QDir(rememberedDirectory(key)).filePath(name);
}
/// Применяет проверенный пользовательский пресет общих параметров опорных лучей.
void applySavedPerspectiveDefaults(DrawingState *state) {
    QSettings settings;
    const double step = settings.value("perspective/common/rayStepDegrees", 10.0).toDouble();
    const int gap = settings.value("perspective/common/rayGap", 12).toInt();
    const int startOpacity = settings.value("perspective/common/rayStartOpacity", 10).toInt();
    const int endOpacity = settings.value("perspective/common/rayEndOpacity", 70).toInt();
    const int fadeLength = settings.value("perspective/common/rayFadeLength", 50).toInt();
    const int pattern = settings.value("perspective/common/rayPattern", 0).toInt();
    if (step < 1 || step > 30 || gap < 0 || gap > 200 || startOpacity < 0 || startOpacity > 100 || endOpacity < 0 ||
        endOpacity > 100 || fadeLength < 0 || fadeLength > 500 || pattern < 0 || pattern > 3)
        return;
    state->rayStepDegrees = step;
    state->rayGap = gap;
    state->rayStartOpacity = startOpacity;
    state->rayEndOpacity = endOpacity;
    state->rayFadeLength = fadeLength;
    state->rayPattern = pattern;
}
/// Восстанавливает параметры отображения оснастки, не входящие в файл проекта.
void applySavedViewSettings(DrawingState *state) {
    QSettings settings;
    state->rayWidth = qBound(0.1, settings.value("perspective/view/rayWidth", 1.0).toDouble(), 20.0);
    state->rayAngleOffset = qBound(-180.0, settings.value("perspective/view/rayAngleOffset", 0.0).toDouble(), 180.0);
    state->horizonVisible = settings.value("perspective/view/horizonVisible", true).toBool();
    state->verticalVisible = settings.value("perspective/view/verticalVisible", true).toBool();
    state->axesVisible = settings.value("perspective/view/axesVisible", false).toBool();
    state->markersVisible = settings.value("perspective/view/markersVisible", true).toBool();
    const bool legacySymmetry = settings.value("perspective/view/symmetricPoints", false).toBool();
    state->horizonSymmetry = settings.value("perspective/view/horizonSymmetry", legacySymmetry).toBool();
    state->verticalSymmetry = settings.value("perspective/view/verticalSymmetry", legacySymmetry).toBool();
    const QColor horizonColor(settings.value("perspective/view/horizonColor", QStringLiteral("#628ed1")).toString());
    state->horizonColor = horizonColor.isValid() ? horizonColor : QColor("#628ed1");
    state->horizonOpacity = qBound(0, settings.value("perspective/view/horizonOpacity", 70).toInt(), 100);
    state->horizonWidth = qBound(0.1, settings.value("perspective/view/horizonWidth", 1.0).toDouble(), 20.0);
    const QColor verticalColor(settings.value("perspective/view/verticalColor", QStringLiteral("#9b6bc0")).toString());
    state->verticalColor = verticalColor.isValid() ? verticalColor : QColor("#9b6bc0");
    state->verticalOpacity = qBound(0, settings.value("perspective/view/verticalOpacity", 70).toInt(), 100);
    state->verticalWidth = qBound(0.1, settings.value("perspective/view/verticalWidth", 1.0).toDouble(), 20.0);
}
/// Выбирает повторяемый цвет по умолчанию для точки с указанным порядковым номером.
QColor defaultPointColor(int index) {
    static const QColor colors[]{
        QColor("#628ed1"), QColor("#d06b4c"), QColor("#4b9b67"), QColor("#896ac1"), QColor("#c08a34")};
    return colors[index % 5];
}
/// Восстанавливает цвет и видимость каждой точки по её текущей позиции в списке.
void applySavedPointAppearance(DrawingState *state) {
    QSettings settings;
    for (int i = 0; i < state->vanishingPoints.size(); ++i) {
        auto &point = state->vanishingPoints[i];
        const QColor color(
            settings.value(QString("perspective/points/%1/color").arg(i), defaultPointColor(i)).toString());
        point.color = color.isValid() ? color : defaultPointColor(i);
        point.visible = settings.value(QString("perspective/points/%1/visible").arg(i), true).toBool();
    }
}
/// Сохраняет единый пользовательский пресет общих параметров лучей.
void savePerspectiveDefaults(const DrawingState &state) {
    QSettings settings;
    settings.setValue("perspective/common/rayStepDegrees", state.rayStepDegrees);
    settings.setValue("perspective/common/rayGap", state.rayGap);
    settings.setValue("perspective/common/rayStartOpacity", state.rayStartOpacity);
    settings.setValue("perspective/common/rayEndOpacity", state.rayEndOpacity);
    settings.setValue("perspective/common/rayFadeLength", state.rayFadeLength);
    settings.setValue("perspective/common/rayPattern", state.rayPattern);
}
/// Удаляет пользовательский пресет, возвращая последующее чтение к заводским значениям.
void clearPerspectiveDefaults() {
    QSettings settings;
    settings.remove("perspective/common");
}
/// Проверяет, совпадает ли активное оформление лучей с сохранённым пресетом.
bool matchesPerspectiveDefaults(const DrawingState &state) {
    DrawingState defaults;
    applySavedPerspectiveDefaults(&defaults);
    return qFuzzyCompare(state.rayStepDegrees, defaults.rayStepDegrees) && state.rayGap == defaults.rayGap &&
           state.rayStartOpacity == defaults.rayStartOpacity && state.rayEndOpacity == defaults.rayEndOpacity &&
           state.rayFadeLength == defaults.rayFadeLength && state.rayPattern == defaults.rayPattern;
}
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), canvas_(new Canvas(this)), toolSettings_(new DrawingToolSettingsModel(this)) {
    initializeWindow();
    setupMenusAndToolbars();
    setupPerspectivePanel();
    setupLayersPanel();
    connectPerspectiveControls();
    setupViewAndStatusBar();
}

void MainWindow::initializeWindow() {
    coordinatePercent_ = QSettings().value("perspective/coordinatePercent", true).toBool();
    rulerPercent_ = QSettings().value("view/rulers/percent", false).toBool();
    canvas_->setRulerPercent(rulerPercent_);
    canvas_->setGuidesVisible(QSettings().value("view/guides/visible", true).toBool());
    canvas_->setSnapToGuides(QSettings().value("view/guides/snap", true).toBool());
    canvas_->setMoveTarget(Canvas::MoveTarget(qBound(0, QSettings().value("tools/moveTarget", 0).toInt(), 1)));
    DrawingState initialState = canvas_->state();
    applySavedPerspectiveDefaults(&initialState);
    applySavedViewSettings(&initialState);
    applySavedPointAppearance(&initialState);
    canvas_->setDocument(initialState, true);
    setObjectName("drawingWindow");
    resize(1200, 800);
    setMinimumSize(720, 480);
    setWindowIcon(QIcon(":/app/techdraw.png"));
    setCentralWidget(canvas_);
    setStyleSheet(
        "QToolBar { spacing: 5px; padding: 5px; border: 0; border-bottom: 1px solid #cdd0d5; background: #f6f6f6; } "
        "QDockWidget { font-weight: 500; } QStatusBar { background: #f6f6f6; } QToolButton { padding: 5px; } "
        "QToolButton:checked { background: #dceaff; border: 1px solid #8aaedb; border-radius: 3px; } "
        "QWidget#vanishingPointCard { background: #f5f6f8; border: 1px solid #d5d8dd; border-radius: 4px; } "
        "QWidget#vanishingPointCard[selected=\"true\"] { background: #e4effd; border-color: #8aaedb; } "
        "QWidget#vanishingPointCard QLineEdit { border: 0; background: transparent; padding: 2px; } "
        "QListWidget#vanishingPointsListControl::item { background: transparent; border: 0; } ");
    setStyleSheet(styleSheet() +
                  " QFrame#rolloutHeader { background: #eef1f5; border: 0; border-bottom: 1px solid #b9c0ca; } "
                  "QFrame#rolloutHeader QToolButton { padding: 0; border: 0; background: transparent; } "
                  "QFrame#rolloutHeader QToolButton:hover { background: #dce7f5; border-radius: 2px; }");
}

void MainWindow::setupMenusAndToolbars() {
    auto *file = menuBar()->addMenu(tr("&Файл"));
    file->setObjectName("fileMenu");
    auto *edit = menuBar()->addMenu(tr("&Правка"));
    edit->setObjectName("editMenu");
    viewMenu_ = menuBar()->addMenu(tr("&Вид"));
    viewMenu_->setObjectName("viewMenu");
    auto *imageMenu = menuBar()->addMenu(tr("&Изображение"));
    imageMenu->setObjectName("imageMenu");
    toolsMenu_ = menuBar()->addMenu(tr("&Инструменты"));
    toolsMenu_->setObjectName("toolsMenu");
    helpMenu_ = menuBar()->addMenu(tr("&Справка"));
    auto *newAction = file->addAction(
        style()->standardIcon(QStyle::SP_FileIcon), tr("Создать…"), this, &MainWindow::newDocument, QKeySequence::New);
    newAction->setObjectName("newAction");
    auto *openAction = file->addAction(style()->standardIcon(QStyle::SP_DirOpenIcon),
                                       tr("Открыть…"),
                                       this,
                                       &MainWindow::openDocument,
                                       QKeySequence::Open);
    openAction->setObjectName("openAction");
    recentFilesMenu_ = file->addMenu(tr("Недавние файлы"));
    recentFilesMenu_->setObjectName("recentFilesMenu");
    recentFiles_ = QSettings().value("files/recentFiles").toStringList();
    while (recentFiles_.size() > 5)
        recentFiles_.removeLast();
    updateRecentFilesMenu();
    file->addSeparator();
    auto *saveAction = file->addAction(
        style()->standardIcon(QStyle::SP_DialogSaveButton),
        tr("Сохранить проект"),
        this,
        [this] { saveDocument(); },
        QKeySequence::Save);
    saveAction->setObjectName("saveAction");
    file->addAction(tr("Сохранить проект как…"), this, [this] { saveDocument(true); }, QKeySequence::SaveAs);
    file->addSeparator();
    file->addAction(tr("Экспортировать изображение…"), this, &MainWindow::exportImage, QKeySequence("Ctrl+Shift+E"));
    file->addSeparator();
    file->addAction(tr("Выход"), this, &QWidget::close, QKeySequence("Alt+F4"));
    auto *undoAction = canvas_->undoStack()->createUndoAction(this, tr("Отменить"));
    undoAction->setObjectName("undoAction");
    undoAction->setShortcut(QKeySequence::Undo);
    undoAction->setIcon(style()->standardIcon(QStyle::SP_ArrowBack));
    edit->addAction(undoAction);
    auto *redoAction = canvas_->undoStack()->createRedoAction(this, tr("Повторить"));
    redoAction->setObjectName("redoAction");
    redoAction->setShortcuts({QKeySequence::Redo, QKeySequence("Ctrl+Shift+Z")});
    redoAction->setIcon(style()->standardIcon(QStyle::SP_ArrowForward));
    edit->addAction(redoAction);
    edit->addSeparator();
    auto *settingsAction = edit->addAction(tr("Настройки…"), this, &MainWindow::showSettings);
    settingsAction->setObjectName("settingsAction");
    auto *guidesMenu = imageMenu->addMenu(tr("Направляющие"));
    guidesMenu->setObjectName("guidesMenu");
    auto *newHorizontalGuide = guidesMenu->addAction(tr("Новая горизонтальная…"), this, [this] {
        newGuide(GuideType::Horizontal);
    });
    newHorizontalGuide->setObjectName("newHorizontalGuideAction");
    auto *newVerticalGuide = guidesMenu->addAction(tr("Новая вертикальная…"), this, [this] {
        newGuide(GuideType::Vertical);
    });
    newVerticalGuide->setObjectName("newVerticalGuideAction");
    guidesMenu->addSeparator();
    removeSelectedGuideAction_ = guidesMenu->addAction(tr("Удалить выбранную"), canvas_, &Canvas::removeSelectedGuide);
    removeSelectedGuideAction_->setObjectName("removeSelectedGuideAction");
    removeAllGuidesAction_ = guidesMenu->addAction(tr("Удалить все"), canvas_, &Canvas::removeAllGuides);
    removeAllGuidesAction_->setObjectName("removeAllGuidesAction");
    viewMenu_->addSeparator();
    showGuidesAction_ = viewMenu_->addAction(tr("Показывать направляющие"));
    showGuidesAction_->setObjectName("showGuidesAction");
    showGuidesAction_->setCheckable(true);
    showGuidesAction_->setChecked(canvas_->guidesVisible());
    connect(showGuidesAction_, &QAction::toggled, this, [this](bool visible) {
        QSettings().setValue("view/guides/visible", visible);
        canvas_->setGuidesVisible(visible);
    });
    snapGuidesAction_ = viewMenu_->addAction(snapIcon(), tr("Прилипание к направляющим"));
    snapGuidesAction_->setObjectName("snapGuidesAction");
    snapGuidesAction_->setCheckable(true);
    snapGuidesAction_->setChecked(canvas_->snapToGuides());
    connect(snapGuidesAction_, &QAction::toggled, this, [this](bool enabled) {
        QSettings().setValue("view/guides/snap", enabled);
        canvas_->setSnapToGuides(enabled);
    });
    newAction->setToolTip(tr("Создать (Ctrl+N)"));
    openAction->setToolTip(tr("Открыть (Ctrl+O)"));
    saveAction->setToolTip(tr("Сохранить проект (Ctrl+S)"));
    undoAction->setToolTip(tr("Отменить (Ctrl+Z)"));
    redoAction->setToolTip(tr("Повторить (Ctrl+Y)"));
    mainToolbar_ = addToolBar(tr("Файл и параметры"));
    mainToolbar_->setObjectName("mainToolbar");
    mainToolbar_->setMovable(false);
    mainToolbar_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    mainToolbar_->addAction(newAction);
    mainToolbar_->addAction(openAction);
    mainToolbar_->addAction(saveAction);
    mainToolbar_->addSeparator();
    mainToolbar_->addAction(undoAction);
    mainToolbar_->addAction(redoAction);
    mainToolbar_->addSeparator();
    mainToolbar_->addAction(snapGuidesAction_);
    auto *frontColorAction = new QAction(tr("Основной цвет (Front)…"), this);
    frontColorAction->setObjectName("frontColorAction");
    auto *backColorAction = new QAction(tr("Фоновый цвет (Back)…"), this);
    backColorAction->setObjectName("backColorAction");
    auto *swap = new QAction(style()->standardIcon(QStyle::SP_BrowserReload), tr("Поменять цвета местами"), this);
    swap->setObjectName("swapColorsAction");
    swap->setToolTip(tr("Поменять цвета местами (X)"));
    swap->setShortcut(QKeySequence("X"));
    connect(frontColorAction, &QAction::triggered, this, [this] {
        const QColor color = QColorDialog::getColor(front_, this, tr("Основной цвет — Front"));
        if (color.isValid()) {
            front_ = color;
            updateColors();
        }
    });
    connect(backColorAction, &QAction::triggered, this, [this] {
        const QColor color = QColorDialog::getColor(back_, this, tr("Цвет фона и ластика — Back"));
        if (color.isValid()) {
            back_ = color;
            updateColors();
        }
    });
    connect(swap, &QAction::triggered, this, [this] {
        qSwap(front_, back_);
        updateColors();
    });
    toolsDock_ = new AutoHideDockWidget(tr("Инструменты"),
                                        Qt::LeftDockWidgetArea,
                                        "tools",
                                        true,
                                        250,
                                        this);
    toolsDock_->setObjectName("toolsDock");
    addDockWidget(Qt::LeftDockWidgetArea, toolsDock_);
    auto *toolsPanel = new QWidget(toolsDock_);
    auto *toolsPanelLayout = new QVBoxLayout(toolsPanel);
    toolsPanelLayout->setContentsMargins(4, 4, 4, 4);
    toolsPanelLayout->setSpacing(4);
    toolsToolbar_ = new QToolBar(tr("Инструменты"), toolsPanel);
    toolsToolbar_->setObjectName("toolsToolbar");
    toolsToolbar_->setMovable(false);
    toolsToolbar_->setFloatable(false);
    toolsToolbar_->setOrientation(Qt::Horizontal);
    toolsToolbar_->setIconSize(QSize(24, 24));
    toolsToolbar_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    toolsPanelLayout->addWidget(toolsToolbar_);
    auto *group = new QActionGroup(this);
    group->setExclusive(true);
    QStringList names{tr("Карандаш"), tr("Кисть"), tr("Ластик"), tr("Перемещение"), tr("Перспектива")};
    QStringList shortcuts{"B", "K", "E", "H", "P"};
    for (int i = 0; i < 5; ++i) {
        auto *action = new QAction(toolIcon(i), names[i], this);
        action->setObjectName(QString("tool%1").arg(i));
        action->setCheckable(true);
        action->setShortcut(QKeySequence(shortcuts[i]));
        action->setToolTip(tr("%1 (%2)").arg(names[i], shortcuts[i]));
        group->addAction(action);
        if (i == int(Canvas::Perspective)) {
            mainToolbar_->addSeparator();
            mainToolbar_->addAction(action);
        } else {
            toolsToolbar_->addAction(action);
        }
        toolsMenu_->addAction(action);
        if (i == 0)
            action->setChecked(true);
        connect(action, &QAction::triggered, this, [this, i, names] { activateTool(Canvas::Tool(i), names[i]); });
        if (i == int(Canvas::Perspective))
            perspectiveAction_ = action;
    }
    connect(canvas_, &Canvas::toolChanged, this, [this, names](Canvas::Tool tool) {
        if (auto *action = findChild<QAction *>(QStringLiteral("tool%1").arg(int(tool))))
            action->setChecked(true);
        toolLabel_->setText(names.value(int(tool)));
        const bool paints = tool <= Canvas::Eraser;
        toolSettings_->setActiveTool(paints ? int(tool) : -1);
        if (toolProperties_)
            toolProperties_->setCanvasTool(int(tool));
        if (paints)
            canvas_->setStrokeSettings(toolSettings_->settingsFor(int(tool)));
    });
    toolsMenu_->addSeparator();
    toolsMenu_->addAction(frontColorAction);
    toolsMenu_->addAction(backColorAction);
    toolsMenu_->addAction(swap);

    toolProperties_ = new ToolPropertiesPanel(toolSettings_, toolsPanel);
    toolsPanelLayout->addWidget(toolProperties_, 1);
    toolsDock_->setPanelWidget(toolsPanel);
    toolsDock_->attachEdgeTab();
    connect(toolProperties_, &ToolPropertiesPanel::frontColorRequested, frontColorAction, &QAction::trigger);
    connect(toolProperties_, &ToolPropertiesPanel::backColorRequested, backColorAction, &QAction::trigger);
    connect(toolProperties_, &ToolPropertiesPanel::swapColorsRequested, swap, &QAction::trigger);
    toolProperties_->setCanvasTool(int(canvas_->tool()));
    toolProperties_->setMoveTarget(int(canvas_->moveTarget()));
    connect(toolProperties_, &ToolPropertiesPanel::moveTargetRequested, this, [this](int target) {
        QSettings().setValue("tools/moveTarget", target);
        canvas_->setMoveTarget(Canvas::MoveTarget(target));
    });
    connect(canvas_, &Canvas::moveTargetChanged, this, [this](Canvas::MoveTarget target) {
        QSettings().setValue("tools/moveTarget", int(target));
        toolProperties_->setMoveTarget(int(target));
    });
    connect(toolSettings_, &DrawingToolSettingsModel::settingsChanged, this, [this] {
        if (toolSettings_->activeTool() >= 0)
            canvas_->setStrokeSettings(toolSettings_->settingsFor(toolSettings_->activeTool()));
    });
    canvas_->setStrokeSettings(toolSettings_->settingsFor(DrawingToolSettingsModel::Pencil));
    updateColors();
}

void MainWindow::setupPerspectivePanel() {
    perspectiveDock_ = new AutoHideDockWidget(tr("Перспектива"),
                                              Qt::RightDockWidgetArea,
                                              "perspective",
                                              false,
                                              330,
                                              this);
    perspectiveDock_->setObjectName("perspectiveDock");
    auto *panel = new QWidget;
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(14, 14, 14, 14);
    gridVisible_ = new QCheckBox(tr("Показать направляющие"));
    gridVisible_->setObjectName("gridVisible");
    layout->addWidget(gridVisible_);
    axesVisible_ = new QCheckBox(tr("Показать координатные оси"));
    axesVisible_->setObjectName("axesVisible");
    layout->addWidget(axesVisible_);
    markersVisible_ = new QCheckBox(tr("Показать управляющие маркеры"));
    markersVisible_->setObjectName("markersVisible");
    layout->addWidget(markersVisible_);
    auto *commonGroup = new RolloutSection(tr("Настройки точек схода"), "vanishingPointSettings");
    auto *commonForm = new QFormLayout(commonGroup->contentWidget());
    rayStep_ = new QDoubleSpinBox;
    rayStep_->setObjectName("rayStep");
    rayStep_->setRange(1, 30);
    rayStep_->setDecimals(1);
    rayStep_->setSingleStep(1);
    rayStep_->setSuffix(tr("°"));
    rayStep_->setKeyboardTracking(false);
    commonForm->addRow(tr("Угловой шаг"), rayStep_);
    rayAngleOffset_ = new QDoubleSpinBox;
    rayAngleOffset_->setObjectName("rayAngleOffset");
    rayAngleOffset_->setRange(-180, 180);
    rayAngleOffset_->setDecimals(1);
    rayAngleOffset_->setSuffix(tr("°"));
    rayAngleOffset_->setKeyboardTracking(false);
    commonForm->addRow(tr("Смещение первого луча"), rayAngleOffset_);
    rayPattern_ = new QComboBox;
    rayPattern_->setObjectName("rayPattern");
    rayPattern_->addItems({tr("Сплошная"), tr("Пунктир"), tr("Точки"), tr("Штрих-пунктир")});
    commonForm->addRow(tr("Шаблон линии"), rayPattern_);
    rayWidth_ = new QDoubleSpinBox;
    rayWidth_->setObjectName("rayWidth");
    rayWidth_->setRange(0.1, 20);
    rayWidth_->setDecimals(1);
    rayWidth_->setSingleStep(0.5);
    rayWidth_->setSuffix(tr(" px"));
    rayWidth_->setKeyboardTracking(false);
    commonForm->addRow(tr("Толщина лучей"), rayWidth_);
    rayGap_ = new QSpinBox;
    rayGap_->setObjectName("rayGap");
    rayGap_->setRange(0, 200);
    rayGap_->setSuffix(tr(" px"));
    rayGap_->setKeyboardTracking(false);
    commonForm->addRow(tr("Отступ от точки"), rayGap_);
    rayStartOpacity_ = new QSpinBox;
    rayStartOpacity_->setObjectName("rayStartOpacity");
    rayStartOpacity_->setRange(0, 100);
    rayStartOpacity_->setSuffix(tr(" %"));
    rayStartOpacity_->setKeyboardTracking(false);
    commonForm->addRow(tr("Непрозрачность у точки"), rayStartOpacity_);
    rayEndOpacity_ = new QSpinBox;
    rayEndOpacity_->setObjectName("rayEndOpacity");
    rayEndOpacity_->setRange(0, 100);
    rayEndOpacity_->setSuffix(tr(" %"));
    rayEndOpacity_->setKeyboardTracking(false);
    commonForm->addRow(tr("Итоговая непрозрачность"), rayEndOpacity_);
    rayFadeLength_ = new QSpinBox;
    rayFadeLength_->setObjectName("rayFadeLength");
    rayFadeLength_->setRange(0, 500);
    rayFadeLength_->setSuffix(tr(" px"));
    rayFadeLength_->setKeyboardTracking(false);
    commonForm->addRow(tr("Длина нарастания"), rayFadeLength_);
    auto *defaultButtons = new QHBoxLayout;
    savePerspectiveDefaultsButton_ = new QPushButton(tr("Сохранить"));
    savePerspectiveDefaultsButton_->setObjectName("savePerspectiveDefaults");
    savePerspectiveDefaultsButton_->setToolTip(tr("Сохранить пять числовых параметров и шаблон линии"));
    resetPerspectiveDefaultsButton_ = new QPushButton(tr("Сбросить"));
    resetPerspectiveDefaultsButton_->setObjectName("resetPerspectiveDefaults");
    resetPerspectiveDefaultsButton_->setToolTip(tr("Вернуть заводские значения"));
    defaultButtons->addWidget(savePerspectiveDefaultsButton_);
    defaultButtons->addWidget(resetPerspectiveDefaultsButton_);
    commonForm->addRow(defaultButtons);
    auto *horizonGroup = new RolloutSection(tr("Линия горизонта"), "horizonSettings");
    auto *horizonForm = new QFormLayout(horizonGroup->contentWidget());
    horizonVisible_ = new QCheckBox(tr("Показывать линию горизонта"));
    horizonVisible_->setObjectName("horizonVisible");
    horizonForm->addRow(horizonVisible_);
    horizonLocked_ = new QCheckBox(tr("Фиксировать"));
    horizonLocked_->setObjectName("horizonLocked");
    horizonForm->addRow(horizonLocked_);
    horizonSymmetry_ = new QCheckBox(tr("Симметрия"));
    horizonSymmetry_->setObjectName("horizonSymmetry");
    horizonSymmetry_->setToolTip(
        tr("Зеркально перемещать ближайшую парную точку на горизонте относительно пересечения осей"));
    horizonForm->addRow(horizonSymmetry_);
    horizonPosition_ = new QDoubleSpinBox;
    horizonPosition_->setObjectName("horizonPosition");
    horizonPosition_->setRange(-1000000, 1000000);
    horizonPosition_->setDecimals(2);
    horizonPosition_->setKeyboardTracking(false);
    horizonForm->addRow(tr("Положение Y"), horizonPosition_);
    horizonUnits_ = new QComboBox;
    horizonUnits_->setObjectName("horizonUnits");
    horizonUnits_->addItems({tr("Проценты"), tr("Пиксели")});
    horizonForm->addRow(tr("Единицы"), horizonUnits_);
    horizonColorButton_ = new QPushButton(tr("Выбрать…"));
    horizonColorButton_->setObjectName("horizonColor");
    horizonForm->addRow(tr("Цвет"), horizonColorButton_);
    horizonOpacity_ = new QSpinBox;
    horizonOpacity_->setObjectName("horizonOpacity");
    horizonOpacity_->setRange(0, 100);
    horizonOpacity_->setSuffix(tr(" %"));
    horizonOpacity_->setKeyboardTracking(false);
    horizonForm->addRow(tr("Непрозрачность"), horizonOpacity_);
    horizonWidth_ = new QDoubleSpinBox;
    horizonWidth_->setObjectName("horizonWidth");
    horizonWidth_->setRange(0.1, 20);
    horizonWidth_->setDecimals(1);
    horizonWidth_->setSingleStep(0.5);
    horizonWidth_->setSuffix(tr(" px"));
    horizonWidth_->setKeyboardTracking(false);
    horizonForm->addRow(tr("Ширина"), horizonWidth_);
    layout->addWidget(horizonGroup);
    auto *verticalGroup = new RolloutSection(tr("Главная вертикаль"), "mainVerticalSettings");
    auto *verticalForm = new QFormLayout(verticalGroup->contentWidget());
    verticalVisible_ = new QCheckBox(tr("Показывать главную вертикаль"));
    verticalVisible_->setObjectName("verticalVisible");
    verticalForm->addRow(verticalVisible_);
    verticalLocked_ = new QCheckBox(tr("Фиксировать"));
    verticalLocked_->setObjectName("verticalLocked");
    verticalForm->addRow(verticalLocked_);
    verticalSymmetry_ = new QCheckBox(tr("Симметрия"));
    verticalSymmetry_->setObjectName("verticalSymmetry");
    verticalSymmetry_->setToolTip(
        tr("Зеркально перемещать ближайшую парную точку на главной вертикали относительно пересечения осей"));
    verticalForm->addRow(verticalSymmetry_);
    verticalPosition_ = new QDoubleSpinBox;
    verticalPosition_->setObjectName("verticalPosition");
    verticalPosition_->setRange(-1000000, 1000000);
    verticalPosition_->setDecimals(2);
    verticalPosition_->setKeyboardTracking(false);
    verticalForm->addRow(tr("Положение X"), verticalPosition_);
    verticalUnits_ = new QComboBox;
    verticalUnits_->setObjectName("verticalUnits");
    verticalUnits_->addItems({tr("Проценты"), tr("Пиксели")});
    verticalForm->addRow(tr("Единицы"), verticalUnits_);
    verticalColorButton_ = new QPushButton(tr("Выбрать…"));
    verticalColorButton_->setObjectName("verticalColor");
    verticalForm->addRow(tr("Цвет"), verticalColorButton_);
    verticalOpacity_ = new QSpinBox;
    verticalOpacity_->setObjectName("verticalOpacity");
    verticalOpacity_->setRange(0, 100);
    verticalOpacity_->setSuffix(tr(" %"));
    verticalOpacity_->setKeyboardTracking(false);
    verticalForm->addRow(tr("Непрозрачность"), verticalOpacity_);
    verticalWidth_ = new QDoubleSpinBox;
    verticalWidth_->setObjectName("verticalWidth");
    verticalWidth_->setRange(0.1, 20);
    verticalWidth_->setDecimals(1);
    verticalWidth_->setSingleStep(0.5);
    verticalWidth_->setSuffix(tr(" px"));
    verticalWidth_->setKeyboardTracking(false);
    verticalForm->addRow(tr("Ширина"), verticalWidth_);
    layout->addWidget(verticalGroup);
    layout->addWidget(commonGroup);
    auto *pointsListGroup = new RolloutSection(tr("Точки схода"), "vanishingPointsList");
    auto *pointsListLayout = new QVBoxLayout(pointsListGroup->contentWidget());
    vanishingPointsList_ = new QListWidget;
    vanishingPointsList_->setObjectName("vanishingPointsListControl");
    vanishingPointsList_->setSelectionMode(QAbstractItemView::SingleSelection);
    vanishingPointsList_->setSpacing(3);
    pointsListLayout->addWidget(vanishingPointsList_);
    auto *pointButtons = new QHBoxLayout;
    addPointButton_ = new QPushButton(tr("Добавить"));
    addPointButton_->setObjectName("addVanishingPoint");
    removePointButton_ = new QPushButton(tr("Удалить"));
    removePointButton_->setObjectName("removeVanishingPoint");
    pointButtons->addWidget(addPointButton_);
    pointButtons->addWidget(removePointButton_);
    pointsListLayout->addLayout(pointButtons);
    auto *pointGroup = new RolloutSection(tr("Свойства выбранной точки"), "selectedVanishingPointSettings");
    auto *pointForm = new QFormLayout(pointGroup->contentWidget());
    pointX_ = new QDoubleSpinBox;
    pointX_->setObjectName("vanishingPointX");
    pointX_->setRange(-1000000, 1000000);
    pointX_->setDecimals(2);
    pointX_->setKeyboardTracking(false);
    pointForm->addRow(tr("X"), pointX_);
    pointY_ = new QDoubleSpinBox;
    pointY_->setObjectName("vanishingPointY");
    pointY_->setRange(-1000000, 1000000);
    pointY_->setDecimals(2);
    pointY_->setKeyboardTracking(false);
    pointForm->addRow(tr("Y"), pointY_);
    pointUnits_ = new QComboBox;
    pointUnits_->setObjectName("vanishingPointUnits");
    pointUnits_->addItems({tr("Проценты"), tr("Пиксели")});
    pointForm->addRow(tr("Единицы"), pointUnits_);
    pointAttachment_ = new QComboBox;
    pointAttachment_->setObjectName("vanishingPointAttachment");
    pointAttachment_->addItems({tr("Свободна"),
                                tr("Прикреплена к горизонту"),
                                tr("Прикреплена к главной вертикали"),
                                tr("Прикреплена к пересечению осей")});
    pointForm->addRow(tr("Состояние"), pointAttachment_);
    selectedPointVisible_ = new QCheckBox(tr("Показывать семейство линий"));
    selectedPointVisible_->setObjectName("selectedPointVisible");
    pointForm->addRow(selectedPointVisible_);
    selectedPointLocked_ = new QCheckBox(tr("Фиксировать"));
    selectedPointLocked_->setObjectName("selectedPointLocked");
    pointForm->addRow(selectedPointLocked_);
    gridColorButton_ = new QPushButton(tr("Выбрать…"));
    gridColorButton_->setObjectName("gridColor");
    pointForm->addRow(tr("Цвет лучей"), gridColorButton_);
    pointsListLayout->addWidget(pointGroup);
    layout->addWidget(pointsListGroup);
    auto *tip = new QLabel(
        tr("P — перемещение точек, горизонта и главной вертикали.\nТочка прилипает к ближайшей оси, но может быть "
           "свободно снята.\nB — вернуться к карандашу.\n\nНаправляющие не попадают\nв экспорт PNG."));
    tip->setWordWrap(true);
    layout->addSpacing(12);
    layout->addWidget(tip);
    layout->addStretch();
    auto *panelScroll = new QScrollArea;
    panelScroll->setObjectName("perspectiveScroll");
    panelScroll->setWidgetResizable(true);
    panelScroll->setFrameShape(QFrame::NoFrame);
    panelScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    panelScroll->setWidget(panel);
    perspectiveDock_->setPanelWidget(panelScroll);
    addDockWidget(Qt::RightDockWidgetArea, perspectiveDock_);
}

void MainWindow::setupLayersPanel() {
    layersDock_ = new AutoHideDockWidget(tr("Слои"), Qt::RightDockWidgetArea, "layers", true, 330, this, 1);
    layersDock_->setObjectName("layersDock");
    layerPanel_ = new LayerPanel(canvas_, layersDock_);
    layersDock_->setPanelWidget(layerPanel_);
    addDockWidget(Qt::RightDockWidgetArea, layersDock_);
    splitDockWidget(perspectiveDock_, layersDock_, Qt::Vertical);
    resizeDocks({perspectiveDock_, layersDock_}, {440, 260}, Qt::Vertical);
    perspectiveDock_->attachEdgeTab();
    layersDock_->attachEdgeTab();
}

void MainWindow::connectPerspectiveControls() {
    auto *mainToolbarToggle = mainToolbar_->toggleViewAction();
    mainToolbarToggle->setObjectName("mainToolbarToggle");
    mainToolbarToggle->setText(tr("Панель команд"));
    viewMenu_->addAction(mainToolbarToggle);
    auto *toolsToolbarToggle = toolsDock_->visibilityAction();
    toolsToolbarToggle->setObjectName("toolsToolbarToggle");
    toolsToolbarToggle->setText(tr("Панель инструментов"));
    viewMenu_->addAction(toolsToolbarToggle);
    auto *perspectivePanelToggle = perspectiveDock_->visibilityAction();
    perspectivePanelToggle->setObjectName("perspectivePanelToggle");
    perspectivePanelToggle->setText(tr("Панель перспективы"));
    viewMenu_->addAction(perspectivePanelToggle);
    auto *layersPanelToggle = layersDock_->visibilityAction();
    layersPanelToggle->setObjectName("layersPanelToggle");
    layersPanelToggle->setText(tr("Панель слоёв"));
    viewMenu_->addAction(layersPanelToggle);
    viewMenu_->addSeparator();
    connect(gridVisible_, &QCheckBox::toggled, canvas_, &Canvas::setGridVisible);
    connect(axesVisible_, &QCheckBox::toggled, this, [this](bool value) {
        QSettings().setValue("perspective/view/axesVisible", value);
        canvas_->setAxesVisible(value);
    });
    connect(markersVisible_, &QCheckBox::toggled, this, [this](bool value) {
        QSettings().setValue("perspective/view/markersVisible", value);
        canvas_->setMarkersVisible(value);
    });
    connect(rayStep_, qOverload<double>(&QDoubleSpinBox::valueChanged), canvas_, &Canvas::setRayStep);
    connect(rayAngleOffset_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double value) {
        QSettings().setValue("perspective/view/rayAngleOffset", value);
        canvas_->setRayAngleOffset(value);
    });
    connect(rayPattern_, qOverload<int>(&QComboBox::currentIndexChanged), canvas_, &Canvas::setRayPattern);
    connect(rayWidth_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double value) {
        QSettings().setValue("perspective/view/rayWidth", value);
        canvas_->setRayWidth(value);
    });
    connect(rayGap_, qOverload<int>(&QSpinBox::valueChanged), canvas_, &Canvas::setRayGap);
    connect(rayStartOpacity_, qOverload<int>(&QSpinBox::valueChanged), canvas_, &Canvas::setRayStartOpacity);
    connect(rayEndOpacity_, qOverload<int>(&QSpinBox::valueChanged), canvas_, &Canvas::setRayEndOpacity);
    connect(rayFadeLength_, qOverload<int>(&QSpinBox::valueChanged), canvas_, &Canvas::setRayFadeLength);
    connect(savePerspectiveDefaultsButton_, &QPushButton::clicked, this, [this] {
        savePerspectiveDefaults(canvas_->state());
        updateState();
        statusBar()->showMessage(tr("Общие настройки направляющих сохранены"), 3000);
    });
    connect(resetPerspectiveDefaultsButton_, &QPushButton::clicked, this, [this] {
        clearPerspectiveDefaults();
        canvas_->setRayAppearance(10, 12, 10, 70, 50, 0);
        updateState();
        statusBar()->showMessage(tr("Восстановлены настройки направляющих по умолчанию"), 3000);
    });
    connect(horizonColorButton_, &QPushButton::clicked, this, [this] {
        const QColor color = QColorDialog::getColor(canvas_->state().horizonColor, this, tr("Цвет линии горизонта"));
        if (color.isValid()) {
            QSettings().setValue("perspective/view/horizonColor", color.name(QColor::HexArgb));
            canvas_->setHorizonColor(color);
        }
    });
    connect(horizonOpacity_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
        QSettings().setValue("perspective/view/horizonOpacity", value);
        canvas_->setHorizonOpacity(value);
    });
    connect(horizonWidth_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double value) {
        QSettings().setValue("perspective/view/horizonWidth", value);
        canvas_->setHorizonWidth(value);
    });
    connect(horizonVisible_, &QCheckBox::toggled, this, [this](bool value) {
        QSettings().setValue("perspective/view/horizonVisible", value);
        canvas_->setHorizonVisible(value);
    });
    connect(horizonLocked_, &QCheckBox::toggled, canvas_, &Canvas::setHorizonLocked);
    connect(horizonSymmetry_, &QCheckBox::toggled, this, [this](bool value) {
        QSettings().setValue("perspective/view/horizonSymmetry", value);
        canvas_->setHorizonSymmetry(value);
    });
    connect(horizonPosition_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double value) {
        canvas_->setHorizonY(imageY(value));
    });
    connect(horizonUnits_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        setCoordinateUnits(index == 0);
    });
    connect(verticalColorButton_, &QPushButton::clicked, this, [this] {
        const QColor color = QColorDialog::getColor(canvas_->state().verticalColor, this, tr("Цвет главной вертикали"));
        if (color.isValid()) {
            QSettings().setValue("perspective/view/verticalColor", color.name(QColor::HexArgb));
            canvas_->setVerticalColor(color);
        }
    });
    connect(verticalOpacity_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
        QSettings().setValue("perspective/view/verticalOpacity", value);
        canvas_->setVerticalOpacity(value);
    });
    connect(verticalWidth_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double value) {
        QSettings().setValue("perspective/view/verticalWidth", value);
        canvas_->setVerticalWidth(value);
    });
    connect(verticalVisible_, &QCheckBox::toggled, this, [this](bool value) {
        QSettings().setValue("perspective/view/verticalVisible", value);
        canvas_->setVerticalVisible(value);
    });
    connect(verticalLocked_, &QCheckBox::toggled, canvas_, &Canvas::setVerticalLocked);
    connect(verticalSymmetry_, &QCheckBox::toggled, this, [this](bool value) {
        QSettings().setValue("perspective/view/verticalSymmetry", value);
        canvas_->setVerticalSymmetry(value);
    });
    connect(verticalPosition_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double value) {
        canvas_->setVerticalX(imageX(value));
    });
    connect(verticalUnits_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        setCoordinateUnits(index == 0);
    });
    connect(vanishingPointsList_, &QListWidget::currentRowChanged, canvas_, &Canvas::selectPoint);
    connect(canvas_, &Canvas::selectedPointChanged, this, [this](int index) {
        QSignalBlocker block(vanishingPointsList_);
        vanishingPointsList_->setCurrentRow(index);
        updateState();
    });
    connect(addPointButton_, &QPushButton::clicked, this, [this] {
        canvas_->addVanishingPoint();
        const int pointIndex = canvas_->selectedPointIndex();
        if (pointIndex < 0)
            return;
        QSettings settings;
        const QColor color(
            settings.value(QString("perspective/points/%1/color").arg(pointIndex), defaultPointColor(pointIndex))
                .toString());
        canvas_->setSelectedPointColor(color.isValid() ? color : defaultPointColor(pointIndex));
        canvas_->setSelectedPointVisible(
            settings.value(QString("perspective/points/%1/visible").arg(pointIndex), true).toBool());
    });
    connect(removePointButton_, &QPushButton::clicked, canvas_, &Canvas::removeSelectedVanishingPoint);
    connect(selectedPointVisible_, &QCheckBox::toggled, this, [this](bool visible) {
        const int pointIndex = canvas_->selectedPointIndex();
        if (pointIndex < 0)
            return;
        QSettings().setValue(QString("perspective/points/%1/visible").arg(pointIndex), visible);
        canvas_->setSelectedPointVisible(visible);
    });
    connect(selectedPointLocked_, &QCheckBox::toggled, canvas_, &Canvas::setSelectedPointLocked);
    connect(pointX_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double value) {
        const int pointIndex = canvas_->selectedPointIndex();
        if (pointIndex < 0)
            return;
        QPointF position = canvas_->state().vanishingPoints[pointIndex].position;
        position.setX(imageX(value));
        canvas_->setSelectedPointPosition(position);
    });
    connect(pointY_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double value) {
        const int pointIndex = canvas_->selectedPointIndex();
        if (pointIndex < 0)
            return;
        QPointF position = canvas_->state().vanishingPoints[pointIndex].position;
        position.setY(imageY(value));
        canvas_->setSelectedPointPosition(position);
    });
    connect(pointUnits_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        setCoordinateUnits(index == 0);
    });
    connect(pointAttachment_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        canvas_->setSelectedPointAttachment(index == 1   ? PerspectiveTarget::horizon()
                                            : index == 2 ? PerspectiveTarget::vertical()
                                            : index == 3 ? PerspectiveTarget::intersection()
                                                         : QString());
    });
    connect(gridColorButton_, &QPushButton::clicked, this, [this] {
        const int pointIndex = canvas_->selectedPointIndex();
        if (pointIndex < 0)
            return;
        const QColor color =
            QColorDialog::getColor(canvas_->state().vanishingPoints[pointIndex].color, this, tr("Цвет направляющих"));
        if (color.isValid()) {
            QSettings().setValue(QString("perspective/points/%1/color").arg(pointIndex), color.name(QColor::HexArgb));
            canvas_->setSelectedPointColor(color);
        }
    });
}

void MainWindow::setupViewAndStatusBar() {
    auto *fitAction = viewMenu_->addAction(style()->standardIcon(QStyle::SP_TitleBarMaxButton),
                                           tr("Вписать холст"),
                                           canvas_,
                                           &Canvas::fit,
                                           QKeySequence("Ctrl+0"));
    fitAction->setToolTip(tr("Вписать холст (Ctrl+0)"));
    auto *actualAction = viewMenu_->addAction(
        actualSizeIcon(), tr("Масштаб 100%"), this, [this] { canvas_->setZoom(1); }, QKeySequence("Ctrl+1"));
    actualAction->setToolTip(tr("Масштаб 100% (Ctrl+1)"));
    viewMenu_->addAction(
        tr("Увеличить"), this, [this] { canvas_->setZoom(canvas_->zoom() * 1.2); }, QKeySequence("Ctrl++"));
    viewMenu_->addAction(
        tr("Уменьшить"), this, [this] { canvas_->setZoom(canvas_->zoom() / 1.2); }, QKeySequence("Ctrl+-"));
    helpMenu_->addAction(tr("Управление"), this, [this] {
        QMessageBox::information(
            this,
            productName() + tr(" — управление"),
            tr("B — карандаш\nK — кисть\nE — ластик (цвет Back)\nH — перемещение холста\nP — точка схода\nX — поменять "
               "Front и Back\n\nShift + щелчок — отрезок от последней точки\nCtrl + Shift — привязка угла по "
               "15°\nКолесо — масштаб под курсором\nСредняя кнопка или Пробел + мышь — перемещение\nCtrl+Z / Ctrl+Y — "
               "отмена / повтор\nCtrl+0 — вписать, Ctrl+1 — 100%\n\nПроект .drw хранит PNG, координаты точек схода, их "
               "фиксацию и горизонт.\nЭкспорт PNG сохраняет только рисунок."));
    });
    auto *aboutAction = helpMenu_->addAction(tr("О программе…"), this, &MainWindow::showAbout);
    aboutAction->setObjectName("aboutAction");
    toolLabel_ = new QLabel(tr("Карандаш"));
    sizeLabel_ = new QLabel;
    positionLabel_ = new QLabel;
    positionLabel_->setObjectName("canvasPosition");
    positionLabel_->setMinimumWidth(125);
    statusBar()->addWidget(toolLabel_);
    statusBar()->addWidget(sizeLabel_);
    statusBar()->addWidget(positionLabel_, 1);
    auto *fitButton = new QToolButton;
    fitButton->setObjectName("fitButton");
    fitButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    fitButton->setDefaultAction(fitAction);
    statusBar()->addPermanentWidget(fitButton);
    auto *actualButton = new QToolButton;
    actualButton->setObjectName("actualSizeButton");
    actualButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    actualButton->setDefaultAction(actualAction);
    statusBar()->addPermanentWidget(actualButton);
    zoom_ = new QDoubleSpinBox;
    zoom_->setObjectName("zoomPercent");
    zoom_->setRange(5, 1600);
    zoom_->setDecimals(0);
    zoom_->setSuffix(tr(" %"));
    zoom_->setKeyboardTracking(false);
    statusBar()->addPermanentWidget(zoom_);
    auto *collapsePanels = new QToolButton;
    collapsePanels->setObjectName("collapsePanelsButton");
    collapsePanels->setIcon(style()->standardIcon(QStyle::SP_TitleBarUnshadeButton));
    collapsePanels->setToolButtonStyle(Qt::ToolButtonIconOnly);
    collapsePanels->setToolTip(tr("Открепить все открытые панели и вписать холст"));
    statusBar()->addPermanentWidget(collapsePanels);
    connect(collapsePanels, &QToolButton::clicked, this, [this] {
        toolsDock_->collapseToTab();
        perspectiveDock_->collapseToTab();
        layersDock_->collapseToTab();
        QTimer::singleShot(0, canvas_, &Canvas::fit);
    });
    connect(zoom_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double value) {
        canvas_->setZoom(value / 100.0);
    });
    connect(canvas_, &Canvas::viewChanged, this, [this] {
        QSignalBlocker block(zoom_);
        zoom_->setValue(canvas_->zoom() * 100);
    });
    connect(canvas_, &Canvas::positionChanged, this, [this](QPointF position) {
        const double xPixels = position.x() - canvas_->state().canvasSize.width() / 2.0,
                     yPixels = canvas_->state().canvasSize.height() / 2.0 - position.y();
        const double x = rulerPercent_ ? xPixels * 100.0 / canvas_->state().canvasSize.width() : xPixels,
                     y = rulerPercent_ ? yPixels * 100.0 / canvas_->state().canvasSize.height() : yPixels;
        const QString suffix = rulerPercent_ ? tr("%") : tr(" px");
        positionLabel_->setText(tr("X: %1%3   Y: %2%3")
                                    .arg(x, 0, 'f', rulerPercent_ ? 1 : 0)
                                    .arg(y, 0, 'f', rulerPercent_ ? 1 : 0)
                                    .arg(suffix));
    });
    connect(canvas_, &Canvas::stateChanged, this, &MainWindow::updateState);
    connect(canvas_, &Canvas::selectedGuideChanged, this, &MainWindow::updateState);
    connect(canvas_->undoStack(), &QUndoStack::cleanChanged, this, &MainWindow::updateState);
    updateState();
    QTimer::singleShot(0, canvas_, &Canvas::fit);
}

void MainWindow::updateColors() {
    toolProperties_->setColors(front_, back_);
    canvas_->setFront(front_);
    canvas_->setBack(back_);
}
void MainWindow::newGuide(GuideType type) {
    const bool horizontal = type == GuideType::Horizontal;
    const double dimension = horizontal ? canvas_->state().canvasSize.height() : canvas_->state().canvasSize.width();
    QDialog dialog(this);
    dialog.setWindowTitle(horizontal ? tr("Новая горизонтальная направляющая")
                                     : tr("Новая вертикальная направляющая"));
    auto *layout = new QFormLayout(&dialog);
    auto *position = new QDoubleSpinBox(&dialog);
    position->setObjectName("guidePosition");
    position->setDecimals(2);
    position->setRange(0, dimension);
    position->setValue(dimension / 2.0);
    position->setSuffix(tr(" px"));
    auto *units = new QComboBox(&dialog);
    units->setObjectName("guideUnits");
    units->addItems({tr("Пиксели"), tr("Проценты")});
    layout->addRow(horizontal ? tr("От верхнего края") : tr("От левого края"), position);
    layout->addRow(tr("Единицы"), units);
    int previousUnits = 0;
    connect(units, qOverload<int>(&QComboBox::currentIndexChanged), &dialog, [=, &previousUnits](int index) {
        const double pixels = previousUnits == 0 ? position->value() : position->value() * dimension / 100.0;
        QSignalBlocker blocker(position);
        position->setRange(0, index == 0 ? dimension : 100.0);
        position->setSuffix(index == 0 ? tr(" px") : tr(" %"));
        position->setValue(index == 0 ? pixels : pixels * 100.0 / dimension);
        previousUnits = index;
    });
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addRow(buttons);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const double pixels = units->currentIndex() == 0 ? position->value() : position->value() * dimension / 100.0;
    canvas_->addGuide(type, pixels);
}
void MainWindow::activateTool(Canvas::Tool tool, const QString &name) {
    canvas_->setTool(tool);
    toolLabel_->setText(name);
    const bool paints = tool <= Canvas::Eraser;
    toolSettings_->setActiveTool(paints ? int(tool) : -1);
    if (paints)
        canvas_->setStrokeSettings(toolSettings_->settingsFor(int(tool)));
    if (tool == Canvas::Perspective) {
        perspectiveDock_->reveal();
        canvas_->setGridVisible(true);
    }
    canvas_->setFocus();
}
void MainWindow::addRecentFile(const QString &path) {
    const QString absolute = QFileInfo(path).absoluteFilePath();
    for (int i = recentFiles_.size() - 1; i >= 0; --i)
        if (QString::compare(recentFiles_[i], absolute, Qt::CaseInsensitive) == 0)
            recentFiles_.removeAt(i);
    recentFiles_.prepend(absolute);
    while (recentFiles_.size() > 5)
        recentFiles_.removeLast();
    QSettings().setValue("files/recentFiles", recentFiles_);
    updateRecentFilesMenu();
}
void MainWindow::updateRecentFilesMenu() {
    recentFilesMenu_->clear();
    if (recentFiles_.isEmpty()) {
        auto *empty = recentFilesMenu_->addAction(tr("Нет недавних файлов"));
        empty->setEnabled(false);
        return;
    }
    for (int i = 0; i < recentFiles_.size(); ++i) {
        const QString path = recentFiles_[i];
        QString label = path;
        label.replace("&", "&&");
        auto *action = recentFilesMenu_->addAction(tr("&%1  %2").arg(i + 1).arg(label));
        action->setObjectName(QString("recentFile%1").arg(i));
        action->setData(path);
        action->setToolTip(path);
        connect(action, &QAction::triggered, this, [this, path] { openPath(path); });
    }
}
int MainWindow::vanishingPointIndex(const QString &id) const {
    const auto &points = canvas_->state().vanishingPoints;
    for (int index = 0; index < points.size(); ++index)
        if (points[index].id == id)
            return index;
    return -1;
}
void MainWindow::updateState() {
    QString name = path_.isEmpty() ? tr("Без имени.drw") : QFileInfo(path_).fileName();
    setWindowTitle(tr("%1[*] — %2").arg(name, productName()));
    setWindowModified(!canvas_->undoStack()->isClean());
    sizeLabel_->setText(
        tr("%1 × %2 px").arg(canvas_->state().canvasSize.width()).arg(canvas_->state().canvasSize.height()));
    DrawingTargetContext target;
    const LayerEntry *activeLayer = canvas_->state().layers.activeEntry();
    const LayerType *activeType = activeLayer ? LayerTypeRegistry::instance().type(activeLayer->typeId) : nullptr;
    const auto *activeRaster = activeLayer && activeLayer->content
                                   ? dynamic_cast<const RasterLayerContent *>(activeLayer->content.get())
                                   : nullptr;
    target.editable = activeLayer && activeType &&
                      activeType->capabilities.testFlag(LayerCapability::RasterPainting) && activeLayer->visible &&
                      !activeLayer->locked;
    target.supportsTransparency =
        activeType && activeType->capabilities.testFlag(LayerCapability::Transparency) && activeRaster &&
        activeRaster->transparencyAvailable;
    target.alphaLocked = !activeRaster || activeRaster->alphaLocked;
    if (!activeLayer)
        target.unavailableReason = tr("Нет активного слоя");
    else if (!activeType || !activeType->capabilities.testFlag(LayerCapability::RasterPainting))
        target.unavailableReason = tr("Активный слой не поддерживает растровое рисование");
    else if (!activeLayer->visible)
        target.unavailableReason = tr("Активный слой скрыт");
    else if (activeLayer->locked)
        target.unavailableReason = tr("Активный слой зафиксирован");
    toolSettings_->setTargetContext(target);
    if (removeSelectedGuideAction_)
        removeSelectedGuideAction_->setEnabled(!canvas_->selectedGuideId().isEmpty());
    if (removeAllGuidesAction_)
        removeAllGuidesAction_->setEnabled(!canvas_->state().guides.isEmpty());
    // Обновление представления модели не должно повторно вызвать обработчики и создать новую команду истории.
    QSignalBlocker a(gridVisible_), b(rayStep_), c(rayGap_), d(rayStartOpacity_), e(rayEndOpacity_), f(rayFadeLength_),
        g(horizonOpacity_), h(horizonWidth_), i(vanishingPointsList_), j(selectedPointVisible_), k(horizonPosition_),
        l(horizonUnits_), m(pointX_), n(pointY_), o(pointUnits_), q(pointAttachment_);
    QSignalBlocker r(rayAngleOffset_), s(rayPattern_), t(rayWidth_), u(horizonVisible_), v(axesVisible_),
        w(markersVisible_), x(horizonSymmetry_), y(horizonLocked_), z(selectedPointLocked_);
    QSignalBlocker aa(verticalWidth_), ab(verticalPosition_), ac(verticalOpacity_), ad(verticalVisible_),
        ae(verticalLocked_), af(verticalUnits_), ag(verticalSymmetry_);
    gridVisible_->setChecked(canvas_->state().gridVisible);
    axesVisible_->setChecked(canvas_->state().axesVisible);
    markersVisible_->setChecked(canvas_->state().markersVisible);
    rayStep_->setValue(canvas_->state().rayStepDegrees);
    rayAngleOffset_->setValue(canvas_->state().rayAngleOffset);
    rayPattern_->setCurrentIndex(canvas_->state().rayPattern);
    rayWidth_->setValue(canvas_->state().rayWidth);
    rayGap_->setValue(canvas_->state().rayGap);
    rayStartOpacity_->setValue(canvas_->state().rayStartOpacity);
    rayEndOpacity_->setValue(canvas_->state().rayEndOpacity);
    rayFadeLength_->setValue(canvas_->state().rayFadeLength);
    savePerspectiveDefaultsButton_->setEnabled(!matchesPerspectiveDefaults(canvas_->state()));
    horizonOpacity_->setValue(canvas_->state().horizonOpacity);
    horizonWidth_->setValue(canvas_->state().horizonWidth);
    horizonVisible_->setChecked(canvas_->state().horizonVisible);
    horizonLocked_->setChecked(canvas_->state().horizonLocked);
    horizonSymmetry_->setChecked(canvas_->state().horizonSymmetry);
    horizonPosition_->setSuffix(coordinatePercent_ ? tr(" %") : tr(" px"));
    horizonPosition_->setValue(displayedY(canvas_->state().horizonY));
    horizonUnits_->setCurrentIndex(coordinatePercent_ ? 0 : 1);
    verticalOpacity_->setValue(canvas_->state().verticalOpacity);
    verticalWidth_->setValue(canvas_->state().verticalWidth);
    verticalVisible_->setChecked(canvas_->state().verticalVisible);
    verticalLocked_->setChecked(canvas_->state().verticalLocked);
    verticalSymmetry_->setChecked(canvas_->state().verticalSymmetry);
    verticalPosition_->setSuffix(coordinatePercent_ ? tr(" %") : tr(" px"));
    verticalPosition_->setValue(displayedX(canvas_->state().verticalX));
    verticalUnits_->setCurrentIndex(coordinatePercent_ ? 0 : 1);
    const auto &points = canvas_->state().vanishingPoints;
    bool rebuild = vanishingPointsList_->count() != points.size();
    if (!rebuild)
        for (int row = 0; row < points.size(); ++row)
            if (vanishingPointsList_->item(row)->data(Qt::UserRole).toString() != points[row].id) {
                rebuild = true;
                break;
            }
    if (rebuild) {
        vanishingPointsList_->clear();
        for (int row = 0; row < points.size(); ++row) {
            const QString id = points[row].id;
            auto *item = new QListWidgetItem;
            item->setData(Qt::UserRole, id);
            vanishingPointsList_->addItem(item);
            auto *card = new QWidget;
            card->setObjectName("vanishingPointCard");
            auto *cardLayout = new QHBoxLayout(card);
            cardLayout->setContentsMargins(7, 3, 4, 3);
            cardLayout->setSpacing(4);
            auto *number = new QLabel;
            number->setObjectName("vanishingPointNumber");
            number->setAlignment(Qt::AlignCenter);
            number->setMinimumWidth(22);
            cardLayout->addWidget(number);
            auto *name = new QLineEdit;
            name->setObjectName("vanishingPointName");
            name->setMaxLength(120);
            name->setProperty("vanishingPointId", id);
            cardLayout->addWidget(name, 1);
            auto *visibility = new QToolButton;
            visibility->setObjectName("vanishingPointVisibility");
            visibility->setCheckable(true);
            visibility->setAutoRaise(true);
            visibility->setIconSize(QSize(20, 20));
            visibility->setFixedSize(30, 30);
            cardLayout->addWidget(visibility);
            auto *lock = new QToolButton;
            lock->setObjectName("vanishingPointLock");
            lock->setCheckable(true);
            lock->setAutoRaise(true);
            lock->setIconSize(QSize(20, 20));
            lock->setFixedSize(30, 30);
            cardLayout->addWidget(lock);
            item->setSizeHint(QSize(card->sizeHint().width(), 36));
            vanishingPointsList_->setItemWidget(item, card);
            connect(name, &QLineEdit::textEdited, this, [this, id](const QString &) {
                const int index = vanishingPointIndex(id);
                if (index >= 0) {
                    vanishingPointsList_->setCurrentRow(index);
                    canvas_->selectPoint(index);
                }
            });
            connect(name, &QLineEdit::editingFinished, this, [this, name, id] {
                const int index = vanishingPointIndex(id);
                if (index < 0)
                    return;
                vanishingPointsList_->setCurrentRow(index);
                canvas_->selectPoint(index);
                canvas_->setSelectedPointName(name->text());
                name->setModified(false);
            });
            connect(visibility, &QToolButton::toggled, this, [this, id](bool visible) {
                const int index = vanishingPointIndex(id);
                if (index < 0)
                    return;
                vanishingPointsList_->setCurrentRow(index);
                canvas_->selectPoint(index);
                QSettings().setValue(QString("perspective/points/%1/visible").arg(index), visible);
                canvas_->setSelectedPointVisible(visible);
            });
            connect(lock, &QToolButton::toggled, this, [this, id](bool locked) {
                const int index = vanishingPointIndex(id);
                if (index < 0)
                    return;
                vanishingPointsList_->setCurrentRow(index);
                canvas_->selectPoint(index);
                canvas_->setSelectedPointLocked(locked);
            });
        }
    }
    const int selected = canvas_->selectedPointIndex();
    vanishingPointsList_->setCurrentRow(selected);
    const bool hasPoint = selected >= 0 && selected < points.size();
    for (int row = 0; row < points.size(); ++row) {
        auto *card = vanishingPointsList_->itemWidget(vanishingPointsList_->item(row));
        if (!card)
            continue;
        card->setProperty("selected", row == selected);
        card->style()->unpolish(card);
        card->style()->polish(card);
        auto *number = card->findChild<QLabel *>("vanishingPointNumber");
        auto *name = card->findChild<QLineEdit *>("vanishingPointName");
        auto *visibility = card->findChild<QToolButton *>("vanishingPointVisibility");
        auto *lock = card->findChild<QToolButton *>("vanishingPointLock");
        if (number)
            number->setText(QString::number(row + 1));
        if (name && !name->isModified()) {
            QSignalBlocker blocker(name);
            name->setText(points[row].name);
            name->setPlaceholderText(tr("Точка схода %1").arg(row + 1));
        }
        if (visibility) {
            QSignalBlocker blocker(visibility);
            visibility->setChecked(points[row].visible);
            visibility->setIcon(eyeIcon(points[row].visible));
            visibility->setToolTip(points[row].visible ? tr("Скрыть семейство линий") : tr("Показать семейство линий"));
        }
        if (lock) {
            QSignalBlocker blocker(lock);
            lock->setChecked(points[row].locked);
            lock->setIcon(lockIcon(points[row].locked));
            lock->setToolTip(points[row].locked ? tr("Снять фиксацию") : tr("Фиксировать"));
        }
    }
    removePointButton_->setEnabled(hasPoint);
    selectedPointVisible_->setEnabled(hasPoint);
    selectedPointLocked_->setEnabled(hasPoint);
    gridColorButton_->setEnabled(hasPoint);
    pointX_->setEnabled(hasPoint);
    pointY_->setEnabled(hasPoint);
    pointUnits_->setEnabled(hasPoint);
    pointAttachment_->setEnabled(hasPoint);
    pointX_->setSuffix(coordinatePercent_ ? tr(" %") : tr(" px"));
    pointY_->setSuffix(coordinatePercent_ ? tr(" %") : tr(" px"));
    pointUnits_->setCurrentIndex(coordinatePercent_ ? 0 : 1);
    if (hasPoint) {
        pointX_->setValue(displayedX(points[selected].position.x()));
        pointY_->setValue(displayedY(points[selected].position.y()));
        const bool onHorizon = points[selected].isAttachedTo(PerspectiveTarget::horizon()),
                   onVertical = points[selected].isAttachedTo(PerspectiveTarget::vertical());
        pointX_->setEnabled(!onVertical);
        pointY_->setEnabled(!onHorizon);
        pointAttachment_->setCurrentIndex(onHorizon && onVertical ? 3 : onHorizon ? 1 : onVertical ? 2 : 0);
        selectedPointLocked_->setChecked(points[selected].locked);
    } else {
        pointX_->setValue(0);
        pointY_->setValue(0);
        pointAttachment_->setCurrentIndex(0);
        selectedPointLocked_->setChecked(false);
    }
    selectedPointVisible_->setChecked(hasPoint && points[selected].visible);
    colorSwatch(gridColorButton_, hasPoint ? points[selected].color : QColor("#d0d0d0"));
    colorSwatch(horizonColorButton_, canvas_->state().horizonColor);
    colorSwatch(verticalColorButton_, canvas_->state().verticalColor);
}
double MainWindow::displayedX(double imageCoordinate) const {
    const double pixels = imageCoordinate - canvas_->state().canvasSize.width() / 2.0;
    return coordinatePercent_ ? pixels * 100.0 / canvas_->state().canvasSize.width() : pixels;
}
double MainWindow::displayedY(double imageCoordinate) const {
    const double pixels = canvas_->state().canvasSize.height() / 2.0 - imageCoordinate;
    return coordinatePercent_ ? pixels * 100.0 / canvas_->state().canvasSize.height() : pixels;
}
double MainWindow::imageX(double displayed) const {
    return canvas_->state().canvasSize.width() / 2.0 +
           (coordinatePercent_ ? displayed * canvas_->state().canvasSize.width() / 100.0 : displayed);
}
double MainWindow::imageY(double displayed) const {
    return canvas_->state().canvasSize.height() / 2.0 -
           (coordinatePercent_ ? displayed * canvas_->state().canvasSize.height() / 100.0 : displayed);
}
void MainWindow::setCoordinateUnits(bool percent) {
    if (coordinatePercent_ == percent) {
        updateState();
        return;
    }
    coordinatePercent_ = percent;
    QSettings().setValue("perspective/coordinatePercent", percent);
    updateState();
}
void MainWindow::showSettings() {
    QDialog dialog(this);
    dialog.setObjectName("settingsDialog");
    dialog.setWindowTitle(tr("Настройки — ") + productName());
    dialog.resize(460, 300);
    auto *dialogLayout = new QVBoxLayout(&dialog);
    auto *tabs = new QTabWidget;
    tabs->setObjectName("settingsTabs");
    dialogLayout->addWidget(tabs);
    auto *viewPage = new QWidget;
    auto *viewForm = new QFormLayout(viewPage);
    auto *rulerUnits = new QComboBox;
    rulerUnits->setObjectName("rulerUnits");
    rulerUnits->addItems({tr("Пиксели"), tr("Проценты")});
    rulerUnits->setCurrentIndex(rulerPercent_ ? 1 : 0);
    viewForm->addRow(tr("Единицы линеек"), rulerUnits);
    viewForm->addRow(
        new QLabel(tr("Единицы числовых координат точек схода и горизонта выбираются отдельно в панели перспективы.")));
    tabs->addTab(viewPage, tr("Вид"));
    auto *systemPage = new QWidget;
    auto *systemLayout = new QVBoxLayout(systemPage);
    auto *systemHint = new QLabel(tr("Системные параметры будут добавляться по мере необходимости."));
    systemHint->setWordWrap(true);
    systemLayout->addWidget(systemHint);
    systemLayout->addStretch();
    tabs->addTab(systemPage, tr("Система"));
    auto *filesPage = new QWidget;
    auto *filesLayout = new QVBoxLayout(filesPage);
    auto *filesHint =
        new QLabel(tr("Параметры файлов и списка недавних документов будут добавлены в следующих задачах."));
    filesHint->setWordWrap(true);
    filesLayout->addWidget(filesHint);
    filesLayout->addStretch();
    tabs->addTab(filesPage, tr("Файлы"));
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    dialogLayout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted)
        return;
    rulerPercent_ = rulerUnits->currentIndex() == 1;
    QSettings().setValue("view/rulers/percent", rulerPercent_);
    canvas_->setRulerPercent(rulerPercent_);
    positionLabel_->clear();
}
void MainWindow::showAbout() {
    QDialog dialog(this);
    dialog.setObjectName("aboutDialog");
    dialog.setWindowTitle(tr("О программе"));
    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(32, 24, 32, 20);
    layout->setSpacing(14);
    auto *icon = new QLabel;
    icon->setObjectName("aboutIcon");
    icon->setAlignment(Qt::AlignCenter);
    icon->setPixmap(QIcon(":/app/techdraw.png").pixmap(96, 96));
    layout->addWidget(icon);
    auto *name = new QLabel(tr("Технический рисунок\nTechnical Draw"));
    name->setObjectName("aboutName");
    name->setAlignment(Qt::AlignCenter);
    QFont nameFont = name->font();
    nameFont.setPointSize(nameFont.pointSize() + 2);
    nameFont.setBold(true);
    name->setFont(nameFont);
    layout->addWidget(name);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    dialog.exec();
}
void MainWindow::showError(const QString &error) {
    QMessageBox::critical(this, productName(), error);
}
bool MainWindow::confirmDiscard() {
    if (canvas_->undoStack()->isClean())
        return true;
    QMessageBox message(QMessageBox::Question,
                        tr("Несохранённые изменения"),
                        tr("Сохранить изменения перед продолжением?"),
                        QMessageBox::NoButton,
                        this);
    auto *save = message.addButton(tr("Сохранить"), QMessageBox::AcceptRole);
    auto *discard = message.addButton(tr("Не сохранять"), QMessageBox::DestructiveRole);
    auto *cancel = message.addButton(tr("Отмена"), QMessageBox::RejectRole);
    message.setDefaultButton(save);
    message.setEscapeButton(cancel);
    message.exec();
    if (message.clickedButton() == save)
        return saveDocument();
    return message.clickedButton() == discard;
}
void MainWindow::newDocument() {
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Новый холст"));
    auto *layout = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout;
    QSettings settings;
    QSize remembered(settings.value("canvas/newWidth", 1000).toInt(), settings.value("canvas/newHeight", 620).toInt());
    if (!Project::validSize(remembered))
        remembered = QSize(1000, 620);
    QSpinBox w, h;
    w.setObjectName("newCanvasWidth");
    h.setObjectName("newCanvasHeight");
    w.setRange(1, 8192);
    h.setRange(1, 8192);
    w.setValue(remembered.width());
    h.setValue(remembered.height());
    w.setSuffix(tr(" px"));
    h.setSuffix(tr(" px"));
    form->addRow(tr("Ширина"), &w);
    form->addRow(tr("Высота"), &h);
    layout->addLayout(form);
    layout->addWidget(new QLabel(tr("До 16 млн пикселей. Фон — цвет Back.")));
    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons.button(QDialogButtonBox::Ok)->setText(tr("Создать"));
    buttons.button(QDialogButtonBox::Cancel)->setText(tr("Отмена"));
    layout->addWidget(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted)
        return;
    if (!Project::validSize(QSize(w.value(), h.value()))) {
        showError(tr("Максимум 16 млн пикселей."));
        return;
    }
    DrawingState state;
    QImage image(w.value(), h.value(), QImage::Format_ARGB32_Premultiplied);
    if (image.isNull()) {
        showError(tr("Не удалось выделить память для холста."));
        return;
    }
    image.fill(back_);
    state.setSingleRasterImage(image, tr("Фон"));
    state.horizonY = h.value() / 2.0;
    state.verticalX = w.value() / 2.0;
    state.vanishingPoints.append({QStringLiteral("vp-1"),
                                  QPointF(w.value() / 2.0, h.value() / 2.0),
                                  PerspectiveTarget::constructionType(),
                                  PerspectiveTarget::horizon()});
    state.vanishingPoints.last().name = tr("Точка схода 1");
    applySavedPerspectiveDefaults(&state);
    applySavedViewSettings(&state);
    applySavedPointAppearance(&state);
    if (!confirmDiscard())
        return;
    settings.setValue("canvas/newWidth", w.value());
    settings.setValue("canvas/newHeight", h.value());
    path_.clear();
    canvas_->setDocument(state, false);
    canvas_->fit();
}
void MainWindow::openDocument() {
    QString path = QFileDialog::getOpenFileName(
        this,
        tr("Открыть"),
        rememberedDirectory("files/openDirectory"),
        tr("Технический рисунок и PNG (*.drw *.png);;Проект «Технический рисунок» (*.drw);;PNG (*.png)"));
    if (!path.isEmpty())
        openPath(path);
}
bool MainWindow::openPath(const QString &path) {
    DrawingState state;
    DrawingHistory history;
    QString error;
    bool project = path.endsWith(".drw", Qt::CaseInsensitive);
    if (project) {
        if (!Project::load(path, &history, &error)) {
            showError(error);
            return false;
        }
        for (auto &historyState : history.states) {
            applySavedPerspectiveDefaults(&historyState);
            applySavedViewSettings(&historyState);
            applySavedPointAppearance(&historyState);
        }
    } else {
        QImage image;
        if (!Project::loadPng(path, &image, &error)) {
            showError(error);
            return false;
        }
        state.setSingleRasterImage(image, tr("Импорт PNG"), image.hasAlphaChannel(), false);
        state.horizonY = state.canvasSize.height() / 2.0;
        state.verticalX = state.canvasSize.width() / 2.0;
        state.vanishingPoints.append({QStringLiteral("vp-1"),
                                      QPointF(state.canvasSize.width() / 2.0, state.canvasSize.height() / 2.0),
                                      PerspectiveTarget::constructionType(),
                                      PerspectiveTarget::horizon()});
        state.vanishingPoints.last().name = tr("Точка схода 1");
        applySavedPerspectiveDefaults(&state);
        applySavedViewSettings(&state);
        applySavedPointAppearance(&state);
    }
    if (!confirmDiscard())
        return false;
    path_ = project ? QFileInfo(path).absoluteFilePath() : QString();
    if (project)
        canvas_->setDocument(history, true);
    else
        canvas_->setDocument(state, false);
    rememberDirectory("files/openDirectory", path);
    addRecentFile(path);
    canvas_->fit();
    return true;
}
bool MainWindow::saveDocument(bool saveAs) {
    QString target = path_;
    if (saveAs || target.isEmpty()) {
        const QString name = target.isEmpty() ? tr("Без имени.drw") : QFileInfo(target).fileName();
        target = QFileDialog::getSaveFileName(this,
                                              tr("Сохранить проект"),
                                              suggestedFile("files/saveDirectory", name),
                                              tr("Проект «Технический рисунок» (*.drw)"));
        if (target.isEmpty())
            return false;
        const QString suffixed = withSuffix(target, ".drw");
        if (suffixed != target && QFileInfo::exists(suffixed) &&
            QMessageBox::question(this, tr("Заменить файл?"), tr("Файл %1 уже существует. Заменить?").arg(suffixed)) !=
                QMessageBox::Yes)
            return false;
        target = suffixed;
    }
    QString error;
    if (!Project::save(target, canvas_->history(), &error)) {
        showError(error);
        return false;
    }
    path_ = QFileInfo(target).absoluteFilePath();
    rememberDirectory("files/saveDirectory", path_);
    addRecentFile(path_);
    canvas_->undoStack()->setClean();
    updateState();
    statusBar()->showMessage(tr("Проект сохранён"), 3000);
    return true;
}
void MainWindow::exportImage() {
    const QString pngFilter = tr("PNG — без потерь (*.png)"), jpegFilter = tr("JPEG — фотография (*.jpg *.jpeg)"),
                  bmpFilter = tr("BMP — без сжатия (*.bmp)"), filters = pngFilter + ";;" + jpegFilter + ";;" + bmpFilter;
    QSettings settings;
    QString selectedFilter = settings.value("export/filter", pngFilter).toString();
    if (selectedFilter != pngFilter && selectedFilter != jpegFilter && selectedFilter != bmpFilter)
        selectedFilter = pngFilter;
    QString target = QFileDialog::getSaveFileName(this,
                                                  tr("Экспортировать рисунок без направляющих"),
                                                  suggestedFile("files/exportDirectory", tr("Рисунок.png")),
                                                  filters,
                                                  &selectedFilter);
    if (target.isEmpty())
        return;

    QByteArray format;
    QString suffix;
    const QString enteredSuffix = QFileInfo(target).suffix().toLower();
    if (enteredSuffix == "jpg" || enteredSuffix == "jpeg") {
        format = "JPEG";
        suffix = ".jpg";
        selectedFilter = jpegFilter;
    } else if (enteredSuffix == "bmp") {
        format = "BMP";
        suffix = ".bmp";
        selectedFilter = bmpFilter;
    } else if (enteredSuffix == "png") {
        format = "PNG";
        suffix = ".png";
        selectedFilter = pngFilter;
    } else if (selectedFilter == jpegFilter) {
        format = "JPEG";
        suffix = ".jpg";
    } else if (selectedFilter == bmpFilter) {
        format = "BMP";
        suffix = ".bmp";
    } else {
        format = "PNG";
        suffix = ".png";
    }
    if (enteredSuffix != "png" && enteredSuffix != "jpg" && enteredSuffix != "jpeg" && enteredSuffix != "bmp")
        target += suffix;
    if (QFileInfo::exists(target) &&
        QMessageBox::question(this, tr("Заменить файл?"), tr("Заменить существующее изображение?")) !=
            QMessageBox::Yes)
        return;

    int quality = -1;
    if (format == QByteArrayLiteral("JPEG")) {
        bool accepted = false;
        quality = QInputDialog::getInt(this,
                                       tr("Качество JPEG"),
                                       tr("Качество изображения"),
                                       qBound(1, settings.value("export/jpegQuality", 90).toInt(), 100),
                                       1,
                                       100,
                                       1,
                                       &accepted);
        if (!accepted)
            return;
        settings.setValue("export/jpegQuality", quality);
    }
    QString error;
    if (!Project::exportImage(target, canvas_->state().flattenedImage(), format, quality, &error))
        showError(error);
    else {
        rememberDirectory("files/exportDirectory", target);
        settings.setValue("export/filter", selectedFilter);
        statusBar()->showMessage(tr("Изображение экспортировано; проект сохраняется отдельно"), 4000);
    }
}
void MainWindow::closeEvent(QCloseEvent *event) {
    if (confirmDiscard())
        event->accept();
    else
        event->ignore();
}
