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
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <sys/sysinfo.h>
#include "ui/dialogs/shortcutsettingspage.h"

SettingsDialog::SettingsDialog(AppSettingsController *settings,
                               NotificationManager   *notificationManager,
                               QWidget               *parent)
    : QDialog(parent), m_settings(settings), m_notificationManager(notificationManager) {
    setWindowTitle(tr("Settings"));
    setMinimumWidth(600);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 10, 0, 10);

    m_tabs = new QTabWidget(this);
    m_tabs->setDocumentMode(true);
    m_tabs->setTabsClosable(false);
    m_tabs->setStyleSheet("QTabBar { padding: 0px; }");

    // Helper to create a centered page structure that fills the width
    auto createPage = [this]() {
        auto *page = new QWidget();
        auto *pageLayout = new QVBoxLayout(page);
        pageLayout->setContentsMargins(10, 10, 10, 10);

        auto *scrollArea = new QScrollArea(page);
        scrollArea->setWidgetResizable(true);
        scrollArea->setFrameShape(QFrame::NoFrame);
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        auto *scrollContent = new QWidget();
        auto *scrollContentLayout = new QVBoxLayout(scrollContent);
        scrollContentLayout->setContentsMargins(20, 20, 20, 20);
        scrollContentLayout->setSpacing(20);
        scrollContentLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);

        scrollArea->setWidget(scrollContent);
        pageLayout->addWidget(scrollArea);

        return std::make_pair(page, scrollContentLayout);
    };

    // Helper to create a group box with a form layout
    auto createGroup = [](QVBoxLayout *parentLayout, const QString& title) {
        auto *group = new QGroupBox(title);
        group->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        auto *layout = new QFormLayout(group);
        layout->setSpacing(15);
        layout->setLabelAlignment(Qt::AlignLeft);
        layout->setContentsMargins(15, 15, 15, 15);
        parentLayout->addWidget(group);
        return layout;
    };

    // --- General Tab ---
    auto [generalPage, generalLayout] = createPage();
    auto *sessionGroup = createGroup(generalLayout, tr("Session"));
    m_cbRestoreSession = new QCheckBox(this);
    m_cbRestoreSession->setChecked(AppSettings::restoreSession());
    m_cbRestoreSession->setToolTip(tr("Reopen all images from the previous session on startup."));
    sessionGroup->addRow(tr("Restore session"), m_cbRestoreSession);
    m_tabs->addTab(generalPage, tr("General"));

    // --- Images Tab ---
    auto [imagesPage, imagesLayout] = createPage();
    auto *autoGroup = createGroup(imagesLayout, tr("Automatic Actions"));
    m_cbAutoreload = new QCheckBox(this);
    m_cbAutoreload->setChecked(m_settings->shouldAutoreloadImages());
    m_cbAutoreload->setToolTip(
        tr("Automatically reload the image if the file is modified on disk."));
    autoGroup->addRow(tr("Auto-reload images"), m_cbAutoreload);

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

    if (m_settings->shouldAutosaveSnapshots()) {
        m_rbAutosave->setChecked(true);
    } else if (AppSettings::snapshotOnReopen()) {
        m_rbSnapshotOnReopen->setChecked(true);
    } else {
        m_rbNone->setChecked(true);
    }

    autoGroup->addRow(tr("Autosave snapshot:"), m_rbNone);
    autoGroup->addRow("", m_rbAutosave);
    autoGroup->addRow("", m_rbSnapshotOnReopen);
    m_tabs->addTab(imagesPage, tr("Images"));

    // --- Performance Tab ---
    auto [perfPage, perfLayout] = createPage();
    auto          *cacheGroup = createGroup(perfLayout, tr("Cache Memory"));
    struct sysinfo si;
    sysinfo(&si);
    int maxMB = static_cast<int>(si.totalram / (1024 * 1024));

    m_sbThumbCacheSize = new QSpinBox(this);
    m_sbThumbCacheSize->setRange(c_minCacheSizeMB, maxMB);
    m_sbThumbCacheSize->setValue(AppSettings::maxThumbnailCacheSizeMB());
    m_sbThumbCacheSize->setToolTip(
        tr("Maximum memory allocated for thumbnail caching. Larger values can speed up browsing."));
    cacheGroup->addRow(tr("Thumbnail cache size (MB):"), m_sbThumbCacheSize);

    m_sbDeltaCacheSize = new QSpinBox(this);
    m_sbDeltaCacheSize->setRange(c_minCacheSizeMB, maxMB);
    m_sbDeltaCacheSize->setValue(AppSettings::maxDeltaCacheSizeMB());
    m_sbDeltaCacheSize->setToolTip(
        tr("Maximum memory allocated for storing snapshot deltas. Larger "
           "values improve snapshot switching speed."));
    cacheGroup->addRow(tr("Snapshot cache size (MB):"), m_sbDeltaCacheSize);
    m_tabs->addTab(perfPage, tr("Performance"));

    // --- Appearance Tab ---
    auto [appPage, appLayout] = createPage();
    auto *styleGroup = createGroup(appLayout, tr("Viewer Style"));
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
    bgHLayout->addStretch();
    bgHLayout->addWidget(btnResetBg);

    styleGroup->addRow(tr("Viewer background color:"), bgWidget);
    m_tabs->addTab(appPage, tr("Appearance"));

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

    auto *shortcutsPage = new ShortcutSettingsPage(m_settings->shortcutManager(), this);
    m_tabs->addTab(shortcutsPage, tr("Shortcuts"));

    mainLayout->addWidget(m_tabs);

    // Button box (OK / Cancel)
    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    auto *bottomLayout = new QHBoxLayout();
    bottomLayout->setContentsMargins(10, 0, 10, 0);
    auto *btnResetAll = new QPushButton(tr("Reset All"), this);
    btnResetAll->setToolTip(tr("Reset all settings to their default values."));

    bottomLayout->addWidget(btnResetAll);
    bottomLayout->addStretch();
    bottomLayout->addWidget(buttonBox);

    mainLayout->addLayout(bottomLayout);

    connect(btnResetAll, &QPushButton::clicked, this, &SettingsDialog::resetAllSettings);

    connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        m_settings->setRestoreSession(m_cbRestoreSession->isChecked());
        m_settings->setAutosaveSnapshots(m_rbAutosave->isChecked());
        m_settings->setAutoreloadImages(m_cbAutoreload->isChecked());
        m_settings->setSnapshotOnReopen(m_rbSnapshotOnReopen->isChecked());
        m_settings->setBackgroundColor(m_bgColor);
        m_settings->setMaxThumbnailCacheSizeMB(m_sbThumbCacheSize->value());
        ThumbnailCache::updateMaxCost(m_sbThumbCacheSize->value());
        m_settings->setMaxDeltaCacheSizeMB(m_sbDeltaCacheSize->value());
        DeltaCache::updateMaxCost(m_sbDeltaCacheSize->value());
        
        // Commit pending shortcuts
        for (int i = 0; i < m_tabs->count(); ++i) {
            auto *sp = qobject_cast<ShortcutSettingsPage *>(m_tabs->widget(i));
            if (sp)
                sp->commitChanges();
        }
        
        m_settings->shortcutManager()->save();
        
        if (m_notificationManager) {
            m_notificationManager->notify(tr("Settings updated successfully."));
        }
        
        accept();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, [this]() {
        for (int i = 0; i < m_tabs->count(); ++i) {
            auto *sp = qobject_cast<ShortcutSettingsPage *>(m_tabs->widget(i));
            if (sp)
                sp->resetPending();
        }
        reject();
    });
}

