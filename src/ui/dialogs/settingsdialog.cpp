#include "ui/dialogs/settingsdialog.h"
#include "config/appsettings.h"
#include "core/thumbnailcache.h"

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

    // Thumbnail cache size

    // Thumbnail cache size
    auto *thumbLayout = new QHBoxLayout();
    auto *thumbLabel = new QLabel(tr("Thumbnail cache size (MB):"), this);
    m_sbThumbCacheSize = new QSpinBox(this);
    m_sbThumbCacheSize->setRange(16, 2048);
    m_sbThumbCacheSize->setValue(AppSettings::maxThumbnailCacheSizeMB());
    thumbLayout->addWidget(thumbLabel);
    thumbLayout->addWidget(m_sbThumbCacheSize);
    layout->addLayout(thumbLayout);

    layout->addStretch();

    // Button box (OK / Cancel)
    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        AppSettings::setRestoreSession(m_cbRestoreSession->isChecked());
        AppSettings::setAutosaveSnapshots(m_cbAutosave->isChecked());
        AppSettings::setMaxThumbnailCacheSizeMB(m_sbThumbCacheSize->value());
        ThumbnailCache::updateMaxCost(m_sbThumbCacheSize->value());
        accept();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}
