#include <QtTest>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QInputMethodEvent>
#include <QStringListModel>
#include "LocalScratchpadInput.h"
#include "ActionRunner.h"
#include <memory>

class FakeLocalEngine:public QObject {
 Q_OBJECT
 Q_PROPERTY(QObject* candidateModel READ candidateModel CONSTANT)
 Q_PROPERTY(QStringList availableLocales READ availableLocales CONSTANT)
 Q_PROPERTY(QString locale MEMBER selectedLocale NOTIFY candidatesChanged)
public:
 QPointer<QObject> editor;
 QList<int> keys;
 bool accepts=true;
 QStringListModel model;
 QString selectedLocale="en_US";
 QObject *candidateModel(){return &model;}
 QStringList availableLocales()const{return {"en_US","ja_JP"};}
 Q_INVOKABLE QVariant setLocale(QVariant locale){
  if(!availableLocales().contains(locale.toString()))return false;
  selectedLocale=locale.toString();model.setStringList({});emit candidatesChanged();return true;
 }
 Q_INVOKABLE QVariant selectCandidate(QVariant index){
  const int row=index.toInt();if(!editor||row<0||row>=model.rowCount())return false;
  QInputMethodEvent event;event.setCommitString(model.data(model.index(row,0)).toString());QCoreApplication::sendEvent(editor,&event);
  model.setStringList({});emit candidatesChanged();return true;
 }
 Q_INVOKABLE QVariant dispatch(QVariant key,QVariant text,QVariant modifiers){
  Q_UNUSED(text);Q_UNUSED(modifiers);keys<<key.toInt();
  if(!accepts||!editor)return false;
  if(keys.size()==1){QInputMethodEvent event(QString::fromUtf8("に"),{});QCoreApplication::sendEvent(editor,&event);}
  else {QInputMethodEvent event;event.setCommitString(QString::fromUtf8("日本"));QCoreApplication::sendEvent(editor,&event);}
  return true;
 }
signals:
 void candidatesChanged();
};

