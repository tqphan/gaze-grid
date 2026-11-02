#pragma once
#include <QLocale>
#include <QString>
#include <QStringList>

// Rules shared by prediction, completion acceptance, and personal learning.
namespace PredictionLanguage {
inline bool characterBased(const QString &language){
 return language=="cmn"||language=="jpn"||language=="tha";
}
inline bool wordCharacter(QChar ch){
 return ch.isLetter()||ch.isMark()||ch=='\''||ch==QChar(0x2019);
}
inline bool wordCharacter(char32_t ch){
 return QChar::isLetter(ch)||QChar::isMark(ch)||ch==U'\''||ch==0x2019;
}
inline bool validWord(const QString &word){
 const auto characters=word.toStdU32String();
 if(characters.empty()||!QChar::isLetter(characters.front()))return false;
 for(auto ch:characters)if(!wordCharacter(ch))return false;
 return true;
}
inline bool endsWithWordCharacter(const QString &text){
 const auto characters=text.toStdU32String();
 return !characters.empty()&&wordCharacter(characters.back());
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
 const auto characters=text.toStdU32String();
 int start=characters.size();while(start>0&&wordCharacter(characters[start-1]))--start;
 if(start==characters.size()||!QChar::isLetter(characters[start])
    ||(start>0&&QChar::isLetterOrNumber(characters[start-1])))return {};
 const auto result=QString::fromUcs4(characters.data()+start,characters.size()-start);
 return result;
}
// Match the trainer's letter/mark tokens; character models attach marks to
// their preceding Unicode letter, including letters outside the BMP.
inline QStringList tokens(const QString &text,const QString &language){
 QStringList words;QString word;
 auto flush=[&]{
  while(word.endsWith('\''))word.chop(1);
  if(!word.isEmpty())words<<word;
  word.clear();
 };
 const auto normalized=lower(normalize(text),language);
 for(auto ch:normalized.toStdU32String()){
  if(QChar::isLetter(ch)||QChar::isMark(ch)||(ch==U'\''&&!word.isEmpty()))word+=QString::fromUcs4(&ch,1);
  else flush();
 }
 flush();
 if(!characterBased(language))return words;
 QStringList clusters;
 for(const auto &entry:words){
  for(auto ch:entry.toStdU32String()){
   if(QChar::isLetter(ch))clusters<<QString::fromUcs4(&ch,1);
   else if(QChar::isMark(ch)&&!clusters.isEmpty())clusters.last()+=QString::fromUcs4(&ch,1);
  }
 }
 return clusters;
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
