#include <QtTest>
#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlProperty>
#include <QQmlError>
#include <QQuickWindow>
#include <QQuickItem>
#include <QInputMethodEvent>
#include <QStringListModel>
#include <QTemporaryDir>
#include "Controller.h"
#include "SymbolProvider.h"
#include "AppPaths.h"
#include <memory>

class UiInputEngine:public QObject {
 Q_OBJECT
 Q_PROPERTY(QObject* candidateModel READ candidateModel CONSTANT)
 Q_PROPERTY(QStringList availableLocales READ availableLocales CONSTANT)
 Q_PROPERTY(QString locale MEMBER locale NOTIFY candidatesChanged)
 Q_PROPERTY(bool requiresFocus MEMBER requiresFocus)
public:
 QPointer<QObject> editor;QStringListModel model;QString locale="en_US",roman;
 int keyCount=0,selected=-1;bool requiresFocus=false,wrongFocus=false;
 QObject *candidateModel(){return &model;}
 QStringList availableLocales()const{return {"en_US","zh_CN"};}
 Q_INVOKABLE QVariant setLocale(QVariant value){
  if(!availableLocales().contains(value.toString()))return false;
  locale=value.toString();roman.clear();model.setStringList({});emit candidatesChanged();return true;
 }
 Q_INVOKABLE QVariant dispatch(QVariant key,QVariant text,QVariant modifiers){
  Q_UNUSED(modifiers);if(!editor)return false;++keyCount;
  if(requiresFocus&&QGuiApplication::focusObject()!=editor)wrongFocus=true;
  if(key.toInt()==Qt::Key_Backspace){roman.chop(1);}else roman+=text.toString();
  QInputMethodEvent event(roman,{});QCoreApplication::sendEvent(editor,&event);
  QStringList words;if(roman.size()>=2){words<<QString::fromUtf8("你好");for(int i=1;i<18;++i)words<<QString("candidate %1").arg(i+1);}
  model.setStringList(words);emit candidatesChanged();return true;
 }
 Q_INVOKABLE void cancel(){roman.clear();model.setStringList({});emit candidatesChanged();}
 Q_INVOKABLE QVariant selectCandidate(QVariant value){
  const int row=value.toInt();if(!editor||row<0||row>=model.rowCount())return false;
  selected=row;const auto word=model.data(model.index(row,0)).toString();
  QInputMethodEvent event;event.setCommitString(word);QCoreApplication::sendEvent(editor,&event);
  roman.clear();model.setStringList({});emit candidatesChanged();return true;
 }
signals:
 void candidatesChanged();
};

