#include <QColor>
#include "config/appsettings.h"

constexpr char c_applicationName[] = "Ottersnap";
constexpr char c_organizationName[] = "";
constexpr char c_organizationDomain[] = "kipwisp.com";

QSettings& AppSettings::settings() {
    static QSettings s;
    return s;
}

bool AppSettings::scaleWithWindow() {
    return settings().value(c_keyResizeToFit, true).toBool();
}

void AppSettings::setScaleWithWindow(bool value) {
    settings().setValue(c_keyResizeToFit, value);
}

bool AppSettings::restoreSession() {
    return settings().value(c_keyRestoreSession, true).toBool();
}

void AppSettings::setRestoreSession(bool value) {
    settings().setValue(c_keyRestoreSession, value);
}

QString AppSettings::fileFilter() {
    return QStringLiteral("Images (*.png *.jpg *.jpeg *.gif *.bmp *.tiff *.webp)");
}

int AppSettings::maxSnapshotCacheSizeMB() {
    return settings().value(c_keySnapshotCacheSizeMB, c_defaultSnapshotCacheMB).toInt();
}

void AppSettings::setMaxSnapshotCacheSizeMB(int value) {
    settings().setValue(c_keySnapshotCacheSizeMB, value);
}

int AppSettings::maxThumbnailCacheSizeMB() {
    return settings().value(c_keyThumbnailCacheSizeMB, c_defaultThumbnailCacheMB).toInt();
}

void AppSettings::setMaxThumbnailCacheSizeMB(int value) {
    settings().setValue(c_keyThumbnailCacheSizeMB, value);
}

int AppSettings::maxDeltaCacheSizeMB() {
    return settings().value(c_keyDeltaCacheSizeMB, c_defaultDeltaCacheMB).toInt();
}

void AppSettings::setMaxDeltaCacheSizeMB(int value) {
    settings().setValue(c_keyDeltaCacheSizeMB, value);
}

bool AppSettings::autosaveSnapshots() {
    return settings().value(c_keyAutosaveSnapshots, c_defaultAutosaveSnapshots).toBool();
}

void AppSettings::setAutosaveSnapshots(bool value) {
    settings().setValue(c_keyAutosaveSnapshots, value);
}

bool AppSettings::autoreloadImages() {
    return settings().value(c_keyAutoreloadImages, c_defaultAutoreloadImages).toBool();
}

void AppSettings::setAutoreloadImages(bool value) {
    settings().setValue(c_keyAutoreloadImages, value);
}

int AppSettings::baseInterval() {
    return settings().value(c_keyBaseInterval, c_defaultBaseInterval).toInt();
}

void AppSettings::setBaseInterval(int value) {
    settings().setValue(c_keyBaseInterval, value);
}

QColor AppSettings::backgroundColor() {
    return settings()
        .value(c_keyBackgroundColor, QString(c_defaultBackgroundColor))
        .value<QColor>();
}

void AppSettings::setBackgroundColor(const QColor& value) {
    settings().setValue(c_keyBackgroundColor, value);
}

void AppSettings::resetBackgroundColor() {
    settings().remove(c_keyBackgroundColor);
}

const char *AppSettings::applicationName() {
    return c_applicationName;
}

const char *AppSettings::organizationName() {
    return c_organizationName;
}

const char *AppSettings::organizationDomain() {
    return c_organizationDomain;
}
