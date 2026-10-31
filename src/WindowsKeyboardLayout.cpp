#include "WindowsKeyboardLayout.h"
#include <utility>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

WindowsKeyboardLayout::WindowsKeyboardLayout(Functions functions):functions(std::move(functions)){}
WindowsKeyboardLayout::Functions WindowsKeyboardLayout::nativeFunctions(){
#ifdef _WIN32
    return {
        []{
            Snapshot result;
            const auto window=GetForegroundWindow();
            if(window){
                DWORD process=0;const auto thread=GetWindowThreadProcessId(window,&process);
                if(thread)result.layout=reinterpret_cast<quintptr>(GetKeyboardLayout(thread));
                result.externalTarget=process!=GetCurrentProcessId();
            }
            // These queries never change the foreground application's state.
            result.modifiers.shift=(GetAsyncKeyState(VK_LSHIFT)&0x8000)||(GetAsyncKeyState(VK_RSHIFT)&0x8000);
            result.modifiers.altGr=(GetAsyncKeyState(VK_RMENU)&0x8000)!=0;
            result.modifiers.caps=(GetKeyState(VK_CAPITAL)&1)!=0;
            return result;
        },
        [](uint scan,quintptr layout){return uint(MapVirtualKeyExW(scan,MAPVK_VSC_TO_VK_EX,reinterpret_cast<HKL>(layout)));},
        [](uint key,uint scan,const KeyState &state,char16_t *output,int capacity,uint flags,quintptr layout){
            wchar_t buffer[16]{};
            const int size=qMin(capacity,16);
            const int count=ToUnicodeEx(key,scan,state.data(),buffer,size,flags,reinterpret_cast<HKL>(layout));
            const int written=count<0?1:qMin(count,size);
            for(int i=0;i<written;++i)output[i]=char16_t(buffer[i]);
            return count;
        }
    };
#else
    return {};
#endif
}
bool WindowsKeyboardLayout::refresh(){
    if(!functions.snapshot)return false;
    const auto snapshot=functions.snapshot();
    // Keep the last external target while the user operates our settings.
    const auto nextLayout=snapshot.externalTarget||!layout?snapshot.layout:layout;
    const bool changed=nextLayout!=layout||snapshot.modifiers!=observed;
    layout=nextLayout;observed=snapshot.modifiers;return changed;
}
WindowsKeyboardLayout::Translation WindowsKeyboardLayout::translate(int scan,Modifiers owned)const{
    if(!available()||scan<1||scan>58||!functions.virtualKey||!functions.unicode)return {};
    const uint key=functions.virtualKey(uint(scan),layout);
    if(!key||key>254)return {};
    KeyState state{};
    if(owned.shift||observed.shift)state[0x10]=state[0xa0]=0x80;
    // Caps Lock is an OS toggle, not a second toggle to combine with our own.
    if(observed.caps)state[0x14]=1;
    if(owned.altGr||observed.altGr)state[0x11]=state[0xa2]=state[0x12]=state[0xa5]=0x80;
    std::array<char16_t,16> buffer{};
    // Bit 2 is mandatory: inspecting a label must never consume a dead key.
    const int count=functions.unicode(key,uint(scan),state,buffer.data(),int(buffer.size()),4,layout);
    const int written=count<0?1:qMin(count,int(buffer.size()));
    if(written<=0)return {};
    const auto text=QString::fromUtf16(buffer.data(),written);
    if(!text.isValidUtf16())return {};
    return {text,count<0};
}
