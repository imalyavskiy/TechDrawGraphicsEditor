#include "mainwindow.h"
#include "selftest.h"
#include "taskbarpin.h"
#include <QApplication>
#include <QLocale>
#include <QSettings>
#include <QTimer>
#include <QTranslator>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
/// Однократно переносит настройки раннего прототипа, если новая группа ещё пуста.
void migrateLegacySettings() {
    QSettings current;
    if (!current.allKeys().isEmpty())
        return;
    QSettings legacy(QStringLiteral("DrawingPrototype"), QStringLiteral("Drawing"));
    for (const QString &key : legacy.allKeys())
        current.setValue(key, legacy.value(key));
}

/// Загружает наиболее подходящий внешний каталог перевода из каталога рядом с EXE.
void loadTranslation(QApplication &application, QTranslator *translator) {
    const QString translationsDirectory = QCoreApplication::applicationDirPath() + QStringLiteral("/translations");
    const QString locale = QLocale::system().name();
    const QString language = locale.section(QLatin1Char('_'), 0, 0);
    const QStringList catalogs{
        QStringLiteral("techdraw_") + locale, QStringLiteral("techdraw_") + language, QStringLiteral("techdraw_ru")};
    for (const QString &catalog : catalogs)
        if (translator->load(catalog, translationsDirectory)) {
            application.installTranslator(translator);
            return;
        }
}
} // namespace

/// Настраивает приложение, запускает самопроверку либо открывает главное окно.
int main(int argc, char **argv) {
#ifdef Q_OS_WIN
    for (int i = 1; i < argc; ++i)
        if (QByteArray(argv[i]) == "--self-test")
            SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
#endif
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication app(argc, argv);
    app.setOrganizationName("TechDraw");
    app.setApplicationName("TechDraw");
    QTranslator translator;
    loadTranslation(app, &translator);
    app.setApplicationDisplayName(QCoreApplication::translate("MainWindow", "Технический рисунок / Technical Draw"));
    migrateLegacySettings();
    app.setWindowIcon(QIcon(":/app/techdraw.png"));
    QFont font("Segoe UI", 9);
    app.setFont(font);
    const auto args = app.arguments();
    const int test = args.indexOf("--self-test");
    if (test >= 0)
        return runSelfTests(test + 1 < args.size() ? args[test + 1] : "test-results");
    MainWindow window;
    window.show();
    QTimer::singleShot(0, &window, requestPendingTaskbarPin);
    if (args.size() > 1)
        QTimer::singleShot(0, &window, [&window, args] { window.openPath(args[1]); });
    return app.exec();
}
