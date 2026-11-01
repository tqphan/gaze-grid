#include "WordPrediction.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QElapsedTimer>
#include <QTextStream>
#include <algorithm>

int main(int argc,char **argv){
 QCoreApplication app(argc,argv);
 if(app.arguments().value(1)=="--language"){
  const auto language=app.arguments().value(2);
  if(!WordPrediction::languages().contains(language)&&language!="eng-tatoeba")return 2;
  QElapsedTimer clock;clock.start();const auto vocabulary=WordPrediction::dictionary(language);
  WordPrediction::prepare(vocabulary,language);const double startup=clock.nsecsElapsed()/1e6;
  const bool characters=WordPrediction::characterLanguage(language);
  auto predict=[&](const QString &text){return WordPrediction::suggest(text,vocabulary,{},3,true,true,"never",language);};
  if(app.arguments().value(3)=="--probe"){
   for(const auto &text:app.arguments().mid(4))QTextStream(stdout)<<text<<" -> "<<predict(text).join(", ")<<'\n';return 0;
  }
  QFile input(app.arguments().value(3));if(!input.open(QIODevice::ReadOnly))return 2;
  int sentences=0,next=0,nextHits=0,completion=0,completionHits=0,covered=0,total=0;QList<double> latency;
  while(!input.atEnd()&&sentences<200){
   const auto row=QJsonDocument::fromJson(input.readLine()).object();const auto words=row["words"].toArray();++sentences;
   QString context;
   for(const auto &item:words){
    const auto word=item.toString();++total;covered+=vocabulary.contains(word);
    if(!context.isEmpty()){
     ++next;clock.restart();auto result=predict(context);latency<<clock.nsecsElapsed()/1e6;
     for(auto &entry:result)entry=entry.toLower();nextHits+=result.contains(word);
    }
    if(!characters&&word.size()>2){
     ++completion;auto result=predict(context+word.left(2));for(auto &entry:result)entry=entry.toLower();completionHits+=result.contains(word);
    }
    context+=word+(characters?QString():QString(" "));
   }
  }
  if(next==0||latency.isEmpty())return 2;
  std::sort(latency.begin(),latency.end());
  const QJsonObject report{{"language",language},{"tokenization",characters?"character":"word"},{"sentences",sentences},
   {"tokens",total},{"vocabulary_coverage_percent",100.0*covered/qMax(1,total)},
   {"next_token_events",next},{"next_token_top3_percent",100.0*nextHits/next},
   {"completion_events",completion},{"completion_top3_percent",completion?QJsonValue(100.0*completionHits/completion):QJsonValue()},
   {"warm_p95_ms",latency[int(latency.size()*.95)]},{"cold_initialization_ms",startup},{"learning",false}};
  QTextStream(stdout)<<QJsonDocument(report).toJson();return 0;
 }
 QFile dict(":/resources/EnglishUS.dic");if(!dict.open(QIODevice::ReadOnly))return 2;
 auto words=QString::fromUtf8(dict.readAll()).remove(QChar(0xfeff)).toLower().split('\n',Qt::SkipEmptyParts);
 for(auto &word:words)word=word.trimmed();
 words.removeIf([](const QString &word){return word.isEmpty()||!std::all_of(word.begin(),word.end(),[](QChar ch){return (ch>='a'&&ch<='z')||ch=='\'';});});
 words.removeDuplicates();std::sort(words.begin(),words.end());
 auto old=[&](QString text){
  const auto prefix=WordPrediction::prefix(text).toLower();QStringList matches;if(prefix.isEmpty())return matches;
  auto start=std::lower_bound(words.cbegin(),words.cend(),prefix);
  for(auto it=start;it!=words.cend()&&it->startsWith(prefix);++it)if(it->size()>prefix.size())matches<<*it;
  std::sort(matches.begin(),matches.end(),[](const QString &a,const QString &b){return a.size()!=b.size()?a.size()<b.size():a<b;});
  return matches.mid(0,3);
 };
 QElapsedTimer timer;timer.start();WordPrediction::suggest("he",words,{});const double startup=timer.nsecsElapsed()/1e6;
 if(app.arguments().value(1)=="--probe"){
  for(const auto &input:app.arguments().mid(2))QTextStream(stdout)<<input<<" -> "<<WordPrediction::suggest(input,words,{},6).join(", ")<<'\n';return 0;
 }
 QFile input(app.arguments().value(1));if(!input.open(QIODevice::ReadOnly))return 2;
 int sentences=0,events=0,newHits=0,oldHits=0,nextEvents=0,nextHits=0,totalKeys=0,newSaved=0,oldSaved=0;
 QList<double> latency;
 auto predict=[&](const QString &text){timer.restart();auto result=WordPrediction::suggest(text,words,{},3);latency<<timer.nsecsElapsed()/1e6;for(auto &s:result)s=s.toLower();return result;};
 while(!input.atEnd()&&sentences<500){
  const auto row=QJsonDocument::fromJson(input.readLine()).object();const auto tokens=row["words"].toArray();++sentences;
  QString context;
  for(auto item:tokens){
   const auto word=item.toString();
   if(word.size()>=3){++events;newHits+=predict(context+word.left(2)).contains(word);oldHits+=old(context+word.left(2)).contains(word);}
   if(!context.isEmpty()){++nextEvents;nextHits+=predict(context).contains(word);}
   // Accepting one of the first three suggestions costs one activation and
   // inserts its trailing space. Search the first prefix where it is available.
   const int full=word.size()+1;totalKeys+=full;
   bool newFound=false,oldFound=false;
   for(int n=0;n<word.size()&&(!newFound||!oldFound);++n){
    const auto text=context+word.left(n);
    if(!newFound&&predict(text).contains(word)){newSaved+=full-n-1;newFound=true;}
    if(!oldFound&&old(text).contains(word)){oldSaved+=full-n-1;oldFound=true;}
   }
   context+=word+' ';
  }
 }
 if(events==0||nextEvents==0||totalKeys==0||latency.isEmpty())return 2;
 std::sort(latency.begin(),latency.end());
 const QJsonObject report{{"sentences",sentences},{"completion_events",events},{"next_word_events",nextEvents},
 {"old_top3_two_letters_percent",100.0*oldHits/events},{"new_top3_two_letters_percent",100.0*newHits/events},
 {"next_word_top3_percent",100.0*nextHits/nextEvents},{"old_keystrokes_saved_percent",100.0*oldSaved/totalKeys},
 {"new_keystrokes_saved_percent",100.0*newSaved/totalKeys},{"warm_p95_ms",latency[int(latency.size()*.95)]},
 {"cold_initialization_ms",startup},{"learning",false}};
 QTextStream(stdout)<<QJsonDocument(report).toJson();
 return newHits>oldHits&&newSaved>oldSaved?0:1;
}
