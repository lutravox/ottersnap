#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>
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
        AppSettings::setAutosaveSnapshots(false);
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

    void testSelectSnapshot() {
        ImageSession *session = m_controller->openImage(m_testFilePath);
        QVERIFY(session != nullptr);
        m_controller->setActiveSession(session);

        QSignalSpy snapSpy(session, &ImageSession::snapshotCreated);
        session->saveSnapshot();
        QVERIFY(snapSpy.wait(2000));

        auto snapshots = session->snapshots();
        QVERIFY(!snapshots.isEmpty());
        QUuid   firstUuid = snapshots.first().uuid;
        QString uuid = firstUuid.toString(QUuid::WithoutBraces);

        m_controller->selectSnapshot(uuid);

        QCOMPARE(session->currentUuid(), uuid);
    }

    void testSaveSnapshot() {
        ImageSession *session = m_controller->openImage(m_testFilePath);
        QVERIFY(session != nullptr);
        m_controller->setActiveSession(session);

        QSignalSpy snapSpy(session, &ImageSession::snapshotCreated);

        int initialCount = session->snapshots().count();
        m_controller->saveSnapshot();

        QVERIFY(snapSpy.wait(2000));
        QCOMPARE(session->snapshots().count(), initialCount + 1);
    }

    void testDeleteSnapshot() {
        ImageSession *session = m_controller->openImage(m_testFilePath);
        QVERIFY(session != nullptr);
        m_controller->setActiveSession(session);

        session->saveSnapshot();
        QSignalSpy snapSpy(session, &ImageSession::snapshotCreated);
        QVERIFY(snapSpy.wait(2000));

        QUuid uuid = session->snapshots().first().uuid;
        qDebug() << "Deleting snapshot:" << uuid;

        QSignalSpy imageSpy(session, &ImageSession::imageChanged);
        m_controller->deleteSnapshot(uuid);

        if (!imageSpy.wait(100)) {
            qDebug() << "imageChanged not emitted immediately, checking count:" << imageSpy.count();
        }
        QVERIFY(imageSpy.count() > 0);

        QCOMPARE(session->snapshots().count(), 0);
    }

    void testDeleteAllSnapshots() {
        ImageSession *session = m_controller->openImage(m_testFilePath);
        QVERIFY(session != nullptr);
        m_controller->setActiveSession(session);

        session->saveSnapshot();
        QSignalSpy snapSpy1(session, &ImageSession::snapshotCreated);
        QVERIFY(snapSpy1.wait(2000));

        QCOMPARE(session->snapshots().count(), 1);

        QSignalSpy snapSpy(session, &ImageSession::snapshotsChanged);
        m_controller->deleteAllSnapshots();

        if (!snapSpy.wait(100)) {
            qDebug() << "snapshotsChanged not emitted immediately, checking count:"
                     << snapSpy.count();
        }
        QVERIFY(snapSpy.count() > 0);

        QCOMPARE(session->snapshots().count(), 0);
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

    void testChangeActiveSessionPath() {
        ImageSession *session = m_controller->openImage(m_testFilePath);
        QVERIFY(session != nullptr);
        m_controller->setActiveSession(session);

        QSignalSpy createdSpy(session, &ImageSession::snapshotCreated);
        session->saveSnapshot();
        QTRY_VERIFY_WITH_TIMEOUT(createdSpy.count() >= 1, 5000);
        QCOMPARE(SnapshotManager::loadSnapshots(m_testFilePath).size(), 1);

        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QString newPath = tempDir.filePath("moved.png");
        QImage  movedImg(100, 100, QImage::Format_ARGB32);
        movedImg.fill(Qt::green);
        QVERIFY(movedImg.save(newPath));

        QVERIFY(m_controller->changeActiveSessionPath(newPath));

        QCOMPARE(m_controller->activeSession(), session);
        QCOMPARE(m_controller->sessionForPath(SnapshotManager::normalizePath(newPath)),
                 session);
        QCOMPARE(m_controller->sessionForPath(m_testFilePath), nullptr);
        QCOMPARE(session->filePath(), SnapshotManager::normalizePath(newPath));
        QCOMPARE(SnapshotManager::loadSnapshots(newPath).size(), 1);
        QCOMPARE(SnapshotManager::loadSnapshots(m_testFilePath).size(), 0);
    }

    void testChangeActiveSessionPathRejected() {
        ImageSession *session = m_controller->openImage(m_testFilePath);
        QVERIFY(session != nullptr);
        m_controller->setActiveSession(session);

        // Same path is rejected.
        QVERIFY(!m_controller->changeActiveSessionPath(m_testFilePath));

        // A path already open in another session is rejected.
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QString otherPath = tempDir.filePath("other.png");
        QImage otherImg(50, 50, QImage::Format_ARGB32);
        otherImg.fill(Qt::yellow);
        QVERIFY(otherImg.save(otherPath));

        ImageSession *other = m_controller->openImage(otherPath);
        QVERIFY(other != nullptr);
        m_controller->setActiveSession(session);

        QVERIFY(!m_controller->changeActiveSessionPath(otherPath));
        QCOMPARE(m_controller->sessionForPath(m_testFilePath), session);
    }

    void testChangeActiveSessionPathNoActive() {
        QVERIFY(!m_controller->changeActiveSessionPath("/tmp/ottersnap_none.png"));
    }
};

QTEST_MAIN(TestImageSessionController)
#include "test_imagesessioncontroller.moc"
