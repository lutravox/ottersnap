#include "ui/dialogs/settingsdialog.h"
#include "config/appsettings.h"
#include "controllers/appsettingscontroller.h"
#include "core/deltacache.h"
#include "core/thumbnailcache.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

#include <sys/sysinfo.h>

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent), m_settings(this) {
    setWindowTitle(tr("Settings"));
    setMinimumWidth(500);

    auto *mainLayout = new QVBoxLayout(this);

    // Create a container for the form to allow it to be centered as a single block
    auto *formContainer = new QWidget(this);
    auto *formLayout = new QFormLayout(formContainer);
    formLayout->setSpacing(15);
    formLayout->setLabelAlignment(Qt::AlignRight);

    // Helper to add a section header and a divider line
    auto addSection = [formLayout](const QString& title) {
        auto *header = new QLabel(title);
        header->setStyleSheet("font-weight: bold; font-size: 13px; margin-top: 20px; "
                              "margin-bottom: 5px; color: #aaa;");
        formLayout->addRow(header);

        auto *line = new QWidget();
        line->setFixedHeight(1);
        line->setStyleSheet("background-color: #444;");
        formLayout->addRow(line);
    };

    addSection(tr("General"));
    m_cbRestoreSession = new QCheckBox(this);
    m_cbRestoreSession->setChecked(AppSettings::restoreSession());
    m_cbRestoreSession->setToolTip(tr("Reopen all images from the previous session on startup."));
    formLayout->addRow(tr("Restore session"), m_cbRestoreSession);

    addSection(tr("Images & Snapshots"));
    m_cbAutoreload = new QCheckBox(this);
    m_cbAutoreload->setChecked(m_settings.shouldAutoreloadImages());
    m_cbAutoreload->setToolTip(
        tr("Automatically reload the image if the file is modified on disk."));
    formLayout->addRow(tr("Auto-reload images"), m_cbAutoreload);

    auto *snapshotGroup = new QButtonGroup(this);
    m_rbNone = new QRadioButton(tr("None"), this);
    m_rbAutosave = new QRadioButton(tr("On auto-reload"), this);
    m_rbSnapshotOnReopen = new QRadioButton(tr("On reopen"), this);

    m_rbNone->setToolTip(tr("Disable automatic snapshot creation."));
    m_rbAutosave->setToolTip(
        tr("Automatically create a snapshot when the image is reloaded or modified."));
    m_rbSnapshotOnReopen->setToolTip(tr("Automatically create a snapshot when opening an image "
                                        "that is already open in a tab."));

    snapshotGroup->addButton(m_rbNone);
    snapshotGroup->addButton(m_rbAutosave);
    snapshotGroup->addButton(m_rbSnapshotOnReopen);

    // Initialize radio buttons based on current settings
    if (m_settings.shouldAutosaveSnapshots()) {
        m_rbAutosave->setChecked(true);
    } else if (AppSettings::snapshotOnReopen()) {
        m_rbSnapshotOnReopen->setChecked(true);
    } else {
        m_rbNone->setChecked(true);
    }

    formLayout->addRow(tr("Autosave snapshot:"), m_rbNone);
    formLayout->addRow("", m_rbAutosave);
    formLayout->addRow("", m_rbSnapshotOnReopen);

    addSection(tr("Performance"));
    struct sysinfo si;
    sysinfo(&si);
    int maxMB = static_cast<int>(si.totalram / (1024 * 1024));

    m_sbThumbCacheSize = new QSpinBox(this);
    m_sbThumbCacheSize->setRange(c_minCacheSizeMB, maxMB);
    m_sbThumbCacheSize->setValue(AppSettings::maxThumbnailCacheSizeMB());
    m_sbThumbCacheSize->setToolTip(
        tr("Maximum memory allocated for thumbnail caching. Larger values can speed up browsing."));
    formLayout->addRow(tr("Thumbnail cache size (MB):"), m_sbThumbCacheSize);

    m_sbDeltaCacheSize = new QSpinBox(this);
    m_sbDeltaCacheSize->setRange(c_minCacheSizeMB, maxMB);
    m_sbDeltaCacheSize->setValue(AppSettings::maxDeltaCacheSizeMB());
    m_sbDeltaCacheSize->setToolTip(
        tr("Maximum memory allocated for storing snapshot deltas. Larger "
           "values improve snapshot switching speed."));
    formLayout->addRow(tr("Snapshot cache size (MB):"), m_sbDeltaCacheSize);

    addSection(tr("Appearance"));
    auto *bgWidget = new QWidget(this);
    auto *bgHLayout = new QHBoxLayout(bgWidget);
    bgHLayout->setContentsMargins(0, 0, 0, 0);
    bgHLayout->setSpacing(10);

    m_btnBackgroundColor = new QPushButton(this);
    m_bgColor = AppSettings::backgroundColor();
    m_btnBackgroundColor->setStyleSheet(QString("background-color: %1;").arg(m_bgColor.name()));
    m_btnBackgroundColor->setFixedWidth(100);
    m_btnBackgroundColor->setToolTip(tr("Set the background color of the image viewer."));

    auto *btnResetBg = new QPushButton(tr("Reset"), this);
    btnResetBg->setFixedWidth(60);

    bgHLayout->addWidget(m_btnBackgroundColor);
    bgHLayout->addWidget(btnResetBg);
    bgHLayout->addStretch();

    formLayout->addRow(tr("Viewer background color:"), bgWidget);

    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *scrollContent = new QWidget();
    auto *scrollContentLayout = new QVBoxLayout(scrollContent);
    scrollContentLayout->setAlignment(Qt::AlignCenter);

    auto *centerLayout = new QHBoxLayout();
    centerLayout->addStretch();
    centerLayout->addWidget(formContainer);
    centerLayout->addStretch();
    scrollContentLayout->addLayout(centerLayout);

    scrollArea->setWidget(scrollContent);
    mainLayout->addWidget(scrollArea);

    connect(m_cbAutoreload, &QCheckBox::toggled, [this](bool checked) {
        m_rbAutosave->setEnabled(checked);
        if (!checked && m_rbAutosave->isChecked()) {
            m_rbNone->setChecked(true);
        }
    });

    connect(m_btnBackgroundColor, &QPushButton::clicked, this, [this]() {
        QColor color = QColorDialog::getColor(m_bgColor, this, tr("Select Background Color"));
        if (color.isValid()) {
            m_bgColor = color;
            m_btnBackgroundColor->setStyleSheet(
                QString("background-color: %1;").arg(m_bgColor.name()));
        }
    });

    connect(btnResetBg, &QPushButton::clicked, this, [this]() {
        m_bgColor = QColor(c_defaultBackgroundColor);
        m_btnBackgroundColor->setStyleSheet(QString("background-color: %1;").arg(m_bgColor.name()));
    });

    mainLayout->addStretch();

    // Button box (OK / Cancel)
    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    auto *bottomLayout = new QHBoxLayout();
    auto *btnResetAll = new QPushButton(tr("Reset All"), this);
    btnResetAll->setToolTip(tr("Reset all settings to their default values."));

    bottomLayout->addWidget(btnResetAll);
    bottomLayout->addStretch();
    bottomLayout->addWidget(buttonBox);

    mainLayout->addLayout(bottomLayout);

    connect(btnResetAll, &QPushButton::clicked, this, &SettingsDialog::resetAllSettings);

    connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        m_settings.setRestoreSession(m_cbRestoreSession->isChecked());
        m_settings.setAutosaveSnapshots(m_rbAutosave->isChecked());
        m_settings.setAutoreloadImages(m_cbAutoreload->isChecked());
        m_settings.setSnapshotOnReopen(m_rbSnapshotOnReopen->isChecked());
        m_settings.setBackgroundColor(m_bgColor);
        m_settings.setMaxThumbnailCacheSizeMB(m_sbThumbCacheSize->value());
        ThumbnailCache::updateMaxCost(m_sbThumbCacheSize->value());
        m_settings.setMaxDeltaCacheSizeMB(m_sbDeltaCacheSize->value());
        DeltaCache::updateMaxCost(m_sbDeltaCacheSize->value());
        accept();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void SettingsDialog::resetAllSettings() {
    m_cbRestoreSession->setChecked(c_defaultRestoreSession);
    m_cbAutoreload->setChecked(c_defaultAutoreloadImages);

    // Reset snapshot saving radio buttons
    if (c_defaultAutosaveSnapshots) {
        m_rbAutosave->setChecked(true);
    } else if (c_defaultSnapshotOnReopen) {
        m_rbSnapshotOnReopen->setChecked(true);
    } else {
        m_rbNone->setChecked(true);
    }
    m_rbAutosave->setEnabled(c_defaultAutoreloadImages);

    m_sbThumbCacheSize->setValue(c_defaultThumbnailCacheMB);
    m_sbDeltaCacheSize->setValue(c_defaultDeltaCacheMB);

    m_bgColor = QColor(c_defaultBackgroundColor);
    m_btnBackgroundColor->setStyleSheet(QString("background-color: %1;").arg(m_bgColor.name()));
}
