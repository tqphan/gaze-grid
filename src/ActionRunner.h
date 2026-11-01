#pragma once
#include <QObject>
#include <QTimer>
#include <QSet>
#include <QVariantList>
#include <functional>
// Ordered, nonblocking execution. Callbacks form the platform boundary.
class ActionRunner:public QObject {
    Q_OBJECT
public:
    explicit ActionRunner(QObject *parent=nullptr);
    std::function<bool(QString,bool)> sendKey;
    std::function<bool(QString)> sendText;
    std::function<bool(QString,QStringList)> sendLocalText;
    std::function<bool(QString,QStringList)> sendLocalKey;
    std::function<bool()> canEditLocal;
    std::function<QString()> localInputError;
    std::function<bool()> canCommitText;
    std::function<bool(QString)> removeSpaceBeforeText;
    std::function<bool(QString)> supportsPhysicalKey;
    std::function<bool(QString)> performAction;
    std::function<void(QString)> speakLabel;
    std::function<bool(QString)> supportsAction;
    std::function<void(QString,bool)> navigate;
    bool textOnly=false; // Conversation keyboards never send desktop input.
    bool physicalText=false; // System layout: let the target compose physical input.
    std::function<bool()> physicalTextReady;
    bool capsLock=false;
    bool start(const QVariantList &commands);
    void cancel(bool preserveModifiers=false,bool preserveToggles=false);
    void suspend();
    void resume(bool success);
    bool busy()const{return running;}
    bool isHeld(const QString &key)const;
    bool isToggled(const QVariantList &commands)const;
    QStringList heldKeys()const{return held.values();}
    QVariantMap modifierStates()const;
    static QString modifierName(const QVariantList &commands);
    static QString validate(const QVariantList &commands,bool unicodeText=false);
    static QString signature(const QVariantList &commands);
    bool shifted()const;
    static QPair<QString,bool> dwellKey(const QVariantList &commands,bool system=false);
    bool beginDwellHold(const QVariantList &commands,const QString &owner={});
    void endDwellHold(const QString &owner={});
    bool toggleDwellKey(const QVariantList &commands);
    void releaseToggledKeys();
signals:
    void stateChanged();
    void failed(QString message);
    void textCommitted(QString text);
    void keyPressed(QString name);
    void finished();
    void cancelled();
private:
    bool editingLocal()const{return textOnly&&sendLocalText&&sendLocalKey&&(!canEditLocal||canEditLocal());}
    void step();
    bool key(QString name,bool down,bool persistent=false);
    bool tap(QString name);
    bool shortcutHeld()const;
    bool cycleModifier(const QString &name);
    void fail(QString message);
    QVariantList queue;
    int index=0;
    bool running=false,waiting=false;
    QTimer timer;
    QSet<QString> held,transient,latched;
    QMap<QString,QPair<QString,bool>> dwellHolds;
    bool dwellShift=false;
    QSet<QString> toggledKeys, shiftedToggles;
    bool toggleOwnShift=false;
};
