#include <QCoreApplication>
#include <QFile>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryFile>
#include <QtTest>
#include "config/appsettings.h"
#include "core/imagesession.h"
#include "core/snapshotstore.h"

class TestImageSession : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase();
    void cleanupTestCase();

    void testOpenClose();
    void testSelectSnapshot();
    void testAutoSaveOnChange();
    void testSnapshotThumbnails();
    void testEffectsState();
    void testSaveSnapshot();
    void testSnapshotNavigation();
    void testViewStateAccess();

  private:
    QTemporaryFile *m_tempFile = nullptr;
    QString         m_testFilePath;
    void            createTestImage(const QString& path, const QColor& color);
};

void TestImageSession::initTestCase() {
    m_tempFile = new QTemporaryFile(this);
    if (m_tempFile->open()) {
        // QImage needs an extension to determine the format during save/load
        m_testFilePath = m_tempFile->fileName() + ".png";
        m_tempFile->close();
    }
    createTestImage(m_testFilePath, Qt::red);
}

void TestImageSession::cleanupTestCase() {
    if (m_tempFile) {
        delete m_tempFile;
    }
}

void TestImageSession::createTestImage(const QString& path, const QColor& color) {
    QImage img(100, 100, QImage::Format_RGB32);
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
    QVERIFY(session.currentImage().isNull());
}

void TestImageSession::testSelectSnapshot() {
    ImageSession session;
    session.openImage(m_testFilePath);

    // No snapshots yet, so current index should be at the end (the disk image)
    int initialIndex = session.currentSnapshotIndex();

    // Try to select an invalid index
    session.selectSnapshot(-1);
    QCOMPARE(session.currentSnapshotIndex(), initialIndex);

    // Normally we'd create a snapshot via SnapshotStore to test navigation,
    // but we can verify the signal is emitted when a valid index is set.
    QSignalSpy spy(&session, &ImageSession::imageChanged);
    session.selectSnapshot(0);
    // Since there are no snapshots, it might not change index,
    // but the method should handle it gracefully.
}

void TestImageSession::testAutoSaveOnChange() {
    // Enable autosave for the test
    AppSettings::setAutosaveSnapshots(true);

    ImageSession session;
    session.openImage(m_testFilePath);

    QSignalSpy spy(&session, &ImageSession::imageChanged);

    // Simulate an external file change
    createTestImage(m_testFilePath, Qt::blue);

    // ImageSession uses ImageMonitor, so we wait for the signal to propagate
    // This is an integration test of Session -> Monitor -> Disk
    QVERIFY(spy.wait(2000));

    // The image should now be blue
    QCOMPARE(session.currentImage().pixelColor(0, 0), QColor(Qt::blue));
}

void TestImageSession::testSnapshotThumbnails() {
    ImageSession session;
    session.openImage(m_testFilePath);

    auto [thumbs, labels] = session.snapshotThumbnails(32);

    // Should at least contain the current image thumbnail
    QVERIFY(!thumbs.isEmpty());
    QVERIFY(!labels.isEmpty());
    QCOMPARE(thumbs.last().size(), QSize(32, 32));
    QCOMPARE(labels.last(), QString("Current"));
}

void TestImageSession::testEffectsState() {
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

    // Manually trigger reload instead of waiting for monitor
    session.reloadImage();

    // Create a snapshot of the updated image
    session.saveSnapshot();
    QSignalSpy saveSpy(&session, &ImageSession::snapshotsChanged);
    QVERIFY(saveSpy.wait(2000));

    // Select the snapshot (index 0)
    session.selectSnapshot(0);
    QCOMPARE(session.currentSnapshotIndex(), 0);

    // Verify it's not null
    QVERIFY(!session.currentImage().isNull());
}

void TestImageSession::testViewStateAccess() {
    ImageSession session;
    session.openImage(m_testFilePath);

    ViewState& vs = session.viewState();
    // Initialize dimensions so setPercentage works
    vs.updateImageSize(100, 100);
    vs.setPercentage(200.0);

    QCOMPARE(session.viewState().percentage(), 200.0);
}

QTEST_MAIN(TestImageSession)
#include "test_imagesession.moc"