class ScratchpadTests:public QObject {
 Q_OBJECT
private slots:
 void completionUsesCaretAndReplacesOnlyWordSuffix(){
  QQmlEngine engine;QQmlComponent component(&engine,QUrl::fromLocalFile(SCRATCHPAD_QML));
  std::unique_ptr<QObject> editor(component.create());QVERIFY2(editor,qPrintable(component.errorString()));
  editor->setProperty("readOnly",false);editor->setProperty("committedText","thank yuo today");editor->setProperty("cursorPosition",7);
  LocalScratchpadInput input;input.setEditor(editor.get());QString text;int cursor=0;
  QVERIFY(input.predictionContext(text,cursor));QCOMPARE(cursor,7);
  QVERIFY(input.complete("you","eng",true,text,cursor));
  QCOMPARE(editor->property("text").toString(),QString("thank you today"));
  QVERIFY(!input.complete("you","eng",true,text,cursor));
  editor->setProperty("committedText","thank y!");editor->setProperty("cursorPosition",7);
  QVERIFY(input.predictionContext(text,cursor));QVERIFY(input.complete("you","eng",true,text,cursor));
  QCOMPARE(editor->property("text").toString(),QString("thank you!"));
  QVERIFY(QMetaObject::invokeMethod(editor.get(),"select",Q_ARG(int,0),Q_ARG(int,5)));
  QVERIFY(!input.predictionContext(text,cursor));
  editor->setProperty("cursorPosition",editor->property("text").toString().size());
  QInputMethodEvent preedit("pending",{});QCoreApplication::sendEvent(editor.get(),&preedit);
  QVERIFY(!input.predictionContext(text,cursor));
 }
 void qtVirtualKeyboardAdapterRoutesRealKeys(){
  if(!qEnvironmentVariableIsSet("GAZE_GRID_TEST_LOCAL_IME"))QSKIP("Optional Qt Virtual Keyboard runtime test");
  QQmlEngine engine;QQmlComponent component(&engine,QUrl::fromLocalFile(SCRATCHPAD_QML));
  std::unique_ptr<QObject> editor(component.create());QVERIFY2(editor,qPrintable(component.errorString()));
  QQmlComponent adapterComponent(&engine,QUrl::fromLocalFile(LOCAL_IME_QML));
  std::unique_ptr<QObject> adapter(adapterComponent.create());QVERIFY2(adapter,qPrintable(adapterComponent.errorString()));
  QSignalSpy candidateChanges(adapter.get(),SIGNAL(candidatesChanged()));
  QQuickWindow window;window.resize(600,200);window.show();
  auto item=qobject_cast<QQuickItem*>(editor.get());QVERIFY(item);item->setParentItem(window.contentItem());item->setSize(QSizeF(600,200));
  auto panel=qobject_cast<QQuickItem*>(adapter.get());QVERIFY(panel);panel->setParentItem(window.contentItem());
  editor->setProperty("readOnly",false);item->forceActiveFocus();window.requestActivate();
  QTRY_VERIFY(item->hasActiveFocus());
  QTRY_COMPARE(QGuiApplication::focusObject(),editor.get());
  LocalScratchpadInput input;input.setEditor(editor.get());input.setEngine(adapter.get());
  QTRY_VERIFY(input.availableLocales().contains("en_US"));
  QVERIFY2(input.setLocale("en_US"),qPrintable(input.error()));
  QTRY_COMPARE(input.locale(),QString("en_US"));
  QTRY_VERIFY(adapter->property("ready").toBool());
  QVERIFY2(input.text("hi",{},true),qPrintable(input.error()));
  QVERIFY2(input.key("SPACE",{},true),qPrintable(input.error()));
  QTRY_COMPARE(editor->property("text").toString(),QString("hi "));
  if(input.availableLocales().contains("zh_CN")){
   QVERIFY2(input.setLocale("zh_CN"),qPrintable(input.error()));QTRY_COMPARE(input.locale(),QString("zh_CN"));
   QVERIFY(input.clear());
   candidateChanges.clear();
   QVERIFY2(input.text("nihao",{},true),qPrintable(input.error()));
   QTRY_VERIFY(!input.candidates().isEmpty());
   QTRY_VERIFY(!candidateChanges.isEmpty());
   const auto expected=input.candidates().front();QVERIFY(!expected.isEmpty());
   QVERIFY2(input.selectCandidate(0,expected),qPrintable(input.error()));
   QTRY_COMPARE(editor->property("text").toString(),expected);
   QVERIFY(!editor->property("inputMethodComposing").toBool());
   qInfo()<<"Real Pinyin candidate committed:"<<expected;
  }else qWarning()<<"zh_CN is unavailable in this Qt Virtual Keyboard build";
 }
 void languageAndCandidatesUseEngineModel(){
  QQmlEngine engine;QQmlComponent component(&engine,QUrl::fromLocalFile(SCRATCHPAD_QML));
  std::unique_ptr<QObject> editor(component.create());QVERIFY2(editor,qPrintable(component.errorString()));
  editor->setProperty("readOnly",false);LocalScratchpadInput input;input.setEditor(editor.get());
  FakeLocalEngine method;method.editor=editor.get();input.setEngine(&method);
  QCOMPARE(input.availableLocales(),QStringList({"en_US","ja_JP"}));QVERIFY(!input.setLocale("xx"));
  QVERIFY(input.setLocale("ja_JP"));QCOMPARE(input.locale(),QString("ja_JP"));
  method.model.setStringList({QString::fromUtf8("日本"),QString::fromUtf8("二本")});
  QCOMPARE(input.candidates().size(),2);QVERIFY(!input.selectCandidate(0,"stale"));
  QVERIFY(!input.selectCandidate(-1,"invalid"));QVERIFY(!input.selectCandidate(2,"invalid"));
  QVERIFY(input.selectCandidate(1,QString::fromUtf8("二本")));
  QCOMPARE(editor->property("text").toString(),QString::fromUtf8("二本"));QVERIFY(input.candidates().isEmpty());
 }
 void realJapaneseCompositionEditingAndCancellation(){
  if(!qEnvironmentVariableIsSet("GAZE_GRID_TEST_LOCAL_IME"))QSKIP("Optional Qt Virtual Keyboard runtime test");
  QQmlEngine engine;QQmlComponent component(&engine,QUrl::fromLocalFile(SCRATCHPAD_QML));
  std::unique_ptr<QObject> editor(component.create());QVERIFY2(editor,qPrintable(component.errorString()));
  QQmlComponent adapterComponent(&engine,QUrl::fromLocalFile(LOCAL_IME_QML));
  std::unique_ptr<QObject> adapter(adapterComponent.create());QVERIFY2(adapter,qPrintable(adapterComponent.errorString()));
  QQuickWindow window;window.resize(600,200);window.show();
  auto item=qobject_cast<QQuickItem*>(editor.get());QVERIFY(item);item->setParentItem(window.contentItem());item->setSize({600,200});
  auto panel=qobject_cast<QQuickItem*>(adapter.get());QVERIFY(panel);panel->setParentItem(window.contentItem());
  editor->setProperty("readOnly",false);item->forceActiveFocus();window.requestActivate();
  QTRY_COMPARE(QGuiApplication::focusObject(),editor.get());
  LocalScratchpadInput input;input.setEditor(editor.get());input.setEngine(adapter.get());
  QTRY_VERIFY(!input.availableLocales().isEmpty());qInfo()<<"Engine locales:"<<input.availableLocales();
  if(!input.availableLocales().contains("ja_JP"))QSKIP("Japanese engine not included in this Qt build");
  QVERIFY(input.setLocale("ja_JP"));QTRY_COMPARE(input.locale(),QString("ja_JP"));
  QTRY_VERIFY(adapter->property("ready").toBool());
  QVERIFY(input.text("nihonn",{},true));
  qInfo()<<"Japanese preedit:"<<editor->property("preeditText")<<"candidates:"<<input.candidates().mid(0,20);
  const QString expected=QString::fromUtf8("\xe6\x97\xa5\xe6\x9c\xac");
  QTRY_VERIFY(input.candidates().contains(expected));
  QVERIFY(input.selectCandidate(input.candidates().indexOf(expected),expected));
  QTRY_COMPARE(editor->property("text").toString(),expected);
  QVERIFY(input.key("BACK",{},true));QTRY_COMPARE(editor->property("text").toString(),expected.left(1));
  QVERIFY(input.clear());QVERIFY(input.text("ka",{},true));
  QTRY_VERIFY(editor->property("inputMethodComposing").toBool());
  input.cancelComposition();QTRY_VERIFY(!editor->property("inputMethodComposing").toBool());
  QVERIFY(editor->property("text").toString().isEmpty());
  QVERIFY(input.setLocale("en_US"));QTRY_COMPARE(input.locale(),QString("en_US"));
  QVERIFY(input.text("ok",{},true));QVERIFY(input.key("SPACE",{},true));
  QTRY_COMPARE(editor->property("text").toString(),QString("ok "));
 }
 void realKoreanComposition(){
  if(!qEnvironmentVariableIsSet("GAZE_GRID_TEST_LOCAL_IME"))QSKIP("Optional Qt Virtual Keyboard runtime test");
  QQmlEngine engine;QQmlComponent component(&engine,QUrl::fromLocalFile(SCRATCHPAD_QML));
  std::unique_ptr<QObject> editor(component.create());QVERIFY2(editor,qPrintable(component.errorString()));
  QQmlComponent adapterComponent(&engine,QUrl::fromLocalFile(LOCAL_IME_QML));
  std::unique_ptr<QObject> adapter(adapterComponent.create());QVERIFY2(adapter,qPrintable(adapterComponent.errorString()));
  QQuickWindow window;window.resize(600,200);window.show();
  auto item=qobject_cast<QQuickItem*>(editor.get());QVERIFY(item);item->setParentItem(window.contentItem());item->setSize({600,200});
  auto panel=qobject_cast<QQuickItem*>(adapter.get());QVERIFY(panel);panel->setParentItem(window.contentItem());
  editor->setProperty("readOnly",false);item->forceActiveFocus();window.requestActivate();
  QTRY_COMPARE(QGuiApplication::focusObject(),editor.get());
  LocalScratchpadInput input;input.setEditor(editor.get());input.setEngine(adapter.get());
  QTRY_VERIFY(!input.availableLocales().isEmpty());qInfo()<<"Engine locales:"<<input.availableLocales();
  if(!input.availableLocales().contains("ko_KR"))QSKIP("Korean engine not included in this Qt build");
  QVERIFY(input.setLocale("ko_KR"));QTRY_COMPARE(input.locale(),QString("ko_KR"));
  QTRY_VERIFY(adapter->property("ready").toBool());
  QCOMPARE(input.inputLabel("g"),QString(QChar(0x314e)));
  QCOMPARE(input.inputLabel("r",true),QString(QChar(0x3132)));
  QVERIFY(input.text("gksrmf",{},true));QVERIFY(input.key("SPACE",{},true));
  const auto expected=QString::fromUtf8("\xed\x95\x9c\xea\xb8\x80 ");
  QTRY_COMPARE(editor->property("text").toString(),expected);
  QVERIFY(!editor->property("inputMethodComposing").toBool());
 }
 void gazeTextUsesCursorSelectionAndQtBackspace(){
  QQmlEngine engine;QQmlComponent component(&engine,QUrl::fromLocalFile(SCRATCHPAD_QML));
  std::unique_ptr<QObject> editor(component.create());QVERIFY2(editor,qPrintable(component.errorString()));
  editor->setProperty("readOnly",false);editor->setProperty("committedText","abcd");editor->setProperty("cursorPosition",2);
  LocalScratchpadInput input;input.setEditor(editor.get());QVERIFY(!input.atEnd());
  QVERIFY(input.text("X",{},false));QCOMPARE(editor->property("text").toString(),QString("abXcd"));
  QVERIFY(QMetaObject::invokeMethod(editor.get(),"select",Q_ARG(int,1),Q_ARG(int,4)));
  QVERIFY(input.text(QString::fromUtf8("😀"),{},false));QCOMPARE(editor->property("text").toString(),QString::fromUtf8("a😀d"));
  QVERIFY(input.key("BACK",{},false));QCOMPARE(editor->property("text").toString(),QString("ad"));
  QVERIFY(input.key("END",{},false));QVERIFY(input.atEnd());
  QVERIFY(input.key("HOME",{},false));QVERIFY(!input.atEnd());
  QVERIFY(input.key("DELETE",{},false));QCOMPARE(editor->property("text").toString(),QString("d"));
 }
 void gazeEngineProducesPreeditWithoutGuessingCommittedText(){
  QQmlEngine engine;QQmlComponent component(&engine,QUrl::fromLocalFile(SCRATCHPAD_QML));
  std::unique_ptr<QObject> editor(component.create());QVERIFY2(editor,qPrintable(component.errorString()));
  editor->setProperty("readOnly",false);LocalScratchpadInput input;input.setEditor(editor.get());
  QVERIFY(!input.text("n",{},true));QVERIFY(editor->property("text").toString().isEmpty());
  FakeLocalEngine method;method.editor=editor.get();input.setEngine(&method);
  QVERIFY(input.text("n",{},true));QCOMPARE(method.keys,QList<int>{Qt::Key_N});
  QVERIFY(editor->property("inputMethodComposing").toBool());QVERIFY(editor->property("text").toString().isEmpty());
  QVERIFY(!input.text("personal",{},false));QVERIFY(editor->property("inputMethodComposing").toBool());
  QVERIFY(input.text("i",{},true));QCOMPARE(editor->property("text").toString(),QString::fromUtf8("日本"));
  method.accepts=false;QVERIFY(!input.text("x",{},true));QCOMPARE(editor->property("text").toString(),QString::fromUtf8("日本"));
  QInputMethodEvent pending("pending",{});QCoreApplication::sendEvent(editor.get(),&pending);
  QVERIFY(input.clear());QVERIFY(editor->property("text").toString().isEmpty());QVERIFY(!editor->property("inputMethodComposing").toBool());
  editor->setProperty("readOnly",true);QVERIFY(!input.key("BACK",{},false));
  editor.reset();QVERIFY(!input.attached());QVERIFY(!input.text("late",{},false));
 }
 void runnerRoutesLocalActionsWithoutDesktopInputOrDuplicateEcho(){
  QQmlEngine engine;QQmlComponent component(&engine,QUrl::fromLocalFile(SCRATCHPAD_QML));
  std::unique_ptr<QObject> editor(component.create());QVERIFY2(editor,qPrintable(component.errorString()));
  editor->setProperty("readOnly",false);editor->setProperty("committedText","abcd");editor->setProperty("cursorPosition",2);
  LocalScratchpadInput input;input.setEditor(editor.get());ActionRunner runner;runner.textOnly=true;int desktop=0;
  runner.sendKey=[&](QString,bool){++desktop;return false;};runner.sendText=[&](QString){++desktop;return false;};
  runner.sendLocalText=[&](QString text,QStringList held){return input.text(text,held,false);};
  runner.sendLocalKey=[&](QString key,QStringList held){return input.key(key,held,false);};
  runner.canEditLocal=[&]{return input.attached();};
  QSignalSpy committed(&runner,&ActionRunner::textCommitted),finished(&runner,&ActionRunner::finished);
  QVERIFY(runner.start({QVariantMap{{"tag","Text"},{"text","X"}}}));QTRY_COMPARE(finished.size(),1);
  QCOMPARE(editor->property("text").toString(),QString("abXcd"));QCOMPARE(committed.size(),0);QCOMPARE(desktop,0);
  QVERIFY(runner.start({QVariantMap{{"tag","Action"},{"text","Back"}}}));QTRY_COMPARE(finished.size(),2);
  QCOMPARE(editor->property("text").toString(),QString("abcd"));QCOMPARE(committed.size(),0);QCOMPARE(desktop,0);
 }
 void qtOwnsPreeditCommitAndReplacement(){
  QQmlEngine engine;QQmlComponent component(&engine,QUrl::fromLocalFile(SCRATCHPAD_QML));
  std::unique_ptr<QObject> editor(component.create());QVERIFY2(editor,qPrintable(component.errorString()));
  editor->setProperty("readOnly",false);
  QSignalSpy edits(editor.get(),SIGNAL(committedTextEdited(QString)));
  QInputMethodEvent preedit(QString::fromUtf8("にほん"),{});QCoreApplication::sendEvent(editor.get(),&preedit);
  QVERIFY(editor->property("inputMethodComposing").toBool());QVERIFY(editor->property("text").toString().isEmpty());QCOMPARE(edits.size(),0);
  QInputMethodEvent commit;commit.setCommitString(QString::fromUtf8("日本語😀"));QCoreApplication::sendEvent(editor.get(),&commit);
  QCOMPARE(editor->property("text").toString(),QString::fromUtf8("日本語😀"));QVERIFY(!editor->property("inputMethodComposing").toBool());
  QVERIFY(!edits.isEmpty());QCOMPARE(edits.last().front().toString(),QString::fromUtf8("日本語😀"));
  QInputMethodEvent replacement;replacement.setCommitString("!",-2,2);QCoreApplication::sendEvent(editor.get(),&replacement);
  QCOMPARE(editor->property("text").toString(),QString::fromUtf8("日本語!"));
  QInputMethodEvent pending("pending",{});QCoreApplication::sendEvent(editor.get(),&pending);
  QInputMethodEvent cancel;QCoreApplication::sendEvent(editor.get(),&cancel);
  QCOMPARE(editor->property("text").toString(),QString::fromUtf8("日本語!"));QVERIFY(!editor->property("inputMethodComposing").toBool());
 }
 void readOnlyScratchpadDoesNotAcceptCommits(){
  QQmlEngine engine;QQmlComponent component(&engine,QUrl::fromLocalFile(SCRATCHPAD_QML));
  std::unique_ptr<QObject> editor(component.create());QVERIFY2(editor,qPrintable(component.errorString()));
  editor->setProperty("committedText","personal");QVERIFY(editor->property("readOnly").toBool());
  QSignalSpy edits(editor.get(),SIGNAL(committedTextEdited(QString)));
  QInputMethodEvent commit;commit.setCommitString("unexpected");QCoreApplication::sendEvent(editor.get(),&commit);
  QCOMPARE(editor->property("text").toString(),QString("personal"));QCOMPARE(edits.size(),0);
 }
 void textRemainsPlainAndControllerUpdatesDoNotEcho(){
  QQmlEngine engine;QQmlComponent component(&engine,QUrl::fromLocalFile(SCRATCHPAD_QML));
  std::unique_ptr<QObject> editor(component.create());QVERIFY2(editor,qPrintable(component.errorString()));
  editor->setProperty("readOnly",false);QSignalSpy edits(editor.get(),SIGNAL(committedTextEdited(QString)));
  editor->setProperty("committedText","<b>literal</b>");
  QCOMPARE(editor->property("text").toString(),QString("<b>literal</b>"));QCOMPARE(edits.size(),0);
 }
};
int main(int argc,char **argv){
 if(qEnvironmentVariableIsSet("GAZE_GRID_TEST_LOCAL_IME")){qputenv("QT_IM_MODULE","qtvirtualkeyboard");qputenv("QT_VIRTUALKEYBOARD_DESKTOP_DISABLE","1");}
 QGuiApplication app(argc,argv);ScratchpadTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "ScratchpadTests.moc"