class LocalImeUiTests:public QObject {
 Q_OBJECT
 struct View {
  QStringList bindingWarnings;
  Controller controller;
  QQmlApplicationEngine engine;
  QQuickWindow *window=nullptr;
  QPointer<QObject> editor;
  UiInputEngine method;
  static QQuickItem *visual(QQuickItem *parent,const QString &name){
   if(parent->objectName()==name)return parent;
   for(auto child:parent->childItems())if(auto found=visual(child,name))return found;
   return nullptr;
  }
  explicit View(bool real=false){
   QObject::connect(&engine,&QQmlEngine::warnings,&engine,[this](const QList<QQmlError> &errors){for(const auto &error:errors)if(error.url().toString()=="qrc:/ui/Main.qml"&&error.description().contains("Binding loop"))bindingWarnings<<error.toString();});
   controller.resetPreferences();controller.setGazeEnabled(false);controller.setProperty("paused",false);
   controller.setAllGroupDwellTimes(250);controller.openLayout("keyboard-conversation-bottom",false);
   engine.addImageProvider("symbols",new SymbolProvider);
   engine.rootContext()->setContextProperty("controller",&controller);
   engine.load(QUrl("qrc:/ui/Main.qml"));
   if(engine.rootObjects().isEmpty())return;
   window=qobject_cast<QQuickWindow*>(engine.rootObjects().front());if(!window)return;
   window->resize(1100,640);window->setProperty("gazeCheck",true);
   window->setProperty("logicalX",0);window->setProperty("logicalY",0);
   window->setProperty("screenWidth",1100);window->setProperty("screenHeight",640);
   QQmlProperty::write(window,"gazeEnabled",true);QQmlProperty::write(window,"trackerConnected",true);
   if(!real){auto loader=window->findChild<QObject*>("localInputMethodLoader");if(loader)QQmlProperty::write(loader,"active",false);}
   window->show();QCoreApplication::processEvents();
   const auto content=keys();for(int i=0;i<content.size();++i)if(content[i].toMap()["kind"]=="Scratchpad"){
    editor=visual(window->contentItem(),QString("scratchpadEditor:%1").arg(i));break;
   }
   if(!real){method.editor=editor;controller.attachScratchpadInputEngine(&method);}
   controller.setSystemKeyboard(true);
   window->setProperty("gazeCheck",false);
  }
  ~View(){if(window)window->setProperty("gazeCheck",true);controller.setProperty("paused",true);}
  QVariantList keys()const{return controller.property("layout").toMap()["keys"].toList();}
  int find(const QString &tag,const QString &text)const{
   const auto list=keys();for(int i=0;i<list.size();++i)for(auto value:list[i].toMap()["commands"].toList()){
    const auto command=value.toMap();if(command["tag"]==tag&&command["text"].toString().trimmed()==text)return i;
   }
   return -1;
  }
  void lookAt(const QPointF &point){
   window->setProperty("gaze",QPointF(point.x()/window->width(),point.y()/window->height()));window->setProperty("gazeValid",true);
  }
  void lookAtKey(int index){
   const auto key=keys()[index].toMap(),layout=controller.property("layout").toMap();
   lookAt({(key["col"].toDouble()+key["width"].toDouble()/2)*window->width()/layout["cols"].toDouble(),(key["row"].toDouble()+key["height"].toDouble()/2)*window->height()/layout["rows"].toDouble()});
  }
  void lookAtCandidate(int slot){
   auto mouse=visual(window->contentItem(),QString("predictionMouseArea:%1").arg(slot));
   if(mouse)lookAt(mouse->mapToScene(QPointF(mouse->width()/2,mouse->height()/2)));
  }
  void stop(){window->setProperty("gazeValid",false);controller.hover("");}
 };
private slots:
 void committedPredictionsUseCaretAndRejectOldTargets(){
  View view;QVERIFY(view.window);QVERIFY(view.editor);
  view.controller.setSystemKeyboard(false);
  QVERIFY(view.controller.setScratchpadText("thank yuo today"));view.editor->setProperty("cursorPosition",7);
  QTRY_VERIFY(view.controller.displaySuggestions().contains("you"));
  const auto stale=view.controller.suggestionPrefix()+QString::number(view.controller.displaySuggestions().indexOf("you"));
  view.editor->setProperty("cursorPosition",0);
  view.controller.clickCommand(stale);
  QCOMPARE(view.editor->property("text").toString(),QString("thank yuo today"));
  view.editor->setProperty("cursorPosition",7);
  QTRY_VERIFY(view.controller.displaySuggestions().contains("you"));
  view.controller.clickCommand(view.controller.suggestionPrefix()+QString::number(view.controller.displaySuggestions().indexOf("you")));
  QCOMPARE(view.editor->property("text").toString(),QString("thank you today"));
  view.controller.setSystemKeyboard(true);
  view.method.model.setStringList({"candidate"});emit view.method.candidatesChanged();
  QVERIFY(view.controller.suggestionPrefix().startsWith("ime:"));
  view.method.model.setStringList({});emit view.method.candidatesChanged();
  QVERIFY(view.controller.setScratchpadText("thank y"));view.editor->setProperty("cursorPosition",7);
  QTRY_VERIFY(view.controller.displaySuggestions().contains("you"));
  QVERIFY(view.controller.suggestionPrefix().startsWith("suggest:"));
  const int before=view.method.keyCount;
  view.controller.clickCommand(view.controller.suggestionPrefix()+QString::number(view.controller.displaySuggestions().indexOf("you")));
  QCOMPARE(view.editor->property("text").toString(),QString("thank you "));
  QCOMPARE(view.method.keyCount,before);
 }
 void fullUiLanguageDwellPagingAndCommit(){
  View view;QVERIFY(view.window);QVERIFY(view.editor);QVERIFY(view.controller.localImeActive());
  auto language=view.window->findChild<QObject*>("localImeLanguageSelector");QVERIFY(language);
  language->setProperty("currentIndex",1);QVERIFY(QMetaObject::invokeMethod(language,"activated",Q_ARG(int,1)));
  QCOMPARE(view.controller.localImeLocale(),QString("zh_CN"));QCOMPARE(view.method.locale,QString("zh_CN"));
  for(const auto &letter:{QString("n"),QString("i")}){
   const int key=view.find("Text",letter);QVERIFY(key>=0);const int before=view.method.keyCount;view.lookAtKey(key);
   QTRY_COMPARE(view.method.keyCount,before+1);
   if(letter=="n"){QTest::qWait(600);QCOMPARE(view.method.keyCount,before+1);}
   view.stop();
  }
  QVERIFY(view.editor->property("inputMethodComposing").toBool());QVERIFY(view.controller.property("scratchpad").toString().isEmpty());
  QCOMPARE(view.controller.displaySuggestions().size(),6);
  const int next=view.find("Action","NextSuggestions");QVERIFY(next>=0);view.lookAtKey(next);
  QTRY_COMPARE(view.controller.displaySuggestions().front(),QString("candidate 7"));
  QTest::qWait(600);QCOMPARE(view.controller.displaySuggestions().front(),QString("candidate 7"));view.stop();
  view.lookAtCandidate(1);QTRY_COMPARE(view.method.selected,7);view.stop();
  QTRY_COMPARE(view.controller.property("scratchpad").toString(),QString("candidate 8"));
  QVERIFY(!view.editor->property("inputMethodComposing").toBool());QVERIFY(!view.controller.ready());QVERIFY(!view.controller.isTrackerConnected());
  QVERIFY2(view.bindingWarnings.isEmpty(),qPrintable(view.bindingWarnings.join('\n')));
 }
 void missingEngineCannotFallBackToDesktop(){
  View view;QVERIFY(view.window);view.controller.attachScratchpadInputEngine(nullptr);
  view.controller.setProperty("actionStatus","waiting");
  const int key=view.find("Text","n");QVERIFY(key>=0);view.lookAtKey(key);
  QTRY_VERIFY(view.controller.property("actionStatus").toString().contains("Local input method unavailable"));view.stop();
  QVERIFY(view.controller.property("scratchpad").toString().isEmpty());QCOMPARE(view.method.keyCount,0);QVERIFY(!view.controller.ready());
 }
 void pauseAndNavigationCancelComposition(){
  View view;QVERIFY(view.window);QVERIFY(view.editor);
  const int key=view.find("Text","n");QVERIFY(key>=0);view.lookAtKey(key);QTRY_COMPARE(view.method.keyCount,1);view.stop();
  QVERIFY(view.editor->property("inputMethodComposing").toBool());view.controller.togglePause();
  QTRY_VERIFY(!view.editor->property("inputMethodComposing").toBool());
  QVERIFY(!view.controller.localImeActive());QVERIFY(view.controller.suggestionPrefix().startsWith("suggest:"));
  view.controller.togglePause();view.controller.openLayout("keyboard-alphabet-14x4-bottom",false);
  QVERIFY(!view.controller.scratchpadEditable());QVERIFY(!view.controller.localImeActive());QVERIFY(!view.controller.ready());
 }
 void moduleFailureLeavesPersonalTypingUsable(){
  View view;QVERIFY(view.window);view.controller.attachScratchpadInputEngine(nullptr);
  auto loader=view.window->findChild<QObject*>("localInputMethodLoader");QVERIFY(loader);
  QQmlProperty::write(loader,"source",QUrl("qrc:/ui/UnavailableInputMethod.qml"));QQmlProperty::write(loader,"active",true);
  QTRY_COMPARE(loader->property("status").toInt(),3);QVERIFY(!view.controller.localImeActive());
  view.controller.setSystemKeyboard(false);const int key=view.find("Text","q");QVERIFY(key>=0);view.lookAtKey(key);
  QTRY_COMPARE(view.controller.property("scratchpad").toString(),QString("q"));view.stop();QVERIFY(!view.controller.ready());
 }
 void focusChangesNeverSendToAnotherEditor(){
  View view;QVERIFY(view.window);QVERIFY(view.editor);view.method.requiresFocus=true;
  QQmlComponent component(&view.engine,QUrl("qrc:/ui/ScratchpadEditor.qml"));std::unique_ptr<QObject> other(component.create());
  QVERIFY2(other,qPrintable(component.errorString()));QQuickWindow otherWindow;otherWindow.resize(300,100);
  auto item=qobject_cast<QQuickItem*>(other.get());QVERIFY(item);item->setParentItem(otherWindow.contentItem());item->setSize(QSizeF(300,100));
  other->setProperty("readOnly",false);other->setProperty("committedText","other");otherWindow.show();item->forceActiveFocus();otherWindow.requestActivate();
  QTRY_COMPARE(QGuiApplication::focusObject(),other.get());
  QInputMethodEvent preedit("pending",{});QCoreApplication::sendEvent(other.get(),&preedit);QVERIFY(other->property("inputMethodComposing").toBool());
  view.controller.togglePause();QVERIFY(other->property("inputMethodComposing").toBool());view.controller.togglePause();
  QInputMethodEvent cancel;QCoreApplication::sendEvent(other.get(),&cancel);
  const int key=view.find("Text","n");QVERIFY(key>=0);view.lookAtKey(key);
  QTRY_VERIFY(view.method.keyCount>0||view.controller.property("actionStatus").toString().contains("Focus the scratchpad"));view.stop();
  QCOMPARE(other->property("text").toString(),QString("other"));QVERIFY(!view.method.wrongFocus);QVERIFY(!view.controller.ready());
 }
 void realCompositionThroughProductionUi_data(){
  QTest::addColumn<QString>("locale");QTest::addColumn<QString>("reading");QTest::addColumn<QString>("expected");
  QTest::newRow("Pinyin")<<QString("zh_CN")<<QString("nihao")<<QString::fromUtf8("\xe4\xbd\xa0\xe5\xa5\xbd");
  QTest::newRow("Japanese")<<QString("ja_JP")<<QString("nihonn")<<QString::fromUtf8("\xe6\x97\xa5\xe6\x9c\xac");
 }
 void realCompositionThroughProductionUi(){
  if(!qEnvironmentVariableIsSet("GAZE_GRID_TEST_LOCAL_IME"))QSKIP("Optional real Qt input engine");
  QFETCH(QString,locale);QFETCH(QString,reading);QFETCH(QString,expected);
  View view(true);QVERIFY(view.window);QVERIFY(view.editor);
  QTRY_VERIFY(!view.controller.localImeLocales().isEmpty());
  if(!view.controller.localImeLocales().contains(locale))QSKIP("Language engine not included in this Qt build");
  auto language=view.window->findChild<QObject*>("localImeLanguageSelector");QVERIFY(language);
  QTRY_COMPARE(language->property("count").toInt(),view.controller.localImeLocales().size());
  language->setProperty("currentIndex",view.controller.localImeLocales().indexOf(locale));
  QTRY_COMPARE(language->property("currentText").toString(),locale);
  QVERIFY(QMetaObject::invokeMethod(language,"activated",Q_ARG(int,language->property("currentIndex").toInt())));
  QMetaObject::invokeMethod(view.editor,"prepareInput");QTRY_COMPARE(QGuiApplication::focusObject(),view.editor.data());
  auto loader=view.window->findChild<QObject*>("localInputMethodLoader");QVERIFY(loader);
  QTRY_VERIFY(loader->property("item").value<QObject*>()&&loader->property("item").value<QObject*>()->property("ready").toBool());
  QTRY_COMPARE(loader->property("item").value<QObject*>()->property("locale").toString(),locale);
  for(const auto letter:reading){
   const auto before=view.editor->property("preeditText").toString();
   const int key=view.find("Text",QString(letter));QVERIFY(key>=0);view.lookAtKey(key);
   QTRY_VERIFY(view.editor->property("preeditText").toString()!=before);view.stop();
  }
  QTRY_VERIFY(view.controller.displaySuggestions().contains(expected));
  view.lookAtCandidate(view.controller.displaySuggestions().indexOf(expected));QTRY_COMPARE(view.controller.property("scratchpad").toString(),expected);view.stop();
  QVERIFY(!view.controller.ready());
  QVERIFY2(view.bindingWarnings.isEmpty(),qPrintable(view.bindingWarnings.join('\n')));
 }
};
int main(int argc,char **argv){
 QTemporaryDir profile;if(!profile.isValid())return 1;
 qputenv("GAZE_GRID_CONFIG_DIR",profile.path().toUtf8());qputenv("XDG_CONFIG_HOME",profile.path().toUtf8());
 qputenv("XDG_CACHE_HOME",(profile.path()+"/cache").toUtf8());qputenv("QT_QPA_PLATFORM","offscreen");qputenv("QT_QUICK_BACKEND","software");
 qputenv("QT_QUICK_CONTROLS_STYLE","Fusion");
 if(qEnvironmentVariableIsSet("GAZE_GRID_TEST_LOCAL_IME")){qputenv("QT_IM_MODULE","qtvirtualkeyboard");qputenv("QT_VIRTUALKEYBOARD_DESKTOP_DISABLE","1");}
 QList<QByteArray> arguments;for(int i=0;i<argc;++i)arguments<<QByteArray(argv[i]);arguments<<"--local-ime";
 QList<char*> pointers;for(auto &argument:arguments)pointers<<argument.data();pointers<<nullptr;int count=arguments.size();
 QApplication app(count,pointers.data());app.setOrganizationName("gaze-gridTests");app.setApplicationName("LocalImeUi");
 LocalImeUiTests tests;auto testArguments=app.arguments();testArguments.removeAll("--local-ime");return QTest::qExec(&tests,testArguments);
}
#include "LocalImeUiTests.moc"
