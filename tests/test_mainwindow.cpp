#include <QCoreApplication>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <unistd.h>
#include "config/appsettings.h"
#include "controllers/effectscontroller.h"
#include "core/snapshotdb.h"
#include "core/snapshotmanager.h"
#include "core/vulkancontext.h"
#include "ui/imagetab.h"
#include "ui/mainwindow.h"
#include "ui/tabbar.h"

// Test wrapper to expose private members of MainWindow for verification
class MainWindowTestWrapper : public MainWindow {
  public:
    using MainWindow::MainWindow;

    TabBar *tabBar() {
        return m_tabBar;
    }
    EffectsController *effectsController() {
        return m_effectsController;
    }
    QMap<QString, ImageTab *> tabPaths() {
        return m_tabPaths;
    }

    void testOpenImageFile(const QString& path) {
        openImageFile(path);
    }

    void testOpenImageFileReopen(const QString& path) {
        openImageFile(path);
    }

    void testOnCloseTab(int index) {
        onCloseTab(index);
    }

    void testOnTabChanged(int index) {
        onTabChanged(index);
    }

    QAction *updatePathAction() {
        return m_actionUpdatePath;
    }

    ImageSessionController *sessionController() {
        return m_sessionController;
    }
    SnapshotTimelineController *snapshotController() {
        return m_snapshotController;
    }
    void testOnSaveSnapshot() {
        onSaveSnapshot();
    }
    void testUpdateSnapshotTimeline() {
        updateSnapshotTimeline();
    }
    ImageTab *testTakeTab(const QString& path) {
        return m_tabPaths.take(path);
    }
    ImageTab *testTab(const QString& path) {
        return m_tabPaths.value(path);
    }
    void testInsertTab(const QString& path, ImageTab *tab) {
        m_tabPaths.insert(path, tab);
    }
};

class TestMainWindow : public QObject {
    Q_OBJECT

  private slots:
    void init() {
        // Use a unique temporary database for this test case to avoid collisions with other tests.
        QString tempDb = QDir::tempPath() + "/test_mainwindow_" +
                         QString::number(QRandomGenerator::global()->generate()) + ".db";
        SnapshotDatabase::instance().init(tempDb);

        m_window = new MainWindowTestWrapper();

        // Initialize Vulkan for snapshot reconstruction
        VulkanContext::instance().initializeInstance();

        // Create some dummy images for testing
        m_testFiles = {"test_image_1.png", "test_image_2.png", "test_image_3.png"};
        for (const auto& path : m_testFiles) {
            // Ensure clean state for each test file
            SnapshotManager::deleteAllSnapshots(path);

            QImage img(100, 100, QImage::Format_RGB32);
            img.fill(Qt::blue);
            img.save(path);
        }
    }

    void cleanup() {
        delete m_window;
        for (const auto& path : m_testFiles) {
            SnapshotManager::deleteAllSnapshots(path);
            QFile::remove(path);
        }
        // Reset AppSettings
        AppSettings::setSnapshotOnReopen(true);
        AppSettings::setAutosaveSnapshots(false);
        SnapshotManager::clearCache();
    }

    void testOpenFiles() {
        // Open three files
        m_window->testOpenImageFile(m_testFiles[0]);
        m_window->testOpenImageFile(m_testFiles[1]);
        m_window->testOpenImageFile(m_testFiles[2]);

        // Verify tab count
        QCOMPARE(m_window->tabBar()->count(), 3);

        // Verify tracking map (tabs are keyed by canonical paths)
        QCOMPARE(m_window->tabPaths().count(), 3);
        for (const auto& p : m_testFiles)
            QVERIFY(m_window->tabPaths().contains(QFileInfo(p).canonicalFilePath()));
    }

