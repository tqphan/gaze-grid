#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "Dwell.h"
#include "PointDwell.h"
#include "KeyboardPack.h"
#include "KeyboardLayout.h"
#include "ActionRunner.h"
#include "ActionCatalog.h"
#include "KeyNames.h"
#include "SystemKeyboard.h"
#ifndef GAZE_GRID_WINDOWS
#include <xkbcommon/xkbcommon.h>
#endif
#include "ProfileStore.h"
#include "PointerActions.h"
#include "RelativePointer.h"
#include "GazeSmoothing.h"
#include <limits>
#include "MagnifierGeometry.h"
#include "WordPrediction.h"
#include "PredictionLanguage.h"
#include "ShortcutMappings.h"
#include "PortalPermissionStore.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QDir>
#include <QLineF>
class CoreTests:public QObject{
 Q_OBJECT
static QVariantMap cmd(QString tag,QString text){return {{"tag",tag},{"text",text}};}
private slots:
 void predictionLanguageRules(){
  const QString decomposed=QString::fromUtf8("cafe\xcc\x81");
  QCOMPARE(WordPrediction::prefix(decomposed,"fra"),QString::fromUtf8("caf\xc3\xa9"));
  QCOMPARE(PredictionLanguage::rawPrefix(decomposed,"fra"),decomposed);
  QCOMPARE(WordPrediction::completedWord(decomposed,"fra"),QString::fromUtf8("caf\xc3\xa9"));
  QCOMPARE(PredictionLanguage::completion(decomposed,QString::fromUtf8("caf\xc3\xa9s"),"fra",true),QString("s "));
  QCOMPARE(PredictionLanguage::completion(QString::fromUtf8("l\xe2\x80\x99"),"l'amour","fra",false),QString("amour"));
  QVERIFY(PredictionLanguage::completion("ca","hello","fra",true).isEmpty());
  QCOMPARE(PredictionLanguage::completion("","hello","eng",false),QString("hello"));
  for(const auto &language:QStringList{"cmn","jpn","tha"}){
   QVERIFY(!PredictionLanguage::appendSpace(language));
   QCOMPARE(PredictionLanguage::completion("",QString::fromUtf8("\xe4\xbd\xa0"),language,true),QString::fromUtf8("\xe4\xbd\xa0"));
  }
  QCOMPARE(PredictionLanguage::lower("I","tur"),QString(QChar(0x131)));
  QCOMPARE(PredictionLanguage::upper("i","tur"),QString(QChar(0x130)));
  QCOMPARE(WordPrediction::completedWord("ISLAK!","tur"),QString(QChar(0x131))+"slak");
  QCOMPARE(WordPrediction::completedWord("ISLAK!","eng"),QString("islak"));
 }

 void predictionUnicodeScalars(){
  const char32_t scalar=0x20000;
  const auto supplementary=QString::fromUcs4(&scalar,1);
  QVERIFY(PredictionLanguage::validWord(supplementary));
  QCOMPARE(WordPrediction::prefix(supplementary,"eng"),supplementary);
  QCOMPARE(WordPrediction::completedWord(supplementary+"!"),supplementary);
  QVERIFY(WordPrediction::prefix("123"+supplementary).isEmpty());
  QCOMPARE(PredictionLanguage::tokens(supplementary+QString(QChar(0x301))+" "+supplementary,"cmn"),QStringList({supplementary+QString(QChar(0x301)),supplementary}));
  QCOMPARE(PredictionLanguage::tokens("L'amour, cafe\xcc\x81!","fra"),QStringList({"l'amour",QString::fromUtf8("caf\xc3\xa9")}));
  const QStringList dictionary{supplementary};
  QCOMPARE(WordPrediction::suggest("",dictionary,{},8,false,true,"never","cmn"),dictionary);
  QCOMPARE(WordPrediction::suggest(supplementary,dictionary,{},8,false,true,"never","cmn"),dictionary);
  QCOMPARE(PredictionLanguage::completion(supplementary,supplementary,"cmn",true),supplementary);
 }

 void vietnamesePhraseCompletion(){
  const auto words=WordPrediction::dictionary("vie");
  const auto phrase=QString::fromUtf8("xin l\xe1\xbb\x97i");
  const auto results=WordPrediction::suggest("xin",words,{},8,true,false,"never","vie");
  QVERIFY(results.contains(phrase));
  QVERIFY(!results.contains("xin"));
  QCOMPARE(PredictionLanguage::completion("xin",phrase,"vie",true),phrase.mid(3)+" ");
  const auto frequency=WordPrediction::suggest("xin",words,{},8,false,false,"never","vie");
  for(const auto &word:frequency)QVERIFY(!word.contains(' '));
  const auto english=WordPrediction::suggest("xin",words,{},8,true,false,"never","eng");
  for(const auto &word:english)QVERIFY(!word.contains(' '));
  QCOMPARE(WordPrediction::suggest("xin",words,{},1,true,false,"always","vie"),QStringList{"xin"});
  QVERIFY(WordPrediction::suggest("xin",words,{},0,true,false,"never","vie").isEmpty());
 }
 void turkishPredictionCasing(){
  const QString word=QString(QChar(0x131))+"slak";
  const QStringList words{word,"istanbul"};
  const auto results=WordPrediction::suggest("IS",words,{},8,false,false,"never","tur");
  QVERIFY(results.contains("ISLAK"));
  QVERIFY(!results.contains("ISTANBUL"));
  const auto dotted=WordPrediction::suggest(QString(QChar(0x130))+"s",words,{},8,false,false,"never","tur");
  QVERIFY(dotted.contains(QString(QChar(0x130))+"stanbul"));
 }

