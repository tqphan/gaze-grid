#include <QtTest>
#include <QCoreApplication>
#include <QProcess>
#include "SystemKeyboard.h"
#include "WindowsKeyboardLayout.h"
#include "WindowsInput.h"
#include <algorithm>
#include <cstdio>
#include <vector>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace {
constexpr UINT changeLayout=WM_APP+1, capsState=WM_APP+2, targetReady=WM_APP+3;
const wchar_t *layouts[]={L"00000409",L"0000040C",L"00000407",L"00000419",L"00020409"};
WNDPROC originalEditorProcedure=nullptr;
LRESULT CALLBACK editorProcedure(HWND editor,UINT message,WPARAM wparam,LPARAM lparam){
    if(message==WM_KEYDOWN||message==WM_CHAR){
        std::printf("EDITOR_MESSAGE=%x VALUE=%x SCAN=%x\n",message,unsigned(wparam),unsigned((lparam>>16)&0xff));std::fflush(stdout);
    }
    return CallWindowProcW(originalEditorProcedure,editor,message,wparam,lparam);
}
struct Target {
    HWND editor=nullptr;
    bool ready=false;
    HKL original=GetKeyboardLayout(0);
    std::vector<HKL> existing;
    Target(){existing.resize(GetKeyboardLayoutList(0,nullptr));GetKeyboardLayoutList(int(existing.size()),existing.data());}
};
LRESULT CALLBACK targetProcedure(HWND window,UINT message,WPARAM wparam,LPARAM lparam){
    auto target=reinterpret_cast<Target*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if(message==WM_CREATE){
        target=static_cast<Target*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
        SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(target));
        target->editor=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_MULTILINE,
            0,0,640,240,window,nullptr,GetModuleHandleW(nullptr),nullptr);
        originalEditorProcedure=reinterpret_cast<WNDPROC>(SetWindowLongPtrW(target->editor,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(editorProcedure)));
        return 0;
    }
    if(target){
        if(message==WM_SIZE){MoveWindow(target->editor,0,0,LOWORD(lparam),HIWORD(lparam),TRUE);return 0;}
        if(message==WM_SETFOCUS){SetFocus(target->editor);return 0;}
        if(message==capsState)return GetKeyState(VK_CAPITAL)&1;
        if(message==targetReady)return target->ready;
        if(message==changeLayout){
            if(wparam>=std::size(layouts)||GetForegroundWindow()!=window){
                std::printf("LAYOUT_REQUEST_REJECTED foreground=%d\n",GetForegroundWindow()==window);std::fflush(stdout);return 0;
            }
            const auto layout=LoadKeyboardLayoutW(layouts[wparam],KLF_ACTIVATE);
            wchar_t name[KL_NAMELENGTH]{};
            const bool activated=layout&&ActivateKeyboardLayout(layout,0);
            const bool named=GetKeyboardLayoutNameW(name);
            std::printf("LAYOUT_REQUEST=%ls ACTUAL=%ls ACTIVATED=%d\n",layouts[wparam],name,activated);std::fflush(stdout);
            if(!activated||!named||wcscmp(name,layouts[wparam]))return 0;
            SetFocus(target->editor);
            return reinterpret_cast<LRESULT>(layout);
        }
        if(message==WM_CLOSE){
            ActivateKeyboardLayout(target->original,0);
            const bool restored=GetKeyboardLayout(0)==target->original;
            std::vector<HKL> loaded(GetKeyboardLayoutList(0,nullptr));
            GetKeyboardLayoutList(int(loaded.size()),loaded.data());
            bool unloaded=true;
            for(auto layout:loaded)if(std::find(target->existing.begin(),target->existing.end(),layout)==target->existing.end())
                unloaded=bool(UnloadKeyboardLayout(layout))&&unloaded;
            std::printf("LAYOUT_RESTORED=%d NEW_LAYOUTS_UNLOADED=%d\n",restored,unloaded);std::fflush(stdout);
            DestroyWindow(window);return 0;
        }
    }
    if(message==WM_DESTROY){PostQuitMessage(0);return 0;}
    return DefWindowProcW(window,message,wparam,lparam);
}
int runTarget(const wchar_t *title){
    Target target;
    WNDCLASSW windowClass{};windowClass.lpfnWndProc=targetProcedure;windowClass.hInstance=GetModuleHandleW(nullptr);
    windowClass.lpszClassName=L"GazeGridLayoutAcceptanceTarget";windowClass.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    if(!RegisterClassW(&windowClass))return 2;
    auto window=CreateWindowExW(0,windowClass.lpszClassName,title,WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,CW_USEDEFAULT,640,240,nullptr,nullptr,windowClass.hInstance,&target);
    if(!window)return 3;
    ShowWindow(window,SW_SHOW);SetForegroundWindow(window);SetFocus(target.editor);
    target.ready=true;
    MSG message{};
    while(GetMessageW(&message,nullptr,0,0)>0){TranslateMessage(&message);DispatchMessageW(&message);}
    return 0;
}
LRESULT call(HWND window,UINT message,WPARAM wparam=0,LPARAM lparam=0){
    DWORD_PTR result=0;
    if(!SendMessageTimeoutW(window,message,wparam,lparam,SMTO_ABORTIFHUNG,3000,&result))return 0;
    return LRESULT(result);
}
}

