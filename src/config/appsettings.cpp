#include "config/appsettings.h"

constexpr char c_applicationName[] = "Ottersnap";
constexpr char c_organizationName[] = "";
constexpr char c_organizationDomain[] = "kipwisp.com";

QSettings& AppSettings::settings() {
    static QSettings s;
    return s;
}

bool AppSettings::resizeToFit() {
    return settings().value("General/resizeToFit", true).toBool();
}

void AppSettings::setResizeToFit(bool value) {
    settings().setValue("General/resizeToFit", value);
}

bool AppSettings::restoreSession() {
    return settings().value("General/restoreSession", true).toBool();
}

void AppSettings::setRestoreSession(bool value) {
    settings().setValue("General/restoreSession", value);
}

QString AppSettings::fileFilter() {
    return QStringLiteral("Images (*.png *.jpg *.jpeg *.gif *.bmp *.tiff *.webp)");
}

int AppSettings::maxSnapshotCacheSizeMB() {
    return settings().value("General/maxSnapshotCacheSizeMB", 256).toInt();
}

void AppSettings::setMaxSnapshotCacheSizeMB(int value) {
    settings().setValue("General/maxSnapshotCacheSizeMB", value);
}

bool AppSettings::autosaveSnapshots() {
    return settings().value("General/autosaveSnapshots", true).toBool();
}

void AppSettings::setAutosaveSnapshots(bool value) {
    settings().setValue("General/autosaveSnapshots", value);
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
