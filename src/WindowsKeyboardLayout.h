#pragma once
#include <QString>
#include <array>
#include <functional>

// Read-only Win32 boundary. Tests supply snapshots and translations without
// activating layouts, changing keyboard state, or sending desktop input.
class WindowsKeyboardLayout {
public:
    struct Modifiers {
        bool shift=false,caps=false,altGr=false;
        bool operator==(const Modifiers &)const=default;
    };
    struct Snapshot {
        quintptr layout=0;
        bool externalTarget=true;
        Modifiers modifiers;
    };
    struct Translation {QString text;bool deadKey=false;};
    using KeyState=std::array<quint8,256>;
    struct Functions {
        std::function<Snapshot()> snapshot;
        std::function<uint(uint,quintptr)> virtualKey;
        std::function<int(uint,uint,const KeyState &,char16_t *,int,uint,quintptr)> unicode;
    };
    explicit WindowsKeyboardLayout(Functions functions=nativeFunctions());
    static Functions nativeFunctions();
    bool refresh();
    bool available()const{return layout!=0;}
    Translation translate(int scan,Modifiers owned)const;
private:
    Functions functions;
    quintptr layout=0;
    Modifiers observed;
};
