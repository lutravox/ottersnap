#include <QCoreApplication>
#include <QFile>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QtTest>
#include "config/appsettings.h"
#include "core/imagesession.h"
#include "core/snapshotmanager.h"
#include "core/snapshotdb.h"
#include "core/vulkancontext.h"

class TestImageSession : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();

    void testOpenClose();
    void testSelectSnapshot();
    void testAutoSaveOnChange();
    void testSnapshotThumbnails();
    void testEffectsModel();
    void testSaveSnapshot();
    void testSnapshotNavigation();
    void testViewModelAccess();
    void testSnapshotDeletion();
    void testGetReconstructionSequence();
    void testThumbnailGeneration();
    void testUIReconstructorInitialization();
    void testSetFilePath();
    void testSetFilePathFailure();
    void testSetFilePathFromSnapshotOnly();

  private:
    QTemporaryFile *m_tempFile = nullptr;
    QString         m_testFilePath;
    void            createTestImage(const QString& path, const QColor& color);
};

void TestImageSession::initTestCase() {
    // Use a unique temporary database for this test case to avoid collisions with other tests.
    QString tempDb = QDir::tempPath() + "/test_imagesession_" +
                     QString::number(QRandomGenerator::global()->generate()) + ".db";
    SnapshotDatabase::instance().init(tempDb);

    // Initialize Vulkan context to enable GPU-accelerated reconstruction tests.
    if (!VulkanContext::instance().initializeInstance()) {
        qWarning() << "VulkanContext failed to initialize. GPU tests may fail.";
    }

    m_tempFile = new QTemporaryFile(this);
    if (m_tempFile->open()) {
        // QImage needs an extension to determine the format during save/load
        m_testFilePath = m_tempFile->fileName() + ".png";
        m_tempFile->close();
    }
    createTestImage(m_testFilePath, Qt::red);
    AppSettings::setAutoreloadImages(true);
}

void TestImageSession::cleanupTestCase() {
    if (m_tempFile) {
        delete m_tempFile;
    }
}

void TestImageSession::cleanup() {
    // Reset AppSettings to prevent test pollution between alphabetically-ordered tests.
    // testSaveSnapshot sets autosaveSnapshots(true) which affects subsequent tests.
    AppSettings::setAutosaveSnapshots(false);
    SnapshotManager::clearCache();
}

void TestImageSession::createTestImage(const QString& path, const QColor& color) {
    QImage img(100, 100, QImage::Format_ARGB32);
    img.fill(color);
    img.save(path);
}

void TestImageSession::testOpenClose() {
    ImageSession session;
    QSignalSpy   spy(&session, &ImageSession::imageChanged);

    QVERIFY(session.openImage(m_testFilePath));
    QCOMPARE(session.filePath(), m_testFilePath);
    QCOMPARE(spy.count(), 1);

    session.close();
    QCOMPARE(session.filePath(), QString());
    QVERIFY(session.diskImage().isNull());
}

void TestImageSession::testSelectSnapshot() {
    ImageSession session;
    session.openImage(m_testFilePath);

    // No snapshots yet, so current index should be at the end (the disk image)
    int initialIndex = session.currentSnapshotIndex();

    // Try to select an invalid index
    session.selectSnapshot(-1);
    QCOMPARE(session.currentSnapshotIndex(), initialIndex);

    // Normally we'd create a snapshot via SnapshotManager to test navigation,
    // but we can verify the signal is emitted when a valid index is set.
    QSignalSpy spy(&session, &ImageSession::imageChanged);
    session.selectSnapshot(0);
    // Since there are no snapshots, it might not change index,
    // but the method should handle it gracefully.
}

void TestImageSession::testAutoSaveOnChange() {
    AppSettings::setAutosaveSnapshots(true);

    ImageSession session;
    session.openImage(m_testFilePath);

    QSignalSpy spy(&session, &ImageSession::imageChanged);

    // Simulate an external file change
    createTestImage(m_testFilePath, Qt::blue);

    // ImageSession uses ImageMonitor, so we wait for the signal to propagate
    QVERIFY(spy.wait(2000));

    // The image should now be blue
    QCOMPARE(session.diskImage().pixelColor(0, 0), QColor(Qt::blue));
}

