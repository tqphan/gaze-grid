#pragma once
#include <QVariantMap>
#include <QStringList>
class KeyboardPack {
public:
    static QVariantMap load(const QString &path, QByteArray *source=nullptr);
    static bool cacheEnabled();
    static void setCacheEnabled(bool enabled);
    static void invalidate(const QString &path);
    static QVariantMap audit(const QString &directory);
};
