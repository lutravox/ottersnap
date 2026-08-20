#include <QtTest>
#include <QSignalSpy>
#include "controllers/shortcutmanager.h"
#include <QCoreApplication>

class TestShortcutManager : public QObject {
    Q_OBJECT

private slots:
    void init() {
        // Use a unique organization/app name for testing to avoid polluting real settings
        QCoreApplication::setOrganizationName("TestOrg");
        QCoreApplication::setApplicationName("TestApp");
    }

    void testDefaults() {
        ShortcutManager manager;
        
        // Test a known default
        QKeySequence openSeq = ShortcutManager::defaults().value("file.open");
        QCOMPARE(manager.shortcutFor("file.open"), openSeq);
        
        // Test a non-existent shortcut
        QCOMPARE(manager.shortcutFor("non.existent"), QKeySequence());
    }

    void testSetShortcut() {
        ShortcutManager manager;
        QSignalSpy spy(&manager, &ShortcutManager::shortcutChanged);
        
        QKeySequence newSeq(Qt::CTRL | Qt::Key_X);
        manager.setShortcut("file.open", newSeq);
        
        QCOMPARE(manager.shortcutFor("file.open"), newSeq);
        QCOMPARE(spy.count(), 1);
        
        QList<QVariant> arguments = spy.takeFirst();
        QCOMPARE(arguments.at(0).toString(), QString("file.open"));
        QCOMPARE(arguments.at(1).value<QKeySequence>(), newSeq);
    }

    void testRedundantSet() {
        ShortcutManager manager;
        QKeySequence currentSeq = manager.shortcutFor("file.open");
        
        QSignalSpy spy(&manager, &ShortcutManager::shortcutChanged);
        manager.setShortcut("file.open", currentSeq);
        
        QCOMPARE(spy.count(), 0);
    }

    void testAllShortcuts() {
        ShortcutManager manager;
        auto all = manager.allShortcuts();
        
        QCOMPARE(all.size(), ShortcutManager::defaults().size());
        
        QKeySequence customSeq(Qt::ALT | Qt::Key_Z);
        manager.setShortcut("app.exit", customSeq);
        
        all = manager.allShortcuts();
        QCOMPARE(all.size(), ShortcutManager::defaults().size());
        QCOMPARE(all.value("app.exit"), customSeq);
        QCOMPARE(all.value("file.open"), ShortcutManager::defaults().value("file.open"));
    }

    void testPersistence() {
        // We create a scope to ensure the first manager is destroyed and settings flushed
        {
            ShortcutManager manager;
            QKeySequence customSeq(Qt::CTRL | Qt::Key_Y);
            manager.setShortcut("tool.grayscale", customSeq);
            manager.save();
        }
        
        // Now create a new manager and see if it loads the saved shortcut
        ShortcutManager manager2;
        // Note: load() is called in constructor
        QCOMPARE(manager2.shortcutFor("tool.grayscale"), QKeySequence(Qt::CTRL | Qt::Key_Y));
        
        // Verify other shortcuts are still defaults
        QCOMPARE(manager2.shortcutFor("file.open"), ShortcutManager::defaults().value("file.open"));
    }

    void cleanup() {
        // Clear settings after test
        QSettings settings;
        settings.clear();
    }
};

#include "test_shortcutmanager.moc"

QTEST_MAIN(TestShortcutManager)