 void keyboardFileCache(){
  QTemporaryDir dir;QVERIFY(dir.isValid());const auto path=dir.filePath("keyboard.xml");
  const QByteArray first="<Keyboard><Name>A</Name></Keyboard>",second="<Keyboard><Name>B</Name></Keyboard>";
  auto write=[&](const QByteArray &bytes){QFile file(path);return file.open(QIODevice::WriteOnly)&&file.write(bytes)==bytes.size();};
  struct ResetCache {~ResetCache(){KeyboardPack::setCacheEnabled(false);}} reset;
  KeyboardPack::setCacheEnabled(true);QVERIFY(write(first));
  QByteArray source;const auto original=KeyboardPack::load(path,&source);QVERIFY(!original.contains("error"));QCOMPARE(source,first);
  QCOMPARE(KeyboardPack::load(path,&source),original);QCOMPARE(source,first);
  // Same size, distinct modification time must reload.
  QVERIFY(write(second));QFile file(path);QVERIFY(file.open(QIODevice::ReadWrite));
  QVERIFY(file.setFileTime(QDateTime::currentDateTimeUtc().addSecs(-60),QFileDevice::FileModificationTime));file.close();
  QVERIFY(KeyboardPack::load(path,&source)!=original);QCOMPARE(source,second);
  QVERIFY(write(first+" "));QCOMPARE(KeyboardPack::load(path,&source),original);QCOMPARE(source,first+" ");
  // Keep metadata unchanged deliberately: reuse proves the hit avoids a content read.
  const auto stamp=QFileInfo(path).lastModified();
  QVERIFY(write(second+" "));QVERIFY(file.open(QIODevice::ReadWrite));
  QVERIFY(file.setFileTime(stamp,QFileDevice::FileModificationTime));file.close();
  QCOMPARE(KeyboardPack::load(path,&source),original);QCOMPARE(source,first+" ");
  KeyboardPack::invalidate(path);QVERIFY(KeyboardPack::load(path,&source)!=original);QCOMPARE(source,second+" ");
  QVERIFY(write(first+" "));QVERIFY(file.open(QIODevice::ReadWrite));
  QVERIFY(file.setFileTime(stamp,QFileDevice::FileModificationTime));file.close();
  KeyboardPack::setCacheEnabled(false);KeyboardPack::setCacheEnabled(true);
  QCOMPARE(KeyboardPack::load(path,&source),original);QCOMPARE(source,first+" ");
  QVERIFY(QFile::remove(path));QVERIFY(KeyboardPack::load(path,&source).contains("error"));QVERIFY(source.isEmpty());
  QVERIFY(write(second));KeyboardPack::setCacheEnabled(false);QCOMPARE(KeyboardPack::load(path,&source).contains("error"),false);QCOMPARE(source,second);
  QVERIFY(write(first));QCOMPARE(KeyboardPack::load(path,&source),original);QCOMPARE(source,first);
  KeyboardPack::setCacheEnabled(true);QCOMPARE(KeyboardPack::load(path,&source),original);QCOMPARE(source,first);
 }
#ifndef GAZE_GRID_WINDOWS
 void nativeXkbLabels(){
  SystemKeyboard keyboard;QVERIFY(!keyboard.available());
  auto context=xkb_context_new(XKB_CONTEXT_NO_FLAGS);QVERIFY(context);
  quint32 shiftMask=0,capsMask=0,altGrMask=0;
  auto load=[&](const char *layout,const char *variant=nullptr){
   xkb_rule_names names{};names.layout=layout;names.variant=variant;
   auto map=xkb_keymap_new_from_names(context,&names,XKB_KEYMAP_COMPILE_NO_FLAGS);
   if(!map)return false;
   auto mask=[&](const char *name){const auto index=xkb_keymap_mod_get_index(map,name);return index==XKB_MOD_INVALID?quint32(0):quint32(1)<<index;};
   shiftMask=mask(XKB_MOD_NAME_SHIFT);capsMask=mask(XKB_MOD_NAME_CAPS);altGrMask=mask("Mod5");
   char *source=xkb_keymap_get_as_string(map,XKB_KEYMAP_FORMAT_TEXT_V1);
   const bool changed=keyboard.setKeymap(QByteArray(source));
   free(source);xkb_keymap_unref(map);return changed;
  };
  QVERIFY(load("us"));QVERIFY(keyboard.available());
  QCOMPARE(keyboard.label(16,{}),QString("q"));
  QCOMPARE(keyboard.label(16,{true,false,false}),QString("Q"));
  QCOMPARE(keyboard.label(16,{false,true,false}),QString("Q"));
  QCOMPARE(keyboard.label(16,{true,true,false}),QString("q"));
  QVERIFY(load("fr"));QCOMPARE(keyboard.label(16,{}),QString("a"));
  QVERIFY(load("de"));QCOMPARE(keyboard.label(21,{}),QString("z"));
  QCOMPARE(keyboard.label(16,{false,false,true}),QString("@"));
  QVERIFY(load("ru"));QCOMPARE(keyboard.label(16,{}),QString::fromUtf8("й"));
  QVERIFY(load("us","intl"));QCOMPARE(keyboard.label(40,{}),QString("acute"));
  QVERIFY(load("us,de"));QVERIFY(!keyboard.available());
  QVERIFY(keyboard.setModifiers(0,0,0,0));QVERIFY(keyboard.available());
  QCOMPARE(keyboard.label(21,{}),QString("y"));
  QVERIFY(keyboard.setModifiers(0,0,0,1));QCOMPARE(keyboard.label(21,{}),QString("z"));
  QVERIFY(!keyboard.setModifiers(0,0,0,1));
  QVERIFY(keyboard.setModifiers(shiftMask,0,0,1));QCOMPARE(keyboard.label(21,{}),QString("Z"));
  QVERIFY(keyboard.setModifiers(0,shiftMask,0,1));QCOMPARE(keyboard.label(21,{}),QString("Z"));
  QVERIFY(keyboard.setModifiers(0,0,capsMask,1));QCOMPARE(keyboard.label(21,{}),QString("Z"));
  QVERIFY(keyboard.setModifiers(0,0,0,1));QCOMPARE(keyboard.label(21,{false,true,false}),QString("z"));
  QVERIFY(keyboard.setModifiers(altGrMask,0,0,1));QCOMPARE(keyboard.label(16,{}),QString("@"));
  QVERIFY(keyboard.setModifiers(0,0,0,99));QVERIFY(!keyboard.available());QVERIFY(keyboard.label(16,{}).isEmpty());
  QVERIFY(load("us,ru"));QVERIFY(!keyboard.available()); // Old group and modifiers cannot survive a replacement map.
  QVERIFY(keyboard.setModifiers(0,0,0,1));QCOMPARE(keyboard.label(16,{}),QString::fromUtf8("й"));
  QVERIFY(load("us,ru"));QVERIFY(!keyboard.available()); // Identical bytes from a new device still need fresh state.
  QVERIFY(keyboard.setKeymap("invalid"));QVERIFY(!keyboard.available());
  QVERIFY(load("us"));QVERIFY(keyboard.setKeymap({}));QVERIFY(!keyboard.available());
  xkb_context_unref(context);
 }
#endif
 void systemLabelsPreservePersonalKeys(){
  int translated=0;
  auto translate=[&](int scan,SystemKeyboard::Modifiers m){++translated;return scan==16?(m.shift?QString("A"):QString("a")):QString("x");};
  for(const auto &name:QDir(PACK_PATH).entryList({"*.xml"},QDir::Files)){
   const auto layout=KeyboardLayout::fromXml(KeyboardPack::load(QDir(PACK_PATH).filePath(name)));
   QVERIFY(!layout.contains("error"));const auto original=layout["keys"].toList();auto copy=original;
   const auto personal=SystemKeyboard::labels(copy,{},[](int,SystemKeyboard::Modifiers){return QString();});
   const auto system=SystemKeyboard::labels(copy,{},translate);
   QCOMPARE(copy,original);QCOMPARE(system.size(),original.size());
   for(int i=0;i<original.size();++i)QCOMPARE(personal[i].toString(),original[i].toMap()["label"].toString());
  }
  QVERIFY(translated>0);
  const QVariantMap key{{"label","q"},{"shiftLabel","Q"},{"commands",QVariantList{cmd("Text","q")}}};
  QCOMPARE(SystemKeyboard::labels({key},{},translate).front().toString(),QString("a"));
  QCOMPARE(SystemKeyboard::labels({key},{true,false,false},translate).front().toString(),QString("A"));
  auto macro=key;macro["commands"]=QVariantList{cmd("Text","q"),cmd("Action","Return")};
  auto custom=key;custom["label"]="Personal";
  QCOMPARE(SystemKeyboard::labels({macro,custom},{},translate),QVariantList({QString("q"),QString("Personal")}));
 }
 void explicitPhysicalKey(){
  QTemporaryDir dir;QFile file(dir.filePath("physical.xml"));
  auto load=[&](QString content){
   if(!file.open(QIODevice::WriteOnly|QIODevice::Truncate))return QVariantMap{{"error","test file"}};
   file.write(("<Keyboard><Grid><Rows>1</Rows><Cols>1</Cols></Grid><Content><DynamicKey><Label>Personal</Label>"+content+"</DynamicKey></Content></Keyboard>").toUtf8());file.close();
   return KeyboardLayout::fromXml(KeyboardPack::load(file.fileName()));
  };
  const auto layout=load("<PhysicalKey>q</PhysicalKey><Text>a</Text>");
  QVERIFY2(!layout.contains("error"),qPrintable(layout["error"].toString()));
  const auto keys=layout["keys"].toList(),commands=keys.front().toMap()["commands"].toList();
  QCOMPARE(commands.front().toMap()["physicalKey"].toString(),QString("Q"));
  QCOMPARE(SystemKeyboard::labels(keys,{},[](int scan,SystemKeyboard::Modifiers){return scan==16?QString("mapped"):QString("wrong");}).front().toString(),QString("mapped"));
  QCOMPARE(SystemKeyboard::labels(keys,{},[](int,SystemKeyboard::Modifiers){return QString();}).front().toString(),QString("Personal"));
  ActionRunner runner;QStringList events;
  runner.sendKey=[&](QString name,bool down){events<<name+(down?"+":"-");return true;};
  QSignalSpy finished(&runner,&ActionRunner::finished);
  QVERIFY(runner.start(commands));QTRY_COMPARE(finished.size(),1);
  QCOMPARE(events,QStringList({"A+","A-"}));events.clear();
  runner.physicalText=true;
  QVERIFY(runner.start(commands));QTRY_COMPARE(finished.size(),2);
  QCOMPARE(events,QStringList({"Q+","Q-"}));events.clear();
  QVERIFY(runner.beginDwellHold(commands));runner.endDwellHold();
  QCOMPARE(events,QStringList({"Q+","Q-"}));events.clear();
  QVERIFY(runner.toggleDwellKey(commands));QVERIFY(runner.isToggled(commands));
  QVERIFY(runner.toggleDwellKey(commands));QVERIFY(!runner.isToggled(commands));
  QCOMPARE(events,QStringList({"Q+","Q-"}));
  runner.textOnly=true;QString local;
  runner.sendLocalText=[&](QString text,QStringList){local=text;return true;};runner.sendLocalKey=[](QString,QStringList){return true;};
  QVERIFY(runner.start(commands));QTRY_COMPARE(finished.size(),3);QCOMPARE(local,QString("q"));
  runner.physicalText=false;
  QVERIFY(runner.start(commands));QTRY_COMPARE(finished.size(),4);QCOMPARE(local,QString("a"));
  for(const QString content:{"<PhysicalKey/><Text>q</Text>","<PhysicalKey>Q</PhysicalKey><PhysicalKey>A</PhysicalKey><Text>q</Text>","<PhysicalKey>Unknown</PhysicalKey><Text>q</Text>","<PhysicalKey>LSHIFT</PhysicalKey><Text>q</Text>","<PhysicalKey>Q</PhysicalKey><Text>q</Text><Action>Return</Action>","<PhysicalKey>Q</PhysicalKey><Action>Return</Action>"})
   QVERIFY(load(content).contains("error"));
 }
 void systemTextUsesPhysicalInput(){
  ActionRunner runner;runner.physicalText=true;QStringList events;int commits=0;
  runner.sendKey=[&](QString name,bool down){events<<name+(down?"+":"-");return true;};
  runner.sendText=[&](QString){++commits;return true;};
  runner.physicalTextReady=[]{return true;};
  QSignalSpy text(&runner,&ActionRunner::textCommitted),finished(&runner,&ActionRunner::finished);
  QVERIFY(runner.start({cmd("Text","q")}));QTRY_COMPARE(finished.size(),1);
  QCOMPARE(events,QStringList({"Q+","Q-"}));QCOMPARE(commits,0);QCOMPARE(text.size(),0);
  events.clear();QVERIFY(!runner.start({cmd("Text",QString::fromUtf8("qé"))}));QVERIFY(events.isEmpty());
  runner.physicalTextReady=[]{return false;};events.clear();
  QVERIFY(!runner.start({cmd("Text","q")}));QVERIFY(events.isEmpty());
  runner.textOnly=true;
  QVERIFY(runner.start({cmd("Text","q")}));QTRY_COMPARE(finished.size(),2);
  QCOMPARE(text.size(),1);QCOMPARE(text.front().front().toString(),QString("q"));QVERIFY(events.isEmpty());
 }
 void systemPredictionTracksTranslatedKeys(){
  ActionRunner runner;runner.physicalText=true;
  runner.physicalTextReady=[]{return true;};
  runner.sendKey=[](QString,bool){return true;};
  runner.physicalKeyText=[&](QString name){return name=="Q"?(runner.shifted()?QString("A"):QString("a")):QString();};
  QString inserted;runner.sendText=[&](QString value){inserted=value;return true;};
  QSignalSpy text(&runner,&ActionRunner::textCommitted),finished(&runner,&ActionRunner::finished);
  QVERIFY(runner.start({cmd("Text","q")}));QTRY_COMPARE(finished.size(),1);
  QCOMPARE(text.takeFirst().front().toString(),QString("a"));
  QVERIFY(runner.start({cmd("Action","LShift")}));QTRY_COMPARE(finished.size(),2);
  QVERIFY(runner.start({cmd("Text","q")}));QTRY_COMPARE(finished.size(),3);
  QCOMPARE(text.takeFirst().front().toString(),QString("A"));
  QVERIFY(runner.start({cmd("Action","Back")}));QTRY_COMPARE(finished.size(),4);
  QCOMPARE(text.takeFirst().front().toString(),QString("\b"));
  QVERIFY(runner.start({cmd("KeyDown","LCONTROL"),cmd("Text","q"),cmd("KeyUp","LCONTROL")}));QTRY_COMPARE(finished.size(),5);
  QVERIFY(text.isEmpty());
  auto completion=cmd("Text",QString::fromUtf8("caf\xc3\xa9 "));completion["literalCompletion"]=true;
  QVERIFY(runner.start({completion}));QTRY_COMPARE(finished.size(),6);
  QCOMPARE(inserted,completion["text"].toString());
  QCOMPARE(text.takeFirst().front().toString(),inserted);
 }
 void systemPunctuationRemovesCompletionSpace(){
  for(bool enabled:{false,true})for(bool shift:{false,true}){
   ActionRunner runner;runner.physicalText=true;
   runner.physicalTextReady=[]{return true;};
   QString output="word ";bool pending=true;
   runner.physicalKeyText=[&](QString name){
    if(name=="Q")return runner.shifted()?QString("!"):QString(".");
    return name=="SPACE"?QString(" "):QString();
   };
   runner.sendKey=[&](QString name,bool down){
    if(down){if(name=="BACK")output.chop(1);else if(name=="Q"||name=="SPACE")output+=runner.physicalKeyText(name);}
    return true;
   };
   runner.removeSpaceBeforeText=[&](QString text){const bool remove=pending&&enabled&&text.size()==1&&text.front().isPunct();pending=false;return remove;};
   runner.appendSpaceAfterPunctuation=[](QString){return true;};
   QSignalSpy finished(&runner,&ActionRunner::finished);
   if(shift){QVERIFY(runner.start({cmd("Action","LShift")}));QTRY_COMPARE(finished.size(),1);}
   auto punctuation=cmd("Text","a");punctuation["systemText"]="q";
   QVERIFY(runner.start({punctuation}));QTRY_COMPARE(finished.size(),shift?2:1);
   const QString mark=shift?"!":".";
   QCOMPARE(output,enabled?QString("word")+mark+" ":QString("word ")+mark);
  }
 }
 void predictionsUseUnicodeOnlyForNonAscii(){
  for(bool system:{false,true}){
   ActionRunner runner;runner.physicalText=system;runner.physicalTextReady=[]{return true;};
   QStringList keys,insertions;
   runner.sendKey=[&](QString name,bool down){keys<<name+(down?"+":"-");return true;};
   runner.sendText=[&](QString text){insertions<<text;return true;};
   QSignalSpy finished(&runner,&ActionRunner::finished);
   auto ascii=cmd("Text","Hi ");ascii["literalCompletion"]=true;
   QVERIFY(runner.start({ascii}));QTRY_COMPARE(finished.size(),1);
   QVERIFY(insertions.isEmpty());QCOMPARE(keys,QStringList({"LSHIFT+","H+","H-","LSHIFT-","I+","I-","SPACE+","SPACE-"}));
   keys.clear();ascii["alwaysUnicode"]=true;
   QVERIFY(runner.start({ascii}));QTRY_COMPARE(finished.size(),2);
   QCOMPARE(insertions.takeFirst(),QString("Hi "));QVERIFY(keys.isEmpty());
   for(const auto value:{QString::fromUtf8("caf\xc3\xa9 "),QString::fromUtf8("\xe4\xbd\xa0 ")}){
    auto unicode=cmd("Text",value);unicode["literalCompletion"]=true;
    const int expected=finished.size()+1;QVERIFY(runner.start({unicode}));QTRY_COMPARE(finished.size(),expected);
    QCOMPARE(insertions.takeFirst(),value);QVERIFY(keys.isEmpty());
   }
  }
 }
 void systemKeyboardProfileCompatibility(){
  auto profile=ProfileStore::defaults();QCOMPARE(profile["systemKeyboard"].toBool(),false);QVERIFY(ProfileStore::valid(profile));
  profile.remove("systemKeyboard");QVERIFY(ProfileStore::valid(profile));
  profile["systemKeyboard"]=true;QVERIFY(ProfileStore::valid(profile));
  profile["systemKeyboard"]="system";QVERIFY(!ProfileStore::valid(profile));
 }
 void bundledDwellGroups(){
  const QStringList groups{"Alphabet","Numbers","Punctuations","Modifiers","Functions","Predictions","Conversations","Navigation","Editing","Mouse","Gamepad","Others"};
  const QDir dir(PACK_PATH);
  for(const auto &name:dir.entryList({"*.xml"},QDir::Files)){
   const auto layout=KeyboardLayout::fromXml(KeyboardPack::load(dir.filePath(name)));
   QVERIFY2(!layout.contains("error"),qPrintable(name));
   for(const auto &value:layout["keys"].toList()){
    const auto key=value.toMap();if(key["kind"]=="Scratchpad")continue;
    QVERIFY2(groups.contains(key["dwellGroup"].toString()),qPrintable(name+": "+key["label"].toString()));
   }
  }
 }
 void groupDwellPreferencesValidation(){
  auto values=ProfileStore::defaults();QVERIFY(ProfileStore::valid(values));
  values["groupDwellTimes"]=QJsonObject{{"Alphabet",250},{"Others",3000}};QVERIFY(ProfileStore::valid(values));
  for(const auto &invalid:QJsonArray{0,249,3001,750.5,"750",true}){
   values["groupDwellTimes"]=QJsonObject{{"Alphabet",invalid}};QVERIFY(!ProfileStore::valid(values));
  }
  values["groupDwellTimes"]=QJsonObject{{"Unknown",750}};QVERIFY(!ProfileStore::valid(values));
  values["groupDwellTimes"]=QJsonArray{};QVERIFY(!ProfileStore::valid(values));
 }
 void keyGroupDwellTimes(){
  QTemporaryDir dir;QFile file(dir.filePath("dwell.xml"));
  auto load=[&](const QString &groups,const QString &attributes,const QString &membership){
   if(!file.open(QIODevice::WriteOnly|QIODevice::Truncate))return QVariantMap{{"error","test file"}};
   file.write(("<Keyboard><Grid><Rows>1</Rows><Cols>1</Cols></Grid>"+groups+
    "<Content><DynamicKey "+attributes+"><Label>A</Label><Text>a</Text>"+membership+
    "</DynamicKey></Content></Keyboard>").toUtf8());file.close();
   return KeyboardLayout::fromXml(KeyboardPack::load(file.fileName()));
  };
  const QString all="<KeyGroup Name=\"All\" CompletionTimes=\"900\"/>";
  const QString defaults="<KeyGroup Name=\"Default\" CompletionTimes=\"1000\"/>";
  const QString slow="<KeyGroup Name=\"Slow\" CompletionTimes=\"1200,1500\"/>";
  const QString member="<KeyGroup>Slow</KeyGroup>";
  struct Case {QString groups,attributes,membership;int expected;};
  const QList<Case> cases{
   {"","","",0},{all,"","",900},{defaults,"","",1000},
   {slow,"","",0},{slow,"",member,1200},
   {all+defaults+slow,"",member,1200},{slow+defaults+all,"",member,900},
   {all+slow,"CompletionTimes=\"600\"",member,600},
   {all+slow,"CompletionTimes=\"0\"",member,0},
   {"","CompletionTimes=\"700,800\"","",700}
  };
  for(const auto &test:cases){
   const auto layout=load(test.groups,test.attributes,test.membership);
   QVERIFY2(!layout.contains("error"),qPrintable(layout["error"].toString()));
   QCOMPARE(layout["keys"].toList().first().toMap()["dwell"].toInt(),test.expected);
   QCOMPARE(layout["keys"].toList().first().toMap()["individualDwell"].toBool(),!test.attributes.isEmpty());
  }
 }
 void processPriorityPreferences(){
  auto values=ProfileStore::defaults();
  QCOMPARE(values["processPriority"].toInt(),0);
  for(int priority=0;priority<=2;++priority){values["processPriority"]=priority;QVERIFY(ProfileStore::valid(values));}
  for(const auto &invalid:QJsonArray{-1,3,1.5,"high",true}){values["processPriority"]=invalid;QVERIFY(!ProfileStore::valid(values));}
  values.remove("processPriority");QVERIFY(ProfileStore::valid(values));
 }
 void overlapSettingOnlyAppliesToCanvas(){
  QTemporaryDir dir;QFile file(dir.filePath("layout.xml"));
  for(const QString mode:{"Canvas","Grid"})for(const QString setting:{"","false","true","TRUE"}){
   QVERIFY(file.open(QIODevice::WriteOnly|QIODevice::Truncate));
   const QString geometry=mode=="Canvas"?"<Canvas><Width>100</Width><Height>100</Height></Canvas>":"<Grid><Rows>1</Rows><Cols>1</Cols></Grid>";
   file.write(("<Keyboard>"+geometry+"<ActivateOverlappingKeys>"+setting+"</ActivateOverlappingKeys><Content/></Keyboard>").toUtf8());file.close();
   const auto layout=KeyboardLayout::fromXml(KeyboardPack::load(file.fileName()));
   QVERIFY(!layout.contains("error"));
   QCOMPARE(layout["activateoverlappingkeys"].toBool(),mode=="Canvas"&&setting.compare("true",Qt::CaseInsensitive)==0);
  }
 }
 void overlappingDwellHoldsReleaseIndependently(){
  ActionRunner r;QStringList events;
  r.sendKey=[&](QString key,bool down){events<<key+(down?"+":"-");return true;};
  QVERIFY(r.beginDwellHold({cmd("Text","!")},"key:0"));
  QVERIFY(r.beginDwellHold({cmd("Text","@")},"key:1"));
  QVERIFY(r.beginDwellHold({cmd("Text","!")},"key:2"));
  QCOMPARE(events,QStringList({"LSHIFT+","1+","2+"}));
  r.endDwellHold("key:0");QVERIFY(r.isHeld("1"));QVERIFY(r.shifted());
  r.endDwellHold("key:2");QVERIFY(!r.isHeld("1"));QVERIFY(r.isHeld("2"));QVERIFY(r.shifted());
  r.endDwellHold("key:1");QVERIFY(r.heldKeys().isEmpty());
  QCOMPARE(events,QStringList({"LSHIFT+","1+","2+","1-","2-","LSHIFT-"}));
  QVERIFY(r.beginDwellHold({cmd("Text","a")},"key:0"));
  QVERIFY(r.beginDwellHold({cmd("Text","b")},"key:1"));
  r.cancel(true);QVERIFY(r.heldKeys().isEmpty());
 }
 void togglesPersistOnlyWhenRequested(){
  ActionRunner r;r.sendKey=[](QString,bool){return true;};
  r.supportsPhysicalKey=[](QString){return true;};
  QVERIFY(r.toggleDwellKey({cmd("Text","!")}));
  r.cancel(true,true);
  QVERIFY(r.isToggled({cmd("Text","!")}));QVERIFY(r.isHeld("LSHIFT"));
  QVERIFY(r.toggleDwellKey({cmd("Text","!")}));QVERIFY(r.heldKeys().isEmpty());
  QVERIFY(r.start({cmd("KeyToggle","A")}));
  QTRY_VERIFY(!r.busy());
  r.cancel(true,true);QVERIFY(r.isToggled({cmd("KeyToggle","A")}));
  QVERIFY(r.beginDwellHold({cmd("Text","b")}));
  r.cancel(true,true);QVERIFY(!r.isHeld("B"));QVERIFY(r.isHeld("A"));
  r.cancel(true);QVERIFY(r.heldKeys().isEmpty());
  QVERIFY(r.toggleDwellKey({cmd("Text","a")}));
  r.cancel();QVERIFY(r.heldKeys().isEmpty());
 }

