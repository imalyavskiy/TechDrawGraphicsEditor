#include "mainwindow.h"
#include <QtWidgets>

namespace {
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
    if (step<1||step>30||gap<0||gap>200||startOpacity<0||startOpacity>100||endOpacity<0||endOpacity>100||fadeLength<0||fadeLength>500) return;
    state->rayStepDegrees=step;state->rayGap=gap;state->rayStartOpacity=startOpacity;state->rayEndOpacity=endOpacity;state->rayFadeLength=fadeLength;
}
void savePerspectiveDefaults(const DrawingState &state) {
    QSettings settings;settings.setValue("perspective/common/rayStepDegrees",state.rayStepDegrees);settings.setValue("perspective/common/rayGap",state.rayGap);
    settings.setValue("perspective/common/rayStartOpacity",state.rayStartOpacity);settings.setValue("perspective/common/rayEndOpacity",state.rayEndOpacity);settings.setValue("perspective/common/rayFadeLength",state.rayFadeLength);
}
void clearPerspectiveDefaults() { QSettings settings;settings.remove("perspective/common"); }
bool matchesPerspectiveDefaults(const DrawingState &state) {
    DrawingState defaults;applySavedPerspectiveDefaults(&defaults);
    return qFuzzyCompare(state.rayStepDegrees,defaults.rayStepDegrees)&&state.rayGap==defaults.rayGap&&
        state.rayStartOpacity==defaults.rayStartOpacity&&state.rayEndOpacity==defaults.rayEndOpacity&&state.rayFadeLength==defaults.rayFadeLength;
}
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), canvas_(new Canvas(this)) {
    DrawingState initialState=canvas_->state();applySavedPerspectiveDefaults(&initialState);canvas_->setDocument(initialState,true);
    setObjectName("drawingWindow");
    resize(1200,800); setMinimumSize(720,480);
    setWindowIcon(QIcon(":/app/techdraw.png"));
    setCentralWidget(canvas_);
    setStyleSheet("QToolBar { spacing: 5px; padding: 5px; border: 0; border-bottom: 1px solid #cdd0d5; background: #f6f6f6; } QDockWidget { font-weight: 500; } QStatusBar { background: #f6f6f6; } QToolButton { padding: 5px; } QToolButton:checked { background: #dceaff; border: 1px solid #8aaedb; border-radius: 3px; } ");
    auto *file = menuBar()->addMenu(tr("&Файл"));
    auto *edit = menuBar()->addMenu(tr("&Правка"));
    auto *view = menuBar()->addMenu(tr("&Вид"));
    auto *help = menuBar()->addMenu(tr("&Справка"));
    auto *newAction = file->addAction(style()->standardIcon(QStyle::SP_FileIcon),tr("Создать…"),this,&MainWindow::newDocument,QKeySequence::New);newAction->setObjectName("newAction");
    auto *openAction = file->addAction(style()->standardIcon(QStyle::SP_DirOpenIcon),tr("Открыть…"),this,&MainWindow::openDocument,QKeySequence::Open);
    recentFilesMenu_=file->addMenu(tr("Недавние файлы"));recentFilesMenu_->setObjectName("recentFilesMenu");
    recentFiles_=QSettings().value("files/recentFiles").toStringList();while(recentFiles_.size()>5)recentFiles_.removeLast();updateRecentFilesMenu();
    file->addSeparator();
    auto *saveAction = file->addAction(style()->standardIcon(QStyle::SP_DialogSaveButton),tr("Сохранить проект"),this,[this]{saveDocument();},QKeySequence::Save);saveAction->setObjectName("saveAction");
    file->addAction(tr("Сохранить проект как…"),this,[this]{saveDocument(true);},QKeySequence::SaveAs);
    file->addSeparator();
    file->addAction(tr("Экспортировать PNG…"),this,&MainWindow::exportImage,QKeySequence("Ctrl+Shift+E"));
    file->addSeparator(); file->addAction(tr("Выход"),this,&QWidget::close,QKeySequence("Alt+F4"));
    auto *undoAction = canvas_->undoStack()->createUndoAction(this,tr("Отменить")); undoAction->setShortcut(QKeySequence::Undo); undoAction->setIcon(style()->standardIcon(QStyle::SP_ArrowBack)); edit->addAction(undoAction);
    auto *redoAction = canvas_->undoStack()->createRedoAction(this,tr("Повторить")); redoAction->setShortcuts({QKeySequence::Redo,QKeySequence("Ctrl+Shift+Z")}); redoAction->setIcon(style()->standardIcon(QStyle::SP_ArrowForward)); edit->addAction(redoAction);
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
    bar->addSeparator();
    frontButton_ = new QPushButton(bar); frontButton_->setToolTip(tr("Основной цвет (Front)")); frontButton_->setObjectName("frontColor"); frontButton_->setFixedWidth(34); bar->addWidget(frontButton_);
    auto *swap = new QAction(style()->standardIcon(QStyle::SP_BrowserReload),tr("Поменять цвета местами"),this); swap->setToolTip(tr("Поменять цвета местами (X)")); swap->setShortcut(QKeySequence("X")); bar->addAction(swap);
    backButton_ = new QPushButton(bar); backButton_->setToolTip(tr("Фоновый цвет и цвет ластика (Back)")); backButton_->setObjectName("backColor"); backButton_->setFixedWidth(34); bar->addWidget(backButton_);
    connect(frontButton_,&QPushButton::clicked,this,[this]{auto c=QColorDialog::getColor(front_,this,tr("Основной цвет — Front"));if(c.isValid()){front_=c;updateColors();}});
    connect(backButton_,&QPushButton::clicked,this,[this]{auto c=QColorDialog::getColor(back_,this,tr("Цвет фона и ластика — Back"));if(c.isValid()){back_=c;updateColors();}});
    connect(swap,&QAction::triggered,this,[this]{qSwap(front_,back_);updateColors();}); updateColors();
    auto *toolBar = new QToolBar(tr("Инструменты"),this); toolBar->setObjectName("toolsToolbar"); toolBar->setMovable(false); toolBar->setIconSize(QSize(24,24)); toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly); addToolBar(Qt::LeftToolBarArea,toolBar);
    auto *group = new QActionGroup(this); group->setExclusive(true);
    QStringList names{tr("Карандаш"),tr("Кисть"),tr("Ластик"),tr("Перемещение холста"),tr("Точка схода")};
    QStringList shortcuts{"B","K","E","H","P"};
    for(int i=0;i<5;++i){
        auto *action = new QAction(toolIcon(i),names[i],this); action->setObjectName(QString("tool%1").arg(i)); action->setCheckable(true); action->setShortcut(QKeySequence(shortcuts[i])); action->setToolTip(names[i]+" ("+shortcuts[i]+")"); group->addAction(action); toolBar->addAction(action); if(i==0)action->setChecked(true);
        connect(action,&QAction::triggered,this,[this,i,names]{activateTool(Canvas::Tool(i),names[i]);});
        if(i==4)perspectiveAction_=action;
    }
    perspectiveDock_ = new QDockWidget(tr("Перспектива · прототип"),this); perspectiveDock_->setObjectName("perspectiveDock"); perspectiveDock_->setAllowedAreas(Qt::LeftDockWidgetArea|Qt::RightDockWidgetArea); perspectiveDock_->setFeatures(QDockWidget::DockWidgetClosable);
    auto *panel = new QWidget; auto *layout = new QVBoxLayout(panel); layout->setContentsMargins(14,14,14,14);
    gridVisible_ = new QCheckBox(tr("Показать направляющие")); gridVisible_->setObjectName("gridVisible"); layout->addWidget(gridVisible_);
    auto *commonGroup = new QGroupBox(tr("Настройки точек схода")); commonGroup->setObjectName("vanishingPointSettings"); auto *commonForm = new QFormLayout(commonGroup);
    rayStep_ = new QDoubleSpinBox; rayStep_->setObjectName("rayStep"); rayStep_->setRange(1,30); rayStep_->setDecimals(1); rayStep_->setSingleStep(1); rayStep_->setSuffix(tr("°")); rayStep_->setKeyboardTracking(false); commonForm->addRow(tr("Угловой шаг"),rayStep_);
    rayGap_ = new QSpinBox; rayGap_->setObjectName("rayGap"); rayGap_->setRange(0,200); rayGap_->setSuffix(tr(" px")); rayGap_->setKeyboardTracking(false); commonForm->addRow(tr("Отступ от точки"),rayGap_);
    rayStartOpacity_ = new QSpinBox; rayStartOpacity_->setObjectName("rayStartOpacity"); rayStartOpacity_->setRange(0,100); rayStartOpacity_->setSuffix(" %"); rayStartOpacity_->setKeyboardTracking(false); commonForm->addRow(tr("Непрозрачность у точки"),rayStartOpacity_);
    rayEndOpacity_ = new QSpinBox; rayEndOpacity_->setObjectName("rayEndOpacity"); rayEndOpacity_->setRange(0,100); rayEndOpacity_->setSuffix(" %"); rayEndOpacity_->setKeyboardTracking(false); commonForm->addRow(tr("Итоговая непрозрачность"),rayEndOpacity_);
    rayFadeLength_ = new QSpinBox; rayFadeLength_->setObjectName("rayFadeLength"); rayFadeLength_->setRange(0,500); rayFadeLength_->setSuffix(tr(" px")); rayFadeLength_->setKeyboardTracking(false); commonForm->addRow(tr("Длина нарастания"),rayFadeLength_);
    auto *defaultButtons = new QHBoxLayout; savePerspectiveDefaultsButton_ = new QPushButton(tr("Сохранить")); savePerspectiveDefaultsButton_->setObjectName("savePerspectiveDefaults"); savePerspectiveDefaultsButton_->setToolTip(tr("Использовать эти пять значений для новых документов"));
    auto *resetDefaults = new QPushButton(tr("Сбросить")); resetDefaults->setObjectName("resetPerspectiveDefaults"); resetDefaults->setToolTip(tr("Вернуть заводские значения")); defaultButtons->addWidget(savePerspectiveDefaultsButton_);defaultButtons->addWidget(resetDefaults);commonForm->addRow(defaultButtons);
    auto *horizonGroup = new QGroupBox(tr("Линия горизонта")); horizonGroup->setObjectName("horizonSettings"); auto *horizonForm = new QFormLayout(horizonGroup);
    horizonColorButton_ = new QPushButton(tr("Выбрать…")); horizonColorButton_->setObjectName("horizonColor"); horizonForm->addRow(tr("Цвет"),horizonColorButton_);
    horizonOpacity_ = new QSpinBox; horizonOpacity_->setObjectName("horizonOpacity"); horizonOpacity_->setRange(0,100); horizonOpacity_->setSuffix(" %"); horizonOpacity_->setKeyboardTracking(false); horizonForm->addRow(tr("Непрозрачность"),horizonOpacity_);
    horizonWidth_ = new QDoubleSpinBox; horizonWidth_->setObjectName("horizonWidth"); horizonWidth_->setRange(0.1,20); horizonWidth_->setDecimals(1); horizonWidth_->setSingleStep(0.5); horizonWidth_->setSuffix(tr(" px")); horizonWidth_->setKeyboardTracking(false); horizonForm->addRow(tr("Ширина"),horizonWidth_);layout->addWidget(horizonGroup);layout->addWidget(commonGroup);
    auto *pointsListGroup = new QGroupBox(tr("Точки схода")); pointsListGroup->setObjectName("vanishingPointsList"); auto *pointsListLayout = new QVBoxLayout(pointsListGroup);
    vanishingPointsList_=new QListWidget;vanishingPointsList_->setObjectName("vanishingPointsListControl");vanishingPointsList_->setSelectionMode(QAbstractItemView::SingleSelection);pointsListLayout->addWidget(vanishingPointsList_);
    auto *pointButtons=new QHBoxLayout;auto *addPointButton=new QPushButton(tr("Добавить"));addPointButton->setObjectName("addVanishingPoint");removePointButton_=new QPushButton(tr("Удалить"));removePointButton_->setObjectName("removeVanishingPoint");pointButtons->addWidget(addPointButton);pointButtons->addWidget(removePointButton_);pointsListLayout->addLayout(pointButtons);
    auto *pointGroup = new QGroupBox(tr("Свойства выбранной точки")); pointGroup->setObjectName("selectedVanishingPointSettings"); auto *pointForm = new QFormLayout(pointGroup);
    selectedPointVisible_=new QCheckBox(tr("Показывать семейство линий"));selectedPointVisible_->setObjectName("selectedPointVisible");pointForm->addRow(selectedPointVisible_);
    gridColorButton_ = new QPushButton(tr("Выбрать…")); gridColorButton_->setObjectName("gridColor"); pointForm->addRow(tr("Цвет лучей"),gridColorButton_);pointsListLayout->addWidget(pointGroup);layout->addWidget(pointsListGroup);
    auto *tip = new QLabel(tr("P — перемещение точки схода и горизонта.\nТочка прилипает к горизонту вблизи него, но может быть свободно снята.\nB — вернуться к карандашу.\n\nНаправляющие не попадают\nв экспорт PNG.")); tip->setWordWrap(true); layout->addSpacing(12); layout->addWidget(tip); layout->addStretch(); perspectiveDock_->setWidget(panel); addDockWidget(Qt::RightDockWidgetArea,perspectiveDock_); perspectiveDock_->hide();
    view->addAction(perspectiveDock_->toggleViewAction());
    connect(gridVisible_,&QCheckBox::toggled,canvas_,&Canvas::setGridVisible);
    connect(rayStep_,qOverload<double>(&QDoubleSpinBox::valueChanged),canvas_,&Canvas::setRayStep);
    connect(rayGap_,qOverload<int>(&QSpinBox::valueChanged),canvas_,&Canvas::setRayGap);
    connect(rayStartOpacity_,qOverload<int>(&QSpinBox::valueChanged),canvas_,&Canvas::setRayStartOpacity);
    connect(rayEndOpacity_,qOverload<int>(&QSpinBox::valueChanged),canvas_,&Canvas::setRayEndOpacity);
    connect(rayFadeLength_,qOverload<int>(&QSpinBox::valueChanged),canvas_,&Canvas::setRayFadeLength);
    connect(savePerspectiveDefaultsButton_,&QPushButton::clicked,this,[this]{savePerspectiveDefaults(canvas_->state());updateState();statusBar()->showMessage(tr("Общие настройки направляющих сохранены"),3000);});
    connect(resetDefaults,&QPushButton::clicked,this,[this]{clearPerspectiveDefaults();canvas_->setRayAppearance(10,12,10,70,50);updateState();statusBar()->showMessage(tr("Восстановлены настройки направляющих по умолчанию"),3000);});
    connect(horizonColorButton_,&QPushButton::clicked,this,[this]{auto c=QColorDialog::getColor(canvas_->state().horizonColor,this,tr("Цвет линии горизонта"));if(c.isValid())canvas_->setHorizonColor(c);});
    connect(horizonOpacity_,qOverload<int>(&QSpinBox::valueChanged),canvas_,&Canvas::setHorizonOpacity);
    connect(horizonWidth_,qOverload<double>(&QDoubleSpinBox::valueChanged),canvas_,&Canvas::setHorizonWidth);
    connect(vanishingPointsList_,&QListWidget::currentRowChanged,canvas_,&Canvas::selectPoint);
    connect(canvas_,&Canvas::selectedPointChanged,this,[this](int index){QSignalBlocker block(vanishingPointsList_);vanishingPointsList_->setCurrentRow(index);updateState();});
    connect(addPointButton,&QPushButton::clicked,canvas_,&Canvas::addVanishingPoint);
    connect(removePointButton_,&QPushButton::clicked,canvas_,&Canvas::removeSelectedVanishingPoint);
    connect(selectedPointVisible_,&QCheckBox::toggled,canvas_,&Canvas::setSelectedPointVisible);
    connect(gridColorButton_,&QPushButton::clicked,this,[this]{const int i=canvas_->selectedPointIndex();if(i<0)return;auto c=QColorDialog::getColor(canvas_->state().vanishingPoints[i].color,this,tr("Цвет направляющих"));if(c.isValid())canvas_->setSelectedPointColor(c);});
    auto *fitAction = view->addAction(style()->standardIcon(QStyle::SP_TitleBarMaxButton),tr("Вписать холст"),canvas_,&Canvas::fit,QKeySequence("Ctrl+0"));fitAction->setToolTip(tr("Вписать холст (Ctrl+0)"));
    auto *actualAction = view->addAction(actualSizeIcon(),tr("Масштаб 100%"),this,[this]{canvas_->setZoom(1);},QKeySequence("Ctrl+1"));actualAction->setToolTip(tr("Масштаб 100% (Ctrl+1)"));
    view->addAction(tr("Увеличить"),this,[this]{canvas_->setZoom(canvas_->zoom()*1.2);},QKeySequence("Ctrl++"));
    view->addAction(tr("Уменьшить"),this,[this]{canvas_->setZoom(canvas_->zoom()/1.2);},QKeySequence("Ctrl+-"));
    help->addAction(tr("Управление"),this,[this]{QMessageBox::information(this,tr("TechDraw — управление"),tr("B — карандаш\nK — кисть\nE — ластик (цвет Back)\nH — перемещение холста\nP — точка схода\nX — поменять Front и Back\n\nShift + щелчок — отрезок от последней точки\nCtrl + Shift — привязка угла по 15°\nКолесо — масштаб под курсором\nСредняя кнопка или Пробел + мышь — перемещение\nCtrl+Z / Ctrl+Y — отмена / повтор\nCtrl+0 — вписать, Ctrl+1 — 100%\n\nПроект .drw хранит PNG, координаты точки схода и горизонта.\nЭкспорт PNG сохраняет только рисунок."));});
    toolLabel_ = new QLabel(tr("Карандаш")); sizeLabel_ = new QLabel; positionLabel_ = new QLabel; positionLabel_->setMinimumWidth(125);
    statusBar()->addWidget(toolLabel_); statusBar()->addWidget(sizeLabel_); statusBar()->addWidget(positionLabel_,1);
    auto *fitButton = new QToolButton; fitButton->setObjectName("fitButton");fitButton->setToolButtonStyle(Qt::ToolButtonIconOnly);fitButton->setDefaultAction(fitAction); statusBar()->addPermanentWidget(fitButton);
    auto *actualButton = new QToolButton; actualButton->setObjectName("actualSizeButton");actualButton->setToolButtonStyle(Qt::ToolButtonIconOnly);actualButton->setDefaultAction(actualAction); statusBar()->addPermanentWidget(actualButton);
    zoom_ = new QDoubleSpinBox; zoom_->setObjectName("zoomPercent"); zoom_->setRange(5,1600); zoom_->setDecimals(0); zoom_->setSuffix(" %"); zoom_->setKeyboardTracking(false); statusBar()->addPermanentWidget(zoom_);
    connect(zoom_,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this](double value){canvas_->setZoom(value/100.0);});
    connect(canvas_,&Canvas::viewChanged,this,[this]{QSignalBlocker block(zoom_);zoom_->setValue(canvas_->zoom()*100);});
    connect(canvas_,&Canvas::positionChanged,this,[this](QPointF p){positionLabel_->setText(QString("X: %1   Y: %2").arg(qRound(p.x())).arg(qRound(p.y())));});
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
    setWindowTitle(name+"[*] — TechDraw"); setWindowModified(!canvas_->undoStack()->isClean());
    sizeLabel_->setText(QString("%1 × %2 px").arg(canvas_->state().image.width()).arg(canvas_->state().image.height()));
    QSignalBlocker a(gridVisible_),b(rayStep_),c(rayGap_),d(rayStartOpacity_),e(rayEndOpacity_),f(rayFadeLength_),g(horizonOpacity_),h(horizonWidth_),i(vanishingPointsList_),j(selectedPointVisible_);
    gridVisible_->setChecked(canvas_->state().gridVisible); rayStep_->setValue(canvas_->state().rayStepDegrees); rayGap_->setValue(canvas_->state().rayGap);
    rayStartOpacity_->setValue(canvas_->state().rayStartOpacity); rayEndOpacity_->setValue(canvas_->state().rayEndOpacity); rayFadeLength_->setValue(canvas_->state().rayFadeLength);
    savePerspectiveDefaultsButton_->setEnabled(!matchesPerspectiveDefaults(canvas_->state()));
    horizonOpacity_->setValue(canvas_->state().horizonOpacity);horizonWidth_->setValue(canvas_->state().horizonWidth);
    const auto &points=canvas_->state().vanishingPoints;
    bool rebuild=vanishingPointsList_->count()!=points.size();
    if(!rebuild)for(int row=0;row<points.size();++row)if(vanishingPointsList_->item(row)->data(Qt::UserRole).toString()!=points[row].id){rebuild=true;break;}
    if(rebuild){vanishingPointsList_->clear();for(int row=0;row<points.size();++row){auto *item=new QListWidgetItem(tr("Точка схода %1").arg(row+1));item->setData(Qt::UserRole,points[row].id);vanishingPointsList_->addItem(item);}}
    const int selected=canvas_->selectedPointIndex();vanishingPointsList_->setCurrentRow(selected);const bool hasPoint=selected>=0&&selected<points.size();
    removePointButton_->setEnabled(hasPoint);selectedPointVisible_->setEnabled(hasPoint);gridColorButton_->setEnabled(hasPoint);
    selectedPointVisible_->setChecked(hasPoint&&points[selected].visible);colorSwatch(gridColorButton_,hasPoint?points[selected].color:QColor("#d0d0d0"));colorSwatch(horizonColorButton_,canvas_->state().horizonColor);
}
void MainWindow::showError(const QString &error){ QMessageBox::critical(this,tr("TechDraw"),error); }
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
    DrawingState state;state.image=QImage(w.value(),h.value(),QImage::Format_ARGB32_Premultiplied);if(state.image.isNull()){showError(tr("Не удалось выделить память для холста."));return;}state.image.fill(back_);state.horizonY=h.value()/2.0;state.vanishingPoints.append({QStringLiteral("vp-1"),QPointF(w.value()/2.0,h.value()/2.0),QStringLiteral("construction"),QStringLiteral("horizon")});applySavedPerspectiveDefaults(&state);
    if(!confirmDiscard())return;
    settings.setValue("canvas/newWidth",w.value());settings.setValue("canvas/newHeight",h.value());
    path_.clear();canvas_->setDocument(state,false);canvas_->fit();
}
void MainWindow::openDocument(){QString path=QFileDialog::getOpenFileName(this,tr("Открыть"),rememberedDirectory("files/openDirectory"),tr("TechDraw и PNG (*.drw *.png);;TechDraw (*.drw);;PNG (*.png)"));if(!path.isEmpty())openPath(path);}
bool MainWindow::openPath(const QString &path){
    DrawingState state;DrawingHistory history;QString error;bool project=path.endsWith(".drw",Qt::CaseInsensitive);
    if(project){if(!Project::load(path,&history,&error)){showError(error);return false;}for(auto &historyState:history.states)applySavedPerspectiveDefaults(&historyState);}
    else{if(!Project::loadPng(path,&state.image,&error)){showError(error);return false;}state.horizonY=state.image.height()/2.0;state.vanishingPoints.append({QStringLiteral("vp-1"),QPointF(state.image.width()/2.0,state.image.height()/2.0),QStringLiteral("construction"),QStringLiteral("horizon")});applySavedPerspectiveDefaults(&state);}
    if(!confirmDiscard())return false;
    path_=project?QFileInfo(path).absoluteFilePath():QString();
    if(project)canvas_->setDocument(history,true);else canvas_->setDocument(state,false);
    rememberDirectory("files/openDirectory",path);addRecentFile(path);canvas_->fit();return true;
}
bool MainWindow::saveDocument(bool saveAs){
    QString target=path_;
    if(saveAs||target.isEmpty()){
        const QString name=target.isEmpty()?tr("Без имени.drw"):QFileInfo(target).fileName();
        target=QFileDialog::getSaveFileName(this,tr("Сохранить проект"),suggestedFile("files/saveDirectory",name),tr("TechDraw (*.drw)"));
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
