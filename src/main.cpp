#include <QApplication>
#include <QIcon>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>
#include "config/appsettings.h"
#include "core/snapshotdb.h"
#include "core/vulkancontext.h"
#include "ui/mainwindow.h"

namespace {

void messageHandler(QtMsgType type, const QMessageLogContext &, const QString &msg) {
    if ((type == QtDebugMsg || type == QtInfoMsg) && !qEnvironmentVariableIsSet("OTTERSNAP_DEBUG"))
        return;
    fprintf(stderr, "%s\n", msg.toLocal8Bit().constData());
}

} // namespace

int main(int argc, char *argv[]) {
    qInstallMessageHandler(messageHandler);

    QApplication app(argc, argv);
    app.setApplicationName(AppSettings::applicationId());
    app.setOrganizationName(AppSettings::organizationName());
    app.setOrganizationDomain(AppSettings::organizationDomain());
    app.setWindowIcon(QIcon(":/icons/ottersnap.svg"));

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
