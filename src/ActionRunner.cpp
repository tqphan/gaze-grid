#include "ActionRunner.h"
#include "KeyNames.h"
#include <algorithm>
namespace {
const QSet<QString> modifierKeys{"LSHIFT","RSHIFT","LCONTROL","RCONTROL","LMENU","RMENU","LWIN","RWIN"};
}
QString ActionRunner::modifierName(const QVariantList &commands){
    if(commands.size()!=1)return {};
    const auto command=commands.front().toMap();
    if(command["tag"]!="Action"&&command["tag"]!="KeyToggle")return {};
    const auto name=KeyNames::canonical(command["text"].toString());
    return modifierKeys.contains(name)?name:QString();
}
QVariantMap ActionRunner::modifierStates()const{
    QVariantMap states;
    for(const auto &name:held)if(modifierKeys.contains(name)&&!transient.contains(name))
        states[name]=latched.contains(name)?"latched":"locked";
    return states;
}
bool ActionRunner::cycleModifier(const QString &name){
    if(latched.remove(name)){emit stateChanged();return true;}
    if(isHeld(name))return key(name,false);
    if(!key(name,true,true))return false;
    latched.insert(name);emit stateChanged();return true;
}
ActionRunner::ActionRunner(QObject *parent):QObject(parent){timer.setSingleShot(true);timer.setTimerType(Qt::PreciseTimer);connect(&timer,&QTimer::timeout,this,&ActionRunner::step);}
bool ActionRunner::isHeld(const QString &k)const{return held.contains(KeyNames::canonical(k));}
bool ActionRunner::shifted()const{return isHeld("LSHIFT")||isHeld("RSHIFT");}
bool ActionRunner::shortcutHeld()const{return isHeld("LCONTROL")||isHeld("RCONTROL")||isHeld("LMENU")||isHeld("RMENU")||isHeld("LWIN")||isHeld("RWIN");}
QString ActionRunner::validate(const QVariantList &commands,bool unicodeText){
    if(commands.size()>256)return "Too many commands";
    for(const auto &v:commands){auto c=v.toMap();auto tag=c["tag"].toString(),value=c["text"].toString();
        if(tag=="Text") {if(!value.isValidUtf16())return "Invalid Unicode text";for(auto ch:value)if((!unicodeText||ch.isNull()||ch.category()==QChar::Other_Control)&&KeyNames::usCharacter(ch).first.isEmpty())return "Text character is not supported by the output encoder";}
        else if(tag=="KeyDown"||tag=="KeyUp"||tag=="KeyToggle"){if(KeyNames::linuxCode(value)<0)return "Unknown physical key: "+value;}
        else if(tag=="Wait"){bool ok=false;int n=value.toInt(&ok);if(!ok||n<0||n>10000)return "Wait must be between 0 and 10000 ms";}
        else if(tag=="ChangeKeyboard"){if(value.trimmed().isEmpty()||value.contains('/')||value.contains('\\')||value.contains(".."))return "Invalid keyboard link";}
        else if(tag!="Action")return "Unsupported command: "+tag;
    }
    return {};
}
QString ActionRunner::signature(const QVariantList &commands){
    QStringList result;for(auto v:commands){auto c=v.toMap();QString value=c["text"].toString(),tag=c["tag"].toString();if(tag.startsWith("Key"))value=KeyNames::canonical(value);else if(tag!="Text")value=value.trimmed();result<<tag+":"+value;}return result.join("|");
}
bool ActionRunner::start(const QVariantList &commands){
    if(running){emit failed("An action is still running");return false;}
    if(physicalText&&!textOnly&&physicalTextReady&&!physicalTextReady()){
        for(const auto &entry:commands)if(entry.toMap()["tag"]=="Text"){
            emit failed("System keyboard layout unavailable");return false;
        }
    }
    auto validationCommands=commands;
    if(physicalText)for(auto &entry:validationCommands){auto command=entry.toMap();if(command["tag"]=="Text"&&command.contains("systemText")){command["text"]=command["systemText"];entry=command;}}
    const bool literalCompletion=commands.size()==1&&commands.front().toMap().value("literalCompletion").toBool();
    auto error=validate(validationCommands,textOnly||((!physicalText||literalCompletion)&&sendText&&(!canCommitText||canCommitText())));if(!error.isEmpty()){emit failed(error);return false;}
    auto plannedHeld=held;
    static const QSet<QString> modifiers{"LSHIFT","RSHIFT","LCONTROL","RCONTROL","LMENU","RMENU","LWIN","RWIN"};
    for(const auto &v:validationCommands){auto c=v.toMap();auto tag=c["tag"].toString();auto name=KeyNames::canonical(c["text"].toString());
        bool toggle=tag=="KeyToggle"||(tag=="Action"&&modifiers.contains(KeyNames::functionKey(name)));
        if(tag=="KeyDown"||tag=="KeyUp"||toggle){
            if(!textOnly&&supportsPhysicalKey&&!supportsPhysicalKey(name)){emit failed("Authorize desktop control for held keys and shortcuts");return false;}
            if(tag=="KeyUp"||(toggle&&plannedHeld.contains(name)))plannedHeld.remove(name);else plannedHeld.insert(name);
        }
        if(tag=="Text"&&!plannedHeld.isEmpty())for(auto ch:c["text"].toString())if(KeyNames::usCharacter(ch).first.isEmpty()){emit failed("Release physical keys before committing Unicode text");return false;}
    }
    for(const auto &v:commands){auto c=v.toMap();const auto action=c["text"].toString().trimmed();
        if(c["tag"]=="Action"&&KeyNames::functionKey(action).isEmpty()&&supportsAction&&!supportsAction(action)){
            emit failed("Unsupported action: "+action);return false;
        }
    }
    queue=commands;index=0;waiting=false;running=true;emit stateChanged();timer.start(0);return true;
}
void ActionRunner::cancel(bool preserveModifiers,bool preserveToggles){
    endDwellHold();
    if(!preserveToggles)releaseToggledKeys();
    timer.stop();running=false;waiting=false;queue.clear();index=0;
    const auto copy=held;
    for(const auto &name:copy){
        if(!transient.contains(name)&&((preserveModifiers&&modifierKeys.contains(name))||(preserveToggles&&!modifierKeys.contains(name))))continue;
        held.remove(name);latched.remove(name);
        if(!textOnly&&sendKey)sendKey(name,false);
    }
    transient.clear();
    emit stateChanged();
    emit cancelled();
}
void ActionRunner::fail(QString message){cancel();emit failed(message);}
bool ActionRunner::key(QString name,bool down,bool persistent){
    name=KeyNames::canonical(name);
    if(held.contains(name)==down)return true;
    if(down)emit keyPressed(name);
    if(!textOnly&&(!sendKey||!sendKey(name,down)))return false;
    if(textOnly&&name=="CAPITAL"&&down)capsLock=!capsLock;
    if(down){held.insert(name);if(!persistent)transient.insert(name);}else{held.remove(name);transient.remove(name);latched.remove(name);}
    emit stateChanged();return true;
}
bool ActionRunner::tap(QString name){
    name=KeyNames::canonical(name);
    if(editingLocal()){emit keyPressed(name);return sendLocalKey(name,heldKeys());}
    if(isHeld(name)){emit failed("Release the held key before typing it again: "+name);return false;}
    return key(name,true)&&key(name,false);
}
void ActionRunner::step(){
    if(!running)return;
    if(index>=queue.size()){
        const auto copy=transient;for(auto k:copy)if(!key(k,false)){fail("Could not release key");return;}
        running=false;queue.clear();emit stateChanged();emit finished();return;
    }
    auto command=queue[index++].toMap();auto tag=command["tag"].toString();QString value=command["text"].toString();bool ok=true;
    if(physicalText&&tag=="Text"&&command.contains("systemText"))value=command["systemText"].toString();
    const bool consumesLatch=(tag=="Text"&&!value.isEmpty())||(tag=="Action"&&!KeyNames::functionKey(value).isEmpty()&&!modifierKeys.contains(KeyNames::functionKey(value)));
    if(tag=="Wait"){timer.start(value.toInt());return;}
    if(tag=="KeyDown")ok=key(value,true);
    else if(tag=="KeyUp")ok=key(value,false);
    else if(tag=="KeyToggle")ok=key(value,!isHeld(value),true);
    else if(tag=="Text"){
        bool shortcut=shortcutHeld();
        QString effective=value;
        if(textOnly||!held.isEmpty())for(auto &ch:effective)ch=KeyNames::usTypedCharacter(ch,shifted()||(capsLock&&ch.isLetter()));
        if(!shortcut&&removeSpaceBeforeText&&physicalText&&!textOnly&&!command.value("literalCompletion").toBool()){
            effective.clear();
            for(auto ch:value){
                auto [name,needsShift]=KeyNames::usCharacter(ch);
                const bool ownShift=needsShift&&!shifted();
                if(ownShift&&!key("LSHIFT",true)){fail("Could not press Shift");return;}
                if(physicalKeyText)effective+=physicalKeyText(name);
                if(ownShift&&!key("LSHIFT",false)){fail("Could not release Shift");return;}
            }
        }
        if(!shortcut&&removeSpaceBeforeText&&removeSpaceBeforeText(effective)){
            if((!textOnly||editingLocal())&&!tap("BACK")){fail("Could not remove the completion space");return;}
            if(!editingLocal())emit textCommitted("\b");
            if(appendSpaceAfterPunctuation&&appendSpaceAfterPunctuation(effective)){value+=' ';effective+=' ';}
        }
        QString removed;
        const int removePrefix=command.value("removePrefix").toInt();
        const int removeSuffix=command.value("removeSuffix").toInt();
        if(removePrefix<0||removePrefix>64||removeSuffix<0||removeSuffix>64||((shortcut||editingLocal()||!held.isEmpty())&&(removePrefix||removeSuffix))){fail("Completion replacement unavailable");return;}
        for(int i=0;i<removePrefix;++i){
            if(!textOnly&&!tap("BACK")){fail("Could not replace completion prefix");return;}
            removed+='\b';
        }
        for(int i=0;i<removeSuffix;++i)if(!textOnly&&!tap("DELETE")){fail("Could not replace completion suffix");return;}
        QString composed;
        const bool completion=command.value("literalCompletion").toBool();
        const bool unicodeCompletion=completion&&(command.value("alwaysUnicode").toBool()||std::any_of(value.cbegin(),value.cend(),[](QChar ch){return ch.unicode()>127;}));
        if(unicodeCompletion&&!textOnly){
            ok=held.isEmpty()&&sendText&&(!canCommitText||canCommitText())&&sendText(value);
            if(ok)composed=value;
        }
        else if(editingLocal())ok=sendLocalText(effective,heldKeys());
        else if(textOnly){for(auto ch:value)composed+=KeyNames::usTypedCharacter(ch,shifted()||(capsLock&&ch.isLetter()));}
        else if(!completion&&!physicalText&&sendText&&(!canCommitText||canCommitText())&&held.isEmpty()){
            QString pending;
            auto flush=[&]{if(pending.isEmpty())return true;bool result=sendText(pending);if(result)composed+=pending;pending.clear();return result;};
            for(auto ch:value){
                if(ch.category()==QChar::Other_Control){if(!flush()||!tap(KeyNames::usCharacter(ch).first)){ok=false;break;}composed+=ch;}
                else pending+=ch;
            }
            if(ok)ok=flush();
        }else{
        for(auto ch:value){auto [name,needsShift]=KeyNames::usCharacter(ch);bool ownShift=needsShift&&!shifted();
            bool upper=shifted()||needsShift;
            if(ownShift&&!key("LSHIFT",true)){ok=false;break;}
            const auto translated=physicalText&&physicalKeyText?physicalKeyText(name):QString();
            ok=tap(name);if(ownShift)ok=key("LSHIFT",false)&&ok;
            if(!ok)break;composed+=physicalText?translated:QString(KeyNames::usTypedCharacter(ch,upper));
        }
        }
        if(ok&&!editingLocal()&&(textOnly||!shortcut)&&(!physicalText||textOnly||!composed.isEmpty()))emit textCommitted(removed+composed);
    }else if(tag=="ChangeKeyboard"){
        bool back=command["attributes"].toMap().value("backreturnshere","true").toString().compare("false",Qt::CaseInsensitive)!=0;
        if(navigate)navigate(value.trimmed(),back);else ok=false;
    }else if(tag=="Action"){
        value=value.trimmed();auto name=KeyNames::functionKey(value);
        static const QSet<QString> modifiers{"LSHIFT","RSHIFT","LCONTROL","RCONTROL","LMENU","RMENU","LWIN","RWIN"};
        if(modifiers.contains(name))ok=queue.size()==1?cycleModifier(name):key(name,!isHeld(name),true);
        else if(!name.isEmpty()){ok=tap(name);if(ok&&!editingLocal()&&name=="BACK"&&!shortcutHeld())emit textCommitted("\b");}
        else if(value=="SpeakLabel"){
            ok=bool(speakLabel);
            if(ok)speakLabel(command["label"].toString().trimmed());
        }
        else ok=performAction&&performAction(value);
    }
    if(!running)return; // A navigation/pause action may cancel the runner.
    if(!ok){const auto localError=editingLocal()&&localInputError?localInputError():QString();fail(localError.isEmpty()?"Action could not complete: "+tag+" "+(tag=="Text"?QString("[text]"):value.trimmed()):localError);return;}
    if(consumesLatch){
        const auto copy=latched;
        for(const auto &name:copy)if(!key(name,false)){fail("Could not release latched modifier");return;}
    }
    if(!waiting)timer.start(0);
}

