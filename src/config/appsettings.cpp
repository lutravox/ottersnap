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

int AppSettings::maxVersionCacheSizeMB() {
    return settings().value("General/maxVersionCacheSizeMB", 256).toInt();
}

void AppSettings::setMaxVersionCacheSizeMB(int value) {
    settings().setValue("General/maxVersionCacheSizeMB", value);
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
