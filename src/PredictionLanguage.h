#pragma once
#include <QLocale>
#include <QString>
#include <QStringList>
#include "WordSegmentation.h"

// Rules shared by prediction, completion acceptance, and personal learning.
namespace PredictionLanguage {
inline QString fromLocale(const QString &locale){
 const QStringList prefixes{"en","es","fr","de","pt","it","nl","ru","uk","pl","tr","vi","id","ar","zh","ja","ko","th"};
 const QStringList codes{"eng","spa","fra","deu","por","ita","nld","rus","ukr","pol","tur","vie","ind","ara","cmn","jpn","kor","tha"};
 const int index=prefixes.indexOf(locale.section('_',0,0).section('-',0,0).toLower());
 return index<0?QString():codes[index];
}
inline bool characterBased(const QString &language){
 return language=="jpn";
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
 if(WordSegmentation::supported(language)){
  const auto prefix=WordSegmentation::prefix(normalize(result),language);
  return result.right(prefix.size());
 }
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
 if(WordSegmentation::supported(language)){
  QStringList segmented;for(const auto &entry:words)segmented+=WordSegmentation::segment(entry,language);return segmented;
 }
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
inline bool appendSpace(const QString &language){return language!="cmn"&&language!="jpn"&&language!="tha";}
inline QString punctuationPrefix(const QString &text,const QString &language){return language=="fra"&&text==":"?QString(" "):QString();}
inline bool spaceAfterPunctuation(const QString &text,const QString &language){
 return appendSpace(language)&&text!="'"&&text!=QString(QChar(0x2019))&&text!="-";
}
inline QString caseReplacement(const QString &text,const QString &candidate,const QString &language){
 const auto raw=rawPrefix(text,language),prefix=normalize(raw);
 if(language=="deu"&&!prefix.isEmpty()&&candidate.left(prefix.size())!=prefix
    &&lower(candidate.left(prefix.size()),language)==lower(prefix,language))return raw;
 return {};
}
// Preserve the original prefix (including decomposed accents and apostrophes).
// Only the suffix is inserted, so acceptance never deletes user text.
inline QString completion(const QString &text,const QString &candidate,const QString &language,bool space){
 const auto prefix=normalize(rawPrefix(text,language));
 if(!lower(normalize(candidate),language).startsWith(lower(prefix,language)))return {};
 return candidate.mid(prefix.size())+(space&&appendSpace(language)?QString(" "):QString());
}
struct CompletionEdit {
 int removeBefore=0,removeAfter=0;
 QString text;
 bool valid=false;
};
inline CompletionEdit edit(const QString &before,const QString &after,const QString &candidate,const QString &language,bool space){
 CompletionEdit result;
 const auto prefix=rawPrefix(before,language);
 if(!lower(normalize(candidate),language).startsWith(lower(normalize(prefix),language)))return result;
 result.removeBefore=caseReplacement(before,candidate,language).size();
 if(!prefix.isEmpty()&&appendSpace(language))for(auto ch:after.toStdU32String()){
  if(!wordCharacter(ch))break;result.removeAfter+=ch>0xffff?2:1;
 }
 if(!prefix.isEmpty()&&WordSegmentation::supported(language)){
  const auto runBefore=rawPrefix(before,"eng");QString runAfter;
  for(auto ch:after.toStdU32String()){
   if(!wordCharacter(ch))break;runAfter+=QString::fromUcs4(&ch,1);
  }
  int offset=0;
  for(const auto &token:WordSegmentation::segment(runBefore+runAfter,language)){
   const int end=offset+token.size();
   if(offset<runBefore.size()&&runBefore.size()<end){result.removeAfter=end-runBefore.size();break;}
   offset=end;
  }
 }
 const auto following=after.mid(result.removeAfter);
 const bool beforeColon=language=="fra"&&following.startsWith(':');
 const bool addSpace=space&&appendSpace(language)&&(following.isEmpty()||beforeColon||(!following.front().isSpace()&&!following.front().isPunct()));
 result.text=result.removeBefore?candidate+(addSpace?QString(" "):QString()):completion(before,candidate,language,addSpace);
 result.valid=true;return result;
}
}
