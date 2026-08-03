#include <QColor>
#include "controllers/appsettingscontroller.h"
#include "config/appsettings.h"

AppSettingsController::AppSettingsController(QObject *parent) : QObject(parent) {
}

bool AppSettingsController::shouldSaveSnapshotOnReopen() const {
    return !AppSettings::autosaveSnapshots() && AppSettings::snapshotOnReopen();
}

bool AppSettingsController::shouldAutoreloadImages() const {
    return AppSettings::autoreloadImages();
}

bool AppSettingsController::shouldAutosaveSnapshots() const {
    return AppSettings::autosaveSnapshots();
}

bool AppSettingsController::isAutosaveSnapshotsEnabled() const {
    return AppSettings::autoreloadImages();
}

void AppSettingsController::setAutosaveSnapshots(bool value) {
    AppSettings::setAutosaveSnapshots(value);
    if (value) {
        // If general autosave is ON, snapshot-on-reopen is redundant.
        AppSettings::setSnapshotOnReopen(false);
    }
}

void AppSettingsController::setSnapshotOnReopen(bool value) {
    AppSettings::setSnapshotOnReopen(value);
    if (value) {
        // If snapshot-on-reopen is specifically requested, we disable general autosave to ensure
        // the "on reopen" logic is the primary mechanism.
        AppSettings::setAutosaveSnapshots(false);
    }
}

void AppSettingsController::setAutoreloadImages(bool value) {
    AppSettings::setAutoreloadImages(value);
    if (!value) {
        // If autoreload is disabled, autosave snapshots (which triggers on reload) is also
        // disabled.
        AppSettings::setAutosaveSnapshots(false);
    }
}

void AppSettingsController::setRestoreSession(bool value) {
    AppSettings::setRestoreSession(value);
}

void AppSettingsController::setBackgroundColor(const QColor& color) {
    AppSettings::setBackgroundColor(color);
}

void AppSettingsController::setMaxThumbnailCacheSizeMB(int size) {
    AppSettings::setMaxThumbnailCacheSizeMB(size);
}

void AppSettingsController::setMaxDeltaCacheSizeMB(int size) {
    AppSettings::setMaxDeltaCacheSizeMB(size);
}

bool AppSettingsController::scaleWithWindow() const {
    return AppSettings::scaleWithWindow();
}

void AppSettingsController::setScaleWithWindow(bool value) {
    AppSettings::setScaleWithWindow(value);
}
