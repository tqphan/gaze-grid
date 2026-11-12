#include "PointerActions.h"
#include <cmath>

PointerActions::PointerActions(QObject *parent):QObject(parent){
    clock.start();
    timer.setTimerType(Qt::PreciseTimer);timer.setSingleShot(true);connect(&timer,&QTimer::timeout,this,&PointerActions::tick);
}
bool PointerActions::supports(const QString &a){
    static const QSet<QString> names{"MouseMoveTo","MouseMoveAndLeftClick","MouseMoveAndLeftDoubleClick",
        "MouseMoveAndMiddleClick","MouseMoveAndRightClick","MouseLeftClick","MouseDrag",
        "MouseLeftDownUp","MouseMiddleDownUp","MouseRightDownUp","MouseScrollToTop","MouseScrollToBottom",
        "MouseMoveToTop","MouseMoveToBottom","MouseMoveToLeft","MouseMoveToRight","RepeatLastMouseAction","MouseMagneticCursor","LookToScrollActive"};
    return names.contains(a);
}
bool PointerActions::needsAbsolute(const QString &a){
    return a=="LookToScrollActive"||a=="MouseMagneticCursor"||a=="MouseDrag"||a=="MouseMoveTo"||a.startsWith("MouseMoveAnd");
}
QStringList PointerActions::heldButtons()const{
    QStringList result;for(int code:{272,273,274})if(held.contains(code))result<< (code==272?"Left":code==273?"Right":"Middle");return result;
}
bool PointerActions::start(const QString &a,int initialDelayMs){
    if(active||!supports(a))return false;
    if(a=="LookToScrollActive"&&!displaySize.isValid())return false;
    if(a=="RepeatLastMouseAction"&&last.action.isEmpty())return false;
    current=a=="RepeatLastMouseAction"?last:Operation{a,position,position};
    // A click or drag must not silently release a separately held button.
    const int clickButton=current.action=="MouseMoveAndRightClick"?273:current.action=="MouseMoveAndMiddleClick"?274:272;
    if((current.action.contains("Click")||current.action=="MouseDrag")&&held.contains(clickButton))return false;
    scrollTrackingPaused=false;
    active=true;sourceChosen=false;phase=0;
    choosing=a!="RepeatLastMouseAction"&&(a=="MouseMoveTo"||a.startsWith("MouseMoveAnd")||a=="MouseDrag"||(a=="LookToScrollActive"&&!lookScroll.active()));
    instruction=a=="MouseDrag"?"Choose the drag source":"Choose a desktop target";
    if(a=="LookToScrollActive")instruction="Choose the centre of the scroll area";
    emit changed();
    if(!choosing)timer.start(qMax(0,initialDelayMs));
    return true;
}
void PointerActions::select(QPointF p){
    if(!active||!choosing||!std::isfinite(p.x())||!std::isfinite(p.y())||p.x()<0||p.x()>1||p.y()<0||p.y()>1)return;
    if(current.action=="MouseDrag"&&!sourceChosen){current.source=p;sourceChosen=true;instruction="Choose the drag destination";emit changed();return;}
    current.target=p;choosing=false;emit changed();
    // Allow the targeting surface to disappear before moving/clicking.
    timer.start(100);
}
bool PointerActions::setButton(int code,bool down){
    if(!button||!button(code,down))return false;
    if(down)held.insert(code);else held.remove(code);emit changed();return true;
}
bool PointerActions::click(int code){
    if(clickDurationMs>0&&timedButton){
        if(!timedButton(code,clickDurationMs))return false;
        held.insert(code);emit changed();
    }else if(!setButton(code,true))return false;
    if(clickDurationMs==0)return setButton(code,false);
    pendingClickButton=code;
    pendingClickDurationMs=clickDurationMs;
    clickClock.start();
    phase=current.action=="MouseMoveAndLeftDoubleClick"&&phase!=3?4:5;
    timer.start(clickDurationMs);return true;
}
bool PointerActions::moveTo(QPointF p){if(!move||!move(p.x(),p.y()))return false;position=p;return true;}
void PointerActions::execute(){
    const auto a=current.action;bool ok=false;
    if(a=="LookToScrollActive"){
        if(lookScroll.active())lookScroll.stop();
        else {
            if(!moveTo(current.target)){finish(false);return;}
            following=false;lookScroll.start({current.target.x()*displaySize.width(),current.target.y()*displaySize.height()},clock.elapsed());
        }
        finish(true);return;
    }
    if(a=="MouseMagneticCursor"){assist.reset();followPositionValid=false;following=!following;if(following)lookScroll.stop();finish(true);return;}
    if(a=="MouseDrag"){
        if(!moveTo(current.source)||!setButton(272,true)){finish(false);return;}
        phase=1;dragStep=0;timer.start(100);return;
    }
    if(a=="MouseLeftDownUp"||a=="MouseMiddleDownUp"||a=="MouseRightDownUp"){
        const int code=a=="MouseLeftDownUp"?272:a=="MouseRightDownUp"?273:274;
        ok=setButton(code,!held.contains(code));if(ok)last={};
    }else if(a=="MouseScrollToTop"||a=="MouseScrollToBottom")ok=scroll&&scroll(a.endsWith("Top")?-120:120);
    else if(a=="MouseLeftClick"){ok=click(272);if(ok&&phase==5)return;}
    else if(a=="MouseMoveToTop"||a=="MouseMoveToBottom"||a=="MouseMoveToLeft"||a=="MouseMoveToRight"){
        const double dx=a.endsWith("Left")?-25:a.endsWith("Right")?25:0;
        const double dy=a.endsWith("Top")?-25:a.endsWith("Bottom")?25:0;
        ok=moveRelative&&moveRelative(dx,dy);
    }else {
        ok=moveTo(current.target);
        if(ok&&a.startsWith("MouseMoveAnd")){
            const int code=a=="MouseMoveAndRightClick"?273:a=="MouseMoveAndMiddleClick"?274:272;
            ok=click(code);
            if(ok&&(phase==4||phase==5))return;
            if(ok&&a=="MouseMoveAndLeftDoubleClick"){phase=3;timer.start(90);return;}
        }
    }
    finish(ok);
}
void PointerActions::tick(){
    if(!active||choosing)return;
    if(phase==0){execute();return;}
    if(phase==1){
        ++dragStep;auto p=current.source+(current.target-current.source)*(dragStep/12.0);
        if(!moveTo(p)){finish(false);return;}
        if(dragStep==12){phase=2;timer.start(100);}else timer.start(16);
    }else if(phase==2)finish(setButton(272,false));
    else if(phase==3){const bool ok=click(272);if(!ok||phase!=5)finish(ok);}
    else if(phase==4||phase==5){
        // A platform timer can wake at a rounded millisecond boundary. Never
        // release before the requested hold duration has actually elapsed.
        const auto remaining=pendingClickDurationMs-clickClock.elapsed();
        if(remaining>0){timer.start(int(remaining));return;}
        const bool anotherClick=phase==4;
        if(!setButton(pendingClickButton,false)){finish(false);return;}
        if(anotherClick){phase=3;timer.start(90);}else finish(true);
    }
}
void PointerActions::finish(bool success){
    timer.stop();active=false;choosing=false;phase=0;
    if(success&&!current.action.endsWith("DownUp")&&current.action!="MouseMagneticCursor"&&current.action!="LookToScrollActive")last=current;
    if(!success)lookScroll.stop();
    if(!success){following=false;if(releaseAll)releaseAll();held.clear();last={};}
    instruction=success?(lookScroll.active()?"Look to scroll active":"Pointer action complete"):"Pointer action failed; held input released";
    emit changed();emit completed(success);
}
void PointerActions::cancel(){
    reset(false);
}
void PointerActions::reset(bool preserveMagnetic){
    followPositionValid=false;
    scrollTrackingPaused=false;
    assist.reset();
    timer.stop();active=false;choosing=false;phase=0;sourceChosen=false;last={};following=preserveMagnetic&&following;
    lookScroll.stop();
    if(releaseAll)releaseAll();held.clear();instruction.clear();emit changed();
}
void PointerActions::trackingLost(){
    // Preserve continuous modes, but cancel pending clicks and target selection.
    if(!active&&lookScroll.active())scrollTrackingPaused=true;
    else reset(true);
}
bool PointerActions::follow(QPointF point,bool overDesktop){
    if((!following&&!lookScroll.active())||active)return false;
    if(!std::isfinite(point.x())||!std::isfinite(point.y())){trackingLost();return false;}
    if(following){
        point.setX(qBound(0.,point.x(),1.));
        point.setY(qBound(0.,point.y(),1.));
    }else if(point.x()<0||point.x()>1||point.y()<0||point.y()>1){
        if(cancelScrollOffscreen){cancel();return false;}
        if(clampScrollGaze){
            point.setX(qBound(0.,point.x(),1.));
            point.setY(qBound(0.,point.y(),1.));
        }
    }
    if(lookScroll.active()){
        if(scrollTrackingPaused){
            scrollTrackingPaused=false;lookScroll.resetTime(clock.elapsed());
            return true;
        }
        const auto delta=lookScroll.update({point.x()*displaySize.width(),point.y()*displaySize.height()},clock.elapsed(),overDesktop);
        if(!lookScroll.active()){emit changed();return false;}
        if(!delta.isNull()&&(!scrollAxes||!scrollAxes(delta.x(),delta.y()))){cancel();return false;}
    }
    if(following){
        if(assistEnabled&&held.isEmpty())point=assist.adjust(point);
        else assist.reset();
        if(relativeMagnetic){
            if(!moveRelativeTo||!moveRelativeTo(point.x(),point.y())){cancel();return false;}
        }else if(!moveTo(point)){cancel();return false;}
        position=point;followPositionValid=true;
    }
    return true;
}
void PointerActions::setDisplaySize(QSizeF size){if(size!=displaySize){assist.reset();cancel();displaySize=size;}}
