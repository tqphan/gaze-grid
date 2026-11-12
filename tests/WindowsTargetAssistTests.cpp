#include <QtTest>
#include "TargetAssist.h"
#include "PointerActions.h"
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

class WindowsTargetAssistTests:public QObject {
 Q_OBJECT
 struct Controls {
  HWND window=nullptr,button=nullptr;
  Controls(){
   WNDCLASSW type{};type.lpfnWndProc=DefWindowProcW;type.hInstance=GetModuleHandleW(nullptr);type.lpszClassName=L"GazeGridTargetAssistTests";
   RegisterClassW(&type);
   window=CreateWindowExW(WS_EX_TOPMOST|WS_EX_NOACTIVATE,type.lpszClassName,L"Target assist test",WS_POPUP|WS_CLIPCHILDREN,80,80,240,240,nullptr,nullptr,type.hInstance,nullptr);
   if(window)button=CreateWindowExW(0,L"BUTTON",L"",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,100,100,20,20,window,nullptr,type.hInstance,nullptr);
  }
  ~Controls(){if(window)DestroyWindow(window);}
  void show(){ShowWindow(window,SW_SHOWNOACTIVATE);UpdateWindow(window);}
  QPointF gaze()const{
   RECT rect{};GetWindowRect(button,&rect);
   return {double(rect.left-5)/(GetSystemMetrics(SM_CXSCREEN)-1),double(rect.top+10)/(GetSystemMetrics(SM_CYSCREEN)-1)};
  }
 };
 struct Assist:TargetAssist {
  // Native controls live on the test thread. Pump their messages until any
  // in-flight UIA request finishes before TargetAssist joins its worker.
  ~Assist(){QTest::qWait(1000);}
  bool pulls(QPointF gaze){
   QElapsedTimer timeout;timeout.start();
   while(timeout.elapsed()<5000){if(adjust(gaze).x()>gaze.x())return true;QTest::qWait(5);}
   return false;
  }
 };
private slots:
 void discoversSmallControlBetweenPointSamples(){
  Controls controls;QVERIFY(controls.window);QVERIFY(controls.button);controls.show();
  Assist assist;const auto gaze=controls.gaze();
  // The old nine samples all missed this 20px button just 5px from gaze.
  QVERIFY(assist.pulls(gaze));
  assist.configure(0,40,8);QCOMPARE(assist.adjust(gaze),gaze);
  assist.configure(100,40,8);
  QVERIFY(assist.pulls(gaze));
 }
 void ignoresDisabledAndHiddenControls(){
  Controls controls;QVERIFY(controls.window);QVERIFY(controls.button);
  EnableWindow(controls.button,FALSE);controls.show();
  Assist assist;const auto gaze=controls.gaze();
  for(int i=0;i<15;++i){QCOMPARE(assist.adjust(gaze),gaze);QTest::qWait(50);}
  EnableWindow(controls.button,TRUE);
  QVERIFY(assist.pulls(gaze));
  ShowWindow(controls.button,SW_HIDE);assist.reset();
  for(int i=0;i<15;++i){QCOMPARE(assist.adjust(gaze),gaze);QTest::qWait(50);}
 }
 void magneticAssistToggle_data(){
  QTest::addColumn<bool>("relative");
  QTest::addColumn<int>("mode");
  QTest::newRow("absolute-center")<<false<<0;
  QTest::newRow("relative-center")<<true<<0;
  QTest::newRow("absolute-edges")<<false<<1;
  QTest::newRow("relative-edges")<<true<<1;
  QTest::newRow("absolute-combined")<<false<<2;
  QTest::newRow("relative-combined")<<true<<2;
 }
 void magneticAssistToggle(){
  QFETCH(bool,relative);
  QFETCH(int,mode);
  Controls controls;QVERIFY(controls.window);QVERIFY(controls.button);controls.show();
  struct Pointer:PointerActions {~Pointer(){QTest::qWait(1000);}} pointer;
  QPointF moved;auto gaze=controls.gaze();
  if(mode==1)gaze.rx()+=4./(GetSystemMetrics(SM_CXSCREEN)-1);
  pointer.configureTargetAssist(65,40,8,mode,2,1);
  pointer.move=pointer.moveRelativeTo=[&](double x,double y){moved={x,y};return true;};
  pointer.button=[](int,bool){return true;};
  pointer.setMagneticRelative(relative);
  QVERIFY(pointer.start("MouseMagneticCursor"));QTRY_VERIFY(!pointer.busy());
  QVERIFY(pointer.follow(gaze));QCOMPARE(moved,gaze);
  pointer.setTargetAssist(true);
  QElapsedTimer timeout;timeout.start();
  while(timeout.elapsed()<5000){QVERIFY(pointer.follow(gaze));if(moved.x()>gaze.x())break;QTest::qWait(5);}
  QVERIFY(moved.x()>gaze.x());
  // Holding a button suspends assistance without turning magnetic mouse off.
  QVERIFY(pointer.start("MouseLeftDownUp"));QTRY_VERIFY(!pointer.busy());
  QVERIFY(pointer.follow(gaze));QCOMPARE(moved,gaze);
  pointer.setTargetAssist(false);
  QVERIFY(pointer.start("MouseLeftDownUp"));QTRY_VERIFY(!pointer.busy());
  QVERIFY(pointer.follow(gaze));QCOMPARE(moved,gaze);
 }
};
QTEST_GUILESS_MAIN(WindowsTargetAssistTests)
#include "WindowsTargetAssistTests.moc"
