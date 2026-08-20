#pragma once

#include <QObject>
#include <QMap>
#include <QKeySequence>
#include <QString>

/// @brief Manages application shortcuts, providing a way to customize and persist them.
class ShortcutManager : public QObject {
    Q_OBJECT
  public:
    explicit ShortcutManager(QObject *parent = nullptr);

    /// @brief Get the shortcut for a given action ID.
    /// @param actionId The unique identifier for the action.
    /// @return The associated QKeySequence.
    QKeySequence shortcutFor(const QString& actionId) const;

    /// @brief Set a new shortcut for a given action ID.
    /// @param actionId The unique identifier for the action.
    /// @param sequence The new key sequence.
    void setShortcut(const QString& actionId, const QKeySequence& sequence);

    /// @brief Return the default shortcuts for the application.
    /// @return A map of action IDs to their default shortcuts.
    static const QMap<QString, QKeySequence>& defaults();

    /// @brief Return all currently managed shortcuts.
    /// @return A map of action IDs to their shortcuts.
    QMap<QString, QKeySequence> allShortcuts() const;

    /// @brief Load shortcuts from settings.
    void load();

    /// @brief Save shortcuts to settings.
    void save();

signals:
    /// @brief Emitted when a shortcut is changed.
    void shortcutChanged(const QString& actionId, const QKeySequence& newSequence);

  private:
    QMap<QString, QKeySequence> m_shortcuts;
};
