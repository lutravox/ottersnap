#include <QDir>
#include <QTemporaryFile>
#include <QtTest>
#include "config/appsettings.h"
#include "controllers/appsettingscontroller.h"
#include "controllers/imagesessioncontroller.h"
#include "core/imagesession.h"
#include "core/snapshotdb.h"
#include "core/snapshotmanager.h"

class TestImageSessionController : public QObject {
    Q_OBJECT

  private:
    AppSettingsController  *m_settings;
    ImageSessionController *m_controller;
    QTemporaryFile         *m_tempFile;
    QTemporaryFile         *m_tempDbFile;
    QString                 m_testFilePath;

  private slots:
    void initTestCase() {
        // Use a unique temporary database for this test case to avoid collisions with other tests.
        QString tempDb = QDir::tempPath() + "/test_imagesessioncontroller_" +
                         QString::number(QRandomGenerator::global()->generate()) + ".db";
        SnapshotDatabase::instance().init(tempDb);
    }

    void cleanupTestCase() {
    }

    void init() {
        m_settings = new AppSettingsController();
        m_controller = new ImageSessionController(m_settings);

        // Create a dummy image file in the system temp directory
        QString tempDir = QDir::tempPath();
        QString fileName = tempDir + "/ottersnap_test_image_" +
                           QString::number(QRandomGenerator::global()->generate()) + ".png";

        QImage img(100, 100, QImage::Format_RGB32);
        img.fill(Qt::blue);
        if (!img.save(fileName)) {
            qWarning() << "Failed to save test image to" << fileName;
        }
        m_testFilePath = fileName;

        // Ensure no existing snapshots for this test file
        SnapshotManager::deleteAllSnapshots(m_testFilePath);

        // Disable autoreload to ensure "snapshot on reopen" can be tested
        AppSettings::setAutoreloadImages(false);
    }

    void cleanup() {
        delete m_controller;
        delete m_settings;
        QFile::remove(m_testFilePath);

        // Reset settings to prevent test pollution
        AppSettings::setAutosaveSnapshots(false);
        AppSettings::setSnapshotOnReopen(false);
        AppSettings::setAutoreloadImages(true);
    }

    void testOpenNewImage() {
        ImageSession *session = m_controller->openImage(m_testFilePath);
        QVERIFY(session != nullptr);
        QCOMPARE(m_controller->openPaths().count(), 1);
        QCOMPARE(m_controller->openPaths().first(), m_testFilePath);
    }

    void testOpenExistingImage() {
        m_controller->openImage(m_testFilePath);

        // Open same path again
        ImageSession *session = m_controller->openImage(m_testFilePath);
        QVERIFY(session != nullptr);
        QCOMPARE(m_controller->openPaths().count(), 1);
    }

    void testSnapshotOnReopen() {
        // Setup: Disable general autosave, enable snapshot on reopen
        AppSettings::setAutosaveSnapshots(false);
        AppSettings::setSnapshotOnReopen(true);
        m_settings->setAutosaveSnapshots(false);
        m_settings->setSnapshotOnReopen(true);

        ImageSession *session = m_controller->openImage(m_testFilePath);
        QVERIFY(session != nullptr);

        // Modify image on disk to trigger "changed" state
        QImage modifiedImg(100, 100, QImage::Format_RGB32);
        modifiedImg.fill(Qt::red);
        if (!modifiedImg.save(m_testFilePath)) {
            QFAIL("Failed to save modified image to disk");
        }

        // Explicitly update the modification time to ensure it's different from the original
        QDateTime newTime = QFileInfo(m_testFilePath).lastModified().addSecs(10);
        QFile     file(m_testFilePath);
        if (file.open(QIODevice::ReadWrite)) {
            file.setFileTime(newTime, QFileDevice::FileModificationTime);
            file.close();
        } else {
            qWarning() << "Failed to open file to set time for" << m_testFilePath;
        }

        // Open again - this should trigger saveSnapshot()
        int initialSnapshots = SnapshotManager::loadSnapshots(m_testFilePath).count();

        // Wait for the async reload AND the snapshot save to finish
        QSignalSpy reloadSpy(session, &ImageSession::imageChanged);
        QSignalSpy snapSpy(session, &ImageSession::snapshotCreated);

        m_controller->openImage(m_testFilePath);

        // Wait for reload to fire
        QVERIFY(reloadSpy.wait(2000));
        snapSpy.wait(2000);

        int finalSnapshots = SnapshotManager::loadSnapshots(m_testFilePath).count();
        QVERIFY(finalSnapshots > initialSnapshots);
    }

    void testCloseSession() {
        m_controller->openImage(m_testFilePath);
        QCOMPARE(m_controller->openPaths().count(), 1);

        m_controller->closeSession(m_testFilePath);
        QCOMPARE(m_controller->openPaths().count(), 0);
    }
};

QTEST_MAIN(TestImageSessionController)
#include "test_imagesessioncontroller.moc"
