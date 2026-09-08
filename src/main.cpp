#include <QApplication>
#include <QFileInfo>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLibraryInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLocale>
#include <QTranslator>
#include "config/appsettings.h"
#include "core/snapshotdb.h"
#include "core/vulkancontext.h"
#include "ui/mainwindow.h"

namespace {

void messageHandler(QtMsgType type, const QMessageLogContext&, const QString& msg) {
    if ((type == QtDebugMsg || type == QtInfoMsg) && !qEnvironmentVariableIsSet("OTTERSNAP_DEBUG"))
        return;
    fprintf(stderr, "%s\n", msg.toLocal8Bit().constData());
}

constexpr char c_singleInstanceSocket[] = "ottersnap-single-instance";

/// @brief Collects file paths passed on the command line.
QStringList filesFromArgs(const QStringList& args) {
    QStringList files;
    for (int i = 1; i < args.size(); ++i) {
        const QString& arg = args.at(i);
        if (arg.startsWith('-'))
            continue;
        if (QFileInfo::exists(arg))
            files << arg;
    }
    return files;
}

/// @brief Forwards to the running instance if there is one.
bool forwardToRunningInstance(const QStringList& files) {
    QLocalSocket socket;
    socket.connectToServer(c_singleInstanceSocket);
    if (!socket.waitForConnected(1000))
        return false;

    QJsonArray array;
    for (const QString& file : files)
        array.append(file);

    socket.write(QJsonDocument(array).toJson(QJsonDocument::Compact));
    if (!socket.waitForBytesWritten(1000))
        return false;

    socket.disconnectFromServer();
    return true;
}

void handleForwardedFiles(const QByteArray& data, MainWindow& window) {
    const QJsonArray array = QJsonDocument::fromJson(data).array();
    QStringList      files;
    for (const QJsonValue& value : array)
        files << value.toString();
    window.openFiles(files);
}

} // namespace

int main(int argc, char *argv[]) {
    qInstallMessageHandler(messageHandler);

    QApplication app(argc, argv);
    app.setApplicationName(AppSettings::applicationId());
    app.setOrganizationName(AppSettings::organizationName());
    app.setOrganizationDomain(AppSettings::organizationDomain());
    app.setWindowIcon(QIcon(":/icons/ottersnap.svg"));

    const QStringList files = filesFromArgs(app.arguments());

    // Delegate to the running instance so one window handles all files.
    if (forwardToRunningInstance(files))
        return 0;

    VulkanContext::instance().initializeInstance();

    // Initialize snapshot database at startup
    SnapshotDatabase::instance().init();

    QTranslator translator;
    if (translator.load(QLocale::system(), "ottersnap", "_", ":/translations")) {
        app.installTranslator(&translator);
    }

    MainWindow window;

    // Receive files from additional instances launched with file arguments.
    QLocalServer server;
    QLocalServer::removeServer(c_singleInstanceSocket);
    if (server.listen(c_singleInstanceSocket)) {
        QObject::connect(&server, &QLocalServer::newConnection, [&window, &server]() {
            QLocalSocket *connection = server.nextPendingConnection();
            if (!connection)
                return;
            QByteArray data;
            while (!connection->atEnd()) {
                if (!connection->waitForReadyRead(2000))
                    break;
                data += connection->readAll();
            }
            handleForwardedFiles(data, window);
            connection->deleteLater();
        });
    }

    window.openFiles(files);
    window.show();

    return app.exec();
}
