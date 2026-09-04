#include <QCoreApplication>
#include <QtTest>
#include "config/appsettings.h"

class TestAppSettings : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase();

    void testRestoreSession();
    void testAutosaveSnapshots();
    void testAutoreloadImages();
    void testSnapshotOnReopen();
    void testMaxThumbnailCacheSize();
    void testMaxDeltaCacheSize();
    void testBaseInterval();
    void testScaleWithWindow();
    void testToolbarVisible();
    void testBackgroundColor();
    void testIdentity();
    void testRepositoryUrl();
};

void TestAppSettings::initTestCase() {
    QCoreApplication::setOrganizationName("OttersnapTest");
    QCoreApplication::setApplicationName("test_appsettings");
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

void TestAppSettings::testAutoreloadImages() {
    AppSettings::setAutoreloadImages(true);
    QCOMPARE(AppSettings::autoreloadImages(), true);
    AppSettings::setAutoreloadImages(false);
    QCOMPARE(AppSettings::autoreloadImages(), false);
}

void TestAppSettings::testSnapshotOnReopen() {
    AppSettings::setSnapshotOnReopen(true);
    QCOMPARE(AppSettings::snapshotOnReopen(), true);
    AppSettings::setSnapshotOnReopen(false);
    QCOMPARE(AppSettings::snapshotOnReopen(), false);
}

void TestAppSettings::testMaxThumbnailCacheSize() {
    int testSize = 256;
    AppSettings::setMaxThumbnailCacheSizeMB(testSize);
    QCOMPARE(AppSettings::maxThumbnailCacheSizeMB(), testSize);
}

void TestAppSettings::testMaxDeltaCacheSize() {
    int testSize = 1024;
    AppSettings::setMaxDeltaCacheSizeMB(testSize);
    QCOMPARE(AppSettings::maxDeltaCacheSizeMB(), testSize);
}

void TestAppSettings::testBaseInterval() {
    int testInterval = 50;
    AppSettings::setBaseInterval(testInterval);
    QCOMPARE(AppSettings::baseInterval(), testInterval);
}

void TestAppSettings::testScaleWithWindow() {
    AppSettings::setScaleWithWindow(true);
    QCOMPARE(AppSettings::scaleWithWindow(), true);
    AppSettings::setScaleWithWindow(false);
    QCOMPARE(AppSettings::scaleWithWindow(), false);
}

void TestAppSettings::testToolbarVisible() {
    AppSettings::setToolbarVisible(true);
    QCOMPARE(AppSettings::toolbarVisible(), true);
    AppSettings::setToolbarVisible(false);
    QCOMPARE(AppSettings::toolbarVisible(), false);
}

void TestAppSettings::testBackgroundColor() {
    QColor testColor = QColor("#ff0000");
    AppSettings::setBackgroundColor(testColor);
    QCOMPARE(AppSettings::backgroundColor(), testColor);

    AppSettings::resetBackgroundColor();
    QCOMPARE(AppSettings::backgroundColor(), QColor(c_defaultBackgroundColor));
}

void TestAppSettings::testIdentity() {
    QVERIFY(QString(AppSettings::applicationName()).contains("Ottersnap"));
    QVERIFY(AppSettings::organizationName() != nullptr);
    QVERIFY(!QString(AppSettings::organizationDomain()).isEmpty());
    QVERIFY(!AppSettings::openFilter().isEmpty());
    QVERIFY(!AppSettings::saveFilter().isEmpty());
}

void TestAppSettings::testRepositoryUrl() {
    QVERIFY(AppSettings::repositoryUrl() != nullptr);
    QVERIFY(!QString(AppSettings::repositoryUrl()).isEmpty());
    QVERIFY(QString(AppSettings::repositoryUrl()).startsWith("http"));
}

QTEST_MAIN(TestAppSettings)
#include "test_appsettings.moc"
