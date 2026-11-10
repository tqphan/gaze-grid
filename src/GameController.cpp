#include "GameController.h"
#include <QTimer>
#include <cmath>
#include <cerrno>
#include <cstring>
#include "GamepadCodes.h"
#ifndef _WIN32
#include <linux/uinput.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#endif
namespace {
const QMap<QString,int> &buttonCodes(){
    static const auto map=[] {QMap<QString,int> m{{"A",BTN_SOUTH},{"B",BTN_EAST},{"X",BTN_WEST},{"Y",BTN_NORTH},{"C",BTN_C},{"Z",BTN_Z},{"LeftShoulder",BTN_TL},{"RightShoulder",BTN_TR},{"LeftTriggerButton",BTN_TL2},{"RightTriggerButton",BTN_TR2},{"Back",BTN_SELECT},{"Start",BTN_START},{"Guide",BTN_MODE},{"LeftStick",BTN_THUMBL},{"RightStick",BTN_THUMBR},{"DpadUp",BTN_DPAD_UP},{"DpadDown",BTN_DPAD_DOWN},{"DpadLeft",BTN_DPAD_LEFT},{"DpadRight",BTN_DPAD_RIGHT}};for(int i=1;i<=32;++i)m[QString("Button%1").arg(i)]=BTN_TRIGGER_HAPPY+i-1;return m;}();return map;
}
const QMap<QString,int> axisCodes{{"LeftX",ABS_X},{"LeftY",ABS_Y},{"RightX",ABS_RX},{"RightY",ABS_RY},{"LeftTrigger",ABS_Z},{"RightTrigger",ABS_RZ},{"HatX",ABS_HAT0X},{"HatY",ABS_HAT0Y},{"Throttle",ABS_THROTTLE},{"Rudder",ABS_RUDDER},{"Wheel",ABS_WHEEL}};
bool trigger(int code){return code==ABS_Z||code==ABS_RZ;}
bool hat(int code){return code==ABS_HAT0X||code==ABS_HAT0Y;}
bool parseAxis(QString action,int &code,double &value){auto parts=action.split(':');if(parts.size()!=2||!parts[0].startsWith("Gamepad"))return false;auto it=axisCodes.constFind(parts[0].mid(7));if(it==axisCodes.end())return false;code=it.value();bool ok=false;value=parts[1].toDouble(&ok);return ok&&std::isfinite(value)&&value>=(trigger(code)?0:-1)&&value<=1;}
bool parseButton(QString action,int &code,QString &mode){if(!action.startsWith("Gamepad"))return false;action=action.mid(7);for(const auto &suffix:{QString("Press"),QString("Release"),QString("Toggle"),QString("Tap")})if(action.endsWith(suffix)){mode=suffix;action.chop(suffix.size());auto it=buttonCodes().constFind(action);if(it==buttonCodes().end())return false;code=it.value();return true;}return false;}
}
GameController::~GameController(){closeDevice();}
bool GameController::supports(const QString &action){int code;double value;QString mode;return action=="GamepadReset"||parseAxis(action,code,value)||parseButton(action,code,mode);}
QString GameController::buttonAction(const QString &action,const QString &mode){
    int code;QString original;
    if(!parseButton(action.trimmed(),code,original)||original=="Release")return {};
    if(mode!="Tap"&&mode!="Press"&&mode!="Toggle"&&mode!="Release")return {};
    return action.trimmed().chopped(original.size())+mode;
}
bool GameController::held(const QString &action)const{int code;QString mode;return parseButton(action,code,mode)&&buttons.value(code)!=0;}
QStringList GameController::actions(){QStringList list{"GamepadReset"};for(auto it=buttonCodes().begin();it!=buttonCodes().end();++it)for(const auto &suffix:{"Press","Release","Toggle","Tap"})list<<"Gamepad"+it.key()+suffix;for(auto it=axisCodes.begin();it!=axisCodes.end();++it)list<<"Gamepad"+it.key()+":0";return list;}
void GameController::closeDevice(){
#ifndef _WIN32
if(fd>=0){ioctl(fd,UI_DEV_DESTROY);::close(fd);fd=-1;}
#else
windowsPad.close();
#endif
buttons.clear();axes.clear();generations.clear();toggledButtons.clear();}
bool GameController::connectDevice(){
    if(fd>=0||output)return true;
#ifdef _WIN32
    return true; // ViGEm connects lazily after validating the requested report.
#else
    fd=::open("/dev/uinput",O_WRONLY|O_NONBLOCK|O_CLOEXEC);
    auto fail=[this]{message="Virtual controller: "+QString::fromLocal8Bit(strerror(errno))+". Grant your session access to /dev/uinput (see docs/game-controller.md).";closeDevice();return false;};
    if(fd<0)return fail();
    if(ioctl(fd,UI_SET_EVBIT,EV_KEY)<0||ioctl(fd,UI_SET_EVBIT,EV_ABS)<0)return fail();
    for(int code:buttonCodes())if(ioctl(fd,UI_SET_KEYBIT,code)<0)return fail();
    for(int code:axisCodes){if(ioctl(fd,UI_SET_ABSBIT,code)<0)return fail();uinput_abs_setup a{};a.code=code;a.absinfo.minimum=trigger(code)?0:hat(code)?-1:-32767;a.absinfo.maximum=hat(code)?1:32767;if(ioctl(fd,UI_ABS_SETUP,&a)<0)return fail();}
    uinput_setup setup{};strcpy(setup.name,"Gaze Grid Virtual Controller");setup.id.bustype=BUS_VIRTUAL;setup.id.vendor=0x1209;setup.id.product=0x0001;setup.id.version=1;
    if(ioctl(fd,UI_DEV_SETUP,&setup)<0||ioctl(fd,UI_DEV_CREATE)<0)return fail();message.clear();return true;
#endif
}
bool GameController::send(int type,int code,int value){
    if(!connectDevice())return false;
    bool ok;
    if(output)ok=output(type,code,value);

#ifndef _WIN32
    else {input_event frame[2]{};frame[0].type=type;frame[0].code=code;frame[0].value=value;frame[1].type=EV_SYN;frame[1].code=SYN_REPORT;ssize_t n;do{n=::write(fd,frame,sizeof(frame));}while(n<0&&errno==EINTR);ok=n==sizeof(frame);}
#else
    else {ok=windowsPad.send(type,code,value);if(!ok)message=windowsPad.error();}
#endif
    if(!ok){
#ifndef _WIN32
message="Virtual controller output failed; device disconnected";
#else
if(output)message="Virtual controller output failed; device disconnected";
#endif
closeDevice();emit changed();emit failed(message);return false;}
    (type==EV_KEY?buttons:axes)[code]=value;emit changed();return true;
}
void GameController::reset(bool preserveToggles){
    if(!preserveToggles)toggledButtons.clear();
    for(auto it=generations.begin();it!=generations.end();++it)it.value()=++sequence;
    const auto b=buttons,a=axes;for(auto it=b.begin();it!=b.end();++it)if(it.value()&&!toggledButtons.contains(it.key())&&!send(EV_KEY,it.key(),0))return;for(auto it=a.begin();it!=a.end();++it)if(it.value()&&!send(EV_ABS,it.key(),0))return;
}
bool GameController::perform(const QString &action){
    if(action=="GamepadReset"){reset();return true;}
    int code;double value;QString mode;
    if(parseAxis(action,code,value))return send(EV_ABS,code,qRound(value*(hat(code)?1:32767)));
    if(!parseButton(action,code,mode))return false;
    if(mode=="Tap"&&buttons.value(code))return true;
    const int generation=generations[code]=++sequence;
    if(!send(EV_KEY,code,mode=="Release"?0:mode=="Toggle"?!buttons.value(code):1))return false;
    if(mode=="Toggle"&&buttons.value(code))toggledButtons.insert(code);else toggledButtons.remove(code);
    if(mode=="Tap")QTimer::singleShot(100,this,[this,code,generation]{if(generations.value(code)==generation)send(EV_KEY,code,0);});
    return true;
}
bool GameController::stick(const QString &name,double x,double y){
    if((name!="Left"&&name!="Right")||!std::isfinite(x)||!std::isfinite(y))return false;
    double length=std::hypot(x,y);if(length<=.15)x=y=0;else {double scale=(qMin(length,1.)-.15)/(.85*length);x*=scale;y*=scale;}
    int xc=name=="Left"?ABS_X:ABS_RX,yc=name=="Left"?ABS_Y:ABS_RY;
    int xv=qRound(x*32767),yv=qRound(y*32767);
    return (axes.value(xc)==xv||send(EV_ABS,xc,xv))&&(axes.value(yc)==yv||send(EV_ABS,yc,yv));
}