void TestImageSession::testSnapshotThumbnails() {
    ImageSession session;
    session.openImage(m_testFilePath);

    auto [thumbs, labels, indices] = session.snapshotTimelineThumbnails(32);

    // Should at least contain the current image thumbnail
    QVERIFY(!thumbs.isEmpty());
    QVERIFY(!labels.isEmpty());
    QCOMPARE(thumbs.last().size(), QSize(32, 32));
    QCOMPARE(labels.last(), QString("Current"));
}

void TestImageSession::testEffectsModel() {
    ImageSession session;
    QSignalSpy   spy(&session, &ImageSession::effectsChanged);

    session.setGrayscale(true);
    QCOMPARE(session.grayscaleEnabled(), true);
    QCOMPARE(spy.count(), 1);

    session.setMirror(true);
    QCOMPARE(session.mirrorEnabled(), true);
    QCOMPARE(spy.count(), 2);

    session.setGrayscale(false);
    QCOMPARE(session.grayscaleEnabled(), false);
    QCOMPARE(spy.count(), 3);
}

void TestImageSession::testSaveSnapshot() {
    ImageSession session;
    session.openImage(m_testFilePath);

    int        initialCount = session.snapshots().size();
    QSignalSpy spy(&session, &ImageSession::snapshotsChanged);

    session.saveSnapshot();

    // saveSnapshot is asynchronous. Wait for the signal.
    QVERIFY(spy.wait(2000));
    QVERIFY(session.snapshots().size() > initialCount);
}

void TestImageSession::testSnapshotNavigation() {
    ImageSession session;
    session.openImage(m_testFilePath);

    // Modify image to a unique color to ensure the snapshot is not a duplicate
    createTestImage(m_testFilePath, Qt::green);

    // Manually trigger reload and wait for it to complete so that saveSnapshot()
    // captures the updated green image rather than the stale red one.
    session.reloadImage();
    QSignalSpy reloadSpy(&session, &ImageSession::imageChanged);
    QVERIFY(reloadSpy.wait(2000));

    // Create a snapshot of the updated image
    session.saveSnapshot();
    QSignalSpy saveSpy(&session, &ImageSession::snapshotsChanged);
    QVERIFY(saveSpy.wait(2000));

    // Select the snapshot (index 0)
    session.selectSnapshot(0);
    QCOMPARE(session.currentSnapshotIndex(), 0);

    // Verify it's not null
    QVERIFY(!session.diskImage().isNull());
}

void TestImageSession::testViewModelAccess() {
    ImageSession session;
    session.openImage(m_testFilePath);

    ViewModel& vs = session.viewModel();
    // Initialize dimensions so setPercentage works
    vs.updateImageSize(100, 100);
    vs.setPercentage(200.0);

    QCOMPARE(session.viewModel().percentage(), 200.0);
}