 void conversationKeyboardPlacement(){
  for(const auto &side:{"bottom","top"}){
   const auto layout=KeyboardLayout::fromXml(KeyboardPack::load(QStringLiteral(PACK_PATH)+"/keyboard-conversation-"+side+".xml"));
   QVERIFY2(!layout.contains("error"),qPrintable(layout["error"].toString()));
   QCOMPARE(layout["position"].toString(),QString(side==QString("top")?"Top":"Bottom"));
   double typingRow=-1,scratchRow=-1,suggestionRow=-1,phraseRow=-1;
   for(const auto &entry:layout["keys"].toList()){
    const auto key=entry.toMap();const auto row=key["row"].toDouble();
    if(key["label"].toString()=="q")typingRow=row;
    if(key["kind"]=="Scratchpad")scratchRow=row;
    if(key["kind"]=="SuggestionRow")suggestionRow=row;
    if(key["label"].toString()=="suction")phraseRow=row;
   }
   QVERIFY(typingRow>=0&&scratchRow>=0&&suggestionRow>=0&&phraseRow>=0);
   if(QString(side)=="top")QVERIFY(suggestionRow==0&&suggestionRow<typingRow&&typingRow<scratchRow&&scratchRow<phraseRow);
   else QVERIFY(phraseRow<scratchRow&&scratchRow<suggestionRow&&suggestionRow<typingRow);
  }
 }
 void conversationSpeakLabels(){
  const auto layout=KeyboardLayout::fromXml(KeyboardPack::load(QStringLiteral(PACK_PATH)+"/keyboard-conversation-bottom.xml"));
  QVERIFY(!layout.contains("error"));
  const QStringList expected{"suction","index","thumb","rotate","slide","shoulders","headrest","button","butt","earbud","deflate","inflate"};
  QStringList spoken;
  ActionRunner runner;
  runner.supportsAction=ActionCatalog::supported;
  runner.speakLabel=[&](QString label){spoken<<label;};
  QSignalSpy committed(&runner,&ActionRunner::textCommitted);
  QSignalSpy failed(&runner,&ActionRunner::failed);
  for(const auto &entry:layout["keys"].toList()){
   const auto key=entry.toMap();
   if(!expected.contains(key["label"].toString().trimmed()))continue;
   const auto commands=key["commands"].toList();
   QCOMPARE(commands.size(),1);
   QCOMPARE(commands.first().toMap()["text"].toString(),QString("SpeakLabel"));
   QVERIFY(runner.start(commands));QTRY_VERIFY(!runner.busy());
  }
  QCOMPARE(spoken,expected);
  QCOMPARE(committed.count(),0);QCOMPARE(failed.count(),0);
 }
 void fixedXmlKeyModes(){
  QTemporaryDir dir;
  auto compile=[&](QString mode,QString command){
   QFile file(dir.filePath("mode.xml"));
   if(!file.open(QIODevice::WriteOnly))return QVariantMap{{"error","test file"}};
   file.write(("<Keyboard><Grid><Rows>1</Rows><Cols>1</Cols></Grid><Content><DynamicKey row=\"0\" col=\"0\" "+mode+">"+command+"</DynamicKey></Content></Keyboard>").toUtf8());
   file.close();return KeyboardLayout::fromXml(KeyboardPack::load(file.fileName()));
  };
  for(const QString mode:{"Tap","Repeat","Hold","Toggle"}){
   for(const QString command:{"<Action>GamepadATap</Action>","<Text>a</Text>","<Action>Space</Action>"}){
    const auto layout=compile("mode=\""+mode.toLower()+"\"",command);
    QVERIFY2(!layout.contains("error"),qPrintable(layout["error"].toString()));
    QCOMPARE(layout["keys"].toList().front().toMap()["mode"].toString(),mode);
   }
  }
  QVERIFY(compile("mode=\"typo\"","<Action>GamepadATap</Action>").contains("error"));
  for(const QString command:{"<Action>LShift</Action>","<Action>GamepadLeftX:1</Action>","<Action>GamepadARelease</Action>","<Text>word</Text>","<Action>GamepadATap</Action><Wait>10</Wait>"}){
   QVERIFY(compile("mode=\"Hold\"",command).contains("error"));
  }
  const auto inherited=compile("","<Action>GamepadATap</Action>");
  QVERIFY(!inherited.contains("error"));
  QVERIFY(inherited["keys"].toList().front().toMap()["mode"].toString().isEmpty());
 }

 void toggleKeyLifecycle(){
  ActionRunner r;QStringList events;
  r.sendKey=[&](QString key,bool down){events<<key+(down?"+":"-");return true;};
  r.supportsPhysicalKey=[](QString){return true;};
  QVERIFY(r.toggleDwellKey({cmd("Text","a")}));
  r.endDwellHold();QVERIFY(r.isHeld("A"));
  QVERIFY(r.isToggled({cmd("Text","a")}));
  QVERIFY(r.toggleDwellKey({cmd("Action","Tab")}));
  QVERIFY(r.toggleDwellKey({cmd("Text","a")}));
  QVERIFY(!r.isHeld("A"));QVERIFY(r.isHeld("TAB"));
  QVERIFY(!r.isToggled({cmd("Text","a")}));
  QVERIFY(r.isToggled({cmd("Action","Tab")}));
  r.releaseToggledKeys();QVERIFY(r.heldKeys().isEmpty());
  QVERIFY(!r.isToggled({cmd("Action","Tab")}));
  QCOMPARE(events,QStringList({"A+","TAB+","A-","TAB-"}));
  events.clear();
  QVERIFY(r.toggleDwellKey({cmd("Text","!")}));
  QVERIFY(r.toggleDwellKey({cmd("Text","?")}));
  QVERIFY(r.toggleDwellKey({cmd("Text","!")}));
  QVERIFY(r.isHeld("LSHIFT"));
  QVERIFY(r.toggleDwellKey({cmd("Text","?")}));
  QVERIFY(r.heldKeys().isEmpty());
  QVERIFY(r.toggleDwellKey({cmd("Text","!")}));
  r.cancel(true);QVERIFY(r.heldKeys().isEmpty());
  QVERIFY(!r.toggleDwellKey({cmd("Action","LShift")}));
  r.supportsPhysicalKey=[](QString){return false;};
  QVERIFY(!r.toggleDwellKey({cmd("Text","a")}));
  QVERIFY(r.heldKeys().isEmpty());
 }

 void dwellHoldPhysicalLifecycle(){
  ActionRunner r;QStringList events;
  r.sendKey=[&](QString key,bool down){events<<key+(down?"+":"-");return true;};
  r.supportsPhysicalKey=[](QString){return true;};
  for(const QString text:{"a","7","!","."," ","\t","\n"}){
   QVERIFY(r.beginDwellHold({cmd("Text",text)}));
   QVERIFY(!r.heldKeys().isEmpty());
   r.endDwellHold();QVERIFY(r.heldKeys().isEmpty());
  }
  events.clear();
  QVERIFY(r.beginDwellHold({cmd("Text","!")}));
  QCOMPARE(events,QStringList({"LSHIFT+","1+"}));
  r.cancel(true);
  QCOMPARE(events,QStringList({"LSHIFT+","1+","1-","LSHIFT-"}));
  for(const QString action:{"Left","Right","Up","Down","BackOne","Enter","Space","Tab","Escape","Delete","Insert","Home","End","PgUp","PgDn","F1","F12","F24","PrintScreen","Pause","A","0","SEMICOLON"}){
   QVERIFY(r.beginDwellHold({cmd("Action",action)}));
   r.cancel();QVERIFY(r.heldKeys().isEmpty());
  }
  for(const QString modifier:{"LShift","RShift","LCtrl","RCtrl","LMenu","RMenu","LWin","RWin"}){
   QVERIFY(ActionRunner::dwellKey({cmd("Action",modifier)}).first.isEmpty());
   QVERIFY(!r.beginDwellHold({cmd("Action",modifier)}));
   QVERIFY(r.heldKeys().isEmpty());
  }
  QVERIFY(!r.beginDwellHold({cmd("Text","word")}));
  QVERIFY(!r.beginDwellHold({cmd("Action","Sleep")}));
  r.textOnly=true;QVERIFY(!r.beginDwellHold({cmd("Text","a")}));
  r.textOnly=false;r.supportsPhysicalKey=[](QString){return false;};
  QVERIFY(!r.beginDwellHold({cmd("Text","a")}));QVERIFY(r.heldKeys().isEmpty());
 }

 void pointDwellToleratesJitterAcrossGridBoundaries(){
  PointDwell dwell;
  QVERIFY(!dwell.update({39,39},0,250,20));
  for(int t=50;t<250;t+=50)QVERIFY(!dwell.update({41,41},t,250,20));
  QVERIFY(dwell.update({39,41},250,250,20));
  QVERIFY(!dwell.update({41,39},300,250,20));
  QVERIFY(!dwell.update({80,80},350,250,20));QCOMPARE(dwell.progress(350,250),0.);
  QVERIFY(!dwell.update({80,80},600,250,20)); // Tracking gap restarts dwell.
  QCOMPARE(dwell.progress(600,250),0.);
  dwell.reset();QCOMPARE(dwell.progress(650,250),0.);
 }
 void exponentialSmoothingFormulaAndSaccades(){
  GazeSmoothing filter;
  QCOMPARE(filter.updateExponential({0.4,0.4},0.25,0.1),QPointF(0.4,0.4));
  const auto smoothed=filter.updateExponential({0.44,0.48},0.25,0.1);
  QVERIFY(QLineF(smoothed,QPointF(0.41,0.42)).length()<1e-12);
  QCOMPARE(filter.updateExponential({0.8,0.8},0.25,0.1),QPointF(0.8,0.8));
  filter.reset();
  QCOMPARE(filter.updateExponential({0.2,0.3},0.25,0.1),QPointF(0.2,0.3));
 }
 void smoothingMatchesOptikeyReference(){
  for(bool transitions:{false,true}){
   GazeSmoothing filter;QCOMPARE(filter.update({100,100},2,transitions),QPointF(100,100));
   const QList<QPointF> input{{110,105},{120,98},{600,300}};
   const QList<QPointF> expected=transitions?QList<QPointF>{{100.0001213459688,100.0000606729844},{100.00081746616452,100.00004553567845},{328.9822048163774,190.39717557114443}}:
    QList<QPointF>{{100.00074915421219,100.00037457710609},{100.0045631146371,99.99999309534304},{599.9999999981073,299.99999999924296}};
   for(int i=0;i<input.size();++i)QVERIFY(QLineF(filter.update(input[i],2,transitions),expected[i]).length()<1e-9);
  }
 }
 void smoothingOffResetStrengthAndLargeJumps(){
  GazeSmoothing filter;QCOMPARE(filter.update({20,30},0,true),QPointF(20,30));
  filter.update({100,100},2,true);filter.update({110,120},2,true);filter.reset();
  QCOMPARE(filter.update({900,800},2,true),QPointF(900,800));
  double previousMovement=1e9;
  for(int level=1;level<=4;++level){
   filter.reset();filter.update({100,100},level,false);
   const auto point=filter.update({120,100},level,false);
   QVERIFY(point.x()>100&&point.x()<120);QVERIFY(point.x()-100<previousMovement);previousMovement=point.x()-100;
   const auto jump=filter.update({100000,100000},level,false);
   QVERIFY(std::isfinite(jump.x())&&std::isfinite(jump.y()));
  }
  filter.update({std::numeric_limits<double>::quiet_NaN(),0},2,true);
  QCOMPARE(filter.update({10,20},2,true),QPointF(10,20));
 }
 void gazeIndicatorProfileValidation(){
  auto v=ProfileStore::defaults();
  for(const auto &key:{"gazeIndicatorSize","gazeIndicatorOpacity","gazeIndicatorColor","gazeIndicatorMorph"})v.remove(key);
  QVERIFY(ProfileStore::valid(v));
  for(const auto &key:{"gazeIndicatorSize","gazeIndicatorOpacity"}){
   for(double invalid:{-1.0,0.0,121.0,12.5}){auto bad=v;bad[key]=invalid;QVERIFY(!ProfileStore::valid(bad));}
  }
  v["gazeIndicatorColor"]="#Ab12EF";QVERIFY(ProfileStore::valid(v));
  for(const auto &color:{"red","#123","#00112233","#xyzxyz"}){v["gazeIndicatorColor"]=color;QVERIFY(!ProfileStore::valid(v));}
  v["gazeIndicatorColor"]="#00bfff";v["gazeIndicatorMorph"]=1;QVERIFY(!ProfileStore::valid(v));
 }
 void smoothingProfileValidationAndLegacyCompatibility(){
  auto values=ProfileStore::defaults();QVERIFY(ProfileStore::valid(values));
  values.remove("gazeSmoothingLevel");values.remove("smoothWhenChangingGazeTarget");QVERIFY(ProfileStore::valid(values));
  for(const QJsonValue invalid:{QJsonValue(-1),QJsonValue(5),QJsonValue(1.5),QJsonValue("2")}){
   values["gazeSmoothingLevel"]=invalid;QVERIFY(!ProfileStore::valid(values));
  }
  values["gazeSmoothingLevel"]=4;values["smoothWhenChangingGazeTarget"]=true;QVERIFY(ProfileStore::valid(values));
  values["smoothWhenChangingGazeTarget"]=1;QVERIFY(!ProfileStore::valid(values));
 }

