#pragma once
#include <QLocale>
#include <QString>

// Rules shared by prediction, completion acceptance, and personal learning.
namespace PredictionLanguage {
inline bool characterBased(const QString &language){
 return language=="cmn"||language=="jpn"||language=="tha";
}
inline bool wordCharacter(QChar ch){
 return ch.isLetter()||ch.isMark()||ch=='\''||ch==QChar(0x2019);
}
inline QString normalize(QString text){
 return text.normalized(QString::NormalizationForm_C).replace(QChar(0x2019),QChar(39));
}
inline QString lower(const QString &text,const QString &language){
 return language=="tur"?QLocale(QLocale::Turkish).toLower(text):text.toLower();
}
inline QString upper(const QString &text,const QString &language){
 return language=="tur"?QLocale(QLocale::Turkish).toUpper(text):text.toUpper();
}
inline QString rawPrefix(const QString &text,const QString &language){
 if(characterBased(language))return {};
 int start=text.size();while(start>0&&wordCharacter(text[start-1]))--start;
 const auto result=text.mid(start);
 if((start>0&&text[start-1].isLetterOrNumber())||result.isEmpty()||!result.front().isLetter())return {};
 return result;
}
inline bool appendSpace(const QString &language){return !characterBased(language);}
// Preserve the original prefix (including decomposed accents and apostrophes).
// Only the suffix is inserted, so acceptance never deletes user text.
inline QString completion(const QString &text,const QString &candidate,const QString &language,bool space){
 const auto prefix=normalize(rawPrefix(text,language));
 if(!lower(normalize(candidate),language).startsWith(lower(prefix,language)))return {};
 return candidate.mid(prefix.size())+(space&&appendSpace(language)?QString(" "):QString());
}
}
