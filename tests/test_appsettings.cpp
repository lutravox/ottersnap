#include <QCoreApplication>
#include <QtTest>
#include "config/appsettings.h"

class TestAppSettings : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase();

    // Round-trip
    void testSetFalseReadFalse();
    void testSetTrueReadTrue();
    void testToggle();
    void testMultipleWrites();
};

void TestAppSettings::initTestCase() {
    // Use a unique organization so test writes don't collide with
    // the real application's QSettings.
    QCoreApplication::setOrganizationName("OttersnapTest");
    QCoreApplication::setApplicationName("test_appsettings");
}

void TestAppSettings::testSetFalseReadFalse() {
    AppSettings::setResizeToFit(false);
    QCOMPARE(AppSettings::resizeToFit(), false);
}

void TestAppSettings::testSetTrueReadTrue() {
    AppSettings::setResizeToFit(true);
    QCOMPARE(AppSettings::resizeToFit(), true);
}

void TestAppSettings::testToggle() {
    AppSettings::setResizeToFit(false);
    QCOMPARE(AppSettings::resizeToFit(), false);

    AppSettings::setResizeToFit(true);
    QCOMPARE(AppSettings::resizeToFit(), true);

    AppSettings::setResizeToFit(false);
    QCOMPARE(AppSettings::resizeToFit(), false);
}

void TestAppSettings::testMultipleWrites() {
    for (int i = 0; i < 10; ++i) {
        bool expected = (i % 2 == 0);
        AppSettings::setResizeToFit(expected);
        QCOMPARE(AppSettings::resizeToFit(), expected);
    }
}

QTEST_MAIN(TestAppSettings)
#include "test_appsettings.moc"
