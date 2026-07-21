#include <QCoreApplication>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryFile>
#include <QThread>
#include <QtTest>
#include "core/imagemonitor.h"

class TestImageMonitor : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase();
    void cleanupTestCase();

    void testWatchAndNotify();
    void testDebounceLogic();
    void testStopMonitoring();
    void testFileDeletion();

  private:
    QTemporaryFile *m_tempFile;
};

void TestImageMonitor::initTestCase() {
    m_tempFile = new QTemporaryFile(this);
    if (!m_tempFile->open()) {
        QFAIL("Could not create temporary file for testing");
    }
    m_tempFile->write("initial content");
    m_tempFile->close();
}

void TestImageMonitor::cleanupTestCase() {
    delete m_tempFile;
}

void TestImageMonitor::testWatchAndNotify() {
    ImageMonitor monitor;
    QSignalSpy   spy(&monitor, &ImageMonitor::fileChanged);

    monitor.watch(m_tempFile->fileName());

    // Modify the file
    QFile file(m_tempFile->fileName());
    if (file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        file.write("update");
        file.close();
    }

    // Wait for the monitor to detect the change and for the stability timer to fire
    // We use a generous timeout as file system events can be slow on some OSs
    QVERIFY(spy.wait(2000));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), m_tempFile->fileName());
}

void TestImageMonitor::testDebounceLogic() {
    ImageMonitor monitor;
    QSignalSpy   spy(&monitor, &ImageMonitor::fileChanged);

    monitor.watch(m_tempFile->fileName());

    // Rapidly modify the file multiple times
    for (int i = 0; i < 5; ++i) {
        QFile file(m_tempFile->fileName());
        if (file.open(QIODevice::WriteOnly | QIODevice::Append)) {
            file.write("rapid update");
            file.close();
        }
        QTest::qSleep(50); // Small sleep between writes
    }

    // Wait for stability
    QVERIFY(spy.wait(2000));

    // Despite 5 writes, only 1 signal should be emitted due to debouncing
    QCOMPARE(spy.count(), 1);
}

void TestImageMonitor::testStopMonitoring() {
    ImageMonitor monitor;
    QSignalSpy   spy(&monitor, &ImageMonitor::fileChanged);

    monitor.watch(m_tempFile->fileName());
    monitor.stop();

    QFile file(m_tempFile->fileName());
    if (file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        file.write("post-stop update");
        file.close();
    }

    // Wait to ensure NO signal is emitted
    QTest::qSleep(500);
    QCOMPARE(spy.count(), 0);
}

void TestImageMonitor::testFileDeletion() {
    ImageMonitor monitor;
    QSignalSpy   spy(&monitor, &ImageMonitor::fileChanged);

    monitor.watch(m_tempFile->fileName());

    // Delete the file
    QFile::remove(m_tempFile->fileName());

    // Wait to see if it signals (depending on implementation, it might signal once or not at all)
    // For this project, we check if it remains stable/doesn't crash.
    QTest::qSleep(500);

    // Re-create file to restore state for other tests
    m_tempFile->open();
    m_tempFile->close();
}

QTEST_MAIN(TestImageMonitor)
#include "test_imagemonitor.moc"