void ActionRunner::suspend(){if(running){timer.stop();waiting=true;}}
void ActionRunner::resume(bool success){if(!running||!waiting)return;waiting=false;if(!success){fail("Pointer action failed");return;}timer.start(0);}

QPair<QString,bool> ActionRunner::dwellKey(const QVariantList &commands,bool system){
    if(commands.size()!=1)return {};
    const auto c=commands.front().toMap();
    const auto text=(system&&c.contains("systemText")?c["systemText"]:c["text"]).toString();
    if(c["tag"]=="Text"&&text.size()==1){
        return KeyNames::usCharacter(text.front());
    }
    if(c["tag"]=="Action"){
        const auto code=KeyNames::functionKey(text);
        if(!code.isEmpty()&&!modifierKeys.contains(code))return {code,false};
    }
    return {};
}
bool ActionRunner::beginDwellHold(const QVariantList &commands,const QString &owner){
    if(running||textOnly)return false;
    endDwellHold(owner);
    const auto [name,shift]=dwellKey(commands,physicalText);
    bool owned=false;
    for(const auto &hold:dwellHolds)if(hold.first==name)owned=true;
    if(name.isEmpty()||(isHeld(name)&&!owned))return false;
    if(supportsPhysicalKey&&(!supportsPhysicalKey(name)||(shift&&!supportsPhysicalKey("LSHIFT")))){
        emit failed("Authorize desktop control for key hold");return false;
    }
    const bool needsShift=shift&&!shifted();
    if(needsShift&&!key("LSHIFT",true,true))return false;
    if(!key(name,true,true)){if(needsShift)key("LSHIFT",false);return false;}
    dwellShift=dwellShift||needsShift;
    dwellHolds[owner]={name,shift};return true;
}
void ActionRunner::endDwellHold(const QString &owner){
    if(dwellHolds.isEmpty())return;
    if(owner.isEmpty()){
        QSet<QString> names;for(const auto &hold:dwellHolds)names.insert(hold.first);
        dwellHolds.clear();for(const auto &name:names)key(name,false);
    }else if(dwellHolds.contains(owner)){
        const auto hold=dwellHolds.take(owner);
        bool stillHeld=false;
        for(const auto &other:dwellHolds)if(other.first==hold.first)stillHeld=true;
        if(!stillHeld)key(hold.first,false);
    }else return;
    if(dwellHolds.isEmpty()){
        const auto copy=latched;
        for(const auto &modifier:copy)key(modifier,false);
    }
    bool needsShift=false;for(const auto &hold:dwellHolds)needsShift=needsShift||hold.second;
    if(dwellShift&&!needsShift){dwellShift=false;key("LSHIFT",false);}
}

