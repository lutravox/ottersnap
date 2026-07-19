#include <QApplication>
#include "config/appsettings.h"
#include "core/versionstore.h"
#include "ui/mainwindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(AppSettings::applicationName());
    app.setOrganizationName(AppSettings::organizationName());
    app.setOrganizationDomain(AppSettings::organizationDomain());

    MainWindow window;
    window.show();

    return app.exec();
}
