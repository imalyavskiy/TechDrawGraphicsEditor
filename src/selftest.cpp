#include "selftest.h"
#include "mainwindow.h"
#include <QtWidgets>
#include <private/qzipwriter_p.h>
#include <stdexcept>

namespace {
QString tracePath;
void require(bool value,const char *message){QFile trace(tracePath);if(trace.open(QIODevice::Append)){trace.write(value?"PASS ":"FAIL ");trace.write(message);trace.write("\n");}if(!value)throw std::runtime_error(message);}
void mouse(Canvas *canvas,QEvent::Type type,QPointF position,Qt::MouseButton button,Qt::MouseButtons buttons,Qt::KeyboardModifiers modifiers=Qt::NoModifier){QMouseEvent event(type,position,button,buttons,modifiers);QApplication::sendEvent(canvas,&event);}
void drag(Canvas *canvas,QPointF a,QPointF b){mouse(canvas,QEvent::MouseButtonPress,canvas->toView(a),Qt::LeftButton,Qt::LeftButton);mouse(canvas,QEvent::MouseMove,canvas->toView(b),Qt::NoButton,Qt::LeftButton);mouse(canvas,QEvent::MouseButtonRelease,canvas->toView(b),Qt::LeftButton,Qt::NoButton);}
void click(Canvas *canvas,QPointF point,Qt::KeyboardModifiers modifiers=Qt::NoModifier){mouse(canvas,QEvent::MouseButtonPress,canvas->toView(point),Qt::LeftButton,Qt::LeftButton,modifiers);mouse(canvas,QEvent::MouseButtonRelease,canvas->toView(point),Qt::LeftButton,Qt::NoButton,modifiers);}
}

