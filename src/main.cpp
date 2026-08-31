#include <QApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>
#include "config/appsettings.h"
#include "core/snapshotdb.h"
#include "core/vulkancontext.h"
#include "ui/mainwindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(AppSettings::applicationId());
    app.setOrganizationName(AppSettings::organizationName());
    app.setOrganizationDomain(AppSettings::organizationDomain());

    VulkanContext::instance().initializeInstance();

    // Initialize snapshot database at startup
    SnapshotDatabase::instance().init();

    QTranslator translator;
    if (translator.load(QLocale::system(), "ottersnap", "_", ":/translations")) {
        app.installTranslator(&translator);
    }

    MainWindow window;
    window.show();

    return app.exec();
}
