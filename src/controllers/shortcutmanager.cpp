#include "controllers/shortcutmanager.h"
#include <QSettings>

const QMap<QString, QKeySequence>& ShortcutManager::defaults() {
    static const QMap<QString, QKeySequence> defaultShortcuts = {
        {"file.open", QKeySequence::Open},
        {"tab.close", QKeySequence::Close},
        {"app.exit", QKeySequence::Quit},
        {"snapshot.save", QKeySequence(Qt::CTRL | Qt::Key_S)},
        {"snapshot.delete", QKeySequence(Qt::CTRL | Qt::Key_D)},
        {"viewer.scaleWithWindow", QKeySequence(Qt::CTRL | Qt::Key_F)},
        {"viewer.toggleToolbar", QKeySequence(Qt::CTRL | Qt::Key_T)},
        {"viewer.resetView", QKeySequence(Qt::CTRL | Qt::Key_0)},
        {"viewer.actualSize", QKeySequence(Qt::CTRL | Qt::Key_1)},
        {"viewer.zoomIn", QKeySequence(Qt::CTRL | Qt::Key_Equal)},
        {"viewer.zoomOut", QKeySequence(Qt::CTRL | Qt::Key_Minus)},
        {"viewer.fullScreen", QKeySequence(Qt::Key_F11)},
        {"tool.colorPicker", QKeySequence(Qt::CTRL | Qt::Key_I)},
        {"tool.grayscale", QKeySequence(Qt::CTRL | Qt::Key_G)},
        {"tool.mirror", QKeySequence(Qt::CTRL | Qt::Key_M)},
        {"tool.swap", QKeySequence(Qt::CTRL | Qt::Key_P)},
        {"nav.prev", QKeySequence(Qt::Key_Left)},
        {"nav.next", QKeySequence(Qt::Key_Right)},
    };
    return defaultShortcuts;
}

ShortcutManager::ShortcutManager(QObject *parent) : QObject(parent) {
    load();
}

QKeySequence ShortcutManager::shortcutFor(const QString& actionId) const {
    return m_shortcuts.value(actionId, defaults().value(actionId));
}

void ShortcutManager::setShortcut(const QString& actionId, const QKeySequence& sequence) {
    if (m_shortcuts.value(actionId, defaults().value(actionId)) != sequence) {
        m_shortcuts[actionId] = sequence;
        emit shortcutChanged(actionId, sequence);
    }
}

QMap<QString, QKeySequence> ShortcutManager::allShortcuts() const {
    QMap<QString, QKeySequence> result = defaults();
    for (auto it = m_shortcuts.constBegin(); it != m_shortcuts.constEnd(); ++it) {
        result.insert(it.key(), it.value());
    }
    return result;
}

void ShortcutManager::load() {
    QSettings settings;
    for (auto it = defaults().constBegin(); it != defaults().constEnd(); ++it) {
        QString key = "shortcuts/" + it.key();
        if (settings.contains(key)) {
            m_shortcuts[it.key()] = QKeySequence::fromString(settings.value(key).toString());
        }
    }
}

void ShortcutManager::save() {
    QSettings settings;
    for (auto it = m_shortcuts.constBegin(); it != m_shortcuts.constEnd(); ++it) {
        settings.setValue("shortcuts/" + it.key(), it.value().toString());
    }
}
