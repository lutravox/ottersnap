#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>

// Runs before main(), so every QStandardPaths and QSettings use in the test
// process resolves to the temporary directory instead of the user's real
// ones.
namespace {
struct TestPathRedirector {
    QTemporaryDir dir;

    TestPathRedirector() {
        if (!dir.isValid())
            return;

        const QString root = dir.path();
        qputenv("XDG_DATA_HOME", (root + "/share").toUtf8());
        qputenv("XDG_CACHE_HOME", (root + "/cache").toUtf8());
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, root + "/config");
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, root + "/config");
    }
};

TestPathRedirector g_testPathRedirector;
} // namespace
