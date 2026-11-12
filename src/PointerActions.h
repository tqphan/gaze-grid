#pragma once
#include <QObject>
#include <QPointF>
#include <QSet>
#include <QTimer>
#include <QElapsedTimer>
#include <QSizeF>
#include "LookScroll.h"
#include "TargetAssist.h"
#include <functional>
#include <optional>

class PointerActions:public QObject {
    Q_OBJECT
public:
    explicit PointerActions(QObject *parent=nullptr);
    std::function<bool(double,double)> move;
    std::function<bool(double,double)> moveRelative;
    std::function<bool(double,double)> moveRelativeTo;
    std::function<bool(int,bool)> button;
    // Optional backend-owned release, independent of this object's Qt timer.
    std::function<bool(int,int)> timedButton;
    std::function<bool(double)> scroll;
    std::function<bool(double,double)> scrollAxes;
    std::function<void()> releaseAll;
    bool start(const QString &action,int initialDelayMs=0);
    void setClickDuration(int ms){clickDurationMs=qBound(0,ms,250);}
    void select(QPointF point);
    void cancel();
    void trackingLost();
    void configureScroll(int speed,int deadzone,int axis){lookScroll.configure(speed,deadzone,axis);}
    void setScrollGazePolicy(bool clamp,bool cancelOffscreen){clampScrollGaze=clamp;cancelScrollOffscreen=cancelOffscreen;}
    void setMagneticRelative(bool relative){relativeMagnetic=relative;}
    void setTargetAssist(bool enabled){if(assistEnabled!=enabled){assistEnabled=enabled;assist.reset();}}
    void configureTargetAssist(int strength,int range,int retention,int mode=0,double edgeRange=2,double edgeInset=1){assist.configure(strength,range,retention,mode,edgeRange,edgeInset);}
    std::optional<QPointF> magneticPosition()const{return following&&followPositionValid?std::optional<QPointF>(position):std::nullopt;}
    bool follow(QPointF point,bool overDesktop=true);
    void setDisplaySize(QSizeF size);
    bool magnetic()const{return following;}
    bool lookScrolling()const{return lookScroll.active();}
    QPointF scrollCentre()const{return lookScroll.centre();}
    bool targeting()const{return choosing;}
    bool busy()const{return active;}
    bool hidesKeyboard()const{return active&&current.action!="MouseMagneticCursor";}
    QString prompt()const{return instruction;}
    static bool supports(const QString &action);
    static bool needsAbsolute(const QString &action);
    QStringList heldButtons()const;
 signals:
    void changed();
    void completed(bool success);
 private:
    void reset(bool preserveMagnetic);
    struct Operation { QString action; QPointF source,target; } current,last;
    void execute();
    void tick();
    void finish(bool success);
    bool setButton(int code,bool down);
    bool click(int code);
    bool moveTo(QPointF p);
    QTimer timer;
    bool scrollTrackingPaused=false;
    QSet<int> held;
    QPointF position{.5,.5};
    bool followPositionValid=false;
    TargetAssist assist;
    bool assistEnabled=false;
    bool relativeMagnetic=false;
    bool clampScrollGaze=true,cancelScrollOffscreen=false;
    bool active=false,choosing=false,sourceChosen=false,following=false;
    int phase=0,dragStep=0;
    int clickDurationMs=0,pendingClickButton=272,pendingClickDurationMs=0;
    QElapsedTimer clickClock;
    QString instruction;
    QSizeF displaySize;
    LookScroll lookScroll;
    QElapsedTimer clock;
};
