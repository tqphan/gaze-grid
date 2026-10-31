#include "PortalInput.h"
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusObjectPath>
#include <QDBusUnixFileDescriptor>
#include <QDBusVariant>
#include <QSocketNotifier>
#include <QTimer>
#include <QUuid>
#include <libei.h>
#include <sys/mman.h>
#include <unistd.h>
#include "AppPaths.h"
#include <QDir>
#include <QGuiApplication>
#include <QScreen>
#include "PortalPermissionStore.h"
namespace {
const QString service="org.freedesktop.portal.Desktop",path="/org/freedesktop/portal/desktop",iface="org.freedesktop.portal.RemoteDesktop";
QDBusMessage call(const QString &method,const QVariantList &args){auto m=QDBusMessage::createMethodCall(service,path,iface,method);m.setArguments(args);return m;}
PortalPermissionStore permissions(){return PortalPermissionStore(QDir(appConfigDirectory()).filePath("desktop-permission.json"));}
}
PortalInput::~PortalInput(){close();}
QVariantMap PortalInput::probe(){
 QDBusInterface props(service,path,"org.freedesktop.DBus.Properties",QDBusConnection::sessionBus());
 QDBusReply<QVariantMap> reply=props.call("GetAll",iface);
 if(!reply.isValid())return {{"error",reply.error().message()}};
 return reply.value();
}
void PortalInput::setReady(bool v){if(available!=v){available=v;emit readyChanged();}}
void PortalInput::request(const QString &method,const QVariantList &args,QVariantMap options,std::function<void(QVariantMap)> next){
 auto bus=QDBusConnection::sessionBus();QString token="gaze_grid_"+QUuid::createUuid().toString(QUuid::Id128);
 QString sender=bus.baseService().mid(1);sender.replace('.','_');
 requestPath="/org/freedesktop/portal/desktop/request/"+sender+"/"+token;
 options["handle_token"]=token;continuation=std::move(next);
 bus.connect(service,requestPath,"org.freedesktop.portal.Request","Response",this,SLOT(response(uint,QVariantMap)));
 auto all=args;all<<options; auto watcher=new QDBusPendingCallWatcher(bus.asyncCall(call(method,all)),this);
 auto epoch=generation;
 connect(watcher,&QDBusPendingCallWatcher::finished,this,[this,epoch](QDBusPendingCallWatcher *w){
  QDBusPendingReply<QDBusObjectPath> reply=*w;w->deleteLater();
  if(epoch!=generation)return;
  if(reply.isError()){emit status("Portal: "+reply.error().message());close();}
 });
}
void PortalInput::response(uint code,const QVariantMap &results){
 if(!calledFromDBus()||message().path()!=requestPath)return;
 QDBusConnection::sessionBus().disconnect(service,requestPath,"org.freedesktop.portal.Request","Response",this,SLOT(response(uint,QVariantMap)));
 requestPath.clear();auto next=std::move(continuation);continuation={};
 if(code!=0){emit status(code==1?"Desktop permission cancelled":"Desktop permission failed");close();return;}
 if(next)next(results);
}
void PortalInput::connectPortal(){
 if(pending||available)return;pending=true;persistenceRequested=false;permissionNotice.clear();emit status("Requesting desktop control…");
 request("CreateSession",{},{{"session_handle_token","gaze_grid_"+QUuid::createUuid().toString(QUuid::Id128)}},[this](QVariantMap r){
  session=r["session_handle"].toString();
  if(session.isEmpty()){emit status("Portal returned no session");close();return;}
  QDBusConnection::sessionBus().connect(service,session,"org.freedesktop.portal.Session","Closed",this,SLOT(sessionClosed()));
  selectDevices();
 });
}
void PortalInput::selectDevices(){
 if(!rememberPermission){beginSelection({{"types",uint(3)}});return;}
 auto message=QDBusMessage::createMethodCall(service,path,"org.freedesktop.DBus.Properties","Get");message.setArguments({iface,"version"});
 const auto epoch=generation;auto watcher=new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message),this);
 connect(watcher,&QDBusPendingCallWatcher::finished,this,[this,epoch](QDBusPendingCallWatcher *watcher){
  QDBusPendingReply<QDBusVariant> reply=*watcher;watcher->deleteLater();if(epoch!=generation)return;
  QVariantMap options{{"types",uint(3)}};
  if(!reply.isError()&&reply.value().variant().toUInt()>=2){
   persistenceRequested=true;options["persist_mode"]=uint(2);QString error;auto token=permissions().take(error);
   if(!token.isEmpty())options["restore_token"]=token;
   if(!error.isEmpty())permissionNotice=" — "+error;
  }else permissionNotice=" — desktop does not support remembered permission";
  beginSelection(options);
 });
}
void PortalInput::beginSelection(QVariantMap options){
 request("SelectDevices",{QVariant::fromValue(QDBusObjectPath(session))},options,[this](QVariantMap){
   request("Start",{QVariant::fromValue(QDBusObjectPath(session)),QString()}, {},[this](QVariantMap result){
    if((result["devices"].toUInt()&3)!=3){emit status("Keyboard and pointer permission required");close();return;}
    if(persistenceRequested){
     const auto token=result["restore_token"].toString();QString error;
     if(token.isEmpty())permissionNotice=" — permission was not remembered by the desktop";
     else if(!permissions().save(token,error))permissionNotice=" — "+error;
     else permissionNotice=" — desktop permission remembered";
    }
    connectEis();
   });
  });
}
void PortalInput::forgetPermission(){
 close();QString error;
 emit status(permissions().forget(error)?"Disconnected and forgot saved desktop permission":error);
}
void PortalInput::connectEis(){
 auto w=new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(call("ConnectToEIS",{QVariant::fromValue(QDBusObjectPath(session)),QVariantMap{}})),this);
 auto epoch=generation;
 connect(w,&QDBusPendingCallWatcher::finished,this,[this,epoch](QDBusPendingCallWatcher *w){
  QDBusPendingReply<QDBusUnixFileDescriptor> reply=*w;w->deleteLater();if(epoch!=generation)return;
  pending=false;
  if(reply.isError()){
   // Only an unsupported EIS method permits fallback; permission failures are not bypassed.
   const auto name=reply.error().name();
   if(name=="org.freedesktop.DBus.Error.UnknownMethod" || name=="org.freedesktop.portal.Error.NotSupported"){
    eisMode=false;setReady(true);emit status("Desktop connected — portal D-Bus input"+permissionNotice);
   }else {emit status("EIS connection: "+reply.error().message());close();}
   return;
  }
  eisMode=true;context=ei_new_sender(this);ei_configure_name(context,"gaze-grid");
  int fd=dup(reply.value().fileDescriptor());
  if(fd<0 || ei_setup_backend_fd(context,fd)!=0){emit status("Unable to initialize EIS");close();return;}
  notifier=new QSocketNotifier(ei_get_fd(context),QSocketNotifier::Read,this);
  connect(notifier,&QSocketNotifier::activated,this,[this]{dispatch();});dispatch();
  emit status("Desktop authorized — waiting for EIS devices");
  auto epoch=generation;
  QTimer::singleShot(5000,this,[this,epoch]{if(epoch==generation && !available){emit status("EIS devices unavailable; reconnect desktop control to retry");close();}});
 });
}
void PortalInput::dispatch(){
 if(!context)return;ei_dispatch(context);
 while(auto event=ei_get_event(context)){
  auto type=ei_event_get_type(event);auto d=ei_event_get_device(event);
  if(type==EI_EVENT_SEAT_ADDED)ei_seat_bind_capabilities(ei_event_get_seat(event),EI_DEVICE_CAP_KEYBOARD,EI_DEVICE_CAP_POINTER,EI_DEVICE_CAP_POINTER_ABSOLUTE,EI_DEVICE_CAP_BUTTON,EI_DEVICE_CAP_SCROLL,nullptr);
  if(type==EI_EVENT_DEVICE_RESUMED){if(!devices.contains(d)){devices.insert(ei_device_ref(d));ei_device_start_emulating(d,1);}}
  if(type==EI_EVENT_KEYBOARD_MODIFIERS){
   const QList<uint> state{ei_event_keyboard_get_xkb_mods_depressed(event),ei_event_keyboard_get_xkb_mods_latched(event),ei_event_keyboard_get_xkb_mods_locked(event),ei_event_keyboard_get_xkb_group(event)};
   keyboardStates[d]=state;
   if(d==keymapDevice)emit keyboardModifiersChanged(state[0],state[1],state[2],state[3]);
  }
  if(type==EI_EVENT_DEVICE_PAUSED||type==EI_EVENT_DEVICE_REMOVED){
   if(keymapDevice==d){keymapDevice=nullptr;emit keyboardKeymapChanged({});}
   keyboardStates.remove(d);
   if(devices.remove(d))ei_device_unref(d);
   held.clear();heldButtons.clear();setReady(false);emit status("Desktop input device paused or removed");
  }
  bool disconnected=type==EI_EVENT_DISCONNECT;ei_event_unref(event);
  if(disconnected){emit status("Desktop input disconnected");close();return;}
 }
 updateKeyboardKeymap();
 bool v=device(EI_DEVICE_CAP_KEYBOARD)&&device(EI_DEVICE_CAP_POINTER)&&device(EI_DEVICE_CAP_BUTTON);
 if(v&&!available)emit status("Desktop connected — libei input ready"+permissionNotice);setReady(v);
}
ei_device *PortalInput::device(int cap){for(auto d:devices)if(ei_device_has_capability(d,static_cast<ei_device_capability>(cap)))return d;return nullptr;}
void PortalInput::updateKeyboardKeymap(){
 auto keyboard=device(EI_DEVICE_CAP_KEYBOARD);if(keyboard==keymapDevice)return;
 keymapDevice=keyboard;QByteArray source;
 auto map=keyboard?ei_device_keyboard_get_keymap(keyboard):nullptr;
 if(map&&ei_keymap_get_type(map)==EI_KEYMAP_TYPE_XKB){
  const auto size=ei_keymap_get_size(map);
  if(size>0&&size<=4*1024*1024){
   auto data=mmap(nullptr,size,PROT_READ,MAP_PRIVATE,ei_keymap_get_fd(map),0);
   if(data!=MAP_FAILED){source=QByteArray(static_cast<const char*>(data),qsizetype(size));munmap(data,size);}
  }
 }
 emit keyboardKeymapChanged(source);
 const auto state=keyboardStates.value(keyboard);
 if(state.size()==4)emit keyboardModifiersChanged(state[0],state[1],state[2],state[3]);
}
bool PortalInput::notify(const QString &method,const QVariantList &args){
 if(!available||eisMode)return false;
 QVariantList all{QVariant::fromValue(QDBusObjectPath(session)),QVariantMap{}};all+=args;
 auto reply=QDBusConnection::sessionBus().call(call(method,all),QDBus::Block,1000);
 if(reply.type()==QDBusMessage::ErrorMessage){emit status("Input failed: "+reply.errorMessage());close();return false;}return true;
}
bool PortalInput::key(int code,bool down){
 if(!available)return false;
 if(eisMode){auto d=device(EI_DEVICE_CAP_KEYBOARD);if(!d)return false;ei_device_keyboard_key(d,code,down);ei_device_frame(d,ei_now(context));}
 else if(!notify("NotifyKeyboardKeycode",{code,uint(down)}))return false;
 if(down)held.insert(code);else held.remove(code);return true;
}
bool PortalInput::move(double x,double y){
 if(!available)return false;if(!eisMode)return notify("NotifyPointerMotion",{x,y});
 auto d=device(EI_DEVICE_CAP_POINTER);if(!d)return false;ei_device_pointer_motion(d,x,y);ei_device_frame(d,ei_now(context));return true;
}
bool PortalInput::button(int code,bool down){
 if(!available)return false;
 if(!eisMode){if(!notify("NotifyPointerButton",{code,uint(down)}))return false;}
 else{auto d=device(EI_DEVICE_CAP_BUTTON);if(!d)return false;ei_device_button_button(d,code,down);ei_device_frame(d,ei_now(context));}
 if(down)heldButtons.insert(code);else heldButtons.remove(code);return true;
}
bool PortalInput::click(){return button(272,true)&&button(272,false);}
DisplayMapping::Mapping PortalInput::absoluteMapping(QList<ei_device*> &owners)const{
 if(!available||!eisMode||!qobject_cast<QGuiApplication*>(QCoreApplication::instance()))return {};
 QList<QRectF> screens,regions;int primary=-1;
 for(auto screen:QGuiApplication::screens()){
  if(screen==QGuiApplication::primaryScreen())primary=screens.size();
  screens<<QRectF(screen->geometry());
 }
 for(auto d:devices)if(ei_device_has_capability(d,EI_DEVICE_CAP_POINTER_ABSOLUTE)){
  for(size_t i=0;auto region=ei_device_get_region(d,i);++i){
   regions<<QRectF(ei_region_get_x(region),ei_region_get_y(region),ei_region_get_width(region),ei_region_get_height(region));
   owners<<d;
  }
 }
 return DisplayMapping::input(screens,primary,regions);
}
bool PortalInput::absoluteAvailable()const{
 QList<ei_device*> owners;return absoluteMapping(owners).valid();
}
bool PortalInput::moveAbsolute(double x,double y){
 QList<ei_device*> owners;const auto mapping=absoluteMapping(owners);
 if(!mapping.valid()||!std::isfinite(x)||!std::isfinite(y))return false;
 const auto point=mapping.point({qBound(0.0,x,1.0),qBound(0.0,y,1.0)});
 auto d=owners[mapping.region];
 ei_device_pointer_motion_absolute(d,point.x(),point.y());ei_device_frame(d,ei_now(context));return true;
}
bool PortalInput::scroll(double y){
 return scrollAxes(0,y);
}
bool PortalInput::scrollAxes(double x,double y){
 if(!available)return false;if(!eisMode)return notify("NotifyPointerAxis",{x,y});
 auto d=device(EI_DEVICE_CAP_SCROLL);if(!d)return false;ei_device_scroll_delta(d,x,y);ei_device_frame(d,ei_now(context));ei_device_scroll_stop(d,x!=0,y!=0);ei_device_frame(d,ei_now(context));return true;
}
void PortalInput::releaseButtons(){auto buttons=heldButtons;for(int code:buttons)button(code,false);heldButtons.clear();}
void PortalInput::releaseAll(){auto copy=held;for(int code:copy)key(code,false);held.clear();releaseButtons();}
void PortalInput::sessionClosed(){if(calledFromDBus()&&message().path()!=session)return;emit status("Desktop permission ended — reconnect to continue");close();}
void PortalInput::close(){
 ++generation;
 // Remove the session first to prevent reentrant error cleanup on D-Bus failures.
 auto oldSession=session;session.clear();
 if(context){auto d=device(EI_DEVICE_CAP_KEYBOARD);if(d){for(int code:held)ei_device_keyboard_key(d,code,false);ei_device_frame(d,ei_now(context));}}
 if(context){auto d=device(EI_DEVICE_CAP_BUTTON);if(d){for(int code:heldButtons)ei_device_button_button(d,code,false);ei_device_frame(d,ei_now(context));}}
 heldButtons.clear();held.clear();setReady(false);pending=false;
 if(keymapDevice){keymapDevice=nullptr;emit keyboardKeymapChanged({});}
 keyboardStates.clear();
 if(notifier){delete notifier;notifier=nullptr;}
 for(auto d:devices){ei_device_stop_emulating(d);ei_device_unref(d);}devices.clear();
 if(context){ei_unref(context);context=nullptr;}eisMode=false;
 auto bus=QDBusConnection::sessionBus();
 if(!requestPath.isEmpty()){
  bus.disconnect(service,requestPath,"org.freedesktop.portal.Request","Response",this,SLOT(response(uint,QVariantMap)));
  bus.asyncCall(QDBusMessage::createMethodCall(service,requestPath,"org.freedesktop.portal.Request","Close"));requestPath.clear();
 }
 continuation={};
 if(!oldSession.isEmpty()){
  bus.disconnect(service,oldSession,"org.freedesktop.portal.Session","Closed",this,SLOT(sessionClosed()));
  bus.asyncCall(QDBusMessage::createMethodCall(service,oldSession,"org.freedesktop.portal.Session","Close"));
 }
}
