#include "WordPrediction.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include <memory>
#include <QBuffer>
#include <QFile>
#include <QHash>
#include <QSet>
#include <QRegularExpression>
namespace {
bool letter(QChar ch){return ch.isLetter();}
bool wordCharacter(QChar ch){return letter(ch)||ch.isMark()||ch=='\''||ch==QChar(0x2019);}
}
bool WordPrediction::characterLanguage(const QString &language){return language=="cmn"||language=="jpn"||language=="tha";}
QString WordPrediction::prefix(const QString &text,const QString &language){
 if(characterLanguage(language))return {};
 int start=text.size();while(start>0&&wordCharacter(text[start-1]))--start;
 auto result=text.mid(start);
 // Do not complete a word embedded in a number or unsupported alphabet.
 if(start>0&&text[start-1].isLetterOrNumber())return {};
 if(result.isEmpty()||!letter(result.front()))return {};
 return result.normalized(QString::NormalizationForm_C).replace(QChar(0x2019),QChar(39));
}
QString WordPrediction::completedWord(const QString &text){
 int end=text.size();while(end>0&&!text[end-1].isLetterOrNumber()&&text[end-1]!='\''){
  if(text[end-1].isSpace())return {};
  --end;
 }
 auto word=prefix(text.left(end));
 while(word.endsWith('\''))word.chop(1);
 return word.size()<64?word.toLower():QString();
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
   word=word.toLower();
   if(word.isEmpty()||!letter(word.front())||!std::all_of(word.begin(),word.end(),wordCharacter)||index.byWord.contains(word))continue;
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
 const auto normalized=text.normalized(QString::NormalizationForm_C).replace(QChar(0x2019),QChar(39));
 const bool characters=characterLanguage(languageCode);
 const auto current=prefix(normalized,languageCode);
 // Never offer an insertion inside numbers/unsupported alphabets or directly
 // after punctuation (accepting a prediction appends text, not a separator).
 if(!text.isEmpty()&&current.isEmpty()&&!text.back().isSpace()&&!(characters&&wordCharacter(text.back())))return {};
 const auto &language=model(languageCode);
 const auto &index=vocabulary(dictionary,languageCode);
 const auto &words=index.sorted;
 const auto lower=current.toLower();
 auto context=normalized.left(normalized.size()-current.size()).toLower();
 const int boundary=context.lastIndexOf(QRegularExpression("[.!?;:\\n\u3002\uff01\uff1f\u061b\u061f]"));
 context=context.mid(boundary+1);
 static const QRegularExpression token("[\\p{L}\\p{M}]+(?:'[\\p{L}\\p{M}]+)*");
 QStringList history{"<s>","<s>","<s>"};
 auto tokens=token.globalMatch(context);
 if(characters){for(auto ch:context){if(ch.isLetter())history<<QString(ch);else if(ch.isMark()&&history.last()!="<s>")history.last()+=ch;}}
 else while(tokens.hasNext())history<<tokens.next().captured();
 const QString previous=history.last(),two=history[history.size()-2]+' '+previous;
 const auto bi=useContext?language.contexts.constFind(previous):language.contexts.cend();
 const auto tri=useContext?language.contexts.constFind(two):language.contexts.cend();
 const auto four=useContext?language.contexts.constFind(history[history.size()-3]+' '+two):language.contexts.cend();
 const bool sentenceStart=history.size()==3;
 struct Result {QString word;double score;};
 QList<Result> ranked;

 auto add=[&](const QString &word,double prior){
  if(!word.startsWith(lower)||(!lower.isEmpty()&&word==lower))return;
  double probability=prior;
  if(bi!=language.contexts.cend())probability=(bi->value(word)+200*probability)/(bi->total+200);
  if(tri!=language.contexts.cend())probability=(tri->value(word)+20*probability)/(tri->total+20);
  if(four!=language.contexts.cend())probability=(four->value(word)+10*probability)/(four->total+10);
  // Bounded personalization: even a million observations cannot overpower
  // strong context by more than a factor of two.
  const double boost=1+std::min(1.0,std::log1p(std::max(0,frequencies.value(word)))/std::log(101.0));
  ranked.append({word,probability*boost});
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
  const auto word=it.key().toLower();
  if(it.value()>0&&!word.isEmpty()&&letter(word.front())&&std::all_of(word.begin(),word.end(),wordCharacter)&&!index.byWord.contains(word))add(word,1e-7);
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
   if(current.size()>1&&current==current.toUpper())suffix=suffix.toUpper();
   word=current+suffix;
  }else if(sentenceStart)word[0]=word[0].toUpper();
  if(languageCode=="eng"&&(word=="i"||word.startsWith("i'")))word[0]='I';
  result<<word;
 }
 return result;
}
