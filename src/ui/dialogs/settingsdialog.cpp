#include "ui/dialogs/settingsdialog.h"
#include "config/appsettings.h"
#include "controllers/appsettingscontroller.h"
#include "core/deltacache.h"
#include "core/thumbnailcache.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <sys/sysinfo.h>

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent), m_settings(this) {
    setWindowTitle(tr("Settings"));
    setMinimumWidth(400);

    auto *layout = new QVBoxLayout(this);

    m_cbRestoreSession = new QCheckBox(tr("Restore session"), this);
    m_cbRestoreSession->setChecked(AppSettings::restoreSession());
    layout->addWidget(m_cbRestoreSession);

    m_cbAutoreload = new QCheckBox(tr("Autoreload images"), this);
    m_cbAutoreload->setChecked(m_settings.shouldAutoreloadImages());
    layout->addWidget(m_cbAutoreload);

    m_cbAutosave = new QCheckBox(tr("Autosave snapshots"), this);
    m_cbAutosave->setChecked(m_settings.shouldAutosaveSnapshots());
    m_cbAutosave->setEnabled(m_settings.isAutosaveSnapshotsEnabled());
    layout->addWidget(m_cbAutosave);

    m_cbSnapshotOnReopen = new QCheckBox(tr("Save snapshot on reopen"), this);
    m_cbSnapshotOnReopen->setChecked(AppSettings::snapshotOnReopen());
    layout->addWidget(m_cbSnapshotOnReopen);

    connect(m_cbAutoreload, &QCheckBox::toggled, [this](bool checked) {
        m_cbAutosave->setEnabled(checked);
        if (!checked) {
            m_cbAutosave->setChecked(false);
        }
    });

    connect(m_cbAutosave, &QCheckBox::toggled, [this](bool checked) {
        if (checked) {
            m_cbSnapshotOnReopen->setChecked(false);
        }
    });

    connect(m_cbSnapshotOnReopen, &QCheckBox::toggled, [this](bool checked) {
        if (checked) {
            m_cbAutosave->setChecked(false);
        }
    });

    // Background color
    auto *bgLayout = new QHBoxLayout();
    auto *bgLabel = new QLabel(tr("Background color:"), this);
    m_btnBackgroundColor = new QPushButton(this);
    m_bgColor = AppSettings::backgroundColor();
    m_btnBackgroundColor->setStyleSheet(QString("background-color: %1;").arg(m_bgColor.name()));

    connect(m_btnBackgroundColor, &QPushButton::clicked, this, [this]() {
        QColor color = QColorDialog::getColor(m_bgColor, this, tr("Select Background Color"));
        if (color.isValid()) {
            m_bgColor = color;
            m_btnBackgroundColor->setStyleSheet(
                QString("background-color: %1;").arg(m_bgColor.name()));
        }
    });

    auto *btnResetBg = new QPushButton(tr("Reset"), this);
    btnResetBg->setFixedWidth(60);
    connect(btnResetBg, &QPushButton::clicked, this, [this]() {
        m_bgColor = QColor(c_defaultBackgroundColor);
        m_btnBackgroundColor->setStyleSheet(QString("background-color: %1;").arg(m_bgColor.name()));
    });

    bgLayout->addWidget(bgLabel);
    bgLayout->addWidget(m_btnBackgroundColor);
    bgLayout->addWidget(btnResetBg);
    layout->addLayout(bgLayout);

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
        m_settings.setRestoreSession(m_cbRestoreSession->isChecked());
        m_settings.setAutosaveSnapshots(m_cbAutosave->isChecked());
        m_settings.setAutoreloadImages(m_cbAutoreload->isChecked());
        m_settings.setSnapshotOnReopen(m_cbSnapshotOnReopen->isChecked());
        m_settings.setBackgroundColor(m_bgColor);
        m_settings.setMaxThumbnailCacheSizeMB(m_sbThumbCacheSize->value());
        ThumbnailCache::updateMaxCost(m_sbThumbCacheSize->value());
        m_settings.setMaxDeltaCacheSizeMB(m_sbDeltaCacheSize->value());
        DeltaCache::updateMaxCost(m_sbDeltaCacheSize->value());
        accept();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}
