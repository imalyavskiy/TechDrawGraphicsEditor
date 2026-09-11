#include "mainwindow.h"
#include <QtWidgets>

namespace {
QString productName(){return QStringLiteral("Технический рисунок / Technical Draw");}
QIcon toolIcon(int kind) {
    QPixmap image(24,24); image.fill(Qt::transparent);
    QPainter p(&image); p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor("#364152"),1.7,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
    if (kind == 0) { p.drawPolygon(QPolygonF(QVector<QPointF>{QPointF(5,15),QPointF(15,5),QPointF(19,9),QPointF(9,19),QPointF(4,20)})); p.drawLine(13,7,17,11); }
    else if (kind == 1) { p.drawLine(6,19,17,8);p.drawLine(14,5,20,11);p.drawLine(17,8,14,5);p.setBrush(QColor("#364152"));p.drawEllipse(QRectF(3,16,7,5)); }
    else if (kind == 2) { p.drawPolygon(QPolygonF(QVector<QPointF>{QPointF(4,14),QPointF(13,5),QPointF(20,12),QPointF(12,20),QPointF(9,20)})); p.drawLine(8,10,16,17); p.drawLine(11,20,21,20); }
    else if (kind == 3) { p.drawRoundedRect(QRectF(7,9,12,12),4,4); p.drawLine(7,14,4,10); p.drawLine(9,10,9,4); p.drawLine(12,9,12,3); p.drawLine(15,10,15,4); p.drawLine(18,11,18,7); }
    else { p.drawEllipse(QPointF(12,10),3,3); p.drawLine(12,1,12,6); p.drawLine(12,14,12,22); p.drawLine(2,10,8,10); p.drawLine(16,10,22,10); p.drawLine(4,22,10,13); p.drawLine(20,22,14,13); }
    return QIcon(image);
}
QIcon actualSizeIcon() {
    QPixmap image(28,20); image.fill(Qt::transparent);
    QPainter p(&image); p.setPen(QColor("#364152"));
    QFont font=p.font();font.setBold(true);font.setPixelSize(11);p.setFont(font);
    p.drawText(image.rect(),Qt::AlignCenter,QStringLiteral("1:1"));
    return QIcon(image);
}
void colorSwatch(QPushButton *button, QColor color) {
    QPixmap swatch(22,22); swatch.fill(color);
    QPainter p(&swatch); p.setPen(QColor("#8e949d")); p.drawRect(0,0,21,21);
    button->setIcon(QIcon(swatch)); button->setIconSize(QSize(22,22));
}
QString withSuffix(QString path, const QString &suffix) {
    if (!path.endsWith(suffix,Qt::CaseInsensitive)) path += suffix;
    return path;
}
QString defaultDirectory() {
    const QString documents=QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return documents.isEmpty()?QDir::homePath():documents;
}
QString rememberedDirectory(const QString &key) {
    const QString path=QSettings().value(key).toString();
    return QDir(path).exists()?path:defaultDirectory();
}
void rememberDirectory(const QString &key,const QString &filePath) {
    QSettings().setValue(key,QFileInfo(filePath).absolutePath());
}
QString suggestedFile(const QString &key,const QString &name) {
    return QDir(rememberedDirectory(key)).filePath(name);
}
QString widthSetting(int tool) {
    static const QStringList keys{"tools/pencilWidth","tools/brushWidth","tools/eraserWidth"};
    return keys[tool];
}
void applySavedPerspectiveDefaults(DrawingState *state) {
    QSettings settings;
    const double step=settings.value("perspective/common/rayStepDegrees",10.0).toDouble();
    const int gap=settings.value("perspective/common/rayGap",12).toInt();
    const int startOpacity=settings.value("perspective/common/rayStartOpacity",10).toInt();
    const int endOpacity=settings.value("perspective/common/rayEndOpacity",70).toInt();
    const int fadeLength=settings.value("perspective/common/rayFadeLength",50).toInt();
    const int pattern=settings.value("perspective/common/rayPattern",0).toInt();
    if (step<1||step>30||gap<0||gap>200||startOpacity<0||startOpacity>100||endOpacity<0||endOpacity>100||fadeLength<0||fadeLength>500||pattern<0||pattern>3) return;
    state->rayStepDegrees=step;state->rayGap=gap;state->rayStartOpacity=startOpacity;state->rayEndOpacity=endOpacity;state->rayFadeLength=fadeLength;state->rayPattern=pattern;
}
void applySavedViewSettings(DrawingState *state){QSettings settings;state->rayWidth=qBound(0.1,settings.value("perspective/view/rayWidth",1.0).toDouble(),20.0);state->rayAngleOffset=qBound(-180.0,settings.value("perspective/view/rayAngleOffset",0.0).toDouble(),180.0);state->horizonVisible=settings.value("perspective/view/horizonVisible",true).toBool();state->axesVisible=settings.value("perspective/view/axesVisible",false).toBool();state->markersVisible=settings.value("perspective/view/markersVisible",true).toBool();state->symmetricPoints=settings.value("perspective/view/symmetricPoints",false).toBool();const QColor horizonColor(settings.value("perspective/view/horizonColor",QStringLiteral("#628ed1")).toString());state->horizonColor=horizonColor.isValid()?horizonColor:QColor("#628ed1");state->horizonOpacity=qBound(0,settings.value("perspective/view/horizonOpacity",70).toInt(),100);state->horizonWidth=qBound(0.1,settings.value("perspective/view/horizonWidth",1.0).toDouble(),20.0);}
QColor defaultPointColor(int index){static const QColor colors[]{QColor("#628ed1"),QColor("#d06b4c"),QColor("#4b9b67"),QColor("#896ac1"),QColor("#c08a34")};return colors[index%5];}
void applySavedPointAppearance(DrawingState *state){QSettings settings;for(int i=0;i<state->vanishingPoints.size();++i){auto &point=state->vanishingPoints[i];const QColor color(settings.value(QString("perspective/points/%1/color").arg(i),defaultPointColor(i)).toString());point.color=color.isValid()?color:defaultPointColor(i);point.visible=settings.value(QString("perspective/points/%1/visible").arg(i),true).toBool();}}
void savePerspectiveDefaults(const DrawingState &state) {
    QSettings settings;settings.setValue("perspective/common/rayStepDegrees",state.rayStepDegrees);settings.setValue("perspective/common/rayGap",state.rayGap);
    settings.setValue("perspective/common/rayStartOpacity",state.rayStartOpacity);settings.setValue("perspective/common/rayEndOpacity",state.rayEndOpacity);settings.setValue("perspective/common/rayFadeLength",state.rayFadeLength);settings.setValue("perspective/common/rayPattern",state.rayPattern);
}
void clearPerspectiveDefaults() { QSettings settings;settings.remove("perspective/common"); }
bool matchesPerspectiveDefaults(const DrawingState &state) {
    DrawingState defaults;applySavedPerspectiveDefaults(&defaults);
    return qFuzzyCompare(state.rayStepDegrees,defaults.rayStepDegrees)&&state.rayGap==defaults.rayGap&&
        state.rayStartOpacity==defaults.rayStartOpacity&&state.rayEndOpacity==defaults.rayEndOpacity&&state.rayFadeLength==defaults.rayFadeLength&&state.rayPattern==defaults.rayPattern;
}
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), canvas_(new Canvas(this)) {
    coordinatePercent_=QSettings().value("perspective/coordinatePercent",true).toBool();
    rulerPercent_=QSettings().value("view/rulers/percent",false).toBool();canvas_->setRulerPercent(rulerPercent_);
    DrawingState initialState=canvas_->state();applySavedPerspectiveDefaults(&initialState);applySavedViewSettings(&initialState);applySavedPointAppearance(&initialState);canvas_->setDocument(initialState,true);
    setObjectName("drawingWindow");
    resize(1200,800); setMinimumSize(720,480);
    setWindowIcon(QIcon(":/app/techdraw.png"));
    setCentralWidget(canvas_);
    setStyleSheet("QToolBar { spacing: 5px; padding: 5px; border: 0; border-bottom: 1px solid #cdd0d5; background: #f6f6f6; } QDockWidget { font-weight: 500; } QStatusBar { background: #f6f6f6; } QToolButton { padding: 5px; } QToolButton:checked { background: #dceaff; border: 1px solid #8aaedb; border-radius: 3px; } ");
    auto *file = menuBar()->addMenu(tr("&Файл"));file->setObjectName("fileMenu");
    auto *edit = menuBar()->addMenu(tr("&Правка"));edit->setObjectName("editMenu");
    auto *view = menuBar()->addMenu(tr("&Вид"));view->setObjectName("viewMenu");
    auto *toolsMenu = menuBar()->addMenu(tr("&Инструменты"));toolsMenu->setObjectName("toolsMenu");
    auto *help = menuBar()->addMenu(tr("&Справка"));
    auto *newAction = file->addAction(style()->standardIcon(QStyle::SP_FileIcon),tr("Создать…"),this,&MainWindow::newDocument,QKeySequence::New);newAction->setObjectName("newAction");
    auto *openAction = file->addAction(style()->standardIcon(QStyle::SP_DirOpenIcon),tr("Открыть…"),this,&MainWindow::openDocument,QKeySequence::Open);openAction->setObjectName("openAction");
    recentFilesMenu_=file->addMenu(tr("Недавние файлы"));recentFilesMenu_->setObjectName("recentFilesMenu");
    recentFiles_=QSettings().value("files/recentFiles").toStringList();while(recentFiles_.size()>5)recentFiles_.removeLast();updateRecentFilesMenu();
    file->addSeparator();
    auto *saveAction = file->addAction(style()->standardIcon(QStyle::SP_DialogSaveButton),tr("Сохранить проект"),this,[this]{saveDocument();},QKeySequence::Save);saveAction->setObjectName("saveAction");
    file->addAction(tr("Сохранить проект как…"),this,[this]{saveDocument(true);},QKeySequence::SaveAs);
    file->addSeparator();
    file->addAction(tr("Экспортировать PNG…"),this,&MainWindow::exportImage,QKeySequence("Ctrl+Shift+E"));
    file->addSeparator(); file->addAction(tr("Выход"),this,&QWidget::close,QKeySequence("Alt+F4"));
    auto *undoAction = canvas_->undoStack()->createUndoAction(this,tr("Отменить"));undoAction->setObjectName("undoAction");undoAction->setShortcut(QKeySequence::Undo); undoAction->setIcon(style()->standardIcon(QStyle::SP_ArrowBack)); edit->addAction(undoAction);
    auto *redoAction = canvas_->undoStack()->createRedoAction(this,tr("Повторить"));redoAction->setObjectName("redoAction");redoAction->setShortcuts({QKeySequence::Redo,QKeySequence("Ctrl+Shift+Z")}); redoAction->setIcon(style()->standardIcon(QStyle::SP_ArrowForward)); edit->addAction(redoAction);
    edit->addSeparator();auto *settingsAction=edit->addAction(tr("Настройки…"),this,&MainWindow::showSettings);settingsAction->setObjectName("settingsAction");
    newAction->setToolTip(tr("Создать (Ctrl+N)"));
    openAction->setToolTip(tr("Открыть (Ctrl+O)"));
    saveAction->setToolTip(tr("Сохранить проект (Ctrl+S)"));
    undoAction->setToolTip(tr("Отменить (Ctrl+Z)"));
    redoAction->setToolTip(tr("Повторить (Ctrl+Y)"));
    auto *bar = addToolBar(tr("Файл и параметры")); bar->setObjectName("mainToolbar"); bar->setMovable(false); bar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    bar->addAction(newAction); bar->addAction(openAction); bar->addAction(saveAction); bar->addSeparator(); bar->addAction(undoAction); bar->addAction(redoAction); bar->addSeparator();
    bar->addWidget(new QLabel(tr(" Ширина "),bar));
    QSettings widthSettings;for(int i=0;i<toolWidths_.size();++i)toolWidths_[i]=qBound(1,widthSettings.value(widthSetting(i),3).toInt(),200);
    strokeWidth_ = new QSpinBox(bar); strokeWidth_->setObjectName("strokeWidth"); strokeWidth_->setRange(1,200); strokeWidth_->setValue(toolWidths_[Canvas::Pencil]); strokeWidth_->setSuffix(tr(" px")); strokeWidth_->setKeyboardTracking(false); bar->addWidget(strokeWidth_);canvas_->setStrokeWidth(strokeWidth_->value());
    connect(strokeWidth_,qOverload<int>(&QSpinBox::valueChanged),this,[this](int value){const int tool=int(canvas_->tool());if(tool<=int(Canvas::Eraser)){toolWidths_[tool]=value;QSettings().setValue(widthSetting(tool),value);canvas_->setStrokeWidth(value);}});
    auto *strokeWidthAction=new QAction(tr("Толщина штриха…"),this);strokeWidthAction->setObjectName("strokeWidthAction");connect(strokeWidthAction,&QAction::triggered,this,[this]{
        QDialog dialog(this);dialog.setObjectName("strokeWidthDialog");dialog.setWindowTitle(tr("Толщина штриха"));auto *dialogLayout=new QVBoxLayout(&dialog);auto *form=new QFormLayout;dialogLayout->addLayout(form);
        QSpinBox *controls[3];const QStringList labels{tr("Карандаш"),tr("Кисть"),tr("Ластик")};const QStringList objectNames{"pencilWidthSetting","brushWidthSetting","eraserWidthSetting"};
        for(int i=0;i<3;++i){controls[i]=new QSpinBox;controls[i]->setObjectName(objectNames[i]);controls[i]->setRange(1,200);controls[i]->setSuffix(tr(" px"));controls[i]->setValue(toolWidths_[i]);form->addRow(labels[i],controls[i]);}
        auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);dialogLayout->addWidget(buttons);connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);if(dialog.exec()!=QDialog::Accepted)return;
        for(int i=0;i<3;++i){toolWidths_[i]=controls[i]->value();QSettings().setValue(widthSetting(i),toolWidths_[i]);}
        const int active=int(canvas_->tool());if(active<=int(Canvas::Eraser)){QSignalBlocker block(strokeWidth_);strokeWidth_->setValue(toolWidths_[active]);canvas_->setStrokeWidth(toolWidths_[active]);}
    });
    bar->addSeparator();
    frontButton_ = new QPushButton(bar); frontButton_->setToolTip(tr("Основной цвет (Front)")); frontButton_->setObjectName("frontColor"); frontButton_->setFixedWidth(34); bar->addWidget(frontButton_);
    auto *frontColorAction=new QAction(tr("Основной цвет (Front)…"),this);frontColorAction->setObjectName("frontColorAction");
    auto *backColorAction=new QAction(tr("Фоновый цвет (Back)…"),this);backColorAction->setObjectName("backColorAction");
    auto *swap = new QAction(style()->standardIcon(QStyle::SP_BrowserReload),tr("Поменять цвета местами"),this); swap->setObjectName("swapColorsAction");swap->setToolTip(tr("Поменять цвета местами (X)")); swap->setShortcut(QKeySequence("X")); bar->addAction(swap);
    backButton_ = new QPushButton(bar); backButton_->setToolTip(tr("Фоновый цвет и цвет ластика (Back)")); backButton_->setObjectName("backColor"); backButton_->setFixedWidth(34); bar->addWidget(backButton_);
    connect(frontColorAction,&QAction::triggered,this,[this]{auto c=QColorDialog::getColor(front_,this,tr("Основной цвет — Front"));if(c.isValid()){front_=c;updateColors();}});
    connect(backColorAction,&QAction::triggered,this,[this]{auto c=QColorDialog::getColor(back_,this,tr("Цвет фона и ластика — Back"));if(c.isValid()){back_=c;updateColors();}});
    connect(frontButton_,&QPushButton::clicked,frontColorAction,&QAction::trigger);
    connect(backButton_,&QPushButton::clicked,backColorAction,&QAction::trigger);
    connect(swap,&QAction::triggered,this,[this]{qSwap(front_,back_);updateColors();}); updateColors();
    auto *toolBar = new QToolBar(tr("Инструменты"),this); toolBar->setObjectName("toolsToolbar"); toolBar->setMovable(false); toolBar->setIconSize(QSize(24,24)); toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly); addToolBar(Qt::LeftToolBarArea,toolBar);
    auto *group = new QActionGroup(this); group->setExclusive(true);
    QStringList names{tr("Карандаш"),tr("Кисть"),tr("Ластик"),tr("Перемещение холста"),tr("Точка схода")};
    QStringList shortcuts{"B","K","E","H","P"};
    for(int i=0;i<5;++i){
        auto *action = new QAction(toolIcon(i),names[i],this); action->setObjectName(QString("tool%1").arg(i)); action->setCheckable(true); action->setShortcut(QKeySequence(shortcuts[i])); action->setToolTip(names[i]+" ("+shortcuts[i]+")"); group->addAction(action); toolBar->addAction(action);toolsMenu->addAction(action);if(i==0)action->setChecked(true);
        connect(action,&QAction::triggered,this,[this,i,names]{activateTool(Canvas::Tool(i),names[i]);});
        if(i==4)perspectiveAction_=action;
    }
    toolsMenu->addSeparator();toolsMenu->addAction(strokeWidthAction);toolsMenu->addSeparator();toolsMenu->addAction(frontColorAction);toolsMenu->addAction(backColorAction);toolsMenu->addAction(swap);
    perspectiveDock_ = new QDockWidget(tr("Перспектива · прототип"),this); perspectiveDock_->setObjectName("perspectiveDock"); perspectiveDock_->setAllowedAreas(Qt::LeftDockWidgetArea|Qt::RightDockWidgetArea); perspectiveDock_->setFeatures(QDockWidget::DockWidgetClosable);
    auto *panel = new QWidget; auto *layout = new QVBoxLayout(panel); layout->setContentsMargins(14,14,14,14);
    gridVisible_ = new QCheckBox(tr("Показать направляющие")); gridVisible_->setObjectName("gridVisible"); layout->addWidget(gridVisible_);
    axesVisible_=new QCheckBox(tr("Показать координатные оси"));axesVisible_->setObjectName("axesVisible");layout->addWidget(axesVisible_);
    markersVisible_=new QCheckBox(tr("Показать управляющие маркеры"));markersVisible_->setObjectName("markersVisible");layout->addWidget(markersVisible_);
    auto *commonGroup = new QGroupBox(tr("Настройки точек схода")); commonGroup->setObjectName("vanishingPointSettings"); auto *commonForm = new QFormLayout(commonGroup);
    rayStep_ = new QDoubleSpinBox; rayStep_->setObjectName("rayStep"); rayStep_->setRange(1,30); rayStep_->setDecimals(1); rayStep_->setSingleStep(1); rayStep_->setSuffix(tr("°")); rayStep_->setKeyboardTracking(false); commonForm->addRow(tr("Угловой шаг"),rayStep_);
    rayAngleOffset_=new QDoubleSpinBox;rayAngleOffset_->setObjectName("rayAngleOffset");rayAngleOffset_->setRange(-180,180);rayAngleOffset_->setDecimals(1);rayAngleOffset_->setSuffix(tr("°"));rayAngleOffset_->setKeyboardTracking(false);commonForm->addRow(tr("Смещение первого луча"),rayAngleOffset_);
    rayPattern_=new QComboBox;rayPattern_->setObjectName("rayPattern");rayPattern_->addItems({tr("Сплошная"),tr("Пунктир"),tr("Точки"),tr("Штрих-пунктир")});commonForm->addRow(tr("Шаблон линии"),rayPattern_);
    rayWidth_=new QDoubleSpinBox;rayWidth_->setObjectName("rayWidth");rayWidth_->setRange(0.1,20);rayWidth_->setDecimals(1);rayWidth_->setSingleStep(0.5);rayWidth_->setSuffix(tr(" px"));rayWidth_->setKeyboardTracking(false);commonForm->addRow(tr("Толщина лучей"),rayWidth_);
    rayGap_ = new QSpinBox; rayGap_->setObjectName("rayGap"); rayGap_->setRange(0,200); rayGap_->setSuffix(tr(" px")); rayGap_->setKeyboardTracking(false); commonForm->addRow(tr("Отступ от точки"),rayGap_);
    rayStartOpacity_ = new QSpinBox; rayStartOpacity_->setObjectName("rayStartOpacity"); rayStartOpacity_->setRange(0,100); rayStartOpacity_->setSuffix(" %"); rayStartOpacity_->setKeyboardTracking(false); commonForm->addRow(tr("Непрозрачность у точки"),rayStartOpacity_);
    rayEndOpacity_ = new QSpinBox; rayEndOpacity_->setObjectName("rayEndOpacity"); rayEndOpacity_->setRange(0,100); rayEndOpacity_->setSuffix(" %"); rayEndOpacity_->setKeyboardTracking(false); commonForm->addRow(tr("Итоговая непрозрачность"),rayEndOpacity_);
    rayFadeLength_ = new QSpinBox; rayFadeLength_->setObjectName("rayFadeLength"); rayFadeLength_->setRange(0,500); rayFadeLength_->setSuffix(tr(" px")); rayFadeLength_->setKeyboardTracking(false); commonForm->addRow(tr("Длина нарастания"),rayFadeLength_);
    symmetricPoints_=new QCheckBox(tr("Симметрично перемещать пару"));symmetricPoints_->setObjectName("symmetricPoints");commonForm->addRow(symmetricPoints_);
    auto *defaultButtons = new QHBoxLayout; savePerspectiveDefaultsButton_ = new QPushButton(tr("Сохранить")); savePerspectiveDefaultsButton_->setObjectName("savePerspectiveDefaults"); savePerspectiveDefaultsButton_->setToolTip(tr("Сохранить пять числовых параметров и шаблон линии"));
    auto *resetDefaults = new QPushButton(tr("Сбросить")); resetDefaults->setObjectName("resetPerspectiveDefaults"); resetDefaults->setToolTip(tr("Вернуть заводские значения")); defaultButtons->addWidget(savePerspectiveDefaultsButton_);defaultButtons->addWidget(resetDefaults);commonForm->addRow(defaultButtons);
    auto *horizonGroup = new QGroupBox(tr("Линия горизонта")); horizonGroup->setObjectName("horizonSettings"); auto *horizonForm = new QFormLayout(horizonGroup);
    horizonVisible_=new QCheckBox(tr("Показывать линию горизонта"));horizonVisible_->setObjectName("horizonVisible");horizonForm->addRow(horizonVisible_);
    horizonLocked_=new QCheckBox(tr("Фиксировать"));horizonLocked_->setObjectName("horizonLocked");horizonForm->addRow(horizonLocked_);
    horizonPosition_=new QDoubleSpinBox;horizonPosition_->setObjectName("horizonPosition");horizonPosition_->setRange(-1000000,1000000);horizonPosition_->setDecimals(2);horizonPosition_->setKeyboardTracking(false);horizonForm->addRow(tr("Положение Y"),horizonPosition_);
    horizonUnits_=new QComboBox;horizonUnits_->setObjectName("horizonUnits");horizonUnits_->addItems({tr("Проценты"),tr("Пиксели")});horizonForm->addRow(tr("Единицы"),horizonUnits_);
    horizonColorButton_ = new QPushButton(tr("Выбрать…")); horizonColorButton_->setObjectName("horizonColor"); horizonForm->addRow(tr("Цвет"),horizonColorButton_);
    horizonOpacity_ = new QSpinBox; horizonOpacity_->setObjectName("horizonOpacity"); horizonOpacity_->setRange(0,100); horizonOpacity_->setSuffix(" %"); horizonOpacity_->setKeyboardTracking(false); horizonForm->addRow(tr("Непрозрачность"),horizonOpacity_);
    horizonWidth_ = new QDoubleSpinBox; horizonWidth_->setObjectName("horizonWidth"); horizonWidth_->setRange(0.1,20); horizonWidth_->setDecimals(1); horizonWidth_->setSingleStep(0.5); horizonWidth_->setSuffix(tr(" px")); horizonWidth_->setKeyboardTracking(false); horizonForm->addRow(tr("Ширина"),horizonWidth_);layout->addWidget(horizonGroup);layout->addWidget(commonGroup);
    auto *pointsListGroup = new QGroupBox(tr("Точки схода")); pointsListGroup->setObjectName("vanishingPointsList"); auto *pointsListLayout = new QVBoxLayout(pointsListGroup);
    vanishingPointsList_=new QListWidget;vanishingPointsList_->setObjectName("vanishingPointsListControl");vanishingPointsList_->setSelectionMode(QAbstractItemView::SingleSelection);pointsListLayout->addWidget(vanishingPointsList_);
    auto *pointButtons=new QHBoxLayout;auto *addPointButton=new QPushButton(tr("Добавить"));addPointButton->setObjectName("addVanishingPoint");removePointButton_=new QPushButton(tr("Удалить"));removePointButton_->setObjectName("removeVanishingPoint");pointButtons->addWidget(addPointButton);pointButtons->addWidget(removePointButton_);pointsListLayout->addLayout(pointButtons);
    auto *pointGroup = new QGroupBox(tr("Свойства выбранной точки")); pointGroup->setObjectName("selectedVanishingPointSettings"); auto *pointForm = new QFormLayout(pointGroup);
    pointX_=new QDoubleSpinBox;pointX_->setObjectName("vanishingPointX");pointX_->setRange(-1000000,1000000);pointX_->setDecimals(2);pointX_->setKeyboardTracking(false);pointForm->addRow(tr("X"),pointX_);
    pointY_=new QDoubleSpinBox;pointY_->setObjectName("vanishingPointY");pointY_->setRange(-1000000,1000000);pointY_->setDecimals(2);pointY_->setKeyboardTracking(false);pointForm->addRow(tr("Y"),pointY_);
    pointUnits_=new QComboBox;pointUnits_->setObjectName("vanishingPointUnits");pointUnits_->addItems({tr("Проценты"),tr("Пиксели")});pointForm->addRow(tr("Единицы"),pointUnits_);
    pointAttachment_=new QComboBox;pointAttachment_->setObjectName("vanishingPointAttachment");pointAttachment_->addItems({tr("Свободна"),tr("Прикреплена к горизонту")});pointForm->addRow(tr("Состояние"),pointAttachment_);
    selectedPointVisible_=new QCheckBox(tr("Показывать семейство линий"));selectedPointVisible_->setObjectName("selectedPointVisible");pointForm->addRow(selectedPointVisible_);
    selectedPointLocked_=new QCheckBox(tr("Фиксировать"));selectedPointLocked_->setObjectName("selectedPointLocked");pointForm->addRow(selectedPointLocked_);
    gridColorButton_ = new QPushButton(tr("Выбрать…")); gridColorButton_->setObjectName("gridColor"); pointForm->addRow(tr("Цвет лучей"),gridColorButton_);pointsListLayout->addWidget(pointGroup);layout->addWidget(pointsListGroup);
    auto *tip = new QLabel(tr("P — перемещение точки схода и горизонта.\nТочка прилипает к горизонту вблизи него, но может быть свободно снята.\nB — вернуться к карандашу.\n\nНаправляющие не попадают\nв экспорт PNG.")); tip->setWordWrap(true); layout->addSpacing(12); layout->addWidget(tip); layout->addStretch();auto *panelScroll=new QScrollArea;panelScroll->setObjectName("perspectiveScroll");panelScroll->setWidgetResizable(true);panelScroll->setFrameShape(QFrame::NoFrame);panelScroll->setWidget(panel);perspectiveDock_->setWidget(panelScroll); addDockWidget(Qt::RightDockWidgetArea,perspectiveDock_); perspectiveDock_->hide();
    auto *mainToolbarToggle=bar->toggleViewAction();mainToolbarToggle->setObjectName("mainToolbarToggle");mainToolbarToggle->setText(tr("Панель команд"));view->addAction(mainToolbarToggle);
    auto *toolsToolbarToggle=toolBar->toggleViewAction();toolsToolbarToggle->setObjectName("toolsToolbarToggle");toolsToolbarToggle->setText(tr("Панель инструментов"));view->addAction(toolsToolbarToggle);
    auto *perspectivePanelToggle=perspectiveDock_->toggleViewAction();perspectivePanelToggle->setObjectName("perspectivePanelToggle");perspectivePanelToggle->setText(tr("Панель перспективы"));view->addAction(perspectivePanelToggle);view->addSeparator();
    connect(gridVisible_,&QCheckBox::toggled,canvas_,&Canvas::setGridVisible);
    connect(axesVisible_,&QCheckBox::toggled,this,[this](bool value){QSettings().setValue("perspective/view/axesVisible",value);canvas_->setAxesVisible(value);});
    connect(markersVisible_,&QCheckBox::toggled,this,[this](bool value){QSettings().setValue("perspective/view/markersVisible",value);canvas_->setMarkersVisible(value);});
    connect(rayStep_,qOverload<double>(&QDoubleSpinBox::valueChanged),canvas_,&Canvas::setRayStep);
    connect(rayAngleOffset_,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this](double value){QSettings().setValue("perspective/view/rayAngleOffset",value);canvas_->setRayAngleOffset(value);});
    connect(rayPattern_,qOverload<int>(&QComboBox::currentIndexChanged),canvas_,&Canvas::setRayPattern);
    connect(rayWidth_,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this](double value){QSettings().setValue("perspective/view/rayWidth",value);canvas_->setRayWidth(value);});
    connect(symmetricPoints_,&QCheckBox::toggled,this,[this](bool value){QSettings().setValue("perspective/view/symmetricPoints",value);canvas_->setSymmetricPoints(value);});
    connect(rayGap_,qOverload<int>(&QSpinBox::valueChanged),canvas_,&Canvas::setRayGap);
    connect(rayStartOpacity_,qOverload<int>(&QSpinBox::valueChanged),canvas_,&Canvas::setRayStartOpacity);
    connect(rayEndOpacity_,qOverload<int>(&QSpinBox::valueChanged),canvas_,&Canvas::setRayEndOpacity);
    connect(rayFadeLength_,qOverload<int>(&QSpinBox::valueChanged),canvas_,&Canvas::setRayFadeLength);
    connect(savePerspectiveDefaultsButton_,&QPushButton::clicked,this,[this]{savePerspectiveDefaults(canvas_->state());updateState();statusBar()->showMessage(tr("Общие настройки направляющих сохранены"),3000);});
    connect(resetDefaults,&QPushButton::clicked,this,[this]{clearPerspectiveDefaults();canvas_->setRayAppearance(10,12,10,70,50,0);updateState();statusBar()->showMessage(tr("Восстановлены настройки направляющих по умолчанию"),3000);});
    connect(horizonColorButton_,&QPushButton::clicked,this,[this]{auto c=QColorDialog::getColor(canvas_->state().horizonColor,this,tr("Цвет линии горизонта"));if(c.isValid()){QSettings().setValue("perspective/view/horizonColor",c.name(QColor::HexArgb));canvas_->setHorizonColor(c);}});
    connect(horizonOpacity_,qOverload<int>(&QSpinBox::valueChanged),this,[this](int value){QSettings().setValue("perspective/view/horizonOpacity",value);canvas_->setHorizonOpacity(value);});
    connect(horizonWidth_,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this](double value){QSettings().setValue("perspective/view/horizonWidth",value);canvas_->setHorizonWidth(value);});
    connect(horizonVisible_,&QCheckBox::toggled,this,[this](bool value){QSettings().setValue("perspective/view/horizonVisible",value);canvas_->setHorizonVisible(value);});
    connect(horizonLocked_,&QCheckBox::toggled,canvas_,&Canvas::setHorizonLocked);
    connect(horizonPosition_,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this](double value){canvas_->setHorizonY(imageY(value));});
    connect(horizonUnits_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int index){setCoordinateUnits(index==0);});
    connect(vanishingPointsList_,&QListWidget::currentRowChanged,canvas_,&Canvas::selectPoint);
    connect(canvas_,&Canvas::selectedPointChanged,this,[this](int index){QSignalBlocker block(vanishingPointsList_);vanishingPointsList_->setCurrentRow(index);updateState();});
    connect(addPointButton,&QPushButton::clicked,this,[this]{canvas_->addVanishingPoint();const int i=canvas_->selectedPointIndex();if(i<0)return;QSettings settings;const QColor color(settings.value(QString("perspective/points/%1/color").arg(i),defaultPointColor(i)).toString());canvas_->setSelectedPointColor(color.isValid()?color:defaultPointColor(i));canvas_->setSelectedPointVisible(settings.value(QString("perspective/points/%1/visible").arg(i),true).toBool());});
    connect(removePointButton_,&QPushButton::clicked,canvas_,&Canvas::removeSelectedVanishingPoint);
    connect(selectedPointVisible_,&QCheckBox::toggled,this,[this](bool visible){const int i=canvas_->selectedPointIndex();if(i<0)return;QSettings().setValue(QString("perspective/points/%1/visible").arg(i),visible);canvas_->setSelectedPointVisible(visible);});
    connect(selectedPointLocked_,&QCheckBox::toggled,canvas_,&Canvas::setSelectedPointLocked);
    connect(pointX_,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this](double value){const int i=canvas_->selectedPointIndex();if(i<0)return;QPointF position=canvas_->state().vanishingPoints[i].position;position.setX(imageX(value));canvas_->setSelectedPointPosition(position);});
    connect(pointY_,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this](double value){const int i=canvas_->selectedPointIndex();if(i<0)return;QPointF position=canvas_->state().vanishingPoints[i].position;position.setY(imageY(value));canvas_->setSelectedPointPosition(position);});
    connect(pointUnits_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int index){setCoordinateUnits(index==0);});
    connect(pointAttachment_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int index){canvas_->setSelectedPointAttachedToHorizon(index==1);});
    connect(gridColorButton_,&QPushButton::clicked,this,[this]{const int i=canvas_->selectedPointIndex();if(i<0)return;auto c=QColorDialog::getColor(canvas_->state().vanishingPoints[i].color,this,tr("Цвет направляющих"));if(c.isValid()){QSettings().setValue(QString("perspective/points/%1/color").arg(i),c.name(QColor::HexArgb));canvas_->setSelectedPointColor(c);}});
    auto *fitAction = view->addAction(style()->standardIcon(QStyle::SP_TitleBarMaxButton),tr("Вписать холст"),canvas_,&Canvas::fit,QKeySequence("Ctrl+0"));fitAction->setToolTip(tr("Вписать холст (Ctrl+0)"));
    auto *actualAction = view->addAction(actualSizeIcon(),tr("Масштаб 100%"),this,[this]{canvas_->setZoom(1);},QKeySequence("Ctrl+1"));actualAction->setToolTip(tr("Масштаб 100% (Ctrl+1)"));
    view->addAction(tr("Увеличить"),this,[this]{canvas_->setZoom(canvas_->zoom()*1.2);},QKeySequence("Ctrl++"));
    view->addAction(tr("Уменьшить"),this,[this]{canvas_->setZoom(canvas_->zoom()/1.2);},QKeySequence("Ctrl+-"));
    help->addAction(tr("Управление"),this,[this]{QMessageBox::information(this,productName()+tr(" — управление"),tr("B — карандаш\nK — кисть\nE — ластик (цвет Back)\nH — перемещение холста\nP — точка схода\nX — поменять Front и Back\n\nShift + щелчок — отрезок от последней точки\nCtrl + Shift — привязка угла по 15°\nКолесо — масштаб под курсором\nСредняя кнопка или Пробел + мышь — перемещение\nCtrl+Z / Ctrl+Y — отмена / повтор\nCtrl+0 — вписать, Ctrl+1 — 100%\n\nПроект .drw хранит PNG, координаты точек схода, их фиксацию и горизонт.\nЭкспорт PNG сохраняет только рисунок."));});
    auto *aboutAction=help->addAction(tr("О программе…"),this,&MainWindow::showAbout);aboutAction->setObjectName("aboutAction");
    toolLabel_ = new QLabel(tr("Карандаш")); sizeLabel_ = new QLabel; positionLabel_ = new QLabel; positionLabel_->setObjectName("canvasPosition");positionLabel_->setMinimumWidth(125);
    statusBar()->addWidget(toolLabel_); statusBar()->addWidget(sizeLabel_); statusBar()->addWidget(positionLabel_,1);
    auto *fitButton = new QToolButton; fitButton->setObjectName("fitButton");fitButton->setToolButtonStyle(Qt::ToolButtonIconOnly);fitButton->setDefaultAction(fitAction); statusBar()->addPermanentWidget(fitButton);
    auto *actualButton = new QToolButton; actualButton->setObjectName("actualSizeButton");actualButton->setToolButtonStyle(Qt::ToolButtonIconOnly);actualButton->setDefaultAction(actualAction); statusBar()->addPermanentWidget(actualButton);
    zoom_ = new QDoubleSpinBox; zoom_->setObjectName("zoomPercent"); zoom_->setRange(5,1600); zoom_->setDecimals(0); zoom_->setSuffix(" %"); zoom_->setKeyboardTracking(false); statusBar()->addPermanentWidget(zoom_);
    connect(zoom_,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this](double value){canvas_->setZoom(value/100.0);});
    connect(canvas_,&Canvas::viewChanged,this,[this]{QSignalBlocker block(zoom_);zoom_->setValue(canvas_->zoom()*100);});
    connect(canvas_,&Canvas::positionChanged,this,[this](QPointF p){const double xPixels=p.x()-canvas_->state().image.width()/2.0,yPixels=canvas_->state().image.height()/2.0-p.y();const double x=rulerPercent_?xPixels*100.0/canvas_->state().image.width():xPixels,y=rulerPercent_?yPixels*100.0/canvas_->state().image.height():yPixels;const QString suffix=rulerPercent_?QStringLiteral("%"):QStringLiteral(" px");positionLabel_->setText(QString("X: %1%3   Y: %2%3").arg(x,0,'f',rulerPercent_?1:0).arg(y,0,'f',rulerPercent_?1:0).arg(suffix));});
    connect(canvas_,&Canvas::stateChanged,this,&MainWindow::updateState);
    connect(canvas_->undoStack(),&QUndoStack::cleanChanged,this,&MainWindow::updateState);
    updateState();
    QTimer::singleShot(0,canvas_,&Canvas::fit);
}

