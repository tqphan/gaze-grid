#pragma once
#include <QString>
#include <QSet>
#include <functional>
#include <cstdint>

// Single-threaded output adapter. Coordinates are normalized across the entire
// Windows virtual desktop; callers must map their chosen display into it.
class WindowsInput {
public:
    struct Event {
        enum Kind { Key, Mouse } kind=Mouse;
        uint32_t flags=0, data=0;
        int32_t x=0, y=0;
        uint16_t scan=0, virtualKey=0;
    };
    using Sender=std::function<bool(const Event &)>;
    explicit WindowsInput(Sender sender=nativeSender());
    ~WindowsInput();
    WindowsInput(const WindowsInput &)=delete;
    WindowsInput &operator=(const WindowsInput &)=delete;
    bool key(const QString &name,bool down);
    bool text(const QString &text);
    bool button(int code,bool down); // Existing pointer codes: 272/273/274.
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
};
