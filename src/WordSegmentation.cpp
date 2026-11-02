#include "WordSegmentation.h"
#include <QFile>
#include <QHash>
#include <QChar>
#include <memory>
#include <vector>
#include <algorithm>
namespace {
bool target(char32_t ch,const QString &language){
 return language=="tha"?(ch>=0x0e00&&ch<=0x0e7f):
  ((ch>=0x3400&&ch<=0x9fff)||(ch>=0xf900&&ch<=0xfaff)||(ch>=0x20000&&ch<=0x323af));
}
struct Lexicon {
 QHash<QString,int> costs;QStringList sorted;int maximum=0;
 explicit Lexicon(const QString &language){
  QFile file(":/resources/prediction/"+language+"-lexicon-v1.bin");
  if(!file.open(QIODevice::ReadOnly))qFatal("Segmentation lexicon missing");
  const auto rows=qUncompress(file.readAll()).split('\n');
  if(rows.isEmpty()||rows.first()!="gaze-grid-lexicon-v1")qFatal("Invalid segmentation lexicon");
  for(const auto &row:rows){
   const auto fields=row.split('\t');if(fields.size()!=2)continue;
   const auto word=QString::fromUtf8(fields[0]);costs.insert(word,fields[1].toInt());
   maximum=qMax(maximum,int(word.toStdU32String().size()));
  }
  sorted=costs.keys();sorted.sort();
 }
};
const Lexicon &lexicon(const QString &language){
 static thread_local QString loaded;static thread_local std::unique_ptr<Lexicon> value;
 if(!value||loaded!=language){value=std::make_unique<Lexicon>(language);loaded=language;}
 return *value;
}
QStringList run(const std::u32string &text,const Lexicon &dictionary){
 const int size=int(text.size());std::vector<qint64> scores(size+1,0);std::vector<int> ends(size,0);
 for(int start=size-1;start>=0;--start){
  int end=start+1;while(end<size&&QChar::isMark(text[end]))++end;
  scores[start]=30000+scores[end];ends[start]=end;
  for(int stop=start+1;stop<=qMin(size,start+dictionary.maximum);++stop){
   if(stop<size&&QChar::isMark(text[stop]))continue;
   const auto word=QString::fromUcs4(text.data()+start,stop-start);
   const auto found=dictionary.costs.constFind(word);if(found==dictionary.costs.cend())continue;
   const auto score=found.value()+scores[stop];
   if(score<scores[start]||(score==scores[start]&&stop>ends[start])){scores[start]=score;ends[start]=stop;}
  }
 }
 QStringList result;
 for(int start=0;start<size;start=ends[start])result<<QString::fromUcs4(text.data()+start,ends[start]-start);
 return result;
}
}
bool WordSegmentation::supported(const QString &language){return language=="cmn"||language=="tha";}
QString WordSegmentation::prefix(const QString &text,const QString &language){
 const auto &dictionary=lexicon(language);const auto characters=text.toStdU32String();
 for(int start=qMax(0,int(characters.size())-dictionary.maximum);start<int(characters.size());++start){
  if(QChar::isMark(characters[start]))continue;
  const auto suffix=QString::fromUcs4(characters.data()+start,characters.size()-start);
  const auto found=std::lower_bound(dictionary.sorted.cbegin(),dictionary.sorted.cend(),suffix);
  if(found!=dictionary.sorted.cend()&&found->startsWith(suffix))return suffix;
 }
 const auto parts=segment(text,language);return parts.isEmpty()?QString():parts.last();
}
QStringList WordSegmentation::segment(const QString &text,const QString &language){
 if(!supported(language))return {text};
 const auto &dictionary=lexicon(language);const auto characters=text.toStdU32String();QStringList result;
 for(size_t start=0;start<characters.size();){
  const bool active=target(characters[start],language);auto end=start+1;
  while(end<characters.size()&&(target(characters[end],language)==active||QChar::isMark(characters[end])))++end;
  const auto part=characters.substr(start,end-start);
  if(active)result+=run(part,dictionary);else result<<QString::fromUcs4(part.data(),part.size());
  start=end;
 }
 return result;
}