bool ActionRunner::toggleDwellKey(const QVariantList &commands){
    if(running||textOnly)return false;
    const auto [name,shift]=dwellKey(commands,physicalText);
    if(name.isEmpty())return false;
    if(toggledKeys.contains(name)){
        if(!key(name,false)){fail("Could not release toggled key");return false;}
        toggledKeys.remove(name);shiftedToggles.remove(name);
        if(shiftedToggles.isEmpty()&&toggleOwnShift){
            toggleOwnShift=false;
            if(!key("LSHIFT",false)){fail("Could not release toggle Shift");return false;}
        }
        return true;
    }
    if(isHeld(name))return false;
    if(supportsPhysicalKey&&(!supportsPhysicalKey(name)||(shift&&!supportsPhysicalKey("LSHIFT")))){
        emit failed("Authorize desktop control for key toggle");return false;
    }
    if(shift&&!shifted()){
        if(!key("LSHIFT",true,true)){fail("Could not press toggle Shift");return false;}
        toggleOwnShift=true;
    }
    if(!key(name,true,true)){fail("Could not press toggled key");return false;}
    toggledKeys.insert(name);
    if(shift)shiftedToggles.insert(name);
    emit stateChanged();
    return true;
}
void ActionRunner::releaseToggledKeys(){
    const auto copy=toggledKeys;toggledKeys.clear();shiftedToggles.clear();
    for(const auto &name:copy)key(name,false);
    if(toggleOwnShift){toggleOwnShift=false;key("LSHIFT",false);}
}

bool ActionRunner::isToggled(const QVariantList &commands)const{
    if(commands.size()!=1)return false;
    const auto command=commands.front().toMap();
    if(command["tag"]=="KeyToggle"){
        const auto name=KeyNames::canonical(command["text"].toString());
        return !modifierKeys.contains(name)&&isHeld(name);
    }
    const auto name=dwellKey(commands,physicalText).first;
    return !name.isEmpty()&&toggledKeys.contains(name)&&isHeld(name);
}
