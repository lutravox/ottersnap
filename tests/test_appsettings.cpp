#include <QCoreApplication>
#include <QtTest>
#include "config/appsettings.h"

class TestAppSettings : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase();

    void testResizeToFit();
    void testRestoreSession();
    void testAutosaveSnapshots();
    void testMaxSnapshotCacheSize();
    void testIdentity();
};

void TestAppSettings::initTestCase() {
    // Use a unique organization so test writes don't collide with
    // the real application's QSettings.
    QCoreApplication::setOrganizationName("OttersnapTest");
    QCoreApplication::setApplicationName("test_appsettings");
}

void TestAppSettings::testResizeToFit() {
    AppSettings::setResizeToFit(true);
    QCOMPARE(AppSettings::resizeToFit(), true);
    AppSettings::setResizeToFit(false);
    QCOMPARE(AppSettings::resizeToFit(), false);
}

void TestAppSettings::testRestoreSession() {
    AppSettings::setRestoreSession(true);
    QCOMPARE(AppSettings::restoreSession(), true);
    AppSettings::setRestoreSession(false);
    QCOMPARE(AppSettings::restoreSession(), false);
}

void TestAppSettings::testAutosaveSnapshots() {
    AppSettings::setAutosaveSnapshots(true);
    QCOMPARE(AppSettings::autosaveSnapshots(), true);
    AppSettings::setAutosaveSnapshots(false);
    QCOMPARE(AppSettings::autosaveSnapshots(), false);
}

void TestAppSettings::testMaxSnapshotCacheSize() {
    int testSize = 512;
    AppSettings::setMaxSnapshotCacheSizeMB(testSize);
    QCOMPARE(AppSettings::maxSnapshotCacheSizeMB(), testSize);
}

void TestAppSettings::testIdentity() {
    QVERIFY(QString(AppSettings::applicationName()).contains("Ottersnap"));
    QVERIFY(!QString(AppSettings::organizationDomain()).isEmpty());
    QVERIFY(!AppSettings::fileFilter().isEmpty());
}

QTEST_MAIN(TestAppSettings)
#include "test_appsettings.moc"