// Deliberately excluded from CTest: this test changes layouts and sends real
// keyboard events. Run only in an isolated, unlocked Windows VM with the opt-in.
class WindowsLayoutAcceptance:public QObject {
    Q_OBJECT
    QProcess target;
    HWND window=nullptr,editor=nullptr,settings=nullptr;
    bool originalCaps=false,haveCaps=false;
    WindowsInput input{[this](const WindowsInput::Event &event){
        // Never send a key-down to an unrelated foreground application.
        if(!(event.flags&KEYEVENTF_KEYUP)&&GetForegroundWindow()!=window)return false;
        return WindowsInput::nativeSender()(event);
    }};
    SystemKeyboard keyboard;
    QString text()const{
        wchar_t buffer[256]{};
        call(editor,WM_GETTEXT,std::size(buffer),reinterpret_cast<LPARAM>(buffer));
        return QString::fromWCharArray(buffer);
    }
    bool press(const QString &key){return input.key(key,true)&&input.key(key,false);}
    void clear(){call(editor,WM_SETTEXT,0,reinterpret_cast<LPARAM>(L""));}
    bool select(int index){
        SetForegroundWindow(window);
        DWORD_PTR selected=0;
        const bool answered=SendMessageTimeoutW(window,changeLayout,WPARAM(index),0,SMTO_NORMAL,30000,&selected)!=0;
        if(!answered)qWarning()<<"Layout request"<<index<<"failed with Win32 error"<<GetLastError();
        keyboard.refresh();
        return answered&&selected!=0;
    }
private slots:
    void initTestCase(){
        const QString title=QString("GazeGrid layout acceptance target %1").arg(GetCurrentProcessId());
        target.start(QCoreApplication::applicationFilePath(),{"--layout-target",title});
        QVERIFY(target.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT((window=FindWindowW(L"GazeGridLayoutAcceptanceTarget",reinterpret_cast<const wchar_t*>(title.utf16()))),15000);
        QTRY_VERIFY_WITH_TIMEOUT((editor=FindWindowExW(window,nullptr,L"EDIT",nullptr)),60000);
        QTRY_VERIFY_WITH_TIMEOUT(call(window,targetReady)==1,60000);
        SetForegroundWindow(window);QTRY_COMPARE_WITH_TIMEOUT(GetForegroundWindow(),window,15000);
        originalCaps=call(window,capsState)!=0;haveCaps=true;
        settings=CreateWindowExW(0,L"STATIC",L"GazeGrid acceptance settings focus",WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,CW_USEDEFAULT,400,160,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        QVERIFY(settings);
    }
    void init(){
        SetForegroundWindow(window);QTRY_COMPARE_WITH_TIMEOUT(GetForegroundWindow(),window,15000);
        QVERIFY(input.releaseAll());
        if(call(window,capsState)){QVERIFY(press("CAPITAL"));QTRY_COMPARE(call(window,capsState),LRESULT(0));}
        QVERIFY(select(0));clear();
    }
    void labelsMatchActualTypedText_data(){
        QTest::addColumn<int>("layout");QTest::addColumn<QString>("key");QTest::addColumn<int>("scan");QTest::addColumn<QString>("expected");
        QTest::newRow("US")<<0<<QString("Q")<<16<<QString("q");
        QTest::newRow("French")<<1<<QString("Q")<<16<<QString("a");
        QTest::newRow("German")<<2<<QString("Y")<<21<<QString("z");
        QTest::newRow("Russian")<<3<<QString("Q")<<16<<QString::fromUtf8("\xd0\xb9");
    }
    void labelsMatchActualTypedText(){
        QFETCH(int,layout);QFETCH(QString,key);QFETCH(int,scan);QFETCH(QString,expected);
        QVERIFY2(select(layout),"Requested layout could not be activated in the target editor");
        QCOMPARE(keyboard.label(scan,{}),expected);
        QVERIFY(press(key));QTRY_COMPARE_WITH_TIMEOUT(text(),expected,15000);
    }
    void physicalShiftAndCapsMatchLabels(){
        QCOMPARE(keyboard.label(16,{true,false,false}),QString("Q"));
        QVERIFY(input.key("LSHIFT",true));
        QTRY_VERIFY((GetAsyncKeyState(VK_LSHIFT)&0x8000)!=0);keyboard.refresh();
        QCOMPARE(keyboard.label(16,{}),QString("Q"));
        QVERIFY(press("Q"));QTRY_COMPARE_WITH_TIMEOUT(text(),QString("Q"),15000);QVERIFY(input.key("LSHIFT",false));
        clear();QVERIFY(press("CAPITAL"));QTRY_COMPARE(call(window,capsState),LRESULT(1));
        QTRY_VERIFY(([this]{keyboard.refresh();return keyboard.label(16,{});})()=="Q");
        QVERIFY(press("Q"));QTRY_COMPARE_WITH_TIMEOUT(text(),QString("Q"),15000);
        QVERIFY(press("CAPITAL"));QTRY_COMPARE(call(window,capsState),LRESULT(0));
    }
    void physicalAltGrMatchesLabels(){
        QVERIFY(select(2));QCOMPARE(keyboard.label(16,{false,false,true}),QString("@"));
        QVERIFY(input.key("RMENU",true));QTRY_VERIFY((GetAsyncKeyState(VK_RMENU)&0x8000)!=0);
        keyboard.refresh();QCOMPARE(keyboard.label(16,{}),QString("@"));
        QVERIFY(press("Q"));QTRY_COMPARE_WITH_TIMEOUT(text(),QString("@"),15000);QVERIFY(input.key("RMENU",false));
    }
    void labelRefreshDoesNotConsumePendingDeadKey(){
        QVERIFY(select(4));
        auto native=WindowsKeyboardLayout::nativeFunctions();WindowsKeyboardLayout layout(native);QVERIFY(layout.refresh());
        QVERIFY(layout.translate(40,{}).deadKey);
        QVERIFY(press("APOSTROPHE"));QTest::qWait(100);QCOMPARE(text(),QString());
        for(int repeat=0;repeat<5;++repeat)for(int scan=2;scan<=53;++scan)keyboard.label(scan,{});
        QVERIFY(press("E"));QTRY_COMPARE_WITH_TIMEOUT(text(),QString::fromUtf8("\xc3\xa9"),15000);
    }
    void settingsRetainsLastExternalLayout(){
        QVERIFY(select(1));QCOMPARE(keyboard.label(16,{}),QString("a"));
        ShowWindow(settings,SW_SHOW);SetForegroundWindow(settings);QTRY_COMPARE_WITH_TIMEOUT(GetForegroundWindow(),settings,15000);
        QVERIFY(!WindowsKeyboardLayout::nativeFunctions().snapshot().externalTarget);
        keyboard.refresh();QCOMPARE(keyboard.label(16,{}),QString("a"));
        ShowWindow(settings,SW_HIDE);SetForegroundWindow(window);QTRY_COMPARE_WITH_TIMEOUT(GetForegroundWindow(),window,15000);
        QVERIFY(select(0));QCOMPARE(keyboard.label(16,{}),QString("q"));
        QVERIFY(press("Q"));QTRY_COMPARE_WITH_TIMEOUT(text(),QString("q"),15000);
    }
    void cleanup(){
        input.releaseAll();
        if(settings)ShowWindow(settings,SW_HIDE);
    }
    void cleanupTestCase(){
        bool capsRestored=true;
        if(window&&IsWindow(window)){
            SetForegroundWindow(window);QTest::qWait(100);input.releaseAll();
            if(haveCaps&&(call(window,capsState)!=0)!=originalCaps){
                capsRestored=press("CAPITAL");
                QTest::qWait(100);capsRestored=capsRestored&&((call(window,capsState)!=0)==originalCaps);
            }
            call(window,WM_CLOSE);
        }
        if(settings)DestroyWindow(settings);
        const bool finished=target.waitForFinished(30000)||target.state()==QProcess::NotRunning;
        if(!finished){target.kill();target.waitForFinished();}
        const auto restored=target.readAllStandardOutput();qInfo().noquote()<<restored;
        QVERIFY(finished);QVERIFY(capsRestored);
        QVERIFY(restored.contains("LAYOUT_RESTORED=1"));QVERIFY(restored.contains("NEW_LAYOUTS_UNLOADED=1"));
    }
};
int main(int argc,char **argv){
    if(qEnvironmentVariable("GAZE_GRID_TEST_WINDOWS_LAYOUTS")!="1"){
        std::fprintf(stderr,"Run only in an isolated Windows VM with GAZE_GRID_TEST_WINDOWS_LAYOUTS=1.\n");return 2;
    }
    if(argc==3&&QByteArray(argv[1])=="--layout-target")return runTarget(reinterpret_cast<const wchar_t*>(QString::fromLocal8Bit(argv[2]).utf16()));
    QCoreApplication app(argc,argv);WindowsLayoutAcceptance tests;return QTest::qExec(&tests,argc,argv);
}
#include "WindowsLayoutAcceptance.moc"
