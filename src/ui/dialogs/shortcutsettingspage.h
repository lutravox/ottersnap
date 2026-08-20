#pragma once

#include <QWidget>
#include <QPushButton>
#include "controllers/shortcutmanager.h"

class ShortcutSettingsPage : public QWidget {
    Q_OBJECT

  public:
    explicit ShortcutSettingsPage(ShortcutManager* manager, QWidget *parent = nullptr);
    void commitChanges();
    void resetPending();

  private slots:
    void onShortcutRequested(const QString& actionId, const QKeySequence& sequence);

  private:
    void setupUi();
    void recordShortcut(const QString& actionId);

    ShortcutManager *m_manager;
    QMap<QString, QKeySequence> m_pendingShortcuts;
};

class ShortcutButton : public QPushButton {
    Q_OBJECT
  public:
    ShortcutButton(const QString& actionId, ShortcutManager* manager, QWidget* parent = nullptr);
    void refresh();
    QString actionId() const { return m_actionId; }

  protected:
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

  signals:
    void shortcutRequested(const QString& actionId, const QKeySequence& sequence);

  private:
    QString m_actionId;
    ShortcutManager *m_manager;
};

