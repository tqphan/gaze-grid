#pragma once
#include <QByteArray>
#include <QStringList>
#include <QVariantList>
#include <functional>
#include <memory>

// Translates labels only. The personal XML geometry and commands stay immutable.
class SystemKeyboard {
public:
    struct Modifiers { bool shift=false, caps=false, altGr=false; };
    using Translate=std::function<QString(int,Modifiers)>;
    SystemKeyboard();
    ~SystemKeyboard();
    bool refresh(); // Windows: follow the foreground application's layout.
    bool setKeymap(const QByteArray &keymap); // Linux: the injection device's XKB map.
    bool setModifiers(quint32 depressed,quint32 latched,quint32 locked,quint32 group);
    bool available()const;
    QString label(int scan,Modifiers modifiers)const;
    static QVariantList labels(const QVariantList &keys,Modifiers modifiers,const Translate &translate);
private:
    struct Impl;
    std::unique_ptr<Impl> d;
};
