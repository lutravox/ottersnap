#include <QtTest>
#include "config/appsettings.h"
#include "controllers/appsettingscontroller.h"

class TestAppSettingsController : public QObject {
    Q_OBJECT

  private slots:
    void init() {
        // Reset settings to defaults
        AppSettings::setAutoreloadImages(true);
        AppSettings::setAutosaveSnapshots(true);
        AppSettings::setSnapshotOnReopen(false);
    }

    void testDependencyChain() {
        AppSettingsController controller;

        // Test Autoreload -> Autosave dependency
        AppSettings::setAutoreloadImages(true);
        AppSettings::setAutosaveSnapshots(true);
        QVERIFY(controller.isAutosaveSnapshotsEnabled());

        AppSettings::setAutoreloadImages(false);
        // Setting autoreload to false should disable autosave
        controller.setAutoreloadImages(false);
        QVERIFY(!AppSettings::autosaveSnapshots());
        QVERIFY(!controller.isAutosaveSnapshotsEnabled());

        // Test Autosave <-> SnapshotOnReopen mutual exclusion
        AppSettings::setAutoreloadImages(true);
        AppSettings::setAutosaveSnapshots(false);
        AppSettings::setSnapshotOnReopen(true);

        // Enabling autosave should disable snapshot on reopen
        controller.setAutosaveSnapshots(true);
        QVERIFY(AppSettings::autosaveSnapshots());
        QVERIFY(!AppSettings::snapshotOnReopen());

        // Enabling snapshot on reopen should disable autosave
        controller.setSnapshotOnReopen(true);
        QVERIFY(AppSettings::snapshotOnReopen());
        QVERIFY(!AppSettings::autosaveSnapshots());
    }

    void testGetters() {
        AppSettingsController controller;

        AppSettings::setAutoreloadImages(true);
        AppSettings::setAutosaveSnapshots(true);
        AppSettings::setSnapshotOnReopen(false);
        AppSettings::setToolbarVisible(true);

        QVERIFY(controller.shouldAutoreloadImages());
        QVERIFY(controller.shouldAutosaveSnapshots());
        QVERIFY(!controller.shouldSaveSnapshotOnReopen());
        QVERIFY(controller.toolbarVisible());

        AppSettings::setAutoreloadImages(true);
        AppSettings::setAutosaveSnapshots(false);
        AppSettings::setSnapshotOnReopen(true);
        AppSettings::setToolbarVisible(false);

        QVERIFY(controller.shouldSaveSnapshotOnReopen());
        QVERIFY(!controller.toolbarVisible());
    }

    void testToolbarVisibility() {
        AppSettingsController controller;

        controller.setToolbarVisible(true);
        QVERIFY(controller.toolbarVisible());
        QVERIFY(AppSettings::toolbarVisible());

        controller.setToolbarVisible(false);
        QVERIFY(!controller.toolbarVisible());
        QVERIFY(!AppSettings::toolbarVisible());
    }
};

QTEST_MAIN(TestAppSettingsController)
#include "test_appsettingscontroller.moc"
