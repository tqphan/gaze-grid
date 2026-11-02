#pragma once
#include <QStringList>
#include <QMap>
namespace WordPrediction {
void prepare(const QStringList &dictionary,const QString &language="eng");
QStringList dictionary(const QString &language);
QStringList languages();
bool characterLanguage(const QString &language);
QString prefix(const QString &text,const QString &language="eng");
QString completedWord(const QString &text,const QString &language="eng");
QStringList suggest(const QString &text,const QStringList &dictionary,const QMap<QString,int> &frequencies,int limit=8,bool useContext=true,bool suggestWhenEmpty=false,const QString &typedWordMode="fallback",const QString &language="eng");
}
