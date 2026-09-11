#include "selftest.h"
#include "mainwindow.h"
#include <QtWidgets>
#include <private/qzipwriter_p.h>
#include <stdexcept>

namespace {
QString tracePath;
void require(bool value,const char *message){QFile trace(tracePath);if(trace.open(QIODevice::Append)){trace.write(value?"PASS ":"FAIL ");trace.write(message);trace.write("\n");}if(!value)throw std::runtime_error(message);}
void mouse(Canvas *canvas,QEvent::Type type,QPointF position,Qt::MouseButton button,Qt::MouseButtons buttons){QMouseEvent event(type,position,button,buttons,Qt::NoModifier);QApplication::sendEvent(canvas,&event);}
void drag(Canvas *canvas,QPointF a,QPointF b){mouse(canvas,QEvent::MouseButtonPress,canvas->toView(a),Qt::LeftButton,Qt::LeftButton);mouse(canvas,QEvent::MouseMove,canvas->toView(b),Qt::NoButton,Qt::LeftButton);mouse(canvas,QEvent::MouseButtonRelease,canvas->toView(b),Qt::LeftButton,Qt::NoButton);}
}

int runSelfTests(const QString &outputDirectory){
    try{
        if(QGuiApplication::platformName()=="offscreen")QApplication::setStyle("Fusion");
        QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR")+"/Fonts/segoeui.ttf");
        QDir().mkpath(outputDirectory);QDir out(outputDirectory);
        tracePath=out.filePath("trace.txt");{QFile trace(tracePath);trace.open(QIODevice::WriteOnly);}
        QTimer watchdog;watchdog.setSingleShot(true);watchdog.setInterval(12000);QObject::connect(&watchdog,&QTimer::timeout,[]{QFile trace(tracePath);trace.open(QIODevice::Append);trace.write("TIMEOUT\n");for(auto *widget:QApplication::topLevelWidgets()){trace.write(widget->metaObject()->className());trace.write(" ");trace.write(widget->windowTitle().toUtf8());trace.write("\n");}trace.close();std::exit(2);});watchdog.start();
        MainWindow window;window.show();QApplication::processEvents();
        Canvas *canvas=window.canvas();
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
        const int undoIndex=canvas->undoStack()->index();const QImage pixels=canvas->state().image;
        canvas->setZoom(1.7,QPointF(100,150));require(canvas->state().image==pixels&&canvas->undoStack()->index()==undoIndex,"zoom must not edit document");
        QPointF point(100,100);require(QLineF(canvas->toImage(canvas->toView(point)),point).length()<0.001,"view coordinate roundtrip failed");
        canvas->setTool(Canvas::Pan);drag(canvas,QPointF(100,100),QPointF(150,130));require(canvas->state().image==pixels&&canvas->undoStack()->index()==undoIndex,"pan must not edit document");
        canvas->fit();canvas->setGridVisible(true);canvas->setTool(Canvas::Perspective);drag(canvas,QPointF(650,240),QPointF(600,210));require(QLineF(canvas->state().vanishing,QPointF(600,210)).length()<0.01,"perspective point did not move");require(canvas->state().image==pixels,"perspective must not alter pixels");
        canvas->undoStack()->undo();require(canvas->state().vanishing==QPointF(650,240),"perspective undo failed");canvas->undoStack()->redo();
        QString error;const auto projectPath=out.filePath("roundtrip.drw");require(Project::save(projectPath,canvas->state(),&error),qPrintable(error));DrawingState loaded;require(Project::load(projectPath,&loaded,&error),qPrintable(error));require(loaded.image==canvas->state().image&&loaded.vanishing==canvas->state().vanishing&&loaded.gridVisible,"DRW roundtrip mismatch");
        require(Project::exportPng(out.filePath("export.png"),canvas->state().image,&error),qPrintable(error));QImage png;require(Project::loadPng(out.filePath("export.png"),&png,&error),qPrintable(error));require(png==pixels,"grid leaked into exported PNG");
        DrawingState transparent=loaded;transparent.image.fill(Qt::transparent);transparent.image.setPixelColor(7,8,QColor(40,80,120,128));require(Project::save(out.filePath("alpha.drw"),transparent,&error),qPrintable(error));require(Project::load(out.filePath("alpha.drw"),&loaded,&error),qPrintable(error));require(loaded.image==transparent.image,"alpha roundtrip failed");
        QFile bad(out.filePath("bad.drw"));bad.open(QIODevice::WriteOnly);bad.write("not a zip");bad.close();DrawingState unchanged=loaded;require(!Project::load(bad.fileName(),&loaded,&error),"bad project accepted");require(loaded.image==unchanged.image,"failed load modified destination");
        require(!Project::save(out.filePath("missing/fail.drw"),loaded,&error),"save to missing directory should fail");
        QZipWriter invalid(out.filePath("version.drw"));invalid.addFile("drawing.png",QByteArray("bad"));invalid.addFile("project.json",QByteArray("{\"format\":\"Drawing\",\"version\":99,\"image\":\"drawing.png\"}"));invalid.close();require(!Project::load(out.filePath("version.drw"),&loaded,&error),"unknown project version accepted");
        require(!Project::validSize(QSize(8192,8192))&&!Project::validSize(QSize(-1,10)),"invalid canvas size accepted");
        if(QGuiApplication::platformName()!="offscreen"){
        bool cancelClicked=false;
        QTimer::singleShot(0,&window,[&]{auto *box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());if(box){for(auto *button:box->buttons())if(button->text()==QStringLiteral("Отмена")){cancelClicked=true;button->click();return;}box->reject();}});
        require(!window.close()&&cancelClicked&&window.isVisible(),"cancel must prevent closing dirty document");
        canvas->undoStack()->setClean();
        require(window.openPath(projectPath),"opening saved project failed");
        canvas->setTool(Canvas::Pencil);drag(canvas,{40,40},{90,40});
        const QImage beforeFailedOpen=canvas->state().image;
        QTimer::singleShot(0,&window,[]{auto *box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());if(box)box->accept();});
        require(!window.openPath(bad.fileName())&&canvas->state().image==beforeFailedOpen&&!canvas->undoStack()->isClean(),"failed open must preserve dirty document");
        bool saveClicked=false;
        QTimer::singleShot(0,&window,[&]{auto *box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());if(box){for(auto *button:box->buttons())if(button->text()==QStringLiteral("Сохранить")){saveClicked=true;button->click();return;}box->reject();}});
        require(window.close()&&saveClicked,"save before closing failed");
        require(Project::load(projectPath,&loaded,&error)&&loaded.image==beforeFailedOpen,"save on close lost changes");
        window.show();QApplication::processEvents();
        }
        canvas->setDocument(initial);canvas->setTool(Canvas::Pencil);canvas->setFront(QColor("#526070"));canvas->setStrokeWidth(2);canvas->fit();
        const QVector<QPointF> house{{230,450},{230,265},{400,173},{563,270},{563,451},{230,450}};
        for(int i=1;i<house.size();++i)drag(canvas,house[i-1],house[i]);
        drag(canvas,{230,265},{400,352});drag(canvas,{400,352},{563,270});drag(canvas,{400,173},{400,352});drag(canvas,{400,352},{563,451});drag(canvas,{442,380},{442,316});drag(canvas,{442,316},{488,294});drag(canvas,{488,294},{488,409});drag(canvas,{183,472},{600,472});
        require(Project::save(out.filePath("Набросок.drw"),canvas->state(),&error),qPrintable(error));
        QApplication::processEvents();require(window.grab().save(out.filePath("editor.png")),"screenshot failed");
        window.resize(720,480);QApplication::processEvents();require(window.grab().save(out.filePath("editor-small.png")),"small screenshot failed");window.resize(1200,800);QApplication::processEvents();
        auto *action=window.findChild<QAction*>("tool3");require(action,"perspective action missing");action->trigger();QApplication::processEvents();require(window.grab().save(out.filePath("perspective.png")),"perspective screenshot failed");
        canvas->undoStack()->setClean();window.close();
        QFile report(out.filePath("result.txt"));report.open(QIODevice::WriteOnly);report.write("PASS: pencil, eraser, undo/redo, pan/zoom, perspective, DRW/PNG, alpha, invalid input, screenshots\n");
        report.write(QGuiApplication::platformName()=="offscreen"?"SKIP: native dialog tests (run with -platform windows)\n":"PASS: close cancel/save, failed open preserves document\n");
        return 0;
    }catch(const std::exception &error){qCritical()<<"SELF_TEST_FAILED:"<<error.what();return 1;}
}