int runSelfTests(const QString &outputDirectory){
    try{
        if(QGuiApplication::platformName()=="offscreen")QApplication::setStyle("Fusion");
        QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR")+"/Fonts/segoeui.ttf");
        QDir().mkpath(outputDirectory);QDir out(outputDirectory);
        QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,out.filePath("settings"));QSettings().clear();
        tracePath=out.filePath("trace.txt");{QFile trace(tracePath);trace.open(QIODevice::WriteOnly);}
        QTimer watchdog;watchdog.setSingleShot(true);watchdog.setInterval(12000);QObject::connect(&watchdog,&QTimer::timeout,[]{QFile trace(tracePath);trace.open(QIODevice::Append);trace.write("TIMEOUT\n");for(auto *widget:QApplication::topLevelWidgets()){trace.write(widget->metaObject()->className());trace.write(" ");trace.write(widget->windowTitle().toUtf8());trace.write("\n");}trace.close();std::exit(2);});watchdog.start();
        MainWindow window;window.show();QApplication::processEvents();
        Canvas *canvas=window.canvas();
        auto *mainToolbar=window.findChild<QToolBar*>("mainToolbar");auto *toolsToolbar=window.findChild<QToolBar*>("toolsToolbar");
        require(mainToolbar&&toolsToolbar&&mainToolbar->toolButtonStyle()==Qt::ToolButtonIconOnly&&toolsToolbar->toolButtonStyle()==Qt::ToolButtonIconOnly,"toolbars must show icons only");
        auto *frontButton=window.findChild<QPushButton*>("frontColor");auto *backButton=window.findChild<QPushButton*>("backColor");
        require(frontButton&&backButton&&frontButton->text().isEmpty()&&backButton->text().isEmpty()&&!frontButton->toolTip().isEmpty()&&!backButton->toolTip().isEmpty(),"color buttons must use tooltips instead of labels");
        auto *fitButton=window.findChild<QToolButton*>("fitButton");auto *actualButton=window.findChild<QToolButton*>("actualSizeButton");
        require(fitButton&&actualButton&&fitButton->toolButtonStyle()==Qt::ToolButtonIconOnly&&actualButton->toolButtonStyle()==Qt::ToolButtonIconOnly&&!fitButton->toolTip().isEmpty()&&!actualButton->toolTip().isEmpty(),"status buttons must show icons and tooltips");
        DrawingState initial;initial.image=QImage(1000,620,QImage::Format_ARGB32_Premultiplied);initial.image.fill(Qt::white);initial.vanishing=QPointF(650,240);
        canvas->setDocument(initial);canvas->fit();
        require(canvas->undoStack()->isClean(),"initial document must be clean");
        canvas->setStrokeWidth(9);canvas->setFront(QColor("#26364a"));
        drag(canvas,QPointF(120,100),QPointF(280,100));
        require(canvas->state().image.pixelColor(200,100)==QColor("#26364a"),"pencil stroke did not reach image");
        require(!canvas->undoStack()->isClean(),"stroke must make document dirty");
        canvas->undoStack()->undo();require(canvas->state().image==initial.image,"undo must restore pixels");require(canvas->undoStack()->isClean(),"undo back to saved image must be clean");
        canvas->undoStack()->redo();require(canvas->state().image!=initial.image,"redo must restore stroke");
        canvas->setBack(QColor("#e8cf9b"));canvas->setTool(Canvas::Eraser);drag(canvas,QPointF(190,95),QPointF(190,105));require(canvas->state().image.pixelColor(190,100)==QColor("#e8cf9b"),"eraser must use Back");
        canvas->setDocument(initial);canvas->setTool(Canvas::Pencil);canvas->setFront(QColor("#26364a"));canvas->setStrokeWidth(3);
        click(canvas,{100,200});click(canvas,{200,200},Qt::ShiftModifier);click(canvas,{200,260},Qt::ShiftModifier);
        require(canvas->state().image.pixelColor(150,200)==QColor("#26364a")&&canvas->state().image.pixelColor(200,230)==QColor("#26364a"),"shift clicks must create connected segments");
        canvas->undoStack()->undo();require(canvas->state().image.pixelColor(150,200)==QColor("#26364a")&&canvas->state().image.pixelColor(200,230)==QColor(Qt::white),"each connected segment must be separately undoable");canvas->undoStack()->redo();
        click(canvas,{300,300});click(canvas,{390,310},Qt::ShiftModifier|Qt::ControlModifier);
        require(canvas->state().image.pixelColor(350,300)==QColor("#26364a"),"ctrl shift must constrain segment angle");
        canvas->setDocument(initial);canvas->setTool(Canvas::Brush);canvas->setFront(QColor("#6c3f88"));canvas->setStrokeWidth(11);drag(canvas,{110,180},{210,180});
        require(canvas->state().image.pixelColor(160,180)==QColor("#6c3f88"),"brush stroke did not reach image");
        canvas->setBack(QColor("#e8cf9b"));canvas->setTool(Canvas::Eraser);click(canvas,{110,180});click(canvas,{210,180},Qt::ShiftModifier);
        require(canvas->state().image.pixelColor(160,180)==QColor("#e8cf9b"),"eraser straight segment must use Back");
        const int undoIndex=canvas->undoStack()->index();const QImage pixels=canvas->state().image;
        canvas->setZoom(1.7,QPointF(100,150));require(canvas->state().image==pixels&&canvas->undoStack()->index()==undoIndex,"zoom must not edit document");
        QPointF point(100,100);require(QLineF(canvas->toImage(canvas->toView(point)),point).length()<0.001,"view coordinate roundtrip failed");
        canvas->setTool(Canvas::Pan);drag(canvas,QPointF(100,100),QPointF(150,130));require(canvas->state().image==pixels&&canvas->undoStack()->index()==undoIndex,"pan must not edit document");
        canvas->fit();canvas->setGridVisible(true);canvas->setTool(Canvas::Perspective);drag(canvas,QPointF(650,240),QPointF(600,210));require(QLineF(canvas->state().vanishing,QPointF(600,210)).length()<0.01,"perspective point did not move");require(canvas->state().image==pixels,"perspective must not alter pixels");
        canvas->undoStack()->undo();require(canvas->state().vanishing==QPointF(650,240),"perspective undo failed");canvas->undoStack()->redo();
        canvas->undoStack()->undo();require(canvas->undoStack()->canUndo()&&canvas->undoStack()->canRedo(),"history fixture must have undo and redo operations");
        const DrawingHistory savedHistory=canvas->history();
        QString error;const auto projectPath=out.filePath("roundtrip.drw");require(Project::save(projectPath,savedHistory,&error),qPrintable(error));
        DrawingHistory loadedHistory;require(Project::load(projectPath,&loadedHistory,&error),qPrintable(error));
        require(loadedHistory.states.size()==savedHistory.states.size()&&loadedHistory.labels==savedHistory.labels&&loadedHistory.index==savedHistory.index,"DRW history index or labels mismatch");
        Canvas restored;restored.setDocument(loadedHistory,true);const DrawingState restoredCurrent=restored.state();const QString undoLabel=restored.undoStack()->undoText();const QString redoLabel=restored.undoStack()->redoText();
        require(restored.undoStack()->isClean()&&restored.undoStack()->canUndo()&&restored.undoStack()->canRedo()&&!undoLabel.isEmpty()&&!redoLabel.isEmpty(),"restored history must preserve clean index and both directions");
        restored.undoStack()->undo();require(restored.state().vanishing!=restoredCurrent.vanishing||restored.state().gridVisible!=restoredCurrent.gridVisible||restored.state().image!=restoredCurrent.image,"restored undo did not change state");
        restored.undoStack()->redo();require(restored.state().image==restoredCurrent.image&&restored.state().vanishing==restoredCurrent.vanishing&&restored.undoStack()->isClean(),"restored redo did not return to saved state");
        DrawingState loaded;require(Project::load(projectPath,&loaded,&error),qPrintable(error));require(loaded.image==canvas->state().image&&loaded.vanishing==canvas->state().vanishing&&loaded.gridVisible,"DRW roundtrip mismatch");
        MainWindow pathWindow;require(pathWindow.openPath(projectPath),"path settings fixture did not open");
        require(QDir(QSettings().value("files/openDirectory").toString())==QDir(out.absolutePath())&&!QSettings().contains("files/saveDirectory"),"open and save directories must be independent");
        auto *pathSaveAction=pathWindow.findChild<QAction*>("saveAction");require(pathSaveAction,"save action missing");pathSaveAction->trigger();QApplication::processEvents();
        require(QDir(QSettings().value("files/saveDirectory").toString())==QDir(out.absolutePath()),"successful save did not remember its directory");pathWindow.close();
        QStringList recentPaths;for(int i=0;i<6;++i){const QString path=out.filePath(QString("recent-%1.drw").arg(i));require(Project::save(path,loaded,&error),qPrintable(error));recentPaths.append(QFileInfo(path).absoluteFilePath());}
        MainWindow recentWindow;for(const auto &path:recentPaths)require(recentWindow.openPath(path),"recent file fixture did not open");
        auto *recentMenu=recentWindow.findChild<QMenu*>("recentFilesMenu");require(recentMenu&&recentMenu->actions().size()==5,"recent files menu must contain five entries");
        const QStringList storedRecent=QSettings().value("files/recentFiles").toStringList();
        require(storedRecent.size()==5&&storedRecent.first()==recentPaths.last()&&!storedRecent.contains(recentPaths.first())&&recentMenu->actions().first()->data().toString()==recentPaths.last(),"recent files order, limit, or menu data is wrong");
        MainWindow persistedRecentWindow;auto *persistedRecentMenu=persistedRecentWindow.findChild<QMenu*>("recentFilesMenu");
        require(persistedRecentMenu&&persistedRecentMenu->actions().size()==5&&persistedRecentMenu->actions().first()->data().toString()==recentPaths.last(),"recent files did not persist between windows");recentWindow.close();persistedRecentWindow.close();
        MainWindow widthsWindow;auto *widthControl=widthsWindow.findChild<QSpinBox*>("strokeWidth");auto *pencilAction=widthsWindow.findChild<QAction*>("tool0");auto *brushAction=widthsWindow.findChild<QAction*>("tool1");auto *eraserAction=widthsWindow.findChild<QAction*>("tool2");auto *panAction=widthsWindow.findChild<QAction*>("tool3");
        require(widthControl&&pencilAction&&brushAction&&eraserAction&&panAction,"per-tool width controls are missing");
        widthControl->setValue(4);brushAction->trigger();require(widthControl->value()==3,"brush must start with its own width");widthControl->setValue(11);eraserAction->trigger();require(widthControl->value()==3,"eraser must start with its own width");widthControl->setValue(17);
        pencilAction->trigger();require(widthControl->value()==4,"pencil width was not restored");brushAction->trigger();require(widthControl->value()==11,"brush width was not restored");eraserAction->trigger();require(widthControl->value()==17,"eraser width was not restored");panAction->trigger();require(!widthControl->isEnabled(),"width control must be disabled for a non-paint tool");widthsWindow.close();
        MainWindow persistedWidthsWindow;widthControl=persistedWidthsWindow.findChild<QSpinBox*>("strokeWidth");brushAction=persistedWidthsWindow.findChild<QAction*>("tool1");eraserAction=persistedWidthsWindow.findChild<QAction*>("tool2");
        require(widthControl&&widthControl->value()==4,"pencil width did not persist");brushAction->trigger();require(widthControl->value()==11,"brush width did not persist");eraserAction->trigger();require(widthControl->value()==17,"eraser width did not persist");persistedWidthsWindow.close();
        require(Project::exportPng(out.filePath("export.png"),canvas->state().image,&error),qPrintable(error));QImage png;require(Project::loadPng(out.filePath("export.png"),&png,&error),qPrintable(error));require(png==pixels,"grid leaked into exported PNG");
        DrawingState transparent=loaded;transparent.image.fill(Qt::transparent);transparent.image.setPixelColor(7,8,QColor(40,80,120,128));require(Project::save(out.filePath("alpha.drw"),transparent,&error),qPrintable(error));require(Project::load(out.filePath("alpha.drw"),&loaded,&error),qPrintable(error));require(loaded.image==transparent.image,"alpha roundtrip failed");
        QFile bad(out.filePath("bad.drw"));bad.open(QIODevice::WriteOnly);bad.write("not a zip");bad.close();DrawingState unchanged=loaded;require(!Project::load(bad.fileName(),&loaded,&error),"bad project accepted");require(loaded.image==unchanged.image,"failed load modified destination");
        require(!Project::save(out.filePath("missing/fail.drw"),loaded,&error),"save to missing directory should fail");
        QZipWriter invalid(out.filePath("version.drw"));invalid.addFile("drawing.png",QByteArray("bad"));invalid.addFile("project.json",QByteArray("{\"format\":\"Drawing\",\"version\":99,\"image\":\"drawing.png\"}"));invalid.close();require(!Project::load(out.filePath("version.drw"),&loaded,&error),"unknown project version accepted");
        QByteArray legacyPng;QBuffer legacyBuffer(&legacyPng);legacyBuffer.open(QIODevice::WriteOnly);require(initial.image.save(&legacyBuffer,"PNG"),"legacy PNG encoding failed");
        QJsonObject legacyPerspective{{"visible",false},{"x",650.0},{"y",240.0},{"rays",16},{"color",QStringLiteral("#ff628ed1")}};
        QJsonObject legacyMetadata{{"format","Drawing"},{"version",1},{"width",1000},{"height",620},{"image","drawing.png"},{"perspective",legacyPerspective}};
        QZipWriter legacy(out.filePath("legacy-v1.drw"));legacy.addFile("drawing.png",legacyPng);legacy.addFile("project.json",QJsonDocument(legacyMetadata).toJson());legacy.close();
        DrawingHistory legacyHistory;require(Project::load(out.filePath("legacy-v1.drw"),&legacyHistory,&error)&&legacyHistory.states.size()==1&&legacyHistory.index==0&&legacyHistory.labels.isEmpty(),"version 1 project compatibility failed");
        require(!Project::validSize(QSize(8192,8192))&&!Project::validSize(QSize(-1,10)),"invalid canvas size accepted");
        if(QGuiApplication::platformName()!="offscreen"){
        bool cancelClicked=false;
        QTimer::singleShot(0,&window,[&]{auto *box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());if(box){for(auto *button:box->buttons())if(button->text()==QStringLiteral("Отмена")){cancelClicked=true;button->click();return;}box->reject();}});
        require(!window.close()&&cancelClicked&&window.isVisible(),"cancel must prevent closing dirty document");
        canvas->undoStack()->setClean();
        require(window.openPath(projectPath)&&canvas->undoStack()->canUndo()&&canvas->undoStack()->canRedo(),"opening saved project did not restore history");
        canvas->setTool(Canvas::Pencil);drag(canvas,{40,40},{90,40});
        const QImage beforeFailedOpen=canvas->state().image;
        QTimer::singleShot(0,&window,[]{auto *box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());if(box)box->accept();});
        require(!window.openPath(bad.fileName())&&canvas->state().image==beforeFailedOpen&&!canvas->undoStack()->isClean(),"failed open must preserve dirty document");
        bool saveClicked=false;
        QTimer::singleShot(0,&window,[&]{auto *box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());if(box){for(auto *button:box->buttons())if(button->text()==QStringLiteral("Сохранить")){saveClicked=true;button->click();return;}box->reject();}});
        require(window.close()&&saveClicked,"save before closing failed");
        require(Project::load(projectPath,&loaded,&error)&&loaded.image==beforeFailedOpen,"save on close lost changes");
        require(Project::load(projectPath,&loadedHistory,&error)&&loadedHistory.index==loadedHistory.labels.size()&&loadedHistory.labels.size()>1,"save on close lost undo history or retained discarded redo branch");
        window.show();QApplication::processEvents();
        }
        canvas->setDocument(initial);canvas->setTool(Canvas::Pencil);canvas->setFront(QColor("#526070"));canvas->setStrokeWidth(2);canvas->fit();
        const QVector<QPointF> house{{230,450},{230,265},{400,173},{563,270},{563,451},{230,450}};
        for(int i=1;i<house.size();++i)drag(canvas,house[i-1],house[i]);
        drag(canvas,{230,265},{400,352});drag(canvas,{400,352},{563,270});drag(canvas,{400,173},{400,352});drag(canvas,{400,352},{563,451});drag(canvas,{442,380},{442,316});drag(canvas,{442,316},{488,294});drag(canvas,{488,294},{488,409});drag(canvas,{183,472},{600,472});
        require(Project::save(out.filePath("Набросок.drw"),canvas->state(),&error),qPrintable(error));
        QApplication::processEvents();require(window.grab().save(out.filePath("editor.png")),"screenshot failed");
        window.resize(720,480);QApplication::processEvents();require(window.grab().save(out.filePath("editor-small.png")),"small screenshot failed");window.resize(1200,800);QApplication::processEvents();
        auto *action=window.findChild<QAction*>("tool4");require(action,"perspective action missing");action->trigger();QApplication::processEvents();require(window.grab().save(out.filePath("perspective.png")),"perspective screenshot failed");
        canvas->undoStack()->setClean();window.close();
        QFile report(out.filePath("result.txt"));report.open(QIODevice::WriteOnly);report.write("PASS: pencil, brush, eraser, connected segments, angle constraint, persistent undo/redo history, DRW v1 compatibility, pan/zoom, perspective, DRW/PNG, alpha, invalid input, screenshots\n");
        report.write(QGuiApplication::platformName()=="offscreen"?"SKIP: native dialog tests (run with -platform windows)\n":"PASS: close cancel/save, failed open preserves document\n");
        return 0;
    }catch(const std::exception &error){qCritical()<<"SELF_TEST_FAILED:"<<error.what();return 1;}
}