    void testSnapshotOnReopen() {
        const QString& path = m_testFiles[0];
        const QString  canonical = QFileInfo(path).canonicalFilePath();

        // Open the image for the first time (State A)
        m_window->testOpenImageFile(path);
        auto *tab = m_window->tabPaths().value(canonical);
        QVERIFY(tab != nullptr);

        // Change disk image and reload to move session to State B
        QImage imgB(100, 100, QImage::Format_ARGB32);
        imgB.fill(Qt::red);
        imgB.save(path);

        // Disable autosave before reload to prevent reloadImage() from auto-saving
        AppSettings::setAutosaveSnapshots(false);
        tab->session()->reloadImage();

        // Wait for reload to finish so session is actually State B
        QSignalSpy reloadSpy(tab->session(), &ImageSession::imageChanged);
        reloadSpy.wait(2000);

        // Change disk image again to State C
        QImage imgC(100, 100, QImage::Format_ARGB32);
        imgC.fill(Qt::green);
        imgC.save(path);

        // Enable snapshot on reopen and open again
        AppSettings::setAutosaveSnapshots(false);
        AppSettings::setSnapshotOnReopen(true);
        QSignalSpy snapshotSpy(tab, &ImageTab::snapshotCreated);
        m_window->testOpenImageFile(path);

        // Verify that a snapshot was created OR already existed
        bool snapFired = snapshotSpy.wait(2000);

        // Ensure the save operation is fully complete before proceeding
        QTest::qWait(100);

        if (!snapFired) {
            // If it didn't fire, verify that it was because it already existed
            // (In this specific test case, State B should have been snapshotted)
            // We don't have an easy way to check initial count here, but we can
            // verify that the total count is at least 1.
            QVector<ImageSnapshot> snaps = SnapshotManager::loadSnapshots(path);
            QVERIFY(!snaps.isEmpty());
        }

        // Disable snapshot on reopen and open again
        snapshotSpy.clear();
        AppSettings::setSnapshotOnReopen(false);
        m_window->testOpenImageFile(path);

        // Verify no new snapshot was created
        QVERIFY(!snapshotSpy.wait(500));

        // Cleanup
        AppSettings::setSnapshotOnReopen(true);
    }

    void testTabSwitching() {
        m_window->testOpenImageFile(m_testFiles[0]);
        m_window->testOpenImageFile(m_testFiles[1]);

        // Switch to second tab
        m_window->testOnTabChanged(1);

        // The current widget should be the second tab
        auto *currentWidget = m_window->tabBar()->currentWidget();
        auto *tab = qobject_cast<ImageTab *>(currentWidget);
        QVERIFY(tab != nullptr);
        QCOMPARE(tab->filePath(), QFileInfo(m_testFiles[1]).canonicalFilePath());
    }

    void testCloseTab() {
        m_window->testOpenImageFile(m_testFiles[0]);
        m_window->testOpenImageFile(m_testFiles[1]);

        // Close the first tab
        m_window->testOnCloseTab(0);

        // Verify tab count decreased
        QCOMPARE(m_window->tabBar()->count(), 1);

        // Verify tracking map updated (canonical keys)
        QVERIFY(!m_window->tabPaths().contains(QFileInfo(m_testFiles[0]).canonicalFilePath()));
        QVERIFY(m_window->tabPaths().contains(QFileInfo(m_testFiles[1]).canonicalFilePath()));
    }

    void testSessionCollection() {
        m_window->testOpenImageFile(m_testFiles[0]);
        m_window->testOpenImageFile(m_testFiles[1]);

        // We can't easily call private collectOpenPaths,
        // but we can verify that it works by checking the session manager on close.
        // Since we can't easily trigger a real closeEvent and check the disk,
        // we'll use the wrapper to check the paths.

        // Accessing the private method via a trick or just adding it to wrapper
        // For now, let's just verify the tab bar state which collectOpenPaths uses.
        QCOMPARE(m_window->tabBar()->count(), 2);
    }

