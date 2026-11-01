#include "WindowsInput.h"
#include "KeyNames.h"
#include <QMap>
#include <cmath>
#include <limits>
#include <utility>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

WindowsInput::WindowsInput(Sender sender):send(std::move(sender)){}
WindowsInput::~WindowsInput(){releaseAll();}
WindowsInput::Sender WindowsInput::nativeSender(){
#ifdef _WIN32
    return [](const Event &event){
        INPUT input{};
        if(event.kind==Event::Key){
            input.type=INPUT_KEYBOARD;
            input.ki.wScan=event.scan;input.ki.wVk=event.virtualKey;input.ki.dwFlags=event.flags;
        }else{
            input.type=INPUT_MOUSE;
            input.mi.dx=event.x;input.mi.dy=event.y;input.mi.mouseData=event.data;input.mi.dwFlags=event.flags;
        }
        return SendInput(1,&input,sizeof(input))==1;
    };
#else
    return [](const Event &){return false;};
#endif
}
bool WindowsInput::key(const QString &name,bool down){
    const auto canonical=KeyNames::canonical(name);
    Event event;event.kind=Event::Key;
    const int code=KeyNames::linuxCode(canonical);
    // Set-1 physical scan codes preserve the imported keyboard's physical semantics.
    if(code>=1&&code<=68)event.scan=uint16_t(code);
    else if(code==87||code==88)event.scan=uint16_t(code);
    else {
        static const QMap<int,int> extended{{97,0x1d},{100,0x38},{102,0x47},{103,0x48},{104,0x49},{105,0x4b},{106,0x4d},{107,0x4f},{108,0x50},{109,0x51},{110,0x52},{111,0x53},{125,0x5b},{126,0x5c},{99,0x37}};
        if(extended.contains(code)){event.scan=uint16_t(extended[code]);event.flags=0x0001;}
        else if(code>=183&&code<=194)event.virtualKey=uint16_t(0x7c+code-183); // F13–F24
        else if(code==119)event.virtualKey=0x13; // Pause uses its special virtual-key sequence.
        else return false;
    }
    if(event.scan)event.flags|=0x0008; // KEYEVENTF_SCANCODE
    if(!down)event.flags|=0x0002;
    if(!send||!send(event))return false;
    if(down)heldKeys.insert(canonical);else heldKeys.remove(canonical);
    return true;
}
bool WindowsInput::text(const QString &text){
    if(!send||!heldKeys.isEmpty()||!text.isValidUtf16()||text.contains(QChar(0)))return false;
    for(auto ch:text){
        Event event;event.kind=Event::Key;event.scan=ch.unicode();event.flags=0x0004; // KEYEVENTF_UNICODE
        if(!send(event))return false;
        event.flags|=0x0002;
        if(!send(event))return false;
    }
    return true;
}
bool WindowsInput::button(int code,bool down){
    Event event;
    switch(code){
    case 272:event.flags=down?0x0002:0x0004;break;
    case 273:event.flags=down?0x0008:0x0010;break;
    case 274:event.flags=down?0x0020:0x0040;break;
    default:return false;
    }
    if(!send||!send(event))return false;
    if(down)heldButtons.insert(code);else heldButtons.remove(code);
    return true;
}
namespace {
bool representable(double n){return std::isfinite(n)&&n>=std::numeric_limits<int32_t>::min()&&n<=std::numeric_limits<int32_t>::max();}
}
bool WindowsInput::move(double dx,double dy){
    if(!representable(dx)||!representable(dy))return false;
    Event event;event.flags=0x0001;event.x=int32_t(std::round(dx));event.y=int32_t(std::round(dy));
    return send&&send(event);
}
bool WindowsInput::moveAbsolute(double x,double y){
    if(!std::isfinite(x)||!std::isfinite(y)||x<0||x>1||y<0||y>1)return false;
    Event event;event.flags=0x0001|0x8000|0x4000;
    event.x=int32_t(std::round(x*65535));event.y=int32_t(std::round(y*65535));
    return send&&send(event);
}
bool WindowsInput::scrollAxes(double horizontal,double vertical){
    const double x=horizontal*120,y=-vertical*120;
    if(!representable(x)||!representable(y))return false;
    if(!send)return false;
    for(const auto axis:{std::pair{x,uint32_t(0x1000)},std::pair{y,uint32_t(0x0800)}}){
        const auto amount=int32_t(std::round(axis.first));if(!amount)continue;
        Event event;event.flags=axis.second;event.data=uint32_t(amount);
        if(!send(event))return false;
    }
    return true;
}
bool WindowsInput::releaseButtons(){
    bool ok=true;const auto buttons=heldButtons;
    for(int code:buttons)if(!button(code,false))ok=false;
    return ok;
}
bool WindowsInput::releaseAll(){
    bool ok=true;const auto keys=heldKeys;
    for(const auto &name:keys)if(!key(name,false))ok=false;
    return releaseButtons()&&ok;
}
