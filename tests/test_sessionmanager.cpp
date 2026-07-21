#include <QCoreApplication>
#include <QSettings>
#include <QStringList>
#include <QtTest>
#include "core/sessionmanager.h"

class TestSessionManager : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase();
    void cleanupTestCase();
    void testSaveLoadRestore();
    void testEmptyRestore();

  private:
    QSettings *m_settings;
};

void TestSessionManager::initTestCase() {
    // Use a temporary settings file to avoid polluting real user settings
    m_settings = new QSettings("test_session_manager.ini", QSettings::IniFormat);
    m_settings->clear();
}

void TestSessionManager::cleanupTestCase() {
    m_settings->clear();
    delete m_settings;
}

void TestSessionManager::testSaveLoadRestore() {
    QSettings      settings("test_session_manager.ini", QSettings::IniFormat);
    SessionManager manager(settings);
    QStringList    paths = {"/path/1.png", "/path/2.png"};

    manager.save(paths);
    manager.load();
    QStringList restored = manager.restorePaths();

    QCOMPARE(restored.count(), 2);
    QCOMPARE(restored[0], QString("/path/1.png"));
    QCOMPARE(restored[1], QString("/path/2.png"));
}

void TestSessionManager::testEmptyRestore() {
    QSettings settings("test_session_manager.ini", QSettings::IniFormat);
    settings.clear();
    SessionManager manager(settings);

    manager.load();
    QStringList restored = manager.restorePaths();

    QVERIFY(restored.isEmpty());
}

QTEST_MAIN(TestSessionManager)
#include "test_sessionmanager.moc"