void TestImageSession::testSnapshotDeletion() {
    ImageSession session;

    QString uniquePath = m_tempFile->fileName() + "_del.png";
    AppSettings::setAutosaveSnapshots(false);
    SnapshotManager::deleteAllSnapshots(uniquePath);
    createTestImage(uniquePath, Qt::red);
    session.openImage(uniquePath);

    // 1. Create snapshots with unique colors to ensure they are saved
    QColor colors[] = {Qt::blue, Qt::green, Qt::yellow};
    QVector<QUuid> ids;
    for (int i = 0; i < 3; ++i) {
        createTestImage(uniquePath, colors[i]);

        QSignalSpy reloadSpy(&session, &ImageSession::imageChanged);
        session.reloadImage();
        QVERIFY(reloadSpy.wait(2000));

        session.saveSnapshot();
        QSignalSpy saveSpy(&session, &ImageSession::snapshotsChanged);
        QVERIFY(saveSpy.wait(2000));
    }
    ids = { session.snapshots()[0].uuid, session.snapshots()[1].uuid, session.snapshots()[2].uuid };

    // Current state: snapshots [S1, S2, S3], current image is Disk Image (index 3)
    QCOMPARE(session.snapshots().size(), 3);
    session.selectSnapshot(3);
    QCOMPARE(session.currentSnapshotIndex(), 3);
    QVERIFY(!session.diskImage().isNull());

    // Scenario A: Delete a snapshot before the current one (e.g., S2)
    session.deleteSnapshot(ids[1]);
    QTRY_VERIFY_WITH_TIMEOUT(session.snapshots().size() == 2, 5000);

    // After deletion, snapshots size is 2. m_currentIndex should be updated to 2.
    QCOMPARE(session.snapshots().size(), 2);
    QCOMPARE(session.currentSnapshotIndex(), 2);
    QVERIFY(!session.diskImage().isNull());

    // Scenario B: Viewing a snapshot and deleting one before it.
    // Current state: [S1, S3]. Select S3 (index 1).
    session.selectSnapshot(1);
    QCOMPARE(session.currentSnapshotIndex(), 1);

    // Delete S1.
    session.deleteSnapshot(ids[0]);
    QTRY_VERIFY_WITH_TIMEOUT(session.snapshots().size() == 1, 5000);

    // After deletion, snapshots size is 1. S3 is now at index 0.
    // m_currentIndex should have shifted from 1 to 0.
    QCOMPARE(session.snapshots().size(), 1);
    QCOMPARE(session.currentSnapshotIndex(), 0);
    QVERIFY(!session.diskImage().isNull());

    // Scenario C: Viewing a snapshot and deleting it.
    session.deleteSnapshot(session.snapshots()[0].uuid); // Delete the only remaining snapshot S3
    QTRY_VERIFY_WITH_TIMEOUT(session.snapshots().size() == 0, 5000);

    // Should move back to "Current" (index 0 now)
    QCOMPARE(session.snapshots().size(), 0);
    QCOMPARE(session.currentSnapshotIndex(), 0);
    QVERIFY(!session.diskImage().isNull());
}

void TestImageSession::testGetReconstructionSequence() {
    AppSettings::setBaseInterval(10);
    ImageSession session;
    QString      uniquePath = m_tempFile->fileName() + "_seq.png";
    SnapshotManager::deleteAllSnapshots(uniquePath);

    // Create a sequence of 3 unique images
    QColor colors[] = {Qt::red, Qt::green, Qt::blue};
    for (int i = 0; i < 3; ++i) {
        createTestImage(uniquePath, colors[i]);
        session.openImage(uniquePath); // Re-open to update diskImage
        session.saveSnapshot();
        QSignalSpy spy(&session, &ImageSession::snapshotsChanged);
        QVERIFY(spy.wait(2000));
    }

    // S1: Base
    // S2: Delta of S1
    // S3: Delta of S2

    auto seq1 = session.getReconstructionSequence(0);
    QVERIFY(seq1.has_value());
    QCOMPARE(seq1->deltas.size(), 0);
    QVERIFY(!seq1->base.isNull());

    auto seq2 = session.getReconstructionSequence(1);
    QVERIFY(seq2.has_value());
    QCOMPARE(seq2->deltas.size(), 1);

    auto seq3 = session.getReconstructionSequence(2);
    QVERIFY(seq3.has_value());
    QCOMPARE(seq3->deltas.size(), 2);

    // Test invalid index
    auto seqInvalid = session.getReconstructionSequence(99);
    QVERIFY(!seqInvalid.has_value());
}

void TestImageSession::testThumbnailGeneration() {
    ImageSession session;
    session.openImage(m_testFilePath);

    // Test thumbnail helper for current image
    QImage thumb = session.thumbnail(32);
    QVERIFY(!thumb.isNull());
    QCOMPARE(thumb.size(), QSize(32, 32));

    // Test thumbnail for invalid index (should return placeholder)
    QImage invalidThumb = session.generateThumbnail(-1, 32);
    QVERIFY(!invalidThumb.isNull());
    QCOMPARE(invalidThumb.size(), QSize(32, 32));
}

