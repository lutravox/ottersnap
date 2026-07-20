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

    // Fit behavior
    m_cbResizeToFit = new QCheckBox(tr("Fit"), this);
    m_cbResizeToFit->setChecked(AppSettings::resizeToFit());
    layout->addWidget(m_cbResizeToFit);

    m_cbRestoreSession = new QCheckBox(tr("Restore session"), this);
    m_cbRestoreSession->setChecked(AppSettings::restoreSession());
    layout->addWidget(m_cbRestoreSession);

    // Version cache size
    auto *cacheLayout = new QHBoxLayout();
    auto *cacheLabel = new QLabel(tr("Version cache size (MB):"), this);
    m_sbCacheSize = new QSpinBox(this);
    m_sbCacheSize->setRange(64, 8192);
    m_sbCacheSize->setValue(AppSettings::maxVersionCacheSizeMB());
    cacheLayout->addWidget(cacheLabel);
    cacheLayout->addWidget(m_sbCacheSize);
    layout->addLayout(cacheLayout);

    layout->addStretch();

    // Button box (OK / Cancel)
    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        AppSettings::setResizeToFit(m_cbResizeToFit->isChecked());
        AppSettings::setRestoreSession(m_cbRestoreSession->isChecked());
        AppSettings::setMaxVersionCacheSizeMB(m_sbCacheSize->value());
        accept();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}
