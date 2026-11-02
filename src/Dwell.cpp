#include "Dwell.h"
#include <algorithm>
bool Dwell::update(const QString &target, qint64 now, int duration, bool repeat, int repeatDuration, int graceMs) {
    if (now < last || (!fired && last >= 0 && now-last > 200)) reset();
    if (graceMs<=0) saved.clear();
    for(auto it=saved.begin();it!=saved.end();)
        if(now-it->left>graceMs) it=saved.erase(it); else ++it;
    if(target!=current){
        // Preserve only unfinished initial dwell; holds and repeats end on leave.
        if(graceMs>0&&!current.isEmpty()&&!fired&&!repeating)
            saved[current]={std::max(qint64(0),last-started),now};
        current=target;started=now;fired=false;repeating=false;
        const auto previous=saved.find(target);
        if(previous!=saved.end()){started=now-previous->elapsed;saved.erase(previous);}
    }
    if(target.isEmpty()){last=now;return false;}
    last=now;
    if (repeating && !repeat) { fired=true; repeating=false; }
    if (fired && repeat) { fired=false; repeating=true; started=now; }
    const int interval=repeating&&repeatDuration>0?repeatDuration:duration;
    if (!fired && now-started >= interval) { fired=true; return true; }
    return false;
}
void Dwell::inhibit(const QString &target, qint64 now) {
    // Busy actions must not rearm the key that started them. A different
    // target starts from zero once input becomes available again.
    saved.clear();
    if (target.isEmpty() || target != current || !fired || now < last) reset();
    else last=now;
}
void Dwell::reset() { saved.clear();current.clear(); started=0; last=-1; fired=false; repeating=false; }
void Dwell::pause(qint64 now) { if(last>=0&&now>=last){started+=now-last;last=now;} }
double Dwell::progress(qint64 now, int duration, int repeatDuration) const {
    const int interval=repeating&&repeatDuration>0?repeatDuration:duration;
    return current.isEmpty() || fired ? 0 : std::clamp(double(now-started)/std::max(1,interval),0.0,1.0);
}

void Dwell::consume(const QString &target, qint64 now) {current=target;started=now;last=now;fired=true;repeating=false;}
QHash<QString,qint64> Dwell::savedElapsed(qint64 now,int graceMs) const {
    QHash<QString,qint64> result;
    if(graceMs<=0)return result;
    for(auto it=saved.cbegin();it!=saved.cend();++it)
        if(now>=it->left&&now-it->left<=graceMs)
            result[it.key()]=it->elapsed;
    return result;
}
