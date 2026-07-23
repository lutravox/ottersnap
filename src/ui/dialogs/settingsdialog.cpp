#include "ui/dialogs/settingsdialog.h"
#include "config/appsettings.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("Settings"));
    setMinimumWidth(400);

    auto *layout = new QVBoxLayout(this);

    m_cbRestoreSession = new QCheckBox(tr("Restore session"), this);
    m_cbRestoreSession->setChecked(AppSettings::restoreSession());
    layout->addWidget(m_cbRestoreSession);

    m_cbAutosave = new QCheckBox(tr("Autosave snapshots"), this);
    m_cbAutosave->setChecked(AppSettings::autosaveSnapshots());
    layout->addWidget(m_cbAutosave);

    // Snapshot cache size
    auto *cacheLayout = new QHBoxLayout();
    auto *cacheLabel = new QLabel(tr("Snapshot cache size (MB):"), this);
    m_sbCacheSize = new QSpinBox(this);
    m_sbCacheSize->setRange(64, 8192);
    m_sbCacheSize->setValue(AppSettings::maxSnapshotCacheSizeMB());
    cacheLayout->addWidget(cacheLabel);
    cacheLayout->addWidget(m_sbCacheSize);
    layout->addLayout(cacheLayout);

    layout->addStretch();

    // Button box (OK / Cancel)
    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        AppSettings::setRestoreSession(m_cbRestoreSession->isChecked());
        AppSettings::setAutosaveSnapshots(m_cbAutosave->isChecked());
        AppSettings::setMaxSnapshotCacheSizeMB(m_sbCacheSize->value());
        accept();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}
