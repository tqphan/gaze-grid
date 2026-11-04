#pragma once
#include <QStringList>
namespace WordSegmentation {
bool supported(const QString &language);
QStringList segment(const QString &text,const QString &language);
QString prefix(const QString &text,const QString &language);
}
