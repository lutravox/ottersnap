#include <QApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>
#include "config/appsettings.h"
#include "core/vulkancontext.h"
#include "ui/mainwindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    VulkanContext::instance().initializeInstance();

    app.setApplicationName(AppSettings::applicationName());
    app.setOrganizationName(AppSettings::organizationName());
    app.setOrganizationDomain(AppSettings::organizationDomain());

    QTranslator translator;
    if (translator.load(QLocale::system(), "ottersnap", "_", ":/translations")) {
        app.installTranslator(&translator);
    }

    MainWindow window;
    window.show();

    return app.exec();
}