void TestImageSession::testUIReconstructorInitialization() {
    ImageSession session;
    session.openImage(m_testFilePath);

    VulkanHandles handles = VulkanContext::instance().getUIHandles();

    // First initialization
    session.setVkReconstructorHandles(handles);
    auto recon1 = session.uiReconstructor();
    QVERIFY(recon1 != nullptr);

    // Second initialization with same handles should not change the pointer
    session.setVkReconstructorHandles(handles);
    auto recon2 = session.uiReconstructor();
    QCOMPARE(recon1, recon2);
}

void TestImageSession::testSetFilePath() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QString newPath = tempDir.filePath("moved.png");
    QImage  movedImg(100, 100, QImage::Format_ARGB32);
    movedImg.fill(Qt::blue);
    QVERIFY(movedImg.save(newPath));

    // Start from a clean slate: earlier tests may have left snapshots of the
    // current file state, which would be deduplicated on save.
    SnapshotManager::deleteAllSnapshots(m_testFilePath);

    ImageSession session;
    QVERIFY(session.openImage(m_testFilePath));

    QSignalSpy createdSpy(&session, &ImageSession::snapshotCreated);
    session.saveSnapshot();
    QTRY_VERIFY_WITH_TIMEOUT(createdSpy.count() >= 1, 5000);
    QCOMPARE(SnapshotManager::loadSnapshots(m_testFilePath).size(), 1);

    QSignalSpy changedSpy(&session, &ImageSession::imageChanged);
    QVERIFY(session.setFilePath(newPath));
    QVERIFY(changedSpy.count() >= 1);

    QCOMPARE(session.filePath(), SnapshotManager::normalizePath(newPath));
    // History follows the file to the new path.
    QCOMPARE(session.snapshots().size(), 1);
    QCOMPARE(SnapshotManager::loadSnapshots(newPath).size(), 1);
    QVERIFY(SnapshotManager::loadSnapshots(m_testFilePath).isEmpty());
    QVERIFY(!SnapshotManager::keyForPath(m_testFilePath).has_value());
    // Disk image reloaded from the new location.
    QCOMPARE(session.diskImage(), movedImg);
}

void TestImageSession::testSetFilePathFromSnapshotOnly() {
    // A missing file with history opens in snapshot-only mode.
    QString missingPath =
        QString("/tmp/ottersnap_test_missing_%1.png").arg(QRandomGenerator::global()->generate());
    SnapshotManager::deleteAllSnapshots(missingPath);
    QImage img(50, 50, QImage::Format_ARGB32);
    img.fill(Qt::cyan);
    auto res = SnapshotManager::saveSnapshot(missingPath, img);
    QVERIFY(res.has_value());

    ImageSession session;
    session.setSnapshotOnly(true);
    QVERIFY(session.openImage(missingPath));
    QVERIFY(session.isSnapshotOnly());

    // Finding the file again exits snapshot-only mode and keeps the history.
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString newPath = tempDir.filePath("found.png");
    QImage  foundImg(50, 50, QImage::Format_ARGB32);
    foundImg.fill(Qt::magenta);
    QVERIFY(foundImg.save(newPath));

    QVERIFY(session.setFilePath(newPath));
    QVERIFY(!session.isSnapshotOnly());
    QCOMPARE(session.filePath(), SnapshotManager::normalizePath(newPath));
    QCOMPARE(session.snapshots().size(), 1);
    QCOMPARE(session.diskImage(), foundImg);

    SnapshotManager::deleteAllSnapshots(newPath);
}

void TestImageSession::testSetFilePathFailure() {
    ImageSession session;
    QVERIFY(session.openImage(m_testFilePath));
    QString oldPath = session.filePath();
    QImage  oldImage = session.diskImage();

    // Non-existent target: update must fail and leave the session unchanged.
    QVERIFY(!session.setFilePath("/nonexistent/ottersnap/nope.png"));
    QCOMPARE(session.filePath(), oldPath);
    QCOMPARE(session.diskImage(), oldImage);
}

QTEST_MAIN(TestImageSession)
#include "test_imagesession.moc"
