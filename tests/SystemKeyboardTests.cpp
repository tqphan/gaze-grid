#include <QtTest>
#include <algorithm>
#include "WindowsKeyboardLayout.h"
#ifdef GAZE_GRID_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

class SystemKeyboardTests:public QObject {
    Q_OBJECT
private slots:
    void followsTargetAndObservesModifiersInSettings(){
        WindowsKeyboardLayout::Snapshot snapshot{1,true,{}};
        WindowsKeyboardLayout::Functions functions;
        functions.snapshot=[&]{return snapshot;};
        quintptr translatedLayout=0;
        functions.virtualKey=[&](uint,quintptr layout){translatedLayout=layout;return 0x51u;};
        functions.unicode=[](uint,uint,const auto &,char16_t *buffer,int,uint,quintptr){buffer[0]=u'q';return 1;};
        WindowsKeyboardLayout keyboard(functions);
        QVERIFY(!keyboard.available());QVERIFY(keyboard.refresh());QVERIFY(keyboard.available());
        QVERIFY(!keyboard.refresh());
        snapshot={2,true,{}};QVERIFY(keyboard.refresh());keyboard.translate(16,{});QCOMPARE(translatedLayout,quintptr(2));
        snapshot={3,false,{}};QVERIFY(!keyboard.refresh());keyboard.translate(16,{});QCOMPARE(translatedLayout,quintptr(2));
        snapshot.modifiers.shift=true;QVERIFY(keyboard.refresh());QVERIFY(!keyboard.refresh());
        QVERIFY(!keyboard.capsLocked());
        snapshot.modifiers.caps=true;QVERIFY(keyboard.refresh());QVERIFY(keyboard.capsLocked());
        snapshot.modifiers.altGr=true;QVERIFY(keyboard.refresh());
        snapshot={0,true,{}};QVERIFY(keyboard.refresh());QVERIFY(!keyboard.available());QVERIFY(!keyboard.capsLocked());
        QVERIFY(keyboard.translate(16,{}).text.isEmpty());
    }
    void buildsLayoutSpecificStateWithoutConsumingDeadKeys(){
        using Layout=WindowsKeyboardLayout;
        Layout::Snapshot snapshot{42,true,{}};
        Layout::KeyState observed{};
        uint observedFlags=0,observedScan=0;quintptr observedLayout=0;
        Layout::Functions functions;
        functions.snapshot=[&]{return snapshot;};
        functions.virtualKey=[](uint scan,quintptr layout){return scan==16&&layout==42?0x51u:0u;};
        functions.unicode=[&](uint key,uint scan,const auto &state,char16_t *buffer,int capacity,uint flags,quintptr layout){
            observed=state;observedFlags=flags;observedScan=scan;observedLayout=layout;
            if(key!=0x51||capacity!=16)return 0;
            buffer[0]=u'@';return 1;
        };
        Layout keyboard(functions);QVERIFY(keyboard.refresh());
        QCOMPARE(keyboard.translate(16,{true,true,true}).text,QString("@"));
        QCOMPARE(observedFlags,4u);QCOMPARE(observedScan,16u);QCOMPARE(observedLayout,quintptr(42));
        QCOMPARE(observed[0x10],quint8(0x80));QCOMPARE(observed[0xa0],quint8(0x80));
        QCOMPARE(observed[0x11],quint8(0x80));QCOMPARE(observed[0xa2],quint8(0x80));
        QCOMPARE(observed[0x12],quint8(0x80));QCOMPARE(observed[0xa5],quint8(0x80));
        QCOMPARE(observed[0x14],quint8(0)); // Owned Caps must not double-toggle OS Caps.
        snapshot.modifiers={true,true,true};QVERIFY(keyboard.refresh());
        keyboard.translate(16,{});QCOMPARE(observed[0x14],quint8(1));QCOMPARE(observed[0xa5],quint8(0x80));
        snapshot.modifiers={};QVERIFY(keyboard.refresh());keyboard.translate(16,{});
        QVERIFY(std::all_of(observed.begin(),observed.end(),[](quint8 value){return value==0;}));
    }
    void preservesDeadKeysAndUnicodeOutput(){
        using Layout=WindowsKeyboardLayout;
        int count=1;QString output="q";int calls=0;
        Layout::Functions functions;
        functions.snapshot=[]{return Layout::Snapshot{1,true,{}};};
        functions.virtualKey=[](uint,quintptr){return 0x51u;};
        functions.unicode=[&](uint,uint,const auto &,char16_t *buffer,int capacity,uint flags,quintptr){
            ++calls;if(flags!=4)return 0;
            for(int i=0;i<qMin(output.size(),qsizetype(capacity));++i)buffer[i]=char16_t(output[i].unicode());
            return count;
        };
        Layout keyboard(functions);QVERIFY(keyboard.refresh());
        output=QString::fromUtf8("´ignored");count=-1;
        const auto dead=keyboard.translate(40,{});QVERIFY(dead.deadKey);QCOMPARE(dead.text,QString::fromUtf8("´"));
        output="ignored";count=0;QVERIFY(keyboard.translate(16,{}).text.isEmpty());
        output=QString::fromUtf8("й");count=1;QCOMPARE(keyboard.translate(16,{}).text,output);
        output=QString::fromUtf8("😀");count=2;QCOMPARE(keyboard.translate(16,{}).text,output);
        output="ss";count=2;QCOMPARE(keyboard.translate(16,{}).text,output);
        output=QString(QChar(0xd800));count=1;QVERIFY(keyboard.translate(16,{}).text.isEmpty());
        const int previous=calls;QVERIFY(keyboard.translate(-1,{}).text.isEmpty());QVERIFY(keyboard.translate(99,{}).text.isEmpty());QCOMPARE(calls,previous);
    }
    void unknownVirtualKeysNeverTranslate(){
        using Layout=WindowsKeyboardLayout;
        uint key=0;int calls=0;
        Layout::Functions functions;
        functions.snapshot=[]{return Layout::Snapshot{1,true,{}};};
        functions.virtualKey=[&](uint,quintptr){return key;};
        functions.unicode=[&](uint,uint,const auto &,char16_t *,int,uint,quintptr){++calls;return 0;};
        Layout keyboard(functions);QVERIFY(keyboard.refresh());
        QVERIFY(keyboard.translate(16,{}).text.isEmpty());key=255;
        QVERIFY(keyboard.translate(16,{}).text.isEmpty());QCOMPARE(calls,0);
    }
#ifdef GAZE_GRID_WINDOWS
    void nativeTranslationLeavesThreadStateUntouched(){
        using Layout=WindowsKeyboardLayout;
        const HKL original=GetKeyboardLayout(0);QVERIFY(original);
        BYTE before[256]{},after[256]{};QVERIFY(GetKeyboardState(before));
        auto functions=Layout::nativeFunctions();
        functions.snapshot=[&]{return Layout::Snapshot{reinterpret_cast<quintptr>(original),true,{}};};
        Layout keyboard(functions);QVERIFY(keyboard.refresh());
        for(int scan=2;scan<=53;++scan)for(const auto state:{Layout::Modifiers{},Layout::Modifiers{true,false,false},Layout::Modifiers{false,false,true}})
            QVERIFY(keyboard.translate(scan,state).text.isValidUtf16());
        QVERIFY(GetKeyboardState(after));QCOMPARE(GetKeyboardLayout(0),original);
        QVERIFY(std::equal(std::begin(before),std::end(before),std::begin(after)));
    }
#endif
};
QTEST_GUILESS_MAIN(SystemKeyboardTests)
#include "SystemKeyboardTests.moc"
