#include <QtTest>
#include "WindowsInput.h"
#include "KeyNames.h"
#include <limits>
class WindowsInputTests:public QObject {
    Q_OBJECT
private slots:
    void pointerCleanupPreservesKeysAndOnlyReleasesOwnedButtons(){
        QList<WindowsInput::Event> events;
        WindowsInput input([&](const auto &e){events.append(e);return true;});
        QVERIFY(input.key("A",true));QVERIFY(input.button(272,true));
        QVERIFY(input.releaseButtons());QCOMPARE(events.size(),3);
        QCOMPARE(events.last().flags,4u);
        QVERIFY(input.releaseButtons());QCOMPARE(events.size(),3);
        QVERIFY(input.releaseAll());QCOMPARE(events.size(),4);
        QCOMPARE(events.last().kind,WindowsInput::Event::Key);
    }
    void controllerKeyCodesRoundTrip(){
        for(const auto name:{"A","RETURN","RCONTROL","RMENU","SYSRQ","PAUSE","LWIN","F24"})
            QCOMPARE(KeyNames::fromLinuxCode(KeyNames::linuxCode(name)),QString(name));
        QVERIFY(KeyNames::fromLinuxCode(-1).isEmpty());
    }
    void physicalKeys(){
        QList<WindowsInput::Event> events;
        WindowsInput input([&](const auto &e){events.append(e);return true;});
        QVERIFY(input.key("a",true));QCOMPARE(events.last().scan,0x1e);QCOMPARE(events.last().flags,8u);
        QVERIFY(input.key("RIGHTCTRL",true));QCOMPARE(events.last().scan,0x1d);QCOMPARE(events.last().flags,9u);
        QVERIFY(input.key("RCONTROL",false));QCOMPARE(events.last().flags,11u);
        QVERIFY(input.key("F24",true));QCOMPARE(events.last().virtualKey,0x87);
        QVERIFY(!input.key("UNKNOWN",true));
        QVERIFY(input.releaseAll());QCOMPARE(events.size(),6);
    }
    void unicodeTextBypassesPhysicalLayout(){
        QList<WindowsInput::Event> events;
        WindowsInput input([&](const auto &event){events.append(event);return true;});
        const auto text=QString::fromUtf8("caf\xc3\xa9 \xe4\xbd\xa0");
        QVERIFY(input.text(text));QCOMPARE(events.size(),text.size()*2);
        for(int i=0;i<text.size();++i){
            QCOMPARE(events[i*2].kind,WindowsInput::Event::Key);
            QCOMPARE(events[i*2].virtualKey,0);QCOMPARE(events[i*2].scan,text[i].unicode());
            QCOMPARE(events[i*2].flags,4u);QCOMPARE(events[i*2+1].flags,6u);
        }
        events.clear();QVERIFY(!input.text(QString(QChar(0))));QVERIFY(events.isEmpty());
        QVERIFY(input.key("LSHIFT",true));events.clear();QVERIFY(!input.text("hello"));QVERIFY(events.isEmpty());
        QVERIFY(input.releaseAll());
    }
    void pointer(){
        QList<WindowsInput::Event> events;
        WindowsInput input([&](const auto &e){events.append(e);return true;});
        QVERIFY(input.moveAbsolute(0,1));QCOMPARE(events.last().x,0);QCOMPARE(events.last().y,65535);QCOMPARE(events.last().flags,0xc001u);
        QVERIFY(!input.moveAbsolute(-0.1,0));QVERIFY(!input.move(std::numeric_limits<double>::infinity(),0));
        QVERIFY(input.scrollAxes(1,1));QCOMPARE(events[1].flags,0x1000u);QCOMPARE(events[1].data,120u);QCOMPARE(int32_t(events[2].data),-120);
        QVERIFY(input.button(273,true));QCOMPARE(events.last().flags,8u);
        QVERIFY(input.releaseAll());QCOMPARE(events.last().flags,16u);
        QVERIFY(!input.button(999,true));
    }
    void failedReleaseIsRetried(){
        bool success=true;int calls=0;
        WindowsInput input([&](const auto &){++calls;return success;});
        QVERIFY(input.key("SHIFT",true)==false);
        QVERIFY(input.key("LSHIFT",true));QVERIFY(input.button(272,true));
        success=false;QVERIFY(!input.releaseAll());QCOMPARE(calls,4);
        success=true;QVERIFY(input.releaseAll());QCOMPARE(calls,6);
        QVERIFY(input.releaseAll());QCOMPARE(calls,6);
    }
};
QTEST_GUILESS_MAIN(WindowsInputTests)
#include "WindowsInputTests.moc"
