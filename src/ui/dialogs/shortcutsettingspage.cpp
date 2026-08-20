#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QTableWidget>
#include <QVBoxLayout>
#include "ui/dialogs/shortcutsettingspage.h"

ShortcutButton::ShortcutButton(const QString& actionId, ShortcutManager *manager, QWidget *parent)
    : QPushButton(parent), m_actionId(actionId), m_manager(manager) {
    setText(manager->shortcutFor(actionId).toString());

    connect(m_manager,
            &ShortcutManager::shortcutChanged,
            this,
            [this](const QString& id, const QKeySequence& seq) {
                if (id == m_actionId) {
                    setText(seq.toString());
                }
            });
}

void ShortcutButton::refresh() {
    setText(m_manager->shortcutFor(m_actionId).toString());
}

void ShortcutButton::mousePressEvent(QMouseEvent *event) {
    setFocus();
    setText("...");
    QPushButton::mousePressEvent(event);
}

void ShortcutButton::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Control || event->key() == Qt::Key_Shift ||
        event->key() == Qt::Key_Alt || event->key() == Qt::Key_Meta) {
        event->ignore();
        return;
    }

    QKeySequence seq = QKeySequence(event->key());
    if (event->modifiers() != Qt::NoModifier) {
        seq = QKeySequence(event->modifiers() | event->key());
    }

    if (seq.isEmpty()) {
        setText(m_manager->shortcutFor(m_actionId).toString());
        return;
    }

    emit shortcutRequested(m_actionId, seq);
    setText(seq.toString());
    clearFocus();
}

void ShortcutSettingsPage::onShortcutRequested(const QString&      actionId,
                                               const QKeySequence& sequence) {
    if (sequence.isEmpty()) {
        m_pendingShortcuts[actionId] = sequence;

        auto buttons = findChildren<ShortcutButton *>();
        for (auto *btn : buttons) {
            if (btn->actionId() == actionId)
                btn->setText("");
        }
        return;
    }

    // Check for duplicates
    QMap<QString, QKeySequence> effectiveShortcuts = m_manager->allShortcuts();
    effectiveShortcuts.insert(m_pendingShortcuts);

    QString duplicateActionId;
    for (auto it = effectiveShortcuts.constBegin(); it != effectiveShortcuts.constEnd(); ++it) {
        if (it.key() != actionId && it.value() == sequence) {
            duplicateActionId = it.key();
            break;
        }
    }

    if (!duplicateActionId.isEmpty()) {
        auto result =
            QMessageBox::question(this,
                                  tr("Duplicate Shortcut"),
                                  tr("The shortcut %1 is already assigned to another action.\nDo "
                                     "you want to reassign it to the current action?")
                                      .arg(sequence.toString()),
                                  QMessageBox::Yes | QMessageBox::No);

        if (result == QMessageBox::No) {
            // Revert the button text to previous state
            auto buttons = findChildren<ShortcutButton *>();
            for (auto *btn : buttons) {
                if (btn->actionId() == actionId) {
                    QKeySequence oldSeq = m_pendingShortcuts.contains(actionId)
                                              ? m_pendingShortcuts[actionId]
                                              : m_manager->shortcutFor(actionId);
                    btn->setText(oldSeq.toString());
                }
            }
            return;
        }

        // Reassign: Clear the duplicate action in pending
        m_pendingShortcuts[duplicateActionId] = QKeySequence();
        auto buttons = findChildren<ShortcutButton *>();
        for (auto *btn : buttons) {
            if (btn->actionId() == duplicateActionId) {
                btn->setText("");
            }
        }
    }

    m_pendingShortcuts[actionId] = sequence;

    // Update the button text immediately
    auto buttons = findChildren<ShortcutButton *>();
    for (auto *btn : buttons) {
        if (btn->actionId() == actionId) {
            btn->setText(sequence.toString());
        }
    }
}

ShortcutSettingsPage::ShortcutSettingsPage(ShortcutManager *manager, QWidget *parent)
    : QWidget(parent), m_manager(manager) {
    setupUi();
}

void ShortcutSettingsPage::commitChanges() {
    for (auto it = m_pendingShortcuts.constBegin(); it != m_pendingShortcuts.constEnd(); ++it) {
        m_manager->setShortcut(it.key(), it.value());
    }
    m_pendingShortcuts.clear();
}

