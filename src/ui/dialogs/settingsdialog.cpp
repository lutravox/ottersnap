#include "ui/dialogs/settingsdialog.h"
#include "config/appsettings.h"

#include <QCheckBox>
#include <QDialogButtonBox>
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

    layout->addStretch();

    // Button box (OK / Cancel)
    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        AppSettings::setResizeToFit(m_cbResizeToFit->isChecked());
        AppSettings::setRestoreSession(m_cbRestoreSession->isChecked());
        accept();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}
