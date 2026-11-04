#include <QtTest>
#include "WindowsTextContext.h"
#include "WindowsAutomationContext.h"
#include <future>
#include <chrono>
class WindowsTextContextTests:public QObject {
 Q_OBJECT
 struct Control {
  HWND window;
  explicit Control(DWORD style=0,const wchar_t *type=L"EDIT"):window(CreateWindowExW(0,type,L"",WS_POPUP|style,0,0,200,100,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr)){}
  ~Control(){if(window)DestroyWindow(window);}
 };
private slots:
 void automationReadsAndReplacesWithoutDesktopInput(){
  Control control;QVERIFY(control.window);SetWindowTextW(control.window,L"thank yuo today");SendMessageW(control.window,EM_SETSEL,7,7);
  auto read=std::async(std::launch::async,[&]{return WindowsAutomationContext::readWindow(control.window);});
  QTRY_VERIFY_WITH_TIMEOUT(read.wait_for(std::chrono::milliseconds(0))==std::future_status::ready,5000);
  const auto context=read.get();QVERIFY(context.supported);QVERIFY(context.available);
  QCOMPARE(context.before,QString("thank y"));QCOMPARE(context.after,QString("uo today"));
  auto replace=std::async(std::launch::async,[&]{return WindowsAutomationContext::replaceWindow(control.window,context,"you","eng",true,[&](const QString &value){
   SendMessageW(control.window,EM_REPLACESEL,TRUE,LPARAM(value.utf16()));return true;
  });});
  QTRY_VERIFY_WITH_TIMEOUT(replace.wait_for(std::chrono::milliseconds(0))==std::future_status::ready,5000);QVERIFY(replace.get());
  const auto updated=WindowsTextContext::readControl(control.window);QCOMPARE(updated.before+updated.after,QString("thank you today"));
 }
 void automationDoesNotReadPasswordText(){
  Control password(ES_PASSWORD);QVERIFY(password.window);SetWindowTextW(password.window,L"secret");
  auto read=std::async(std::launch::async,[&]{return WindowsAutomationContext::readWindow(password.window);});
  QTRY_VERIFY_WITH_TIMEOUT(read.wait_for(std::chrono::milliseconds(0))==std::future_status::ready,5000);
  const auto context=read.get();QVERIFY(context.privateField);QVERIFY(!context.available);QVERIFY(context.before.isEmpty());QVERIFY(context.after.isEmpty());
 }
 void committedUnicodeAndCaret(){
  Control control;QVERIFY(control.window);
  const auto text=QString::fromUtf8("hello \xf0\x9f\x98\x80 yuo later");
  SetWindowTextW(control.window,reinterpret_cast<const wchar_t*>(text.utf16()));
  SendMessageW(control.window,EM_SETSEL,10,10);
  auto context=WindowsTextContext::readControl(control.window);
  QVERIFY(context.supported);QVERIFY(context.available);QVERIFY(!context.privateField);
  QCOMPARE(context.before,text.left(10));QCOMPARE(context.after,text.mid(10));
  SetWindowTextW(control.window,L"external edit");SendMessageW(control.window,EM_SETSEL,3,3);
  context=WindowsTextContext::readControl(control.window);QCOMPARE(context.before,QString("ext"));
 }
 void completionUsesNativeReplacementAndRejectsStaleSnapshots(){
  Control control;QVERIFY(control.window);SetWindowTextW(control.window,L"thank yuo today");SendMessageW(control.window,EM_SETSEL,7,7);
  const auto context=WindowsTextContext::readControl(control.window);
  QVERIFY(WindowsTextContext::replaceControl(control.window,context,"you","eng",true));
  const auto updated=WindowsTextContext::readControl(control.window);
  QCOMPARE(updated.before+updated.after,QString("thank you today"));
  QVERIFY(!WindowsTextContext::replaceControl(control.window,context,"you","eng",true));
  SetWindowTextW(control.window,L"ein haus!");SendMessageW(control.window,EM_SETSEL,6,6);
  QVERIFY(WindowsTextContext::replaceControl(control.window,WindowsTextContext::readControl(control.window),"Haus","deu",true));
  const auto german=WindowsTextContext::readControl(control.window);QCOMPARE(german.before+german.after,QString("ein Haus!"));
 }
 void richEditNewlinesUseNativeCharacterPositions(){
  const auto module=LoadLibraryW(L"Msftedit.dll");QVERIFY(module);
  {
   Control control(ES_MULTILINE,L"RICHEDIT50W");QVERIFY(control.window);
   SetWindowTextW(control.window,L"first\r\nthank yuo\r\nlast");SendMessageW(control.window,EM_SETSEL,13,13);
   const auto context=WindowsTextContext::readControl(control.window);
   QVERIFY(context.available);QCOMPARE(context.before,QString("first\rthank y"));
   QCOMPARE(context.after,QString("uo\rlast"));
   QVERIFY(WindowsTextContext::replaceControl(control.window,context,"you","eng",true));
   const auto updated=WindowsTextContext::readControl(control.window);QCOMPARE(updated.before+updated.after,QString("first\rthank you\rlast"));
  }
  FreeLibrary(module);
 }
 void selectedAndPrivateFieldsExposeNoText(){
  Control readonly(ES_READONLY),numeric(ES_NUMBER);
  QVERIFY(readonly.window);QVERIFY(numeric.window);
  QVERIFY(!WindowsTextContext::readControl(readonly.window).available);
  QVERIFY(!WindowsTextContext::readControl(numeric.window).available);
  Control control;QVERIFY(control.window);SetWindowTextW(control.window,L"private");SendMessageW(control.window,EM_SETSEL,0,2);
  auto context=WindowsTextContext::readControl(control.window);QVERIFY(context.supported);QVERIFY(!context.available);QVERIFY(context.before.isEmpty());
  Control password(ES_PASSWORD);QVERIFY(password.window);SetWindowTextW(password.window,L"secret");
  context=WindowsTextContext::readControl(password.window);QVERIFY(context.privateField);QVERIFY(!context.available);QVERIFY(context.before.isEmpty());QVERIFY(context.after.isEmpty());
  Control unknown(0,L"STATIC");QVERIFY(unknown.window);SetWindowTextW(unknown.window,L"unsupported");
  context=WindowsTextContext::readControl(unknown.window);QVERIFY(!context.supported);QVERIFY(context.before.isEmpty());
 }
};
QTEST_GUILESS_MAIN(WindowsTextContextTests)
#include "WindowsTextContextTests.moc"
