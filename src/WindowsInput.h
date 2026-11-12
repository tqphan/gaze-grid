#pragma once
#include <QString>
#include <QSet>
#include <functional>
#include <cstdint>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <future>
#include <map>
#include <mutex>
#include <thread>

// Ordered desktop output on a dedicated worker. Calls wait for the send result,
// but the sender and held-input state never run on the caller/UI thread.
// Coordinates are normalized across the Windows virtual desktop.
class WindowsInput {
public:
    struct Event {
        enum Kind { Key, Mouse } kind=Mouse;
        uint32_t flags=0, data=0;
        int32_t x=0, y=0;
        uint16_t scan=0, virtualKey=0;
    };
    using Sender=std::function<bool(const Event &)>; // Invoked only on the input worker.
    explicit WindowsInput(Sender sender=nativeSender());
    ~WindowsInput();
    WindowsInput(const WindowsInput &)=delete;
    WindowsInput &operator=(const WindowsInput &)=delete;
    bool key(const QString &name,bool down);
    bool text(const QString &text);
    bool button(int code,bool down); // Existing pointer codes: 272/273/274.
    // The worker also owns the deadline, independently of Qt/UI timers.
    bool timedButton(int code,int durationMs);
    bool move(double dx,double dy);
    bool moveAbsolute(double x,double y);
    bool scrollAxes(double horizontal,double vertical); // Wheel detents; positive vertical is down.
    bool releaseButtons();
    bool releaseAll(); // Failed releases remain tracked for retry.
    static Sender nativeSender();
private:
    Sender send;
    QSet<QString> heldKeys;
    QSet<int> heldButtons;
    QSet<uint16_t> heldUnicode;
    std::map<int,std::chrono::steady_clock::time_point> releases;
    std::mutex queueMutex;
    std::condition_variable queueChanged;
    std::deque<std::packaged_task<bool()>> jobs;
    bool stopping=false;
    std::thread inputThread;
    bool dispatch(std::function<bool()> operation);
    void run();
    void releaseDueButtons();
};
