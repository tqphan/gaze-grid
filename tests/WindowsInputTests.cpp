#include <QtTest>
#include "WindowsInput.h"
#include "KeyNames.h"
#include <limits>
#include <atomic>
#include <thread>
class WindowsInputTests:public QObject {
    Q_OBJECT
private slots:
    void allOutputAndCleanupUseOneWorker(){
        const auto caller=std::this_thread::get_id();
        QList<std::thread::id> threads;
        QList<WindowsInput::Event> events;
        {
            WindowsInput input([&](const auto &e){
                threads.append(std::this_thread::get_id());events.append(e);return true;
            });
            QVERIFY(input.key("LCONTROL",true));
            QVERIFY(input.key("A",true));QVERIFY(input.key("A",false));
            QVERIFY(input.key("LCONTROL",false));
            QVERIFY(input.text("x"));
            QVERIFY(input.move(1,2));QVERIFY(input.moveAbsolute(.3,.4));
            QVERIFY(input.scrollAxes(1,1));
            QVERIFY(input.button(273,true));QVERIFY(input.releaseButtons());
            QVERIFY(input.key("LSHIFT",true));QVERIFY(input.button(272,true));
            // Destruction must release both remaining holds on the same worker.
        }
        QVERIFY(!threads.isEmpty());QVERIFY(threads.front()!=caller);
        for(const auto &thread:threads)QVERIFY(thread==threads.front());
        QCOMPARE(events[0].scan,0x1d);QCOMPARE(events[0].flags,8u);
        QCOMPARE(events[1].scan,0x1e);QCOMPARE(events[1].flags,8u);
        QCOMPARE(events[2].scan,0x1e);QCOMPARE(events[2].flags,10u);
        QCOMPARE(events[3].scan,0x1d);QCOMPARE(events[3].flags,10u);
        QCOMPARE(events[events.size()-2].scan,0x2a);
        QCOMPARE(events[events.size()-2].flags,10u);
        QCOMPARE(events.last().flags,4u);
    }
    void concurrentCallersKeepTheirCommandOrder(){
        QList<WindowsInput::Event> events;
        WindowsInput input([&](const auto &e){events.append(e);return true;});
        std::atomic<bool> success=true;
        std::vector<std::thread> callers;
        for(int producer=0;producer<4;++producer)callers.emplace_back([&,producer]{
            for(int sequence=0;sequence<30;++sequence)
                if(!input.move(producer,sequence))success=false;
        });
        for(auto &caller:callers)caller.join();
        QVERIFY(success.load());QCOMPARE(events.size(),120);
        int next[4]={};
        for(const auto &event:events){
            QVERIFY(event.x>=0&&event.x<4);QCOMPARE(event.y,next[event.x]++);
        }
        for(auto count:next)QCOMPARE(count,30);
    }
    void unicodeFailureRemainsOwnedForCleanup(){
        QList<WindowsInput::Event> events;bool rejectRelease=true;
        WindowsInput input([&](const auto &e){
            events.append(e);return !(rejectRelease&&e.flags==6u);
        });
        QVERIFY(!input.text("x"));QCOMPARE(events.size(),2);
        QVERIFY(!input.text("y"));QCOMPARE(events.size(),2);
        QVERIFY(!input.releaseAll());QCOMPARE(events.size(),3);
        rejectRelease=false;QVERIFY(input.releaseAll());QCOMPARE(events.size(),4);
        QCOMPARE(events.last().scan,uint16_t('x'));QCOMPARE(events.last().flags,6u);
        QVERIFY(input.text("y"));QCOMPARE(events.size(),6);
    }
    void longTextDoesNotDelayTimedPointerRelease(){
        std::atomic<int> pointerReleaseAt=0;
        int characters=0;
        WindowsInput input([&](const auto &e){
            if(e.kind==WindowsInput::Event::Key&&e.flags==4u){
                ++characters;
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
            if(e.kind==WindowsInput::Event::Mouse&&e.flags==4u)pointerReleaseAt=characters;
            return true;
        });
        QVERIFY(input.timedButton(272,20));
        QVERIFY(input.text(QString(80,'x')));
        QVERIFY(pointerReleaseAt.load()>0);QVERIFY(pointerReleaseAt.load()<80);
    }
    void timedReleaseDoesNotNeedEventLoop(){
        std::atomic<int> presses=0,releases=0;
        WindowsInput input([&](const auto &e){
            if(e.flags==2u)++presses;
            if(e.flags==4u)++releases;
            return true;
        });
        QVERIFY(input.timedButton(272,40));
        // Deliberately block the calling thread without processing Qt events.
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        QCOMPARE(presses.load(),1);QCOMPARE(releases.load(),1);
        QVERIFY(input.button(272,false));QCOMPARE(releases.load(),1);
        QVERIFY(input.timedButton(272,20));
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        QCOMPARE(releases.load(),2);
    }
    void cancelledReleaseCannotReleaseLaterDrag(){
        std::atomic<int> releases=0;
        WindowsInput input([&](const auto &e){if(e.flags==4u)++releases;return true;});
        QVERIFY(input.timedButton(272,80));
        QVERIFY(!input.timedButton(272,10));
        QVERIFY(input.releaseButtons());
        QVERIFY(input.button(272,true));
        std::this_thread::sleep_for(std::chrono::milliseconds(180));
        QCOMPARE(releases.load(),1);
        QVERIFY(input.releaseButtons());QCOMPARE(releases.load(),2);
    }
    void destructionReleasesPendingClick(){
        std::atomic<int> releases=0;
        {
            WindowsInput input([&](const auto &e){if(e.flags==4u)++releases;return true;});
            QVERIFY(input.timedButton(272,250));
        }
        QCOMPARE(releases.load(),1);
    }
    void failedTimedReleaseCanBeRetried(){
        std::atomic<bool> reject=true;
        std::atomic<int> attempts=0;
        WindowsInput input([&](const auto &e){
            if(e.flags!=4u)return true;
            ++attempts;return !reject.load();
        });
        QVERIFY(input.timedButton(272,20));
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        QCOMPARE(attempts.load(),1);
        reject=false;QVERIFY(input.releaseButtons());QCOMPARE(attempts.load(),2);
    }
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
