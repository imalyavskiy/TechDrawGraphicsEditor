#include "mainwindow.h"
#include "selftest.h"
#include <QApplication>
#include <QSettings>
#include <QTimer>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
void migrateLegacySettings() {
    QSettings current;
    if (!current.allKeys().isEmpty()) return;
    QSettings legacy(QStringLiteral("DrawingPrototype"),QStringLiteral("Drawing"));
    for (const QString &key:legacy.allKeys()) current.setValue(key,legacy.value(key));
}
}

int main(int argc, char **argv) {
#ifdef Q_OS_WIN
    for(int i=1;i<argc;++i)if(QByteArray(argv[i])=="--self-test")SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
#endif
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication app(argc,argv);
    app.setOrganizationName("TechDraw");app.setApplicationName("TechDraw");app.setApplicationDisplayName("TechDraw");migrateLegacySettings();
    app.setWindowIcon(QIcon(":/app/techdraw.png"));
    QFont font("Segoe UI",9);app.setFont(font);
    const auto args=app.arguments();
    const int test=args.indexOf("--self-test");
    if(test>=0)return runSelfTests(test+1<args.size()?args[test+1]:"test-results");
    MainWindow window;window.show();
    if(args.size()>1)QTimer::singleShot(0,&window,[&window,args]{window.openPath(args[1]);});
    return app.exec();
}