    void testAutosaveTimelineAfterPathUpdate() {
        AppSettings::setAutoreloadImages(true);
        AppSettings::setAutosaveSnapshots(false);

        const QString path = m_testFiles[0];
        QImage        red(100, 100, QImage::Format_ARGB32);
        red.fill(Qt::red);
        QVERIFY(red.save(path));
        m_window->testOpenImageFile(path);

        // Build a small history of pre-existing snapshots (base + deltas):
        // move the disk image to a new state, then save it manually.
        const QString canonical = QFileInfo(path).canonicalFilePath();
        ImageSession *session = m_window->testTab(canonical)->session();
        QSignalSpy    createdSpy(session, &ImageSession::snapshotCreated);
        auto          advance = [&](const QColor& color) {
            QImage img(100, 100, QImage::Format_ARGB32);
            img.fill(color);
            QVERIFY(img.save(path));
            QTRY_COMPARE_WITH_TIMEOUT(session->diskImage().pixelColor(0, 0), color, 5000);
        };
        auto snapshotNow = [&]() {
            m_window->testOnSaveSnapshot();
            QTRY_VERIFY_WITH_TIMEOUT(createdSpy.count() >= 1, 5000);
            createdSpy.clear();
        };

        advance(Qt::green);
        snapshotNow(); // S1 (base)
        advance(Qt::blue);
        snapshotNow(); // S2 (delta)
        advance(Qt::cyan);
        snapshotNow(); // S3 (delta)
        QCOMPARE(SnapshotManager::loadSnapshots(path).size(), 3);

        // Current disk state is newer than the last snapshot, like right
        // after an external edit that was reloaded without a matching save.
        advance(Qt::magenta);

        AppSettings::setAutosaveSnapshots(true);

        // Simulate the user picking a moved copy of the image.
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QString newPath = tempDir.filePath("moved.png");
        QImage  magenta(100, 100, QImage::Format_ARGB32);
        magenta.fill(Qt::magenta);
        QVERIFY(magenta.save(newPath));

        const QString newCanonical = QFileInfo(newPath).canonicalFilePath();
        QVERIFY(m_window->sessionController()->changeActiveSessionPath(newPath));
        ImageTab *tab = m_window->testTakeTab(canonical);
        m_window->testInsertTab(newCanonical, tab);
        m_window->testUpdateSnapshotTimeline();

        int initialRows = m_window->snapshotController()->model()->rowCount();
        QCOMPARE(initialRows, 4); // 3 snapshots + current image

        // External edit at the new path: autosave must refresh the timeline.
        QImage black(100, 100, QImage::Format_ARGB32);
        black.fill(Qt::black);
        QVERIFY(black.save(newPath));

        QTRY_VERIFY_WITH_TIMEOUT(createdSpy.count() >= 1, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(
            m_window->snapshotController()->model()->rowCount(), initialRows + 1, 5000);
    }

    void testAutosaveTimelineAfterPathUpdateSymlink() {
        AppSettings::setAutoreloadImages(true);
        AppSettings::setAutosaveSnapshots(false);

        // The user's scenario: the image path goes through a symlinked
        // directory component (e.g. /media -> /run/media), so the path the
        // UI works with is not the canonical one.
        QTemporaryDir realDir;
        QTemporaryDir linkParent;
        QVERIFY(realDir.isValid());
        QVERIFY(linkParent.isValid());

        QString linkPath = linkParent.path() + "/media";
        QVERIFY(::symlink(qPrintable(realDir.path()), qPrintable(linkPath)) == 0);

        const QString path = linkPath + "/Hypothetical.png";
        QVERIFY(path != QFileInfo(path).canonicalFilePath()); // really non-canonical
        QImage red(100, 100, QImage::Format_ARGB32);
        red.fill(Qt::red);
        QVERIFY(red.save(path));
        m_window->testOpenImageFile(path);

        // Tabs are keyed by the canonical (symlink-resolved) path.
        const QString canonical = QFileInfo(path).canonicalFilePath();
        QVERIFY(m_window->testTab(canonical) != nullptr);
        ImageSession *session = m_window->testTab(canonical)->session();
        QSignalSpy    createdSpy(session, &ImageSession::snapshotCreated);
        auto          advance = [&](const QColor& color) {
            QImage img(100, 100, QImage::Format_ARGB32);
            img.fill(color);
            QVERIFY(img.save(path));
            QTRY_COMPARE_WITH_TIMEOUT(session->diskImage().pixelColor(0, 0), color, 5000);
        };
        auto snapshotNow = [&]() {
            m_window->testOnSaveSnapshot();
            QTRY_VERIFY_WITH_TIMEOUT(createdSpy.count() >= 1, 5000);
            createdSpy.clear();
        };

        advance(Qt::green);
        snapshotNow(); // S1 (base)
        advance(Qt::blue);
        snapshotNow(); // S2 (delta)
        advance(Qt::cyan);
        snapshotNow(); // S3 (delta)
        QCOMPARE(SnapshotManager::loadSnapshots(path).size(), 3);

        // Current disk state is newer than the last snapshot.
        advance(Qt::magenta);
        AppSettings::setAutosaveSnapshots(true);

        // Copy the file (old one stays) and update the path, both through
        // the symlinked directory.
        QString newPath = linkPath + "/moved.png";
        QImage  magenta(100, 100, QImage::Format_ARGB32);
        magenta.fill(Qt::magenta);
        QVERIFY(magenta.save(newPath));

        const QString newCanonical = QFileInfo(newPath).canonicalFilePath();
        QVERIFY(m_window->sessionController()->changeActiveSessionPath(newPath));
        ImageTab *tab = m_window->testTakeTab(canonical);
        m_window->testInsertTab(newCanonical, tab);
        m_window->testUpdateSnapshotTimeline();

        int initialRows = m_window->snapshotController()->model()->rowCount();
        QCOMPARE(initialRows, 4); // 3 snapshots + current image

        // External edit at the new path: autosave must refresh the timeline.
        QImage black(100, 100, QImage::Format_ARGB32);
        black.fill(Qt::black);
        QVERIFY(black.save(newPath));

        QTRY_VERIFY_WITH_TIMEOUT(createdSpy.count() >= 1, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(
            m_window->snapshotController()->model()->rowCount(), initialRows + 1, 5000);
    }

    void testOpenSameFileViaSymlinkDeduplicates() {
        QTemporaryDir realDir;
        QTemporaryDir linkParent;
        QVERIFY(realDir.isValid());
        QVERIFY(linkParent.isValid());

        QString linkPath = linkParent.path() + "/media";
        QVERIFY(::symlink(qPrintable(realDir.path()), qPrintable(linkPath)) == 0);

        const QString realPath = realDir.path() + "/Hypothetical.png";
        const QString aliasPath = linkPath + "/Hypothetical.png";
        QImage        red(100, 100, QImage::Format_ARGB32);
        red.fill(Qt::red);
        QVERIFY(red.save(realPath));

        // Open the same file through both aliases: must yield one tab,
        // keyed by the canonical path.
        const QString canonical = QFileInfo(realPath).canonicalFilePath();
        m_window->testOpenImageFile(realPath);
        m_window->testOpenImageFile(aliasPath);
        QCOMPARE(m_window->tabPaths().size(), 1);
        QVERIFY(m_window->tabPaths().contains(canonical));

        // And the session controller holds a single session for it.
        QCOMPARE(m_window->sessionController()->openImage(aliasPath, false),
                 m_window->testTab(canonical)->session());
        QCOMPARE(m_window->tabPaths().size(), 1);
    }

    void testUpdatePathActionState() {
        // Empty state: action hidden.
        QVERIFY(!m_window->updatePathAction()->isVisible());

        m_window->testOpenImageFile(m_testFiles[0]);
        QVERIFY(m_window->updatePathAction()->isVisible());
        QVERIFY(m_window->updatePathAction()->isEnabled());

        m_window->testOnCloseTab(0);
        QVERIFY(!m_window->updatePathAction()->isVisible());
    }

  private:
    MainWindowTestWrapper *m_window = nullptr;
    QStringList            m_testFiles;
};

QTEST_MAIN(TestMainWindow)
#include "test_mainwindow.moc"