void MainWindow::updateColors(){ colorSwatch(frontButton_,front_);colorSwatch(backButton_,back_);canvas_->setFront(front_);canvas_->setBack(back_); }
void MainWindow::activateTool(Canvas::Tool tool,const QString &name){
    canvas_->setTool(tool);toolLabel_->setText(name);
    const bool paints=tool<=Canvas::Eraser;strokeWidth_->setEnabled(paints);
    if(paints){QSignalBlocker block(strokeWidth_);strokeWidth_->setValue(toolWidths_[int(tool)]);canvas_->setStrokeWidth(toolWidths_[int(tool)]);}
    if(tool==Canvas::Perspective){perspectiveDock_->show();canvas_->setGridVisible(true);}canvas_->setFocus();
}
void MainWindow::addRecentFile(const QString &path){
    const QString absolute=QFileInfo(path).absoluteFilePath();
    for(int i=recentFiles_.size()-1;i>=0;--i)if(QString::compare(recentFiles_[i],absolute,Qt::CaseInsensitive)==0)recentFiles_.removeAt(i);
    recentFiles_.prepend(absolute);while(recentFiles_.size()>5)recentFiles_.removeLast();
    QSettings().setValue("files/recentFiles",recentFiles_);updateRecentFilesMenu();
}
void MainWindow::updateRecentFilesMenu(){
    recentFilesMenu_->clear();
    if(recentFiles_.isEmpty()){auto *empty=recentFilesMenu_->addAction(tr("Нет недавних файлов"));empty->setEnabled(false);return;}
    for(int i=0;i<recentFiles_.size();++i){
        const QString path=recentFiles_[i];QString label=path;label.replace("&","&&");
        auto *action=recentFilesMenu_->addAction(QString("&%1  %2").arg(i+1).arg(label));action->setObjectName(QString("recentFile%1").arg(i));action->setData(path);action->setToolTip(path);
        connect(action,&QAction::triggered,this,[this,path]{openPath(path);});
    }
}
void MainWindow::updateState(){
    QString name=path_.isEmpty()?tr("Без имени.drw"):QFileInfo(path_).fileName();
    setWindowTitle(name+"[*] — "+productName()); setWindowModified(!canvas_->undoStack()->isClean());
    sizeLabel_->setText(QString("%1 × %2 px").arg(canvas_->state().image.width()).arg(canvas_->state().image.height()));
    QSignalBlocker a(gridVisible_),b(rayStep_),c(rayGap_),d(rayStartOpacity_),e(rayEndOpacity_),f(rayFadeLength_),g(horizonOpacity_),h(horizonWidth_),i(vanishingPointsList_),j(selectedPointVisible_),k(horizonPosition_),l(horizonUnits_),m(pointX_),n(pointY_),o(pointUnits_),q(pointAttachment_);
    QSignalBlocker r(rayAngleOffset_),s(rayPattern_),t(rayWidth_),u(horizonVisible_),v(axesVisible_),w(markersVisible_),x(symmetricPoints_),y(horizonLocked_),z(selectedPointLocked_);
    gridVisible_->setChecked(canvas_->state().gridVisible);axesVisible_->setChecked(canvas_->state().axesVisible);markersVisible_->setChecked(canvas_->state().markersVisible);rayStep_->setValue(canvas_->state().rayStepDegrees);rayAngleOffset_->setValue(canvas_->state().rayAngleOffset);rayPattern_->setCurrentIndex(canvas_->state().rayPattern);rayWidth_->setValue(canvas_->state().rayWidth);symmetricPoints_->setChecked(canvas_->state().symmetricPoints);rayGap_->setValue(canvas_->state().rayGap);
    rayStartOpacity_->setValue(canvas_->state().rayStartOpacity); rayEndOpacity_->setValue(canvas_->state().rayEndOpacity); rayFadeLength_->setValue(canvas_->state().rayFadeLength);
    savePerspectiveDefaultsButton_->setEnabled(!matchesPerspectiveDefaults(canvas_->state()));
    horizonOpacity_->setValue(canvas_->state().horizonOpacity);horizonWidth_->setValue(canvas_->state().horizonWidth);horizonVisible_->setChecked(canvas_->state().horizonVisible);horizonLocked_->setChecked(canvas_->state().horizonLocked);horizonPosition_->setSuffix(coordinatePercent_?" %":tr(" px"));horizonPosition_->setValue(displayedY(canvas_->state().horizonY));horizonUnits_->setCurrentIndex(coordinatePercent_?0:1);
    const auto &points=canvas_->state().vanishingPoints;
    bool rebuild=vanishingPointsList_->count()!=points.size();
    if(!rebuild)for(int row=0;row<points.size();++row)if(vanishingPointsList_->item(row)->data(Qt::UserRole).toString()!=points[row].id){rebuild=true;break;}
    if(rebuild){vanishingPointsList_->clear();for(int row=0;row<points.size();++row){auto *item=new QListWidgetItem(tr("Точка схода %1").arg(row+1));item->setData(Qt::UserRole,points[row].id);vanishingPointsList_->addItem(item);}}
    const int selected=canvas_->selectedPointIndex();vanishingPointsList_->setCurrentRow(selected);const bool hasPoint=selected>=0&&selected<points.size();
    removePointButton_->setEnabled(hasPoint);selectedPointVisible_->setEnabled(hasPoint);selectedPointLocked_->setEnabled(hasPoint);gridColorButton_->setEnabled(hasPoint);pointX_->setEnabled(hasPoint);pointY_->setEnabled(hasPoint);pointUnits_->setEnabled(hasPoint);pointAttachment_->setEnabled(hasPoint);
    pointX_->setSuffix(coordinatePercent_?" %":tr(" px"));pointY_->setSuffix(coordinatePercent_?" %":tr(" px"));pointUnits_->setCurrentIndex(coordinatePercent_?0:1);
    if(hasPoint){pointX_->setValue(displayedX(points[selected].position.x()));pointY_->setValue(displayedY(points[selected].position.y()));const bool attached=points[selected].attachmentType==QStringLiteral("construction")&&points[selected].attachmentTargetId==QStringLiteral("horizon");pointAttachment_->setCurrentIndex(attached?1:0);selectedPointLocked_->setChecked(points[selected].locked);}
    else{pointX_->setValue(0);pointY_->setValue(0);pointAttachment_->setCurrentIndex(0);selectedPointLocked_->setChecked(false);}
    selectedPointVisible_->setChecked(hasPoint&&points[selected].visible);colorSwatch(gridColorButton_,hasPoint?points[selected].color:QColor("#d0d0d0"));colorSwatch(horizonColorButton_,canvas_->state().horizonColor);
}
double MainWindow::displayedX(double imageCoordinate) const {const double pixels=imageCoordinate-canvas_->state().image.width()/2.0;return coordinatePercent_?pixels*100.0/canvas_->state().image.width():pixels;}
double MainWindow::displayedY(double imageCoordinate) const {const double pixels=canvas_->state().image.height()/2.0-imageCoordinate;return coordinatePercent_?pixels*100.0/canvas_->state().image.height():pixels;}
double MainWindow::imageX(double displayed) const {return canvas_->state().image.width()/2.0+(coordinatePercent_?displayed*canvas_->state().image.width()/100.0:displayed);}
double MainWindow::imageY(double displayed) const {return canvas_->state().image.height()/2.0-(coordinatePercent_?displayed*canvas_->state().image.height()/100.0:displayed);}
void MainWindow::setCoordinateUnits(bool percent){if(coordinatePercent_==percent){updateState();return;}coordinatePercent_=percent;QSettings().setValue("perspective/coordinatePercent",percent);updateState();}
void MainWindow::showSettings(){
    QDialog dialog(this);dialog.setObjectName("settingsDialog");dialog.setWindowTitle(tr("Настройки — ")+productName());dialog.resize(460,300);
    auto *dialogLayout=new QVBoxLayout(&dialog);auto *tabs=new QTabWidget;tabs->setObjectName("settingsTabs");dialogLayout->addWidget(tabs);
    auto *viewPage=new QWidget;auto *viewForm=new QFormLayout(viewPage);auto *rulerUnits=new QComboBox;rulerUnits->setObjectName("rulerUnits");rulerUnits->addItems({tr("Пиксели"),tr("Проценты")});rulerUnits->setCurrentIndex(rulerPercent_?1:0);viewForm->addRow(tr("Единицы линеек"),rulerUnits);viewForm->addRow(new QLabel(tr("Единицы числовых координат точек схода и горизонта выбираются отдельно в панели перспективы.")));tabs->addTab(viewPage,tr("Вид"));
    auto *systemPage=new QWidget;auto *systemLayout=new QVBoxLayout(systemPage);auto *systemHint=new QLabel(tr("Системные параметры будут добавляться по мере необходимости."));systemHint->setWordWrap(true);systemLayout->addWidget(systemHint);systemLayout->addStretch();tabs->addTab(systemPage,tr("Система"));
    auto *filesPage=new QWidget;auto *filesLayout=new QVBoxLayout(filesPage);auto *filesHint=new QLabel(tr("Параметры файлов и списка недавних документов будут добавлены в следующих задачах."));filesHint->setWordWrap(true);filesLayout->addWidget(filesHint);filesLayout->addStretch();tabs->addTab(filesPage,tr("Файлы"));
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);dialogLayout->addWidget(buttons);connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    rulerPercent_=rulerUnits->currentIndex()==1;QSettings().setValue("view/rulers/percent",rulerPercent_);canvas_->setRulerPercent(rulerPercent_);positionLabel_->clear();
}
void MainWindow::showAbout(){
    QDialog dialog(this);dialog.setObjectName("aboutDialog");dialog.setWindowTitle(tr("О программе"));auto *layout=new QVBoxLayout(&dialog);layout->setContentsMargins(32,24,32,20);layout->setSpacing(14);
    auto *icon=new QLabel;icon->setObjectName("aboutIcon");icon->setAlignment(Qt::AlignCenter);icon->setPixmap(QIcon(":/app/techdraw.png").pixmap(96,96));layout->addWidget(icon);
    auto *name=new QLabel(QStringLiteral("Технический рисунок\nTechnical Draw"));name->setObjectName("aboutName");name->setAlignment(Qt::AlignCenter);QFont nameFont=name->font();nameFont.setPointSize(nameFont.pointSize()+2);nameFont.setBold(true);name->setFont(nameFont);layout->addWidget(name);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Close);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);layout->addWidget(buttons);dialog.exec();
}
void MainWindow::showError(const QString &error){ QMessageBox::critical(this,productName(),error); }
bool MainWindow::confirmDiscard(){
    if(canvas_->undoStack()->isClean())return true;
    QMessageBox message(QMessageBox::Question,tr("Несохранённые изменения"),tr("Сохранить изменения перед продолжением?"),QMessageBox::NoButton,this);
    auto *save=message.addButton(tr("Сохранить"),QMessageBox::AcceptRole); auto *discard=message.addButton(tr("Не сохранять"),QMessageBox::DestructiveRole); auto *cancel=message.addButton(tr("Отмена"),QMessageBox::RejectRole); message.setDefaultButton(save); message.setEscapeButton(cancel); message.exec();
    if(message.clickedButton()==save)return saveDocument();
    return message.clickedButton()==discard;
}
void MainWindow::newDocument(){
    QDialog dialog(this);dialog.setWindowTitle(tr("Новый холст"));auto *layout=new QVBoxLayout(&dialog);auto *form=new QFormLayout;
    QSettings settings;QSize remembered(settings.value("canvas/newWidth",1000).toInt(),settings.value("canvas/newHeight",620).toInt());if(!Project::validSize(remembered))remembered=QSize(1000,620);
    QSpinBox w,h;w.setObjectName("newCanvasWidth");h.setObjectName("newCanvasHeight");w.setRange(1,8192);h.setRange(1,8192);w.setValue(remembered.width());h.setValue(remembered.height());w.setSuffix(" px");h.setSuffix(" px");form->addRow(tr("Ширина"),&w);form->addRow(tr("Высота"),&h);layout->addLayout(form);layout->addWidget(new QLabel(tr("До 16 млн пикселей. Фон — цвет Back.")));
    QDialogButtonBox buttons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);buttons.button(QDialogButtonBox::Ok)->setText(tr("Создать"));buttons.button(QDialogButtonBox::Cancel)->setText(tr("Отмена"));layout->addWidget(&buttons);connect(&buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(&buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    if(!Project::validSize(QSize(w.value(),h.value()))){showError(tr("Максимум 16 млн пикселей."));return;}
    DrawingState state;state.image=QImage(w.value(),h.value(),QImage::Format_ARGB32_Premultiplied);if(state.image.isNull()){showError(tr("Не удалось выделить память для холста."));return;}state.image.fill(back_);state.horizonY=h.value()/2.0;state.vanishingPoints.append({QStringLiteral("vp-1"),QPointF(w.value()/2.0,h.value()/2.0),QStringLiteral("construction"),QStringLiteral("horizon")});applySavedPerspectiveDefaults(&state);applySavedViewSettings(&state);applySavedPointAppearance(&state);
    if(!confirmDiscard())return;
    settings.setValue("canvas/newWidth",w.value());settings.setValue("canvas/newHeight",h.value());
    path_.clear();canvas_->setDocument(state,false);canvas_->fit();
}
void MainWindow::openDocument(){QString path=QFileDialog::getOpenFileName(this,tr("Открыть"),rememberedDirectory("files/openDirectory"),tr("Технический рисунок и PNG (*.drw *.png);;Проект «Технический рисунок» (*.drw);;PNG (*.png)"));if(!path.isEmpty())openPath(path);}
bool MainWindow::openPath(const QString &path){
    DrawingState state;DrawingHistory history;QString error;bool project=path.endsWith(".drw",Qt::CaseInsensitive);
    if(project){if(!Project::load(path,&history,&error)){showError(error);return false;}for(auto &historyState:history.states){applySavedPerspectiveDefaults(&historyState);applySavedViewSettings(&historyState);applySavedPointAppearance(&historyState);}}
    else{if(!Project::loadPng(path,&state.image,&error)){showError(error);return false;}state.horizonY=state.image.height()/2.0;state.vanishingPoints.append({QStringLiteral("vp-1"),QPointF(state.image.width()/2.0,state.image.height()/2.0),QStringLiteral("construction"),QStringLiteral("horizon")});applySavedPerspectiveDefaults(&state);applySavedViewSettings(&state);applySavedPointAppearance(&state);}
    if(!confirmDiscard())return false;
    path_=project?QFileInfo(path).absoluteFilePath():QString();
    if(project)canvas_->setDocument(history,true);else canvas_->setDocument(state,false);
    rememberDirectory("files/openDirectory",path);addRecentFile(path);canvas_->fit();return true;
}
bool MainWindow::saveDocument(bool saveAs){
    QString target=path_;
    if(saveAs||target.isEmpty()){
        const QString name=target.isEmpty()?tr("Без имени.drw"):QFileInfo(target).fileName();
        target=QFileDialog::getSaveFileName(this,tr("Сохранить проект"),suggestedFile("files/saveDirectory",name),tr("Проект «Технический рисунок» (*.drw)"));
        if(target.isEmpty())return false;
        const QString suffixed=withSuffix(target,".drw");
        if(suffixed!=target&&QFileInfo::exists(suffixed)&&QMessageBox::question(this,tr("Заменить файл?"),tr("Файл %1 уже существует. Заменить?").arg(suffixed))!=QMessageBox::Yes)return false;
        target=suffixed;
    }
    QString error;if(!Project::save(target,canvas_->history(),&error)){showError(error);return false;}
    path_=QFileInfo(target).absoluteFilePath();rememberDirectory("files/saveDirectory",path_);addRecentFile(path_);canvas_->undoStack()->setClean();updateState();statusBar()->showMessage(tr("Проект сохранён"),3000);return true;
}
void MainWindow::exportImage(){
    QString target=QFileDialog::getSaveFileName(this,tr("Экспортировать рисунок без направляющих"),suggestedFile("files/saveDirectory",tr("Рисунок.png")),tr("PNG (*.png)"));if(target.isEmpty())return;
    QString suffixed=withSuffix(target,".png");if(suffixed!=target&&QFileInfo::exists(suffixed)&&QMessageBox::question(this,tr("Заменить файл?"),tr("Заменить существующий PNG?"))!=QMessageBox::Yes)return;
    QString error;if(!Project::exportPng(suffixed,canvas_->state().image,&error))showError(error);else{rememberDirectory("files/saveDirectory",suffixed);statusBar()->showMessage(tr("PNG экспортирован; проект сохраняется отдельно"),4000);}
}
void MainWindow::closeEvent(QCloseEvent *event){if(confirmDiscard())event->accept();else event->ignore();}