void ShortcutSettingsPage::resetPending() {
    m_pendingShortcuts.clear();
    auto buttons = findChildren<ShortcutButton *>();
    for (auto *btn : buttons) {
        btn->refresh();
    }
}

void ShortcutSettingsPage::resetToDefaults() {
    const auto& defaults = ShortcutManager::defaults();
    for (auto it = defaults.constBegin(); it != defaults.constEnd(); ++it) {
        m_pendingShortcuts[it.key()] = it.value();
    }

    auto buttons = findChildren<ShortcutButton *>();
    for (auto *btn : buttons) {
        btn->setText(defaults.value(btn->actionId()).toString());
    }
}

void ShortcutSettingsPage::setupUi() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *container = new QWidget();
    auto *containerLayout = new QVBoxLayout(container);
    containerLayout->setContentsMargins(20, 20, 20, 20);
    containerLayout->setSpacing(20);
    containerLayout->setAlignment(Qt::AlignTop);

    auto shortcuts = m_manager->allShortcuts();

    static const QMap<QString, QString> names = {
        {"file.open", tr("Open Image")},
        {"tab.close", tr("Close Tab")},
        {"app.exit", tr("Exit")},
        {"snapshot.save", tr("Create Snapshot")},
        {"snapshot.delete", tr("Delete Snapshot")},
        {"viewer.scaleWithWindow", tr("Scale with Window")},
        {"viewer.toggleToolbar", tr("Show Toolbar")},
        {"viewer.resetView", tr("Reset View")},
        {"viewer.actualSize", tr("Actual Size (100%)")},
        {"viewer.zoomIn", tr("Zoom In")},
        {"viewer.zoomOut", tr("Zoom Out")},
        {"viewer.fullScreen", tr("Toggle Full Screen")},
        {"tool.colorPicker", tr("Color Picker")},
        {"tool.grayscale", tr("Grayscale")},
        {"tool.mirror", tr("Mirror")},
        {"tool.swap", tr("Swap Comparison")},
        {"nav.prev", tr("Previous Snapshot")},
        {"nav.next", tr("Next Snapshot")},
    };

    auto createGroup =
        [this, &shortcuts](QVBoxLayout *parent, const QString& title, const QStringList& ids) {
            auto *group = new QGroupBox(title, this);
            auto *layout = new QFormLayout(group);
            layout->setSpacing(15);
            layout->setLabelAlignment(Qt::AlignLeft);
            layout->setContentsMargins(15, 15, 15, 15);

            for (const QString& id : ids) {
                if (!shortcuts.contains(id))
                    continue;

                QString name = names.value(id, id);

                auto *controls = new QWidget(this);
                auto *hLayout = new QHBoxLayout(controls);
                hLayout->setContentsMargins(0, 0, 0, 0);
                hLayout->setSpacing(5);

                auto *btn = new ShortcutButton(id, m_manager, this);
                auto *resetBtn = new QPushButton(tr("Reset"), this);
                resetBtn->setFixedWidth(60);

                hLayout->addStretch();
                hLayout->addWidget(btn);
                hLayout->addWidget(resetBtn);

                connect(btn,
                        &ShortcutButton::shortcutRequested,
                        this,
                        &ShortcutSettingsPage::onShortcutRequested);
                connect(resetBtn, &QPushButton::clicked, [this, id]() {
                    onShortcutRequested(id, ShortcutManager::defaults().value(id));
                });

                layout->addRow(name, controls);
            }
            parent->addWidget(group);
        };

    createGroup(containerLayout, tr("Application"), {"file.open", "tab.close", "app.exit"});
    createGroup(containerLayout, tr("Snapshots"), {"snapshot.save", "snapshot.delete"});
    createGroup(containerLayout,
                tr("Viewer"),
                {"viewer.scaleWithWindow",
                 "viewer.toggleToolbar",
                 "viewer.resetView",
                 "viewer.actualSize",
                 "viewer.zoomIn",
                 "viewer.zoomOut",
                 "viewer.fullScreen"});
    createGroup(containerLayout,
                tr("Tools"),
                {"tool.colorPicker", "tool.grayscale", "tool.mirror", "tool.swap"});
    createGroup(containerLayout, tr("Navigation"), {"nav.prev", "nav.next"});

    scrollArea->setWidget(container);
    mainLayout->addWidget(scrollArea);
}