void SettingsDialog::resetAllSettings() {
    int currentIndex = m_tabs->currentIndex();

    switch (currentIndex) {
        case 0: // General
            m_cbRestoreSession->setChecked(c_defaultRestoreSession);
            break;

        case 1: // Images
            m_cbAutoreload->setChecked(c_defaultAutoreloadImages);
            if (c_defaultAutosaveSnapshots) {
                m_rbAutosave->setChecked(true);
            } else if (c_defaultSnapshotOnReopen) {
                m_rbSnapshotOnReopen->setChecked(true);
            } else {
                m_rbNone->setChecked(true);
            }
            m_rbAutosave->setEnabled(c_defaultAutoreloadImages);
            break;

        case 2: // Performance
            m_sbThumbCacheSize->setValue(c_defaultThumbnailCacheMB);
            m_sbDeltaCacheSize->setValue(c_defaultDeltaCacheMB);
            break;

        case 3: // Appearance
            m_bgColor = QColor(c_defaultBackgroundColor);
            m_btnBackgroundColor->setStyleSheet(QString("background-color: %1;").arg(m_bgColor.name()));
            break;

        case 4: { // Shortcuts
            auto *sp = qobject_cast<ShortcutSettingsPage *>(m_tabs->widget(currentIndex));
            if (sp) {
                sp->resetToDefaults();
            }
            break;
        }

        default:
            break;
    }
}
