#include "ui/dialogs/settingsdialog.h"
#include "config/appsettings.h"
#include "core/deltacache.h"
#include "core/thumbnailcache.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

#include <sys/sysinfo.h>

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

    // Cap the spinbox max at total system RAM
    struct sysinfo si;
    sysinfo(&si);
    int maxMB = static_cast<int>(si.totalram / (1024 * 1024));

    // Thumbnail cache size
    auto *thumbLayout = new QHBoxLayout();
    auto *thumbLabel = new QLabel(tr("Thumbnail cache size (MB):"), this);
    m_sbThumbCacheSize = new QSpinBox(this);

    m_sbThumbCacheSize->setRange(c_minCacheSizeMB, maxMB);
    m_sbThumbCacheSize->setValue(AppSettings::maxThumbnailCacheSizeMB());
    thumbLayout->addWidget(thumbLabel);
    thumbLayout->addWidget(m_sbThumbCacheSize);
    layout->addLayout(thumbLayout);

    // Delta cache size
    auto *deltaLayout = new QHBoxLayout();
    auto *deltaLabel = new QLabel(tr("Delta cache size (MB):"), this);
    m_sbDeltaCacheSize = new QSpinBox(this);
    m_sbDeltaCacheSize->setRange(c_minCacheSizeMB, maxMB);
    m_sbDeltaCacheSize->setValue(AppSettings::maxDeltaCacheSizeMB());
    deltaLayout->addWidget(deltaLabel);
    deltaLayout->addWidget(m_sbDeltaCacheSize);
    layout->addLayout(deltaLayout);

    layout->addStretch();

    // Button box (OK / Cancel)
    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        AppSettings::setRestoreSession(m_cbRestoreSession->isChecked());
        AppSettings::setAutosaveSnapshots(m_cbAutosave->isChecked());
        AppSettings::setMaxThumbnailCacheSizeMB(m_sbThumbCacheSize->value());
        ThumbnailCache::updateMaxCost(m_sbThumbCacheSize->value());
        AppSettings::setMaxDeltaCacheSizeMB(m_sbDeltaCacheSize->value());
        DeltaCache::updateMaxCost(m_sbDeltaCacheSize->value());
        accept();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}
