#include "SystemKeyboard.h"
#include "ActionRunner.h"
#include "KeyNames.h"
#ifdef _WIN32
#include "WindowsKeyboardLayout.h"
#else
#include <xkbcommon/xkbcommon.h>
#endif

struct SystemKeyboard::Impl {
#ifdef _WIN32
    WindowsKeyboardLayout windows;
#else
    xkb_context *context=xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    xkb_keymap *keymap=nullptr;
    QByteArray source;
    quint32 depressed=0,latched=0,locked=0,group=0;
    bool stateKnown=false;
    ~Impl(){if(keymap)xkb_keymap_unref(keymap);if(context)xkb_context_unref(context);}
#endif
};
SystemKeyboard::SystemKeyboard():d(std::make_unique<Impl>()){}
SystemKeyboard::~SystemKeyboard()=default;
bool SystemKeyboard::refresh(){
#ifdef _WIN32
    return d->windows.refresh();
#else
    return false;
#endif
}
bool SystemKeyboard::setKeymap(const QByteArray &source){
#ifdef _WIN32
    Q_UNUSED(source);return false;
#else
    if(source==d->source){
        // A different injection device may expose identical keymap bytes.
        const bool changed=d->stateKnown;
        d->depressed=d->latched=d->locked=d->group=0;d->stateKnown=false;
        return changed;
    }
    auto map=source.isEmpty()||!d->context?nullptr:xkb_keymap_new_from_string(d->context,source.constData(),XKB_KEYMAP_FORMAT_TEXT_V1,XKB_KEYMAP_COMPILE_NO_FLAGS);
    if(d->keymap)xkb_keymap_unref(d->keymap);
    d->keymap=map;d->source=source;
    d->depressed=d->latched=d->locked=d->group=0;d->stateKnown=false;
    return true;
#endif
}
bool SystemKeyboard::setModifiers(quint32 depressed,quint32 latched,quint32 locked,quint32 group){
#ifdef _WIN32
    Q_UNUSED(depressed);Q_UNUSED(latched);Q_UNUSED(locked);Q_UNUSED(group);return false;
#else
    if(d->stateKnown&&d->depressed==depressed&&d->latched==latched&&d->locked==locked&&d->group==group)return false;
    d->depressed=depressed;d->latched=latched;d->locked=locked;d->group=group;d->stateKnown=true;return true;
#endif
}
bool SystemKeyboard::available()const{
#ifdef _WIN32
    return d->windows.available();
#else
    return d->keymap&&(d->stateKnown?d->group<xkb_keymap_num_layouts(d->keymap):xkb_keymap_num_layouts(d->keymap)==1);
#endif
}
QString SystemKeyboard::label(int scan,Modifiers m)const{
    if(!available()||scan<1||scan>58)return {};
#ifdef _WIN32
    return d->windows.translate(scan,{m.shift,m.caps,m.altGr}).text;
#else
    auto state=xkb_state_new(d->keymap);if(!state)return {};
    xkb_mod_mask_t depressed=d->depressed,locked=d->locked;
    auto mask=[&](const char *name){const auto i=xkb_keymap_mod_get_index(d->keymap,name);return i==XKB_MOD_INVALID?xkb_mod_mask_t(0):xkb_mod_mask_t(1)<<i;};
    if(m.shift)depressed|=mask(XKB_MOD_NAME_SHIFT);
    if(m.altGr)depressed|=mask("Mod5");
    if(m.caps&&!d->stateKnown)locked|=mask(XKB_MOD_NAME_CAPS);
    xkb_state_update_mask(state,depressed,d->latched,locked,0,0,d->group);
    const auto key=xkb_keycode_t(scan+8);
    const int count=xkb_state_key_get_utf8(state,key,nullptr,0);
    QByteArray buffer(count+1,0);
    if(count>0)xkb_state_key_get_utf8(state,key,buffer.data(),size_t(buffer.size()));
    QString result=QString::fromUtf8(buffer.constData(),count);
    if(result.isEmpty()){
        // Dead keys have no UTF-8 output until composed; show their XKB name.
        char name[96]{};xkb_keysym_get_name(xkb_state_key_get_one_sym(state,key),name,sizeof(name));
        if(QByteArray(name).startsWith("dead_"))result=QString::fromLatin1(name+5);
    }
    xkb_state_unref(state);return result;
#endif
}
QVariantList SystemKeyboard::labels(const QVariantList &keys,Modifiers modifiers,const Translate &translate){
    QVariantList result;
    for(const auto &entry:keys){
        const auto key=entry.toMap();
        QString text=modifiers.shift&&!key["shiftLabel"].toString().isEmpty()?key["shiftLabel"].toString():key["label"].toString();
        const auto commands=key["commands"].toList();
        // Explicit typing positions opt in; older files infer only matching plain character labels.
        if(commands.size()==1&&key["symbol"].toString().isEmpty()){
            const auto command=commands.front().toMap();const auto value=command["text"].toString();
            const auto physical=command["physicalKey"].toString();
            if(command["tag"]=="Text"&&(!physical.isEmpty()||(value.size()==1&&!value.front().isSpace()&&key["label"].toString()==value))){
                const auto [name,shift]=physical.isEmpty()?KeyNames::usCharacter(value.front()):qMakePair(physical,false);
                auto state=modifiers;state.shift|=shift;
                const auto translated=translate(KeyNames::linuxCode(name),state);
                if(!translated.isEmpty()&&translated.front().isPrint())text=translated;
            }
        }
        result<<text;
    }
    return result;
}