 void inactiveNativeContextUsesPhysicalFallback(){
  ActionRunner runner;QStringList events;runner.canCommitText=[] {return false;};runner.sendText=[](QString){return false;};
  runner.sendKey=[&](QString key,bool down){events<<key+(down?"+":"-");return true;};QSignalSpy done(&runner,&ActionRunner::finished);
  QVERIFY(runner.start({cmd("Text","a")}));QTRY_COMPARE(done.count(),1);QCOMPARE(events,QStringList({"A+","A-"}));
  events.clear();QVERIFY(!runner.start({cmd("Text",QString::fromUtf8("é"))}));QVERIFY(events.isEmpty());
 }
 void nativeTextCommitsUnicodeAndPreservesControlKeyOrder(){
  ActionRunner runner;QStringList events;
  runner.sendText=[&](QString text){events<<"text:"+text;return true;};
  runner.sendKey=[&](QString key,bool down){events<<key+(down?"+":"-");return true;};
  QSignalSpy done(&runner,&ActionRunner::finished);
  QVERIFY(runner.start({cmd("Text",QString::fromUtf8("é€😀\tß\n"))}));QTRY_COMPARE(done.count(),1);
  QCOMPARE(events,QStringList({QString::fromUtf8("text:é€😀"),"TAB+","TAB-",QString::fromUtf8("text:ß"),"RETURN+","RETURN-"}));
 }
 void nativeTextDoesNotReplaceShortcutKeys(){
  ActionRunner runner;QStringList events;
  runner.sendText=[&](QString text){events<<"text:"+text;return true;};
  runner.sendKey=[&](QString key,bool down){events<<key+(down?"+":"-");return true;};
  QSignalSpy done(&runner,&ActionRunner::finished);
  QVERIFY(runner.start({cmd("KeyDown","LCTRL"),cmd("Text","c"),cmd("KeyUp","LCTRL")}));QTRY_COMPARE(done.count(),1);
  QCOMPARE(events,QStringList({"LCONTROL+","C+","C-","LCONTROL-"}));
  events.clear();QVERIFY(!runner.start({cmd("Text","first"),cmd("KeyDown","LCTRL"),cmd("Text",QString::fromUtf8("é"))}));QVERIFY(events.isEmpty());
 }
 void nativePhysicalPreflightRejectsBeforeAnyText(){
  ActionRunner runner;QStringList events;runner.sendText=[&](QString text){events<<text;return true;};runner.supportsPhysicalKey=[](QString){return false;};
  QVERIFY(!runner.start({cmd("Text","first"),cmd("KeyDown","W")}));QVERIFY(events.isEmpty());
  QVERIFY(!runner.start({cmd("Action","LeftShift")}));
 }
 void desktopPermissionTokensArePrivateAndSingleUse(){
  QTemporaryDir dir;const auto path=dir.filePath("nested/permission.json");PortalPermissionStore store(path);QString error;
  QVERIFY(store.save("private-test-token",error));QVERIFY(error.isEmpty());
#ifndef GAZE_GRID_WINDOWS
  // Portal permission persistence is Linux-only; these bits do not describe Windows ACLs.
  const auto permissions=QFile::permissions(path);QVERIFY(!(permissions&(QFileDevice::ReadGroup|QFileDevice::WriteGroup|QFileDevice::ReadOther|QFileDevice::WriteOther)));
#endif
  QCOMPARE(store.take(error),QString("private-test-token"));QVERIFY(error.isEmpty());QVERIFY(!QFile::exists(path));
  QVERIFY(store.take(error).isEmpty());QVERIFY(error.isEmpty());
  QVERIFY(store.save("replacement-token",error));QVERIFY(store.forget(error));QVERIFY(!QFile::exists(path));
 }
 void corruptPermissionTokenCannotBeReused(){
  QTemporaryDir dir;const auto path=dir.filePath("permission.json");QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));file.write("{broken");file.close();
  PortalPermissionStore store(path);QString error;QVERIFY(store.take(error).isEmpty());QVERIFY(!error.isEmpty());QVERIFY(!QFile::exists(path));
  QVERIFY(!store.save("",error));QVERIFY(!error.isEmpty());
 }
 void mappedAndLiteralShortcutsPreserveOrderedRelease(){
  const QVariantList original{cmd("KeyDown","LWIN"),cmd("Text","c"),cmd("KeyUp","LWIN")};
  const QVariantList effective{cmd("KeyDown","LCONTROL"),cmd("Text","c"),cmd("KeyUp","LCONTROL")};
  const QJsonObject rule{{"match",ActionRunner::signature(original)},{"description","Test copy override"},{"commands",QJsonArray::fromVariantList(effective)}};
  QString error;const auto profile=ShortcutMappings::parse(QJsonDocument(QJsonObject{{"version",1},{"platform","linux"},{"overrides",QJsonArray{rule}}}).toJson(),error);
  QVERIFY(error.isEmpty());
  for(bool literal:{false,true}){
   auto result=ShortcutMappings::resolve(original,profile,literal);QVERIFY(result.allowed);
   QCOMPARE(result.commands,literal?original:effective);
   if(!literal){QVERIFY(result.message.contains("original:"));QVERIFY(result.message.contains("effective:"));}
   ActionRunner runner;QStringList events;runner.sendKey=[&](QString key,bool down){events<<key+(down?"+":"-");return true;};QSignalSpy done(&runner,&ActionRunner::finished);
   QVERIFY(runner.start(result.commands));QTRY_COMPARE(done.count(),1);
   const auto modifier=literal?QString("LWIN"):QString("LCONTROL");QCOMPARE(events,QStringList({modifier+"+","C+","C-",modifier+"-"}));QVERIFY(runner.heldKeys().isEmpty());
  }
 }
 void mappingPresetIsExplicitAndUnknownShortcutsStayBlocked(){
  QFile file(":/resources/linux-mappings.json");QVERIFY(file.open(QIODevice::ReadOnly));QString error;auto profile=ShortcutMappings::parse(file.readAll(),error);QVERIFY(error.isEmpty());
  auto preset=profile["overrides"].toArray().first().toObject();auto commands=preset["commands"].toArray().toVariantList();
  auto result=ShortcutMappings::resolve(commands,profile,false);QVERIFY(result.allowed);QVERIFY(result.message.contains("KDE:"));
  const QVariantList unknown{cmd("KeyDown","LWIN"),cmd("Text","c"),cmd("KeyUp","LWIN")};
  QVERIFY(!ShortcutMappings::resolve(unknown,profile,false).allowed);QVERIFY(ShortcutMappings::resolve(unknown,profile,true).allowed);
  const QVariantList game{cmd("KeyToggle","W")};QCOMPARE(ShortcutMappings::resolve(game,profile,false).commands,game);
 }
 void invalidMappingProfilesAreRejected(){
  QString error;
  QVERIFY(ShortcutMappings::parse("{bad",error).isEmpty());QVERIFY(!error.isEmpty());
  QVERIFY(ShortcutMappings::parse(R"({"version":1.5,"platform":"linux","overrides":[]})",error).isEmpty());
  const QJsonObject rule{{"match","test"},{"description","Test"},{"commands",QJsonArray{QJsonObject{{"tag","Text"},{"text","a"}}}}};
  auto json=QJsonDocument(QJsonObject{{"version",1},{"platform","linux"},{"overrides",QJsonArray{rule,rule}}}).toJson();
  QVERIFY(ShortcutMappings::parse(json,error).isEmpty());QVERIFY(!error.isEmpty());
 }
 void predictionUnderstandsWhitespaceAndPunctuation(){
  QCOMPARE(WordPrediction::prefix("one\ttw"),QString("tw"));
  QCOMPARE(WordPrediction::prefix("one,tw"),QString("tw"));
  QCOMPARE(WordPrediction::prefix("one\ntw"),QString("tw"));
  QCOMPARE(WordPrediction::prefix("one "),QString());
  QCOMPARE(WordPrediction::prefix("123abc"),QString());
  QCOMPARE(WordPrediction::completedWord("Hello!"),QString("hello"));
  QCOMPARE(WordPrediction::completedWord("don't"),QString("don't"));
  QCOMPARE(WordPrediction::completedWord("123"),QString());
  QCOMPARE(WordPrediction::completedWord("hello "),QString());
  QCOMPARE(WordPrediction::completedWord("hello !"),QString());
 }
 void typedWordModeProfileValidation(){
  auto values=ProfileStore::defaults();QCOMPARE(values["typedWordSuggestionMode"].toString(),QString("fallback"));
  values.remove("typedWordSuggestionMode");QVERIFY(ProfileStore::valid(values));
  for(const QString mode:{"always","fallback","never"}){values["typedWordSuggestionMode"]=mode;QVERIFY(ProfileStore::valid(values));}
  for(const QJsonValue invalid:{QJsonValue("invalid"),QJsonValue(1),QJsonValue(false)}){values["typedWordSuggestionMode"]=invalid;QVERIFY(!ProfileStore::valid(values));}
 }
 void predictionTypedWordModes(){
  const QStringList words{"hello","help","helmet"};
  for(const QString mode:{"always","fallback","never"}){
   const auto unknown=WordPrediction::suggest("say Zorblax",words,{},8,true,false,mode);
   QCOMPARE(unknown,mode=="never"?QStringList{}:QStringList{"Zorblax"});
   const auto matched=WordPrediction::suggest("He",words,{},3,true,false,mode);
   QCOMPARE(matched.size(),3);
   QCOMPARE(matched.contains("He"),mode=="always");
   if(mode=="always")QCOMPARE(matched.first(),QString("He"));
   QVERIFY(matched.contains("Hello"));
   QCOMPARE(WordPrediction::suggest("He",words,{},1,true,false,mode).size(),1);
   QVERIFY(WordPrediction::suggest("He",words,{},0,true,false,mode).isEmpty());
   QCOMPARE(WordPrediction::suggest("hello",words,{},8,true,false,mode).count("hello"),mode=="never"?0:1);
   QCOMPARE(WordPrediction::suggest("say ",words,{},8,true,false,mode),WordPrediction::suggest("say ",words,{}));
  }
 }
 void predictionRanksFrequencyAndPreservesCase(){
  const QStringList words{"Help","hello","HELLO","helmet","héllo"};
  const auto ranked=WordPrediction::suggest("he",words,{});
  QCOMPARE(ranked.size(),3);QVERIFY(ranked.indexOf("help")<ranked.indexOf("helmet"));
  QCOMPARE(WordPrediction::suggest("HE",words,{}).first(),ranked.first().toUpper());
  auto title=ranked.first();title[0]=title[0].toUpper();
  QCOMPARE(WordPrediction::suggest("He",words,{}).first(),title);
  QCOMPARE(WordPrediction::suggest("he",words,{},1).size(),1);
  QVERIFY(WordPrediction::suggest("he",words,{},0).isEmpty());
  QVERIFY(WordPrediction::suggest("123he",words,{}).isEmpty());
  QCOMPARE(WordPrediction::suggest("hé",words,{}).first(),QString("héllo"));
  QVERIFY(WordPrediction::suggest("",words,{}).isEmpty());
  QCOMPARE(WordPrediction::suggest("",words,{},8,true,true),WordPrediction::suggest(" ",words,{}));
 }
 void predictionOptionsValidateAndChangeRanking(){
  auto defaults=ProfileStore::defaults();
  QCOMPARE(defaults["predictionMode"].toString(),QString("context"));QVERIFY(defaults["predictNextWord"].toBool());
  auto legacy=defaults;legacy.remove("predictionMode");legacy.remove("predictNextWord");QVERIFY(ProfileStore::valid(legacy));
  for(const QString mode:{"context","frequency","off"}){auto values=defaults;values["predictionMode"]=mode;QVERIFY(ProfileStore::valid(values));}
  for(const QJsonValue invalid:{QJsonValue("unknown"),QJsonValue(1),QJsonValue(false)}){auto values=defaults;values["predictionMode"]=invalid;QVERIFY(!ProfileStore::valid(values));}
  auto invalidStarters=defaults;invalidStarters["suggestSentenceStarters"]=1;QVERIFY(!ProfileStore::valid(invalidStarters));
  auto invalid=defaults;invalid["predictNextWord"]=1;QVERIFY(!ProfileStore::valid(invalid));
  const QStringList words{"you","the","light","table"};
  QCOMPARE(WordPrediction::suggest("turn on the ",words,{},4,true).first(),QString("light"));
  QCOMPARE(WordPrediction::suggest("turn on the ",words,{},4,false),WordPrediction::suggest("thank ",words,{},4,false));
 }
 void predictionWorksBeforeLearning(){
  QFile file(":/resources/EnglishUS.dic");QVERIFY(file.open(QIODevice::ReadOnly));
  auto words=QString::fromUtf8(file.readAll()).remove(QChar(0xfeff)).split('\n',Qt::SkipEmptyParts);
  for(auto &word:words)word=word.trimmed();
  QCOMPARE(WordPrediction::suggest("th",words,{}).first(),QString("the"));
  QCOMPARE(WordPrediction::suggest("thank ",words,{}).first(),QString("you"));
  QCOMPARE(WordPrediction::suggest("please turn on the ",words,{}).first(),QString("light"));
  QCOMPARE(WordPrediction::suggest("thank ",words,{},1000).mid(0,8),WordPrediction::suggest("thank ",words,{}));
  QVERIFY(WordPrediction::suggest("I would like to dr",words,{},3).contains("drink"));
  QVERIFY(WordPrediction::suggest("I want to eat br",words,{},3).contains("bread"));
  QVERIFY(WordPrediction::suggest("thank y",words,{},3).contains("you"));
  QVERIFY(!WordPrediction::suggest("thank you",words,{},3).contains("you"));
  // A single unusual selection, or an enormous old count, cannot replace the
  // clearly more likely default. Personal vocabulary remains completable.
  QCOMPARE(WordPrediction::suggest("th",words,{{"thy",1000000}}).first(),QString("the"));
  QCOMPARE(WordPrediction::suggest("zorb",words,{{"zorbella",1}}).first(),QString("zorbella"));
  const auto fresh=WordPrediction::suggest("Finished. th",words,{});
  QCOMPARE(fresh,WordPrediction::suggest("th",words,{}));
  const auto next=WordPrediction::suggest("Finished. ",words,{});
  QVERIFY(!next.isEmpty());QVERIFY(next.first().front().isUpper());
  QVERIFY(WordPrediction::suggest("Finished.",words,{}).isEmpty());
 }
 void multilingualModelsAndUnicode(){
  QCOMPARE(WordPrediction::prefix(QString::fromUtf8("\xc3\xa9")),QString::fromUtf8("\xc3\xa9"));
  for(const auto &language:WordPrediction::languages()){
   const auto words=WordPrediction::dictionary(language);QVERIFY2(words.size()>20,qPrintable(language));
   const auto starters=WordPrediction::suggest("",words,{},8,true,true,"never",language);
   QVERIFY2(!starters.isEmpty(),qPrintable(language));
   if(WordPrediction::characterLanguage(language)){
    QVERIFY(WordPrediction::prefix(starters.first(),language).isEmpty());
    QVERIFY(!WordPrediction::suggest(starters.first(),words,{},8,true,true,"never",language).isEmpty());
   }else{
    const auto word=*std::find_if(words.cbegin(),words.cend(),[](const QString &value){return value.size()>3;});
    const auto results=WordPrediction::suggest(word.left(2),words,{},1000,false,false,"never",language);
    QVERIFY2(results.contains(word)||results.contains(word.left(2)+word.mid(2).toUpper()),qPrintable(language+":"+word));
   }
  }
  auto values=ProfileStore::defaults();
  for(const auto &language:WordPrediction::languages()){values["predictionLanguage"]=language;QVERIFY(ProfileStore::valid(values));}
  values["predictionLanguage"]="../invalid";QVERIFY(!ProfileStore::valid(values));
 }
 void predictionPreservesFullContextRankings_data(){
  QTest::addColumn<QString>("text");QTest::addColumn<QStringList>("expected");
  // Full rankings recorded before compacting the model's context storage.
  QTest::newRow("one-word")<<QString("thank ")<<QStringList{"you","god","goodness","heavens","me","him","her","the"};
  QTest::newRow("other-side")<<QString("on the other ")<<QStringList{"side","hand","day","one","way","is","room","half"};
  QTest::newRow("same-time")<<QString("at the same ")<<QStringList{"time","place","hotel","table","speed","thing","as","way"};
  QTest::newRow("as-soon-as")<<QString("as soon as ")<<QStringList{"possible","he","you","we","the","she","they","it"};
  QTest::newRow("cup-of")<<QString("a cup of ")<<QStringList{"coffee","tea","the","my","his","this","your","you"};
  QTest::newRow("would-like")<<QString("I would like ")<<QStringList{"to","some","you","me","them","that","this","the"};
  QTest::newRow("would-you-mind")<<QString("Would you mind ")<<QStringList{"telling","taking","if","helping","turning","leaving","my","doing"};
  QTest::newRow("weather")<<QString("The weather is ")<<QStringList{"good","very","nice","so","getting","bad","cold","fine"};
  QTest::newRow("completion")<<QString("I would like to dr")<<QStringList{"drink","drive","draw","dress","drop","dry","drinking","dream"};
 }
 void predictionPreservesFullContextRankings(){
  QFETCH(QString,text);QFETCH(QStringList,expected);
  QFile file(":/resources/EnglishUS.dic");QVERIFY(file.open(QIODevice::ReadOnly));
  auto words=QString::fromUtf8(file.readAll()).remove(QChar(0xfeff)).split('\n',Qt::SkipEmptyParts);
  for(auto &word:words)word=word.trimmed();
  QCOMPARE(WordPrediction::suggest(text,words,{}),expected);
 }
 void progressiveZoomFollowsCurrentPoint(){
  const QSize image(1000,800);const QRect original(0,0,1000,800);
  const auto left=MagnifierGeometry::zoomAt(original,image,{.2,.3},2);
  QCOMPARE(left,QRect(100,120,500,400));
  QCOMPARE(MagnifierGeometry::map(left,image,{.2,.3}),QPointF(.2,.3));
  const auto right=MagnifierGeometry::zoomAt(left,image,{.8,.7},4);
  QCOMPARE(right,QRect(300,260,250,200));
  QCOMPARE(MagnifierGeometry::map(right,image,{.8,.7}),MagnifierGeometry::map(left,image,{.8,.7}));
  QVERIFY(QRect(QPoint(0,0),image).contains(MagnifierGeometry::zoomAt(right,image,{1,1},8)));
 }
 void magnifierCoordinatesStayInsideCapturedDesktop(){
  const QSize image(1920,1080);
  for(int zoom=2;zoom<=8;++zoom)for(const auto &point:QList<QPointF>{{0,0},{1,1},{.5,.5},{.01,.99}}){
   auto crop=MagnifierGeometry::crop(image,point,zoom);QVERIFY(QRect(QPoint(0,0),image).contains(crop));
   QCOMPARE(crop.size(),QSize(image.width()/zoom,image.height()/zoom));
   const auto top=MagnifierGeometry::map(crop,image,{0,0}),bottom=MagnifierGeometry::map(crop,image,{1,1});
   QVERIFY(MagnifierGeometry::validPoint(top));QVERIFY(MagnifierGeometry::validPoint(bottom));
   QVERIFY(qAbs(bottom.x()-top.x()-double(crop.width())/image.width())<1e-12);QVERIFY(qAbs(bottom.y()-top.y()-double(crop.height())/image.height())<1e-12);
  }
  QVERIFY(!MagnifierGeometry::crop(image,{qQNaN(),.5}).isValid());
  QVERIFY(!MagnifierGeometry::crop(image,{.5,.5},0).isValid());
  QVERIFY(!MagnifierGeometry::crop(image,{.5,.5},9).isValid());
  QCOMPARE(MagnifierGeometry::map({},image,{.5,.5}),QPointF(-1,-1));
 }
 void fractionalZoomKeepsCoordinatesInsideDesktop(){
  const QSize image(1920,1080);
  for(double zoom:{1.,1.5,2.75,4.,8.})for(QPointF point:QList<QPointF>{{0,0},{.5,.5},{1,1}}){
   const auto crop=MagnifierGeometry::crop(image,point,zoom);
   QVERIFY(QRect(QPoint(),image).contains(crop));
   QCOMPARE(crop.size(),QSize(int(image.width()/zoom),int(image.height()/zoom)));
   QVERIFY(MagnifierGeometry::validPoint(MagnifierGeometry::map(crop,image,{.5,.5})));
  }
  QVERIFY(!MagnifierGeometry::crop(image,{.5,.5},qQNaN()).isValid());
 }

 void lookScrollDeadzoneDirectionAndTimeAreDeterministic(){
  LookScroll scroll;scroll.start({500,500},0);
  QCOMPARE(scroll.update({520,530},25,true),QPointF());
  const auto delta=scroll.update({600,400},50,true);
  QVERIFY(delta.x()>0);QVERIFY(delta.y()<0);QCOMPARE(delta.x(),-delta.y());
  QCOMPARE(scroll.update({800,800},75,false),QPointF()); // Keyboard/controls never scroll.
  QCOMPARE(scroll.update({800,800},400,true),QPointF());QVERIFY(scroll.active());
  scroll.start({500,500},500);QCOMPARE(scroll.update({800,800},499,true),QPointF());QVERIFY(!scroll.active());
 }
 void lookScrollSettingsControlSpeedDeadzoneAndAxes(){
  LookScroll normal,custom;normal.start({500,500},0);custom.start({500,500},0);
  const auto baseline=normal.update({600,400},25,true);
  custom.configure(200,50,0);QCOMPARE(custom.update({600,400},25,true),baseline*2);
  custom.configure(100,150,0);QCOMPARE(custom.update({600,400},50,true),QPointF());
  custom.configure(100,50,1);QCOMPARE(custom.update({600,400},75,true),QPointF(0,baseline.y()));
  custom.configure(100,50,2);QCOMPARE(custom.update({600,400},100,true),QPointF(baseline.x(),0));
 }
 void lookScrollSelectsAnchorAndTogglesOff(){
  PointerActions p;p.setDisplaySize({1000,1000});QList<QPointF> moves,scrolls;
  p.move=[&](double x,double y){moves<<QPointF(x,y);return true;};
  p.scrollAxes=[&](double x,double y){scrolls<<QPointF(x,y);return true;};QSignalSpy done(&p,&PointerActions::completed);
  QVERIFY(p.start("LookToScrollActive"));QVERIFY(p.targeting());p.select({.5,.5});
  QTRY_COMPARE(done.count(),1);QVERIFY(p.lookScrolling());QCOMPARE(moves.last(),QPointF(.5,.5));
  QCOMPARE(p.scrollCentre(),QPointF(500,500));
  QTest::qWait(25);QVERIFY(p.follow({.8,.8},true));QCOMPARE(scrolls.size(),1);QVERIFY(scrolls[0].x()>0);QVERIFY(scrolls[0].y()>0);
  QVERIFY(p.start("LookToScrollActive"));QVERIFY(!p.targeting());QTRY_COMPARE(done.count(),2);QVERIFY(!p.lookScrolling());
 }
 void lookScrollOffscreenPolicy(){
  for(bool clamp:{false,true})for(bool cancel:{false,true}){
   PointerActions p;p.setDisplaySize({1000,1000});p.setScrollGazePolicy(clamp,cancel);
   p.move=[](double,double){return true;};
   QList<QPointF> deltas;p.scrollAxes=[&](double x,double y){deltas<<QPointF(x,y);return true;};
   QVERIFY(p.start("LookToScrollActive"));p.select({.5,.5});QTRY_VERIFY(p.lookScrolling());
   QTest::qWait(25);QVERIFY(p.follow({1.2,.8})!=cancel);QCOMPARE(p.lookScrolling(),!cancel);
   if(!cancel){
    QVERIFY(!deltas.isEmpty());
    const auto delta=deltas.last();
    const double expectedRatio=(.1+(clamp?450:650)*.1)/(.1+250*.1);
    QVERIFY(qAbs(delta.x()/delta.y()-expectedRatio)<.000001);
   }
  }
  PointerActions p;p.setDisplaySize({1000,1000});p.move=[](double,double){return true;};
  p.scrollAxes=[](double,double){return true;};
  QVERIFY(p.start("LookToScrollActive"));p.select({.5,.5});QTRY_VERIFY(p.lookScrolling());
  QVERIFY(p.follow({-.1,1.1}));QVERIFY(p.lookScrolling());
 }
 void lookScrollPausesThroughTrackingLossAndResumes(){
  PointerActions p;p.setDisplaySize({1000,1000});QList<QPointF> scrolls;
  p.move=[](double,double){return true;};
  p.scrollAxes=[&](double x,double y){scrolls<<QPointF(x,y);return true;};
  QVERIFY(p.start("LookToScrollActive"));p.select({.5,.5});QTRY_VERIFY(p.lookScrolling());
  QTest::qWait(25);QVERIFY(p.follow({.5,.8},true));QCOMPARE(scrolls.size(),1);
  p.trackingLost();QTest::qWait(250);QVERIFY(p.lookScrolling());QCOMPARE(scrolls.size(),1);
  QVERIFY(p.follow({.5,.8},true));QCOMPARE(scrolls.size(),1); // No accumulated scroll after a blink.
  QTest::qWait(25);QVERIFY(p.follow({.5,.8},true));QCOMPARE(scrolls.size(),2);
  p.trackingLost();
  for(int i=0;i<4;++i){QTest::qWait(200);p.trackingLost();}
  QVERIFY(p.lookScrolling());
  QVERIFY(!p.follow({qQNaN(),0},true));QVERIFY(p.lookScrolling());QCOMPARE(scrolls.size(),2);
  QVERIFY(p.follow({.5,.8},true));QCOMPARE(scrolls.size(),2);
  QTest::qWait(25);QVERIFY(p.follow({.5,.8},true));QCOMPARE(scrolls.size(),3);
  p.cancel();QVERIFY(!p.lookScrolling());
  QVERIFY(p.start("MouseMoveAndLeftClick"));p.select({.5,.5});p.trackingLost();
  QVERIFY(!p.busy()); // Other pointer actions still cancel immediately.
 }
 void magneticCursorTracksOnlyWhenEnabledAndIdle(){
  PointerActions p;QList<QPointF> moves;p.move=[&](double x,double y){moves<<QPointF(x,y);return true;};
  QVERIFY(!p.follow({.2,.3}));QSignalSpy done(&p,&PointerActions::completed);
  QVERIFY(p.start("MouseMagneticCursor"));QTRY_COMPARE(done.count(),1);QVERIFY(p.magnetic());
  QVERIFY(p.follow({.2,.3}));QCOMPARE(moves.last(),QPointF(.2,.3));
  QVERIFY(p.start("MouseMoveTo"));QVERIFY(!p.follow({.8,.9}));QCOMPARE(moves.size(),1);p.cancel();
  QVERIFY(!p.magnetic());QVERIFY(!p.follow({.4,.5}));
  QVERIFY(p.start("MouseMagneticCursor"));QTRY_COMPARE(done.count(),2);
  QVERIFY(!p.follow({qQNaN(),0}));QVERIFY(p.magnetic());QCOMPARE(moves.size(),1);
  QVERIFY(p.follow({.4,.5}));QCOMPARE(moves.last(),QPointF(.4,.5));
 }
 void magneticCursorClampsOffscreenGaze(){
  for(bool relative:{false,true}){
   PointerActions p;p.setMagneticRelative(relative);QList<QPointF> moves;
   p.move=[&](double x,double y){moves<<QPointF(x,y);return true;};
   p.moveRelativeTo=p.move;
   QSignalSpy done(&p,&PointerActions::completed);
   QVERIFY(p.start("MouseMagneticCursor"));QTRY_COMPARE(done.count(),1);
   for(const QPointF point:{QPointF(-.01,.5),QPointF(1.01,.5),QPointF(.5,-.01),QPointF(.5,1.01),QPointF(-.1,1.1)}){
    QVERIFY(p.follow(point));QVERIFY(p.magnetic());
    QCOMPARE(moves.last(),QPointF(qBound(0.,point.x(),1.),qBound(0.,point.y(),1.)));
   }
   QVERIFY(p.follow({.4,.6}));QCOMPARE(moves.last(),QPointF(.4,.6));
   QVERIFY(!p.follow({qQNaN(),0}));QVERIFY(p.magnetic());
   const auto moveCount=moves.size();
   p.trackingLost();p.trackingLost();QVERIFY(p.magnetic());QCOMPARE(moves.size(),moveCount);
   QVERIFY(p.follow({.3,.7}));QCOMPARE(moves.last(),QPointF(.3,.7));
   p.cancel();QVERIFY(!p.magnetic());
  }
 }
 void magneticRelativeTargetsGazeImmediatelyAndWhileStationary(){
  PointerActions p;p.setMagneticRelative(true);
  QList<QPointF> targets;int absoluteMoves=0,rawMoves=0;
  p.move=[&](double,double){++absoluteMoves;return true;};
  p.moveRelative=[&](double,double){++rawMoves;return true;};
  p.moveRelativeTo=[&](double x,double y){targets<<QPointF(x,y);return true;};
  QSignalSpy done(&p,&PointerActions::completed);
  QVERIFY(p.start("MouseMagneticCursor"));QTRY_COMPARE(done.count(),1);
  QVERIFY(p.follow({.25,.25}));QCOMPARE(targets.last(),QPointF(.25,.25));
  // Re-read cursor feedback even when gaze is unchanged or the user moved the mouse.
  QVERIFY(p.follow({.25,.25}));QCOMPARE(targets.size(),2);
  QCOMPARE(absoluteMoves,0);QCOMPARE(rawMoves,0);
  p.setMagneticRelative(false);QVERIFY(p.follow({.75,.75}));QCOMPARE(absoluteMoves,1);
  p.setMagneticRelative(true);QVERIFY(p.follow({.1,.1}));QCOMPARE(targets.last(),QPointF(.1,.1));
  p.trackingLost();QVERIFY(p.magnetic());
  QVERIFY(p.follow({.9,.9}));QCOMPARE(targets.last(),QPointF(.9,.9));
  p.moveRelativeTo=[](double,double){return false;};
  QVERIFY(!p.follow({.5,.5}));QVERIFY(!p.magnetic());
 }
 void relativeCursorFeedbackConvergesDespiteOffsetsAndPointerSpeed(){
  for(double speed:{0.25,1.0,2.0,4.0}){
   RelativePointer feedback;
   QPointF cursor(-800,250),target(1000,600);
   auto advance=[&]{
    const QPointF observed(std::round(cursor.x()),std::round(cursor.y()));
    cursor+=feedback.step(observed,target)*speed;
   };
   // First gaze must move toward the target, including from another monitor.
   advance();QVERIFY(cursor.x()>-800);QVERIFY(cursor.y()>250);
   for(int i=0;i<60;++i)advance();
   QVERIFY2(QLineF(cursor,target).length()<=3,qPrintable(QString::number(speed)));
   // An independent physical-mouse move must not leave a permanent gaze offset.
   cursor+=QPointF(140,-80);
   for(int i=0;i<60;++i)advance();
   QVERIFY(QLineF(cursor,target).length()<=3);
   target={0,0};
   for(int i=0;i<60;++i)advance();
   QVERIFY(QLineF(cursor,target).length()<=3);
  }
 }
 void actionCoverageListsMissingPackFeatures(){
  auto report=KeyboardPack::audit(PACK_PATH);QCOMPARE(report["layouts"].toList().size(),32);
  QVERIFY(report["unsupportedActions"].toStringList().isEmpty());
  QVERIFY(!report["behavioralParityVerified"].toBool());
 }
 void unsupportedActionsAreRejectedBeforeOutput(){
  ActionRunner runner;int output=0;runner.sendKey=[&](QString,bool){++output;return true;};
  runner.supportsAction=[](QString){return false;};QSignalSpy errors(&runner,&ActionRunner::failed);
  QVERIFY(!runner.start({cmd("KeyDown","LSHIFT"),cmd("Text","a"),cmd("Action","Missing")}));
  QCOMPARE(output,0);QCOMPARE(errors.count(),1);QVERIFY(!runner.busy());
 }
 void arrowActionsTapInsteadOfLocking(){
  ActionRunner runner;QStringList events;runner.sendKey=[&](QString key,bool down){events<<key+(down?"+":"-");return true;};
  QSignalSpy done(&runner,&ActionRunner::finished);QVERIFY(runner.start({cmd("Action","Left"),cmd("Action","Right")}));
  QTRY_COMPARE(done.count(),1);QCOMPARE(events,QStringList({"LEFT+","LEFT-","RIGHT+","RIGHT-"}));QVERIFY(runner.heldKeys().isEmpty());
 }
 void directionActionsMoveRelativeAndRepeat(){
  PointerActions p;QList<QPointF> moves;p.moveRelative=[&](double x,double y){moves<<QPointF(x,y);return true;};
  QSignalSpy done(&p,&PointerActions::completed);
  const QStringList directions{"Top","Bottom","Left","Right"};int count=0;
  for(const auto &direction:directions){QVERIFY(p.start("MouseMoveTo"+direction));++count;QTRY_COMPARE(done.count(),count);}
  QVERIFY(p.start("RepeatLastMouseAction"));QTRY_COMPARE(done.count(),5);
  QCOMPARE(moves,QList<QPointF>({{0,-25},{0,25},{-25,0},{25,0},{25,0}}));
 }
 void dragChoosesTwoPointsAndReleases(){
  PointerActions p;QStringList events;QList<QPointF> moves;
  p.move=[&](double x,double y){moves<<QPointF(x,y);events<<"move";return true;};
  p.button=[&](int code,bool down){events<<QString::number(code)+(down?"+":"-");return true;};
  QSignalSpy done(&p,&PointerActions::completed);
  QVERIFY(p.start("MouseDrag"));p.select({.1,.2});QVERIFY(p.targeting());QVERIFY(events.isEmpty());
  p.select({.8,.9});QVERIFY(!p.targeting());QTRY_COMPARE(done.count(),1);QVERIFY(done[0][0].toBool());
  QCOMPARE(moves.first(),QPointF(.1,.2));QVERIFY(QLineF(moves.last(),QPointF(.8,.9)).length()<.00001);
  QCOMPARE(events[1],QString("272+"));QCOMPARE(events.last(),QString("272-"));QVERIFY(p.heldButtons().isEmpty());
  const int count=moves.size();QVERIFY(p.start("RepeatLastMouseAction"));QVERIFY(!p.targeting());QTRY_COMPARE(done.count(),2);
  QCOMPARE(moves[count],QPointF(.1,.2));QVERIFY(QLineF(moves.last(),QPointF(.8,.9)).length()<.00001);
 }
 void cancellingDragStopsMovementAndReleases(){
  PointerActions p;int moves=0,releases=0;bool held=false;
  p.move=[&](double,double){++moves;return true;};p.button=[&](int,bool down){held=down;return true;};
  p.releaseAll=[&]{held=false;++releases;};QSignalSpy done(&p,&PointerActions::completed);
  QVERIFY(p.start("MouseDrag"));p.select({.1,.1});p.select({.9,.9});QTRY_VERIFY(held);
  p.cancel();int count=moves;QTest::qWait(350);QCOMPARE(moves,count);QVERIFY(!held);QCOMPARE(releases,1);
  QCOMPARE(done.count(),0);QVERIFY(!p.busy());QVERIFY(!p.start("RepeatLastMouseAction"));
 }
 void pointerFailureReleasesAndStops(){
  PointerActions p;int moves=0,releases=0;
  p.move=[&](double,double){return ++moves==1;};p.button=[](int,bool){return true;};p.releaseAll=[&]{++releases;};
  QSignalSpy done(&p,&PointerActions::completed);QVERIFY(p.start("MouseDrag"));p.select({.2,.2});p.select({.8,.8});
  QTRY_COMPARE(done.count(),1);QVERIFY(!done[0][0].toBool());QCOMPARE(releases,1);QVERIFY(p.heldButtons().isEmpty());
 }
 void repeatClickUsesCompletedTarget(){
  PointerActions p;QList<QPointF> moves;int clicks=0;
  p.move=[&](double x,double y){moves<<QPointF(x,y);return true;};p.button=[&](int,bool down){if(down)++clicks;return true;};
  QSignalSpy done(&p,&PointerActions::completed);QVERIFY(p.start("MouseMoveAndLeftDoubleClick"));p.select({.3,.4});
  QTRY_COMPARE(done.count(),1);QCOMPARE(clicks,2);QVERIFY(p.start("RepeatLastMouseAction"));QVERIFY(!p.targeting());
  QTRY_COMPARE(done.count(),2);QCOMPARE(clicks,4);QCOMPARE(moves,QList<QPointF>({{.3,.4},{.3,.4}}));
 }
 void clickDurationProfileRoundTrip(){
  auto values=ProfileStore::defaults();QCOMPARE(values["clickDurationMs"].toInt(),0);
  QTemporaryDir dir;ProfileStore store(dir.filePath("profile.json"));QString error;
  for(int duration:{0,25,250}){values["clickDurationMs"]=duration;QVERIFY(store.save(values,error));QCOMPARE(store.load().values["clickDurationMs"].toInt(),duration);}
  for(const auto &invalid:QJsonArray{-1,251,1.5,"25",true}){values["clickDurationMs"]=invalid;QVERIFY(!ProfileStore::valid(values));}
  values.remove("clickDurationMs");QVERIFY(ProfileStore::valid(values));
 }
 void transitionProfileValidation(){
  auto values=ProfileStore::defaults();
  QTemporaryDir dir;ProfileStore store(dir.filePath("profile.json"));QString error;
  for(const auto &style:QStringList{"instant","fade","lift","slide","zoom","rows"}){
   values["keyboardTransition"]=style;values["transitionDurationMs"]=320;
   QVERIFY(store.save(values,error));QCOMPARE(store.load().values["keyboardTransition"].toString(),style);
   QCOMPARE(store.load().values["transitionDurationMs"].toInt(),320);
  }
  values["keyboardTransition"]="unknown";QVERIFY(!ProfileStore::valid(values));values["keyboardTransition"]="instant";
  for(const auto &invalid:QJsonArray{0,49,1001,180.5,"180",true}){values["transitionDurationMs"]=invalid;QVERIFY(!ProfileStore::valid(values));}
  values.remove("keyboardTransition");values.remove("transitionDurationMs");QVERIFY(ProfileStore::valid(values));
 }
 void delayedClicksHoldEachButton(){
  for(const auto &action:QStringList{"MouseLeftClick","MouseMoveAndMiddleClick","MouseMoveAndRightClick","MouseMoveAndLeftDoubleClick"}){
   PointerActions p;p.setClickDuration(100);QElapsedTimer clock;clock.start();
   QList<qint64> times;QList<bool> states;
   p.move=[](double,double){return true;};
   p.button=[&](int,bool down){times<<clock.elapsed();states<<down;return true;};
   QSignalSpy done(&p,&PointerActions::completed);QVERIFY(p.start(action));
   if(p.targeting())p.select({.3,.4});
   QTRY_COMPARE(done.count(),1);QVERIFY(done[0][0].toBool());
   const int count=action.endsWith("DoubleClick")?4:2;QCOMPARE(times.size(),count);
   for(int i=0;i<count;i+=2){QVERIFY(states[i]);QVERIFY(!states[i+1]);QVERIFY(times[i+1]-times[i]>=100);}
   QVERIFY(p.heldButtons().isEmpty());
  }
 }
 void cancellingDelayedClickReleases(){
  PointerActions p;p.setClickDuration(250);bool held=false;int events=0;
  p.button=[&](int,bool down){held=down;++events;return true;};
  p.releaseAll=[&]{held=false;};QSignalSpy done(&p,&PointerActions::completed);
  QVERIFY(p.start("MouseLeftClick"));QTRY_VERIFY(held);QVERIFY(p.busy());
  p.cancel();QVERIFY(!held);QTest::qWait(300);QCOMPARE(events,1);QCOMPARE(done.count(),0);QVERIFY(!p.busy());
 }
 void heldButtonCannotBeReleasedByClick(){
  PointerActions p;QStringList events;p.button=[&](int,bool down){events<< (down?"down":"up");return true;};
  QSignalSpy done(&p,&PointerActions::completed);QVERIFY(p.start("MouseLeftDownUp"));QTRY_COMPARE(done.count(),1);
  QVERIFY(!p.start("MouseLeftClick"));QVERIFY(!p.start("MouseDrag"));QCOMPARE(events,QStringList({"down"}));
  QVERIFY(p.start("MouseLeftDownUp"));QTRY_COMPARE(done.count(),2);QCOMPARE(events,QStringList({"down","up"}));
 }
 void invalidPointerTargetsDoNotExecute(){
  PointerActions p;int moves=0;p.move=[&](double,double){++moves;return true;};
  QVERIFY(p.start("MouseMoveTo"));p.select({-1,.5});p.select({qQNaN(),.5});p.select({.5,2});
  QTest::qWait(130);QCOMPARE(moves,0);QVERIFY(p.targeting());p.cancel();
 }
 void speechProfileValidation(){
  auto values=ProfileStore::defaults();QVERIFY(ProfileStore::valid(values));
  values.remove("speechProfiles");values.remove("activeSpeechProfile");QVERIFY(ProfileStore::valid(values));
  values=ProfileStore::defaults();values["activeSpeechProfile"]=1;QVERIFY(!ProfileStore::valid(values));
  values["activeSpeechProfile"]=0.5;QVERIFY(!ProfileStore::valid(values));
  values=ProfileStore::defaults();auto profiles=values["speechProfiles"].toArray();auto profile=profiles[0].toObject();auto items=profile["items"].toArray();
  items.append(items[0]);profile["items"]=items;profiles[0]=profile;values["speechProfiles"]=profiles;QVERIFY(!ProfileStore::valid(values));
  items=QJsonArray{QJsonObject{{"label","Help"},{"phrase"," "}}};profile["items"]=items;profiles[0]=profile;values["speechProfiles"]=profiles;QVERIFY(!ProfileStore::valid(values));
  profile["items"]=QJsonArray{};profiles[0]=profile;values["speechProfiles"]=profiles;QVERIFY(ProfileStore::valid(values));
 }
 void keyCycleModesValidation(){
  auto values=ProfileStore::defaults();QVERIFY(ProfileStore::valid(values));
  values.remove("keyCycleModes");QVERIFY(ProfileStore::valid(values));
  for(const auto &invalid:QJsonArray{QJsonArray{},QJsonArray{"Tap","Tap"},QJsonArray{"Unknown"},QJsonArray{1},"Tap"}){
   values["keyCycleModes"]=invalid;QVERIFY(!ProfileStore::valid(values));
  }
  values["keyCycleModes"]=QJsonArray{"Hold"};QVERIFY(ProfileStore::valid(values));
 }

 void preferencesPersistAndRecover(){
  QTemporaryDir dir;QString path=dir.filePath("nested/profile.json"),error;ProfileStore store(path);
  QCOMPARE(store.load().values,ProfileStore::defaults());QVERIFY(store.load().message.isEmpty());
  auto first=ProfileStore::defaults();first["dwellMs"]=1200;first["learning"]=true;
  QVERIFY2(store.save(first,error),qPrintable(error));QCOMPARE(ProfileStore(path).load().values,first);
  auto second=first;second["literal"]=true;second["startLayout"]="main-14x1-bottom";
  QVERIFY(store.save(second,error));QCOMPARE(store.load().values,second);
  QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));file.write("{broken");file.close();
  QCOMPARE(store.load().values,first);QVERIFY(!store.load().message.isEmpty());
  // Saving after corruption preserves the known-good backup.
  QVERIFY(store.save(second,error));QVERIFY(file.open(QIODevice::WriteOnly));file.write("null");file.close();
  QCOMPARE(store.load().values,first);
  QVERIFY(store.save(ProfileStore::defaults(),error,true));
  QVERIFY(file.open(QIODevice::WriteOnly));file.write("broken again");file.close();
  QCOMPARE(store.load().values,ProfileStore::defaults());
 }
 void invalidPreferencesAreRejected(){
  QTemporaryDir dir;ProfileStore store(dir.filePath("profile.json"));QString error;
  const auto defaults=ProfileStore::defaults();QVERIFY(store.save(defaults,error));
  const QList<QPair<QString,QJsonValue>> invalid{{"predictionTextScale",49},{"predictionTextScale",301},{"predictionTextScale",100.5},{"predictionTextScale","100"},{"scratchpadTextScale",49},{"scratchpadTextScale",301},{"scratchpadTextScale",100.5},{"scratchpadTextScale","100"},{"version",2},{"dwellMs",249},{"dwellMs",3001},{"dwellMs",750.5},{"dwellMs","750"},{"literal",1},{"learning","false"},{"keyboardScale",49},{"keyboardScale",151},{"keyboardScale",100.5},{"keyboardTheme","unknown"},{"keyboardTheme",42},{"highContrast","yes"},{"enlargeShortLabels","yes"},{"shortLabelScale",49},{"shortLabelScale",201},{"shortLabelScale",100.5},{"shortLabelScale","100"},{"liveZoom","true"},{"progressiveZoom","true"},{"targetDwellEnabled","true"},{"targetStartDwellMs",249},{"targetStartDwellMs",3001},{"targetStartDwellMs",750.5},{"targetStartDwellMs","750"},{"targetConfirmDwellMs",249},{"targetConfirmDwellMs",3001},{"targetDwellRadius",9},{"targetDwellRadius",101},{"targetDwellRadius",40.5},{"targetDwellRadius","40"},{"zoomDurationMs",499},{"zoomDurationMs",5001},{"zoomDurationMs",1500.5},{"zoomDurationMs","1500"},{"magnifierZoom",1},{"magnifierZoom",9},{"magnifierZoom",4.5},{"magnifierZoom","4"},{"longLabelScale",49},{"longLabelScale",201},{"longLabelScale",100.5},{"longLabelScale","100"},{"rememberDesktop","yes"},{"startLayout","../other"},{"startLayout",""}};
  for(const auto &entry:invalid){auto values=defaults;values[entry.first]=entry.second;QVERIFY(!store.save(values,error));QVERIFY(!error.isEmpty());QCOMPARE(store.load().values,defaults);}
 }
 void newerPreferencesArePreservedUntilReset(){
  QTemporaryDir dir;QString path=dir.filePath("profile.json"),error;ProfileStore store(path);
  QVERIFY(store.save(ProfileStore::defaults(),error));
  QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));const QByteArray future="{\"version\":2,\"future\":true}";file.write(future);file.close();
  QVERIFY(!store.load().message.isEmpty());QVERIFY(!store.save(ProfileStore::defaults(),error));
  QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),future);file.close();
  QVERIFY(store.save(ProfileStore::defaults(),error,true));QCOMPARE(store.load().values,ProfileStore::defaults());
 }
 void corruptPreferencesFallBackAndWriteFailuresSurface(){
  QTemporaryDir dir;QString path=dir.filePath("profile.json"),error;
  for(const auto &name:{path,path+".bak"}){QFile file(name);QVERIFY(file.open(QIODevice::WriteOnly));file.write("[]");}
  ProfileStore store(path);QCOMPARE(store.load().values,ProfileStore::defaults());QVERIFY(!store.load().message.isEmpty());
  ProfileStore blocked(path+"/child.json");QVERIFY(!blocked.save(ProfileStore::defaults(),error));QVERIFY(!error.isEmpty());
  QFile file(path);QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),QByteArray("[]"));
 }
 void heldShiftCommitsEffectivePunctuation(){
  for(const QString shift:{QString("LSHIFT"),QString("RSHIFT")}){
   ActionRunner runner;runner.sendKey=[](QString,bool){return true;};
   QSignalSpy text(&runner,&ActionRunner::textCommitted),done(&runner,&ActionRunner::finished);
   QVERIFY(runner.start({cmd("KeyToggle",shift),cmd("Text","a1234567890-=[];'`\\,./ !\t\n")}));
   QTRY_COMPARE(done.count(),1);
   QCOMPARE(text.count(),1);
   QCOMPARE(text.at(0).at(0).toString(),QString("A!@#$%^&*()_+{}:\"~|<>? !\t\n"));
   QVERIFY(runner.isHeld(shift));runner.cancel();
  }
 }
 void shortcutBackspaceDoesNotCommitPlainDeletion(){
  for(const QString modifier:{QString("LCONTROL"),QString("RCONTROL"),QString("LMENU"),QString("RMENU"),QString("LWIN"),QString("RWIN"),QString()}){
   ActionRunner runner;QStringList events;
   runner.sendKey=[&](QString key,bool down){events<<key+(down?"+":"-");return true;};
   QSignalSpy text(&runner,&ActionRunner::textCommitted),done(&runner,&ActionRunner::finished);
   QVariantList commands;if(!modifier.isEmpty())commands<<cmd("KeyDown",modifier);
   commands<<cmd("Action","BackOne");QVERIFY(runner.start(commands));QTRY_COMPARE(done.count(),1);
   QVERIFY(events.contains("BACK+"));QVERIFY(events.contains("BACK-"));
   QCOMPARE(text.count(),modifier.isEmpty()?1:0);
   if(modifier.isEmpty())QCOMPARE(text.at(0).at(0).toString(),QString("\b"));
   QVERIFY(runner.heldKeys().isEmpty());
  }
 }
 void saveCanvasGeometry(){
  QTemporaryDir dir;QVERIFY(dir.isValid());const auto path=dir.filePath("keyboard.xml");
  QByteArray source=R"(<Keyboard><!--keep comment--><Name>Test</Name><TransparentBackground>true</TransparentBackground><Canvas><Width>100</Width><Height>100</Height></Canvas><Content><DynamicKey X="1" Y="2" Width="10" Height="20" Mode="Tap"><Label>A</Label><Text>a</Text></DynamicKey><Scratchpad X="0" Y="70" Width="100" Height="20"/></Content></Keyboard>)";
  auto write=[&](const QByteArray &bytes){QFile f(path);return f.open(QIODevice::WriteOnly)&&f.write(bytes)==bytes.size();};
  auto read=[&](){QFile f(path);if(!f.open(QIODevice::ReadOnly))return QByteArray();return f.readAll();};
  QVERIFY(write(source));
  auto layout=KeyboardLayout::fromXml(KeyboardPack::load(path));QVERIFY(!layout.contains("error"));
  auto keys=layout["keys"].toList();auto key=keys[0].toMap();key["col"]=12.25;key["row"]=30.5;key["width"]=25.75;key["height"]=15.25;keys[0]=key;
  QString error;QVERIFY2(KeyboardLayout::saveCanvasGeometry(path,source,keys,error),qPrintable(error));
  QCOMPARE(read(),source);QVERIFY(source.contains("<!--keep comment-->"));QVERIFY(source.contains("<Text>a</Text>"));
  auto loaded=KeyboardLayout::fromXml(KeyboardPack::load(path));auto saved=loaded["keys"].toList()[0].toMap();
  QCOMPARE(saved["col"].toDouble(),12.25);QCOMPARE(saved["row"].toDouble(),30.5);
  QCOMPARE(saved["width"].toDouble(),25.75);QCOMPARE(saved["height"].toDouble(),15.25);
  QCOMPARE(saved["label"],key["label"]);QCOMPARE(saved["commands"],key["commands"]);
  QCOMPARE(loaded["transparentbackground"].toString(),QString("true"));
  QCOMPARE(loaded["keys"].toList()[1],keys[1]);
  // Subsequent saves use the updated source snapshot.
  QVERIFY(KeyboardLayout::saveCanvasGeometry(path,source,keys,error));
  const auto good=source;key["width"]=1000;keys[0]=key;
  QVERIFY(!KeyboardLayout::saveCanvasGeometry(path,source,keys,error));QCOMPARE(read(),good);
  key["width"]=25.75;keys[0]=key;
  QVERIFY(write(good+"\n<!--external edit-->"));
  const auto external=read();QVERIFY(!KeyboardLayout::saveCanvasGeometry(path,source,keys,error));QCOMPARE(read(),external);
  QVERIFY(!KeyboardLayout::saveCanvasGeometry(dir.filePath("missing.xml"),source,keys,error));
 }
 void canvasLayout(){
  QTemporaryDir dir;
  auto parse=[&](QByteArray geometry,QByteArray content){
   QFile file(dir.filePath("canvas.xml"));
   if(!file.open(QIODevice::WriteOnly|QIODevice::Truncate))return QVariantMap{{"error","write failed"}};
   file.write("<Keyboard>"+geometry+"<Content>"+content+"</Content></Keyboard>");file.close();
   return KeyboardLayout::fromXml(KeyboardPack::load(file.fileName()));
  };
  const QByteArray canvas="<Canvas><Width>100.5</Width><Height>100</Height></Canvas>";
  auto layout=parse(canvas,"<DynamicKey X='12.25' Y='7.5' Width='20.5' Height='10.25'><Label>A</Label><Text>a</Text></DynamicKey>"
                           "<DynamicKey X='20' Y='10' Width='0.5' Height='0.25'/>"
                           "<SuggestionRow X='0' Y='80' Width='100.5' Height='20' Count='3'/>");
  QVERIFY2(!layout.contains("error"),qPrintable(layout["error"].toString()));
  QCOMPARE(layout["layoutMode"].toString(),QString("canvas"));
  QCOMPARE(layout["cols"].toDouble(),100.5);
  auto keys=layout["keys"].toList();QCOMPARE(keys.size(),3);
  QCOMPARE(keys[0].toMap()["col"].toDouble(),12.25);
  QCOMPARE(keys[0].toMap()["height"].toDouble(),10.25);
  QCOMPARE(keys[0].toMap()["commands"].toList().size(),1);
  QCOMPARE(keys[2].toMap()["suggestionCount"].toInt(),3);
  for(const auto &item:QList<QByteArray>{
       "<DynamicKey X='0' Y='0' Width='0' Height='1'/>",
       "<DynamicKey X='-1' Y='0' Width='1' Height='1'/>",
       "<DynamicKey X='100' Y='0' Width='1' Height='1'/>",
       "<DynamicKey X='nan' Y='0' Width='1' Height='1'/>",
       "<DynamicKey X='0' Width='1' Height='1'/>",
       "<DynamicKey X='0' Y='0' Row='0' Width='1' Height='1'/>",
       "<SuggestionRow X='0' Y='0' Width='10' Height='10' Count='0'/>"})
   QVERIFY2(parse(canvas,item).contains("error"),item.constData());
  QVERIFY(parse(canvas+"<Grid><Rows>2</Rows><Cols>2</Cols></Grid>",{}).contains("error"));
  QVERIFY(parse("<Canvas><Width>inf</Width><Height>100</Height></Canvas>",{}).contains("error"));
 }
 void automaticLayoutPlacement(){
  QTemporaryDir dir;QFile file(dir.filePath("layout.xml"));QVERIFY(file.open(QIODevice::WriteOnly));
  file.write("<Keyboard><Grid><Rows>2</Rows><Cols>4</Cols></Grid><Content>"
             "<DynamicKey Row='0' Col='0' Height='2'/><DynamicKey Width='2'/>"
             "<DynamicKey/><Scratchpad Width='3'/></Content></Keyboard>");file.close();
  auto layout=KeyboardLayout::fromXml(KeyboardPack::load(file.fileName()));
  QVERIFY2(!layout.contains("error"),qPrintable(layout["error"].toString()));
  auto keys=layout["keys"].toList();QCOMPARE(keys.size(),4);
  QCOMPARE(keys[1].toMap()["col"].toInt(),1);QCOMPARE(keys[2].toMap()["col"].toInt(),3);
  QCOMPARE(keys[3].toMap()["row"].toInt(),1);QCOMPARE(keys[3].toMap()["col"].toInt(),1);
  QVERIFY(file.open(QIODevice::WriteOnly|QIODevice::Truncate));
  file.write("<Keyboard><Grid><Rows>1</Rows><Cols>1</Cols></Grid><Content><DynamicKey/><DynamicKey/></Content></Keyboard>");file.close();
  QVERIFY(KeyboardLayout::fromXml(KeyboardPack::load(file.fileName())).contains("error"));
 }
 void conversationNeverSendsDesktopInput(){
  ActionRunner runner;runner.textOnly=true;int output=0;
  runner.sendKey=[&](QString,bool){++output;return true;};
  runner.sendText=[&](QString){++output;return true;};
  runner.supportsPhysicalKey=[](QString){return false;};
  QSignalSpy text(&runner,&ActionRunner::textCommitted),done(&runner,&ActionRunner::finished);
  QVERIFY(runner.start({cmd("Action","LeftShift")}));QTRY_COMPARE(done.count(),1);
  QVERIFY(runner.start({cmd("Text","a\t")}));QTRY_COMPARE(done.count(),2);
  QCOMPARE(text[0][0].toString(),QString("A\t"));QVERIFY(!runner.shifted());
  QVERIFY(runner.start({cmd("Action","BackOne")}));QTRY_COMPARE(done.count(),3);
  QCOMPARE(text[1][0].toString(),QString("\b"));runner.cancel();QCOMPARE(output,0);
 }
 void extendedFunctionKeys(){
  for(int i=13;i<=24;++i)QCOMPARE(KeyNames::linuxCode(QString("F%1").arg(i)),170+i);
  QCOMPARE(KeyNames::linuxCode("PrintScreen"),99);
  QCOMPARE(KeyNames::linuxCode("VK_1"),2);
 }
 void allLayoutsCompile(){
  for(const auto &file:QDir(PACK_PATH).entryList({"*.xml"})){
   auto layout=KeyboardLayout::fromXml(KeyboardPack::load(QDir(PACK_PATH).filePath(file)));
   QVERIFY2(!layout.contains("error"),qPrintable(file+": "+layout["error"].toString()));
   for(auto v:layout["keys"].toList())QVERIFY2(ActionRunner::validate(v.toMap()["commands"].toList()).isEmpty(),qPrintable(file));
  }
 }
 void timedMacroPreservesOrder(){
  ActionRunner runner;QStringList events;QElapsedTimer clock;clock.start();qint64 aTime=-1;
  runner.sendKey=[&](QString key,bool down){events<<key+(down?"+":"-");if(key=="A"&&down)aTime=clock.elapsed();return true;};
  QSignalSpy done(&runner,&ActionRunner::finished);
  QVERIFY(runner.start({cmd("KeyDown","LCTRL"),cmd("Wait","60"),cmd("Text","a"),cmd("KeyUp","LCONTROL")}));
  QTRY_COMPARE(done.count(),1);QVERIFY(aTime>=50);QCOMPARE(events,QStringList({"LCONTROL+","A+","A-","LCONTROL-"}));
 }
 void cancellationReleasesAndStopsQueue(){
  ActionRunner runner;QStringList events;runner.sendKey=[&](QString key,bool down){events<<key+(down?"+":"-");return true;};
  runner.start({cmd("KeyDown","LSHIFT"),cmd("Wait","100"),cmd("Text","a")});
  QTRY_VERIFY(runner.isHeld("LSHIFT"));runner.cancel();QTest::qWait(130);
  QCOMPARE(events,QStringList({"LSHIFT+","LSHIFT-"}));QVERIFY(!runner.busy());
 }
 void heldKeysPersistUntilToggleOrCancel(){
  ActionRunner runner;QStringList events;runner.sendKey=[&](QString key,bool down){events<<key+(down?"+":"-");return true;};
  QSignalSpy done(&runner,&ActionRunner::finished);
  runner.start({cmd("KeyToggle","a")});QTRY_COMPARE(done.count(),1);QVERIFY(runner.isHeld("A"));
  runner.start({cmd("KeyToggle","A")});QTRY_COMPARE(done.count(),2);QVERIFY(!runner.isHeld("A"));
  runner.start({cmd("KeyToggle","LSHIFT")});QTRY_COMPARE(done.count(),3);runner.cancel();
  QCOMPARE(events,QStringList({"A+","A-","A+","A-"}).mid(0,2)+QStringList({"LSHIFT+","LSHIFT-"}));
 }
 void modifierLatchLockCycle(){
  for(const QString action:{"LeftShift","RightShift","LeftCtrl","RightCtrl","LeftAlt","RightAlt","LeftWin","RightWin"}){
   ActionRunner runner;QStringList events;
   runner.sendKey=[&](QString key,bool down){events<<key+(down?"+":"-");return true;};
   QSignalSpy done(&runner,&ActionRunner::finished);
   const auto commands=QVariantList{cmd("Action",action)};
   const auto name=ActionRunner::modifierName(commands);QVERIFY(!name.isEmpty());
   QVERIFY(runner.start(commands));QTRY_COMPARE(done.count(),1);
   QCOMPARE(runner.modifierStates().value(name).toString(),QString("latched"));
   QVERIFY(runner.start({cmd("Text","a")}));QTRY_COMPARE(done.count(),2);
   QVERIFY(runner.modifierStates().isEmpty());QVERIFY(runner.heldKeys().isEmpty());
   QCOMPARE(events,QStringList({name+"+","A+","A-",name+"-"}));events.clear();
   QVERIFY(runner.start(commands));QTRY_COMPARE(done.count(),3);
   QVERIFY(runner.start(commands));QTRY_COMPARE(done.count(),4);
   QCOMPARE(runner.modifierStates().value(name).toString(),QString("locked"));
   QCOMPARE(events,QStringList({name+"+"}));
   QVERIFY(runner.start({cmd("Text","b")}));QTRY_COMPARE(done.count(),5);
   QCOMPARE(runner.modifierStates().value(name).toString(),QString("locked"));
   QVERIFY(runner.start(commands));QTRY_COMPARE(done.count(),6);
   QVERIFY(runner.modifierStates().isEmpty());QVERIFY(runner.heldKeys().isEmpty());
  }
 }
 void navigationPreservesModifiersAndReleasesOtherKeys(){
  for(const QString action:{"LeftShift","RightShift","LeftCtrl","RightCtrl","LeftAlt","RightAlt","LeftWin","RightWin"}){
   for(bool locked:{false,true}){
    ActionRunner runner;QStringList events;
    runner.sendKey=[&](QString key,bool down){events<<key+(down?"+":"-");return true;};
    runner.navigate=[&](QString,bool){runner.cancel(true);};
    QSignalSpy done(&runner,&ActionRunner::finished);
    const QVariantList modifier{cmd("Action",action)};
    QVERIFY(runner.start(modifier));QTRY_COMPARE(done.count(),1);
    if(locked){QVERIFY(runner.start(modifier));QTRY_COMPARE(done.count(),2);}
    const auto states=runner.modifierStates();events.clear();
    QVERIFY(runner.start({cmd("KeyDown","A"),cmd("ChangeKeyboard","another"),cmd("Text","z")}));
    QTRY_VERIFY(!runner.busy());QCOMPARE(runner.modifierStates(),states);
    QCOMPARE(events,QStringList({"A+","A-"}));
    QVERIFY(runner.start({cmd("Text","b")}));QTRY_VERIFY(!runner.busy());
    QCOMPARE(runner.modifierStates().isEmpty(),!locked);
    runner.cancel();QVERIFY(runner.heldKeys().isEmpty());QVERIFY(runner.modifierStates().isEmpty());
   }
  }
 }
 void latchedModifiersReleaseOnActionCancelAndFailure(){
  ActionRunner runner;runner.sendKey=[](QString,bool){return true;};
  QSignalSpy done(&runner,&ActionRunner::finished),failed(&runner,&ActionRunner::failed);
  QVERIFY(runner.start({cmd("Action","LeftCtrl")}));QTRY_COMPARE(done.count(),1);
  QVERIFY(runner.start({cmd("Action","LeftShift")}));QTRY_COMPARE(done.count(),2);
  QCOMPARE(runner.modifierStates().size(),2);
  QVERIFY(runner.start({cmd("Action","Left")}));QTRY_COMPARE(done.count(),3);
  QVERIFY(runner.heldKeys().isEmpty());QVERIFY(runner.modifierStates().isEmpty());
  QVERIFY(runner.start({cmd("Action","LeftShift")}));QTRY_COMPARE(done.count(),4);
  runner.cancel();QVERIFY(runner.modifierStates().isEmpty());QVERIFY(runner.heldKeys().isEmpty());
  QVERIFY(runner.start({cmd("Action","LeftShift")}));QTRY_COMPARE(done.count(),5);
  runner.sendKey=[](QString key,bool down){return key!="A"||!down;};
  QVERIFY(runner.start({cmd("Text","a")}));QTRY_COMPARE(failed.count(),1);
  QVERIFY(runner.modifierStates().isEmpty());QVERIFY(runner.heldKeys().isEmpty());
 }
 void failureReleasesEarlierKeys(){
  ActionRunner runner;QStringList events;runner.sendKey=[&](QString key,bool down){if(key=="A")return false;events<<key+(down?"+":"-");return true;};
  QSignalSpy failed(&runner,&ActionRunner::failed);
  runner.start({cmd("KeyDown","LSHIFT"),cmd("Text","a")});QTRY_VERIFY(failed.count()>0);
  QCOMPARE(events,QStringList({"LSHIFT+","LSHIFT-"}));QVERIFY(runner.heldKeys().isEmpty());
 }
 void asyncActionWaitsForTarget(){
  ActionRunner runner;QStringList events;runner.sendKey=[&](QString key,bool down){events<<key+(down?"+":"-");return true;};
  runner.performAction=[&](QString){runner.suspend();return true;};
  runner.start({cmd("Action","MouseMoveTo"),cmd("Text","a")});QTest::qWait(25);QVERIFY(events.isEmpty());QVERIFY(runner.busy());
  runner.resume(true);QTRY_COMPARE(events.size(),2);QCOMPARE(events,QStringList({"A+","A-"}));
 }
 void invalidMacroEmitsNothing(){ActionRunner runner;int count=0;runner.sendKey=[&](QString,bool){++count;return true;};QVERIFY(!runner.start({cmd("KeyDown","LSHIFT"),cmd("KeyDown","UNKNOWN")}));QCOMPARE(count,0);}
 void navigationPreservesBackFlag(){ActionRunner runner;QString target;bool back=true;runner.navigate=[&](QString name,bool b){target=name;back=b;};auto c=cmd("ChangeKeyboard","main-14x1-top");c["attributes"]=QVariantMap{{"backreturnshere","false"}};runner.start({c});QTRY_COMPARE(target,QString("main-14x1-top"));QVERIFY(!back);}
 void shiftIsOwnedAndReleased(){ActionRunner runner;QStringList events;runner.sendKey=[&](QString key,bool down){events<<key+(down?"+":"-");return true;};QSignalSpy done(&runner,&ActionRunner::finished);runner.start({cmd("Text","A!")});QTRY_COMPARE(done.count(),1);QCOMPARE(events,QStringList({"LSHIFT+","A+","A-","LSHIFT-","LSHIFT+","1+","1-","LSHIFT-"}));}

 void dwellGraceValidation(){
  auto values=ProfileStore::defaults();QCOMPARE(values["dwellGraceMs"].toInt(),0);
  for(const auto &value:QJsonArray{-1,1001,1.5,"150"}){
   values["dwellGraceMs"]=value;QVERIFY(!ProfileStore::valid(values));
  }
  values.remove("dwellGraceMs");QVERIFY(ProfileStore::valid(values));
  values["dwellGraceMs"]=150;QVERIFY(ProfileStore::valid(values));
  QVERIFY(values["showPausedDwellProgress"].toBool());
  values["showPausedDwellProgress"]="true";QVERIFY(!ProfileStore::valid(values));
  values.remove("showPausedDwellProgress");QVERIFY(ProfileStore::valid(values));
 }
 void dwellGracePausesAndResumes(){
  Dwell d;
  QVERIFY(!d.update("a",0,500,false,-1,150));
  QVERIFY(!d.update("a",100,500,false,-1,150));
  QVERIFY(!d.update("",125,500,false,-1,150));
  QCOMPARE(d.progress(200,500),0.0);
  QCOMPARE(d.savedElapsed(200,150).value("a"),qint64(100));
  QCOMPARE(d.savedElapsed(250,150).value("a"),qint64(100));
  QVERIFY(d.savedElapsed(276,150).isEmpty());
  QVERIFY(d.savedElapsed(200,0).isEmpty());
  QVERIFY(!d.update("a",225,500,false,-1,150));
  QCOMPARE(d.progress(225,500),0.2);
  QVERIFY(!d.update("a",400,500,false,-1,150));
  QVERIFY(!d.update("a",575,500,false,-1,150));
  QVERIFY(d.update("a",625,500,false,-1,150));
 }
 void dwellGraceSwitchStartsImmediatelyAndExpires(){
  Dwell d;
  d.update("a",0,500,false,-1,150);d.update("a",100,500,false,-1,150);
  QVERIFY(!d.update("b",125,500,false,-1,150));
  QCOMPARE(d.progress(125,500),0.0);
  d.update("b",225,500,false,-1,150);QCOMPARE(d.progress(225,500),0.2);
  d.update("a",250,500,false,-1,150);QCOMPARE(d.progress(250,500),0.2);
  d.update("",275,500,false,-1,150);d.update("",400,500,false,-1,150);
  d.update("a",450,500,false,-1,150);QCOMPARE(d.progress(450,500),0.0);
 }
 void dwellGraceZeroAndConsumedTargetsReset(){
  Dwell d;
  d.update("a",0,500);d.update("a",100,500);d.update("",125,500);
  d.update("a",150,500);QCOMPARE(d.progress(150,500),0.0);
  d.consume("a",150);d.update("",175,500,false,-1,150);
  d.update("a",200,500,false,-1,150);QCOMPARE(d.progress(200,500),0.0);
 }
 void dwellRequiresContinuousPresence(){
  Dwell d;QVERIFY(!d.update("a",0,500));
  for(int t=100;t<500;t+=100)QVERIFY(!d.update("a",t,500));
  QVERIFY(d.update("a",500,500));QVERIFY(!d.update("a",600,500));
  d.update("",650,500);QVERIFY(!d.update("a",700,500));
  QVERIFY(!d.update("a",1300,500)); // stale interval cancels accumulated dwell
 }
 void dwellPausePreservesProgressWithoutCountingLostTime(){
  Dwell key;PointDwell point;
  QVERIFY(!key.update("a",0,500));QVERIFY(!point.update({10,10},0,500,20));
  QVERIFY(!key.update("a",100,500));QVERIFY(!point.update({10,10},100,500,20));
  key.pause(450);point.pause(450);
  QCOMPARE(key.progress(450,500),.2);QCOMPARE(point.progress(450,500),.2);
  QVERIFY(!key.update("a",500,500));QVERIFY(!point.update({10,10},500,500,20));
  QCOMPARE(key.progress(500,500),.3);QCOMPARE(point.progress(500,500),.3);
  QVERIFY(!key.update("b",550,500));QVERIFY(!point.update({80,80},550,500,20));
  QCOMPARE(key.progress(550,500),0.);QCOMPARE(point.progress(550,500),0.);
 }
 void invalidGazeToleranceProfileValidation(){
  auto values=ProfileStore::defaults();QCOMPARE(values["invalidGazeToleranceMs"].toInt(),500);
  values.remove("invalidGazeToleranceMs");QVERIFY(ProfileStore::valid(values));
  for(const auto &invalid:QList<QJsonValue>{-1,1001,1.5,"100",true}){
   values["invalidGazeToleranceMs"]=invalid;QVERIFY(!ProfileStore::valid(values));
  }
  values["invalidGazeToleranceMs"]=1000;QVERIFY(ProfileStore::valid(values));
 }
 void gazeClampingProfileValidation(){
  for(const auto name:{"clampGazeForKeyboard","clampGazeForPointSelection"}){
   auto values=ProfileStore::defaults();QCOMPARE(values[name].toBool(),QString(name)=="clampGazeForPointSelection");
   values.remove(name);QVERIFY(ProfileStore::valid(values));
   values[name]=false;QVERIFY(ProfileStore::valid(values));
   values[name]="false";QVERIFY(!ProfileStore::valid(values));
   values[name]=1;QVERIFY(!ProfileStore::valid(values));
  }
 }
 void repeatRateDoesNotChangeInitialDwell(){
  Dwell d;QVERIFY(!d.update("a",0,500,true,100));
  for(int t=100;t<500;t+=100)QVERIFY(!d.update("a",t,500,true,100));
  QCOMPARE(d.progress(400,500,100),0.8);QVERIFY(d.update("a",500,500,true,100));d.consume("a",500);
  QVERIFY(!d.update("a",525,500,true,100));QVERIFY(!d.update("a",575,500,true,100));QCOMPARE(d.progress(575,500,100),0.5);
  QVERIFY(d.update("a",625,500,true,100));d.consume("a",625);
  QVERIFY(!d.update("b",650,500,true,100));QVERIFY(!d.update("b",750,500,true,100));QCOMPARE(d.progress(750,500,100),0.2);
  auto v=ProfileStore::defaults();v.remove("repeatIntervalMs");QVERIFY(ProfileStore::valid(v));
  for(double invalid:{99.0,2001.0,750.5}){v["repeatIntervalMs"]=invalid;QVERIFY(!ProfileStore::valid(v));}
 }
 void repeatingDwellStartsFreshIntervals(){
  Dwell d;d.consume("modifier",0);d.inhibit("modifier",25);
  QVERIFY(!d.update("modifier",50,250,true));QCOMPARE(d.progress(50,250),0.0);
  QVERIFY(!d.update("modifier",150,250,true));QCOMPARE(d.progress(150,250),0.4);
  QVERIFY(d.update("modifier",300,250,true));d.consume("modifier",300);
  QVERIFY(!d.update("modifier",325,250,true));QCOMPARE(d.progress(325,250),0.0);
  QVERIFY(!d.update("modifier",425,250,true));QVERIFY(d.update("modifier",575,250,true));
  d.consume("modifier",575);QVERIFY(!d.update("modifier",600,250,true));
  QVERIFY(!d.update("modifier",700,250,true));QVERIFY(d.progress(700,250)>0);
  QVERIFY(!d.update("modifier",725,250,false));QCOMPARE(d.progress(725,250),0.0);
  QVERIFY(!d.update("modifier",1000,250,false));
 }
 void busyDwellKeepsConsumedTarget(){
  Dwell d;d.consume("modifier",0);
  for(int t=25;t<500;t+=25)d.inhibit("modifier",t);
  QVERIFY(!d.update("modifier",500,250));QCOMPARE(d.progress(500,250),0.0);
  QVERIFY(!d.update("modifier",1000,250)); // A delayed UI tick is not a new hover.
  d.inhibit("other",1025);d.inhibit("other",1200);
  QVERIFY(!d.update("other",1300,250));QCOMPARE(d.progress(1300,250),0.0);
  QVERIFY(!d.update("other",1400,250));QCOMPARE(d.progress(1400,250),0.4);
  d.consume("modifier",1500);d.inhibit("",1525);
  QVERIFY(!d.update("modifier",1550,250));
  QVERIFY(!d.update("modifier",1650,250));QCOMPARE(d.progress(1650,250),0.4);
 }
 void consumedDwellDoesNotRefill(){
  Dwell d;QVERIFY(!d.update("key",0,500));
  for(int t=100;t<500;t+=100){QVERIFY(!d.update("key",t,500));QCOMPARE(d.progress(t,500),t/500.0);}
  QVERIFY(d.update("key",500,500));QCOMPARE(d.progress(500,500),0.0);
  d.consume("key",500);
  for(int t=525;t<2000;t+=25){QVERIFY(!d.update("key",t,500));QCOMPARE(d.progress(t,500),0.0);}
  d.update("",2000,500);QVERIFY(!d.update("key",2025,500));
  QVERIFY(!d.update("key",2125,500));QCOMPARE(d.progress(2125,500),0.2);
  d.consume("clicked",2200);
  for(int t=2225;t<3000;t+=25){QVERIFY(!d.update("clicked",t,500));QCOMPARE(d.progress(t,500),0.0);}
  QVERIFY(!d.update("other",3000,500));QVERIFY(!d.update("other",3100,500));QCOMPARE(d.progress(3100,500),0.2);
 }
 void explicitActivationDoesNotFireAgain(){Dwell d;d.consume("Pause",0);for(int t=25;t<1500;t+=25)QVERIFY(!d.update("Pause",t,500));d.update("",1500,500);QVERIFY(!d.update("Pause",1550,500));}
 void changingTargetCancels(){Dwell d;d.update("a",0,150);d.update("a",100,150);QVERIFY(!d.update("b",160,150));QVERIFY(!d.update("b",260,150));QVERIFY(d.update("b",310,150));}
 void packLoadsAndLinksResolve(){auto r=KeyboardPack::audit(PACK_PATH);QCOMPARE(r["fileCount"].toInt(),32);QCOMPARE(r["keyCount"].toInt(),841);QVERIFY2(r["errors"].toStringList().isEmpty(),qPrintable(r["errors"].toStringList().join('\n')));}
 void textPreservesWhitespaceAndOrder(){
  QTemporaryDir dir;QFile f(dir.filePath("test.xml"));QVERIFY(f.open(QIODevice::WriteOnly));f.write("<Keyboard><DynamicKey row=\"2\"><KeyDown>LSHIFT</KeyDown><Text>&#9;</Text><Wait>50</Wait><KeyUp>LSHIFT</KeyUp></DynamicKey></Keyboard>");f.close();
  auto r=KeyboardPack::load(f.fileName());auto key=r["children"].toList()[0].toMap();QCOMPARE(key["attributes"].toMap()["row"].toString(),QString("2"));
  auto children=key["children"].toList();QCOMPARE(children[1].toMap()["text"].toString(),QString("\t"));QCOMPARE(children[2].toMap()["tag"].toString(),QString("Wait"));
 }
 void rejectsMalformedAndEntities(){
  QTemporaryDir dir;QFile f(dir.filePath("bad.xml"));QVERIFY(f.open(QIODevice::WriteOnly));f.write("<!DOCTYPE Keyboard [<!ENTITY x 'value'>]><Keyboard>&x;</Keyboard>");f.close();QVERIFY(KeyboardPack::load(f.fileName()).contains("error"));
 }
};
QTEST_GUILESS_MAIN(CoreTests)
#include "CoreTests.moc"
