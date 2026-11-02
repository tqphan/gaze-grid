#include "WordPrediction.h"
#include "PredictionLanguage.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include <memory>
#include <QBuffer>
#include <QFile>
#include <QHash>
#include <QSet>
#include <QRegularExpression>
bool WordPrediction::characterLanguage(const QString &language){return PredictionLanguage::characterBased(language);}
QString WordPrediction::prefix(const QString &text,const QString &language){
 return PredictionLanguage::normalize(PredictionLanguage::rawPrefix(text,language));
}
QString WordPrediction::completedWord(const QString &text,const QString &language){
 const auto characters=PredictionLanguage::normalize(text).toStdU32String();
 int end=characters.size();
 while(end>0&&!QChar::isLetterOrNumber(characters[end-1])&&!QChar::isMark(characters[end-1])&&characters[end-1]!=U'\''){
  if(QChar::isSpace(characters[end-1]))return {};
  --end;
 }
 auto word=prefix(QString::fromUcs4(characters.data(),end),language);
 while(word.endsWith('\''))word.chop(1);
 return word.size()<64?PredictionLanguage::lower(word,language):QString();
}
namespace {
struct Distribution {
 struct Count {QString word;double value;};
 // Most contexts have only one or two continuations. A hash table for each
 // wastes hundreds of megabytes on minimum-size allocations.
 std::vector<Count> counts;
 double total=0;
 void prepare(){
  std::stable_sort(counts.begin(),counts.end(),[](const Count &a,const Count &b){return a.word<b.word;});
  // Preserve QHash::insert's last-value behavior if a model repeats a word.
  size_t output=0;
  for(size_t first=0;first<counts.size();){
   size_t last=first+1;while(last<counts.size()&&counts[first].word==counts[last].word)++last;
   if(output!=last-1)counts[output]=std::move(counts[last-1]);
   ++output;first=last;
  }
  counts.resize(output);counts.shrink_to_fit();
 }
 double value(const QString &word)const{
  const auto it=std::lower_bound(counts.cbegin(),counts.cend(),word,[](const Count &entry,const QString &key){return entry.word<key;});
  return it!=counts.cend()&&it->word==word?it->value:0;
 }
};
struct Model {
 QHash<QString,double> prior;
 QHash<QString,Distribution> contexts;
 Model(const QString &language){
  QFile file(":/resources/prediction/"+language+"-v1.bin");
  if(!file.open(QIODevice::ReadOnly))qFatal("Bundled prediction model is missing");
  QBuffer input;input.setData(qUncompress(file.readAll()));input.open(QIODevice::ReadOnly);
  // Parse one line at a time instead of retaining hundreds of thousands of
  // temporary byte arrays alongside the finished model during startup.
  auto readLine=[&]{auto line=input.readLine();if(line.endsWith('\n'))line.chop(1);return line;};
  if(readLine()!="gaze-grid-prediction-v1")qFatal("Invalid prediction model");
  while(!input.atEnd()){
   const auto fields=readLine().split('\t');
   if(fields.size()==3&&fields[0]=="u")prior.insert(QString::fromUtf8(fields[1]),fields[2].toDouble());
   else if((fields.size()==4&&fields[0]=="b")||(fields.size()==5&&fields[0]=="t")||(fields.size()==6&&fields[0]=="q")){
    QString context=QString::fromUtf8(fields[1]);
    for(int i=2;i<fields.size()-2;++i)context+=' '+QString::fromUtf8(fields[i]);
    auto &distribution=contexts[context];
    const double count=fields.back().toDouble();
    distribution.counts.push_back({QString::fromUtf8(fields[fields.size()-2]),count});
    distribution.total+=count;
   }
  }
  for(auto &distribution:contexts)distribution.prepare();
 }
};
const Model &model(const QString &language){
 static QString loaded;static std::unique_ptr<Model> value;
 if(!value||loaded!=language){value=std::make_unique<Model>(language);loaded=language;}
 return *value;
}
struct Candidate {QString word;double prior;};
struct Index {
 QList<Candidate> sorted,frequent;
 QHash<QString,double> byWord;
};
const Index &vocabulary(const QStringList &dictionary,const QString &language){
 static thread_local QStringList previous;
 static thread_local QString previousLanguage;
 static thread_local Index index;
 if(previous!=dictionary||previousLanguage!=language||index.sorted.isEmpty()){
  previous=dictionary;previousLanguage=language;index={};
  for(auto word:dictionary){
   word=PredictionLanguage::lower(PredictionLanguage::normalize(word),language);
   if(!PredictionLanguage::validWord(word)||index.byWord.contains(word))continue;
   const double prior=model(language).prior.value(word,1e-10);
   index.byWord.insert(word,prior);index.sorted.append({word,prior});
  }
  std::sort(index.sorted.begin(),index.sorted.end(),[](const Candidate &a,const Candidate &b){return a.word<b.word;});
  index.frequent=index.sorted;
  std::sort(index.frequent.begin(),index.frequent.end(),[](const Candidate &a,const Candidate &b){return a.prior!=b.prior?a.prior>b.prior:a.word<b.word;});
 }
 return index;
}
}
QStringList WordPrediction::languages(){return {"eng","spa","fra","deu","por","ita","nld","rus","ukr","pol","tur","vie","ind","ara","cmn","jpn","kor","tha"};}
QStringList WordPrediction::dictionary(const QString &language){
 const auto &prior=model(language).prior;QStringList result=prior.keys();result.sort();return result;
}
void WordPrediction::prepare(const QStringList &dictionary,const QString &language){vocabulary(dictionary,language);}
QStringList WordPrediction::suggest(const QString &text,const QStringList &dictionary,const QMap<QString,int> &frequencies,int limit,bool useContext,bool suggestWhenEmpty,const QString &typedWordMode,const QString &languageCode){
 if(limit<=0||(text.isEmpty()&&!suggestWhenEmpty))return {};
 const auto normalized=PredictionLanguage::normalize(text);
 const bool characters=characterLanguage(languageCode);
 const auto current=prefix(normalized,languageCode);
 // Never offer an insertion inside numbers/unsupported alphabets or directly
 // after punctuation (accepting a prediction appends text, not a separator).
 if(!text.isEmpty()&&current.isEmpty()&&!text.back().isSpace()&&!(characters&&PredictionLanguage::endsWithWordCharacter(text)))return {};
 const auto &language=model(languageCode);
 const auto &index=vocabulary(dictionary,languageCode);
 const auto &words=index.sorted;
 const auto lower=PredictionLanguage::lower(current,languageCode);
 auto context=PredictionLanguage::lower(normalized.left(normalized.size()-current.size()),languageCode);
 const int boundary=context.lastIndexOf(QRegularExpression("[.!?;:\\n\u3002\uff01\uff1f\u061b\u061f]"));
 context=context.mid(boundary+1);
 QStringList history{"<s>","<s>","<s>"};
 history+=PredictionLanguage::tokens(context,languageCode);
 const QString previous=history.last(),two=history[history.size()-2]+' '+previous;
 const auto bi=useContext?language.contexts.constFind(previous):language.contexts.cend();
 const auto tri=useContext?language.contexts.constFind(two):language.contexts.cend();
 const auto four=useContext?language.contexts.constFind(history[history.size()-3]+' '+two):language.contexts.cend();
 const bool sentenceStart=history.size()==3;
 struct Result {QString word;double score;};
 QList<Result> ranked,phraseSeeds;

 auto add=[&](const QString &word,double prior){
  if(!word.startsWith(lower))return;
  double probability=prior;
  if(bi!=language.contexts.cend())probability=(bi->value(word)+200*probability)/(bi->total+200);
  if(tri!=language.contexts.cend())probability=(tri->value(word)+20*probability)/(tri->total+20);
  if(four!=language.contexts.cend())probability=(four->value(word)+10*probability)/(four->total+10);
  // Bounded personalization: even a million observations cannot overpower
  // strong context by more than a factor of two.
  const double boost=1+std::min(1.0,std::log1p(std::max(0,frequencies.value(word)))/std::log(101.0));
  const Result result{word,probability*boost};
  if(word!=lower||lower.isEmpty())ranked.append(result);
  if(languageCode=="vie"&&useContext)phraseSeeds.append(result);
 };
 if(lower.isEmpty()){
  // All words absent from these contexts share the same backoff multiplier.
  // The most frequent limit words plus context/learned words therefore contain
  // the exact top limit, without scanning the dictionary after every space.
  QSet<QString> candidates;
  for(int i=0;i<std::min(std::max(limit,128),int(index.frequent.size()));++i)candidates.insert(index.frequent[i].word);
  for(auto distribution:{bi,tri,four})if(distribution!=language.contexts.cend())
   for(const auto &entry:distribution->counts)if(index.byWord.contains(entry.word))candidates.insert(entry.word);
  for(auto it=frequencies.cbegin();it!=frequencies.cend();++it)if(index.byWord.contains(it.key()))candidates.insert(it.key());
  for(const auto &word:candidates)add(word,index.byWord.value(word));
 }else{
  const auto begin=std::lower_bound(words.cbegin(),words.cend(),lower,[](const Candidate &a,const QString &b){return a.word<b;});
  for(auto it=begin;it!=words.cend()&&it->word.startsWith(lower);++it)add(it->word,it->prior);
 }
 // Names and other personal vocabulary need not already be in the dictionary.
 for(auto it=frequencies.cbegin();it!=frequencies.cend();++it){
  const auto word=PredictionLanguage::lower(PredictionLanguage::normalize(it.key()),languageCode);
  if(it.value()>0&&PredictionLanguage::validWord(word)&&!index.byWord.contains(word))add(word,1e-7);
 }
 // Extend only the best syllables with a strongly observed continuation.
 // Singles remain available; the joint score keeps each phrase below its seed.
 // Bound expansion independently of dictionary size and requested page size.
 if(!phraseSeeds.isEmpty()){
  const auto order=[](const Result &a,const Result &b){return a.score!=b.score?a.score>b.score:a.word<b.word;};
  const int seeds=std::min(32,int(phraseSeeds.size()));
  std::partial_sort(phraseSeeds.begin(),phraseSeeds.begin()+seeds,phraseSeeds.end(),order);
  for(int i=0;i<seeds;++i){
   const auto &seed=phraseSeeds[i];
   const auto continuation=language.contexts.constFind(seed.word);
   if(continuation==language.contexts.cend()||continuation->total<=0)continue;
   const Distribution::Count *best=nullptr;
   for(const auto &entry:continuation->counts){
    if(entry.value<3||!index.byWord.contains(entry.word))continue;
    if(!best||entry.value>best->value||(entry.value==best->value&&entry.word<best->word))best=&entry;
   }
   if(best&&best->value/continuation->total>=0.25)
    ranked.append({seed.word+' '+best->word,seed.score*best->value/continuation->total*0.9});
  }
 }
 // Reserve the literal slot before ranking to keep pagination consistent.
 const bool includeTyped=!current.isEmpty()&&(typedWordMode=="always"||(typedWordMode=="fallback"&&ranked.isEmpty()));
 const int count=std::min(limit-(includeTyped?1:0),int(ranked.size()));
 std::partial_sort(ranked.begin(),ranked.begin()+count,ranked.end(),[](const Result &a,const Result &b){
  if(a.score!=b.score)return a.score>b.score;
  return a.word<b.word;
 });
 QStringList result;
 if(includeTyped)result<<current;
 for(int i=0;i<count;++i){
  auto word=ranked[i].word;
  if(!current.isEmpty()){
   auto suffix=word.mid(current.size());
   if(current.size()>1&&current==PredictionLanguage::upper(current,languageCode))suffix=PredictionLanguage::upper(suffix,languageCode);
   word=current+suffix;
  }else if(sentenceStart)word=PredictionLanguage::upper(word.left(1),languageCode)+word.mid(1);
  if(languageCode=="eng"&&(word=="i"||word.startsWith("i'")))word[0]='I';
  result<<word;
 }
 return result;
}
