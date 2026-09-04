#include <QColor>
#include "config/appsettings.h"

constexpr char c_applicationName[] = "Ottersnap";
constexpr char c_applicationId[] = "ottersnap";
constexpr char c_repositoryUrl[] = "https://github.com/lutravox/ottersnap";
constexpr char c_organizationName[] = "ottersnap";
constexpr char c_organizationDomain[] = "io.github.lutravox";

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

bool AppSettings::toolbarVisible() {
    return settings().value(c_keyToolbarVisible, c_defaultToolbarVisible).toBool();
}

void AppSettings::setToolbarVisible(bool value) {
    settings().setValue(c_keyToolbarVisible, value);
}

bool AppSettings::restoreSession() {
    return settings().value(c_keyRestoreSession, false).toBool();
}

void AppSettings::setRestoreSession(bool value) {
    settings().setValue(c_keyRestoreSession, value);
}

QString AppSettings::openFilter() {
    return QStringLiteral("Images (*.png *.jpg *.jpeg *.gif *.bmp *.tif *.tiff *.webp)");
}

QString AppSettings::saveFilter() {
    return QStringLiteral("PNG (*.png);;JPEG (*.jpg *.jpeg);;BMP (*.bmp);;TIFF (*.tif *.tiff);;GIF (*.gif)");
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

bool AppSettings::snapshotOnReopen() {
    return settings().value(c_keySnapshotOnReopen, c_defaultSnapshotOnReopen).toBool();
}

void AppSettings::setSnapshotOnReopen(bool value) {
    settings().setValue(c_keySnapshotOnReopen, value);
}

bool AppSettings::forceCpuReconstruction() {
    QByteArray env = qgetenv("OTTERSNAP_FORCE_CPU_RECONSTRUCTION");
    if (!env.isEmpty()) {
        return env == "1" || env.toLower() == "true";
    }
    return settings().value(c_keyForceCpuReconstruction, c_defaultForceCpuReconstruction).toBool();
}

void AppSettings::setForceCpuReconstruction(bool value) {
    settings().setValue(c_keyForceCpuReconstruction, value);
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

const char *AppSettings::applicationId() {
    return c_applicationId;
}

const char *AppSettings::repositoryUrl() {
    return c_repositoryUrl;
}

const char *AppSettings::organizationName() {
    return c_organizationName;
}

const char *AppSettings::organizationDomain() {
    return c_organizationDomain;
}
