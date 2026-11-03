#include "PlasmaInputMethod.h"
#include "KeyNames.h"
#include "PredictionLanguage.h"
#include "input-method-client.h"
#include <QDateTime>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusVariant>
#include <QTimer>
#include <QWindow>
#include <QPointer>
#include <LayerShellQt/Window>
#include <QtWaylandClient/QWaylandClientExtension>
#include <QtWaylandClient/private/qwaylandwindow_p.h>
#include <QtWaylandClient/private/qwaylandscreen_p.h>
#include <QtWaylandClient/private/qwaylandshellintegration_p.h>
#include <QtWaylandClient/private/qwaylandshellsurface_p.h>
#include <xkbcommon/xkbcommon.h>

namespace {
class Extension:public QWaylandClientExtension {
public:
 Extension(const wl_interface *type):QWaylandClientExtension(1),type(type){}
 ~Extension()override{if(proxy)wl_proxy_destroy(proxy);}
 const wl_interface *extensionInterface()const override{return type;}
 void bind(wl_registry *registry,int id,int)override{proxy=static_cast<wl_proxy*>(wl_registry_bind(registry,id,type,1));if(bound)bound();}
 bool start(){initialize();return isActive()&&proxy;}
 const wl_interface *type;wl_proxy *proxy=nullptr;std::function<void()> bound;
};
class PanelSurface:public QtWaylandClient::QWaylandShellSurface {
public:
 PanelSurface(QtWaylandClient::QWaylandWindow *window,zwp_input_panel_v1 *panel):QWaylandShellSurface(window){
  surface=zwp_input_panel_v1_get_input_panel_surface(panel,window->wlSurface());window->applyConfigureWhenPossible();
 }
 ~PanelSurface()override{zwp_input_panel_surface_v1_destroy(surface);}
 void applyConfigure()override{
  if(auto screen=window()->waylandScreen())zwp_input_panel_surface_v1_set_toplevel(surface,screen->output(),ZWP_INPUT_PANEL_SURFACE_V1_POSITION_CENTER_BOTTOM);
 }
 zwp_input_panel_surface_v1 *surface;
};
class PanelIntegration:public QtWaylandClient::QWaylandShellIntegration {
public:
 Extension panel{&zwp_input_panel_v1_interface};
 bool initialize(QtWaylandClient::QWaylandDisplay*)override{return panel.start();}
 QtWaylandClient::QWaylandShellSurface *createShellSurface(QtWaylandClient::QWaylandWindow *window)override{
  return panel.isActive()?new PanelSurface(window,reinterpret_cast<zwp_input_panel_v1*>(panel.proxy)):nullptr;
 }
};
}
struct PlasmaInputMethod::Impl {
 PlasmaInputMethod *owner;
 Extension method{&zwp_input_method_v1_interface};PanelIntegration panel;
 zwp_input_method_context_v1 *context=nullptr;uint32_t serial=0;bool committed=false,privateField=true;
 DesktopTextContext pendingText,lastText;quint64 generation=0;
 QTimer activationRecovery;
 QPointer<QWindow> panelWindow;
 QPointer<LayerShellQt::Window> topLayer;
 bool topPlacement=false;
 xkb_context *xkb=xkb_context_new(XKB_CONTEXT_NO_FLAGS);xkb_keymap *keymap=nullptr;xkb_state *state=nullptr;
 explicit Impl(PlasmaInputMethod *owner):owner(owner){
  activationRecovery.setSingleShot(true);
  activationRecovery.setInterval(250);
  QObject::connect(&activationRecovery,&QTimer::timeout,owner,[this]{
   if(!context||committed)return;
   // KWin 6.8 can activate a late-binding input method without replaying
   // the focused field's state. Re-adopt its current context once; never
   // infer readiness or a field's privacy from this D-Bus request.
   auto request=QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"),
    QStringLiteral("/VirtualKeyboard"),QStringLiteral("org.freedesktop.DBus.Properties"),QStringLiteral("Set"));
   request.setArguments({QStringLiteral("org.kde.kwin.VirtualKeyboard"),QStringLiteral("active"),QVariant::fromValue(QDBusVariant(true))});
   QDBusConnection::sessionBus().asyncCall(request);
  });
  xkb_rule_names names{};names.layout="us";
  if(xkb)keymap=xkb_keymap_new_from_names(xkb,&names,XKB_KEYMAP_COMPILE_NO_FLAGS);
  if(keymap)state=xkb_state_new(keymap);
  method.bound=[this]{static const zwp_input_method_v1_listener listener{activate,deactivate};zwp_input_method_v1_add_listener(reinterpret_cast<zwp_input_method_v1*>(method.proxy),&listener,this);};
 }
 ~Impl(){clear();if(state)xkb_state_unref(state);if(keymap)xkb_keymap_unref(keymap);if(xkb)xkb_context_unref(xkb);}
 void clear(){activationRecovery.stop();if(context)zwp_input_method_context_v1_destroy(context);context=nullptr;committed=false;privateField=true;serial=0;pendingText={};lastText={};}
 static void activate(void *data,zwp_input_method_v1*,zwp_input_method_context_v1 *context){
  auto self=static_cast<Impl*>(data);self->clear();++self->generation;emit self->owner->contextInvalidated();self->context=context;
  static const zwp_input_method_context_v1_listener listener{surrounding,reset,content,invoke,commit,language};
  zwp_input_method_context_v1_add_listener(context,&listener,self);self->activationRecovery.start();emit self->owner->stateChanged();
 }
 static void deactivate(void *data,zwp_input_method_v1*,zwp_input_method_context_v1 *context){
  auto self=static_cast<Impl*>(data);if(context!=self->context)return;self->clear();emit self->owner->contextInvalidated();emit self->owner->stateChanged();
 }
 static void surrounding(void *data,zwp_input_method_context_v1 *context,const char *text,uint32_t cursor,uint32_t anchor){
  auto self=static_cast<Impl*>(data);if(context!=self->context)return;
  self->pendingText=DesktopTextContext::fromUtf8(QByteArray(text),int(cursor),int(anchor));
 }
 static void reset(void *data,zwp_input_method_context_v1*){auto self=static_cast<Impl*>(data);self->committed=false;self->pendingText={};self->lastText={};emit self->owner->contextInvalidated();emit self->owner->stateChanged();}
 static void content(void *data,zwp_input_method_context_v1*,uint32_t hints,uint32_t purpose){
  auto self=static_cast<Impl*>(data);const bool sensitive=(hints&0xc0)||purpose==8;
  if(sensitive)self->pendingText={};
  if(self->privateField!=sensitive){self->privateField=sensitive;emit self->owner->contextInvalidated();emit self->owner->stateChanged();}
 }
 static void invoke(void*,zwp_input_method_context_v1*,uint32_t,uint32_t){}
 static void commit(void *data,zwp_input_method_context_v1*,uint32_t serial){
  auto self=static_cast<Impl*>(data);self->serial=serial;const bool wasReady=self->committed;self->committed=true;if(!wasReady)emit self->owner->stateChanged();
  self->lastText=self->privateField?DesktopTextContext{}:self->pendingText;
  self->lastText.supported=true;self->lastText.privateField=self->privateField;
  self->lastText.identity=QString("plasma:%1").arg(self->generation);
  emit self->owner->surroundingTextChanged(self->lastText);
 }
 static void language(void*,zwp_input_method_context_v1*,const char*){}
};
PlasmaInputMethod::PlasmaInputMethod(QObject *parent):QObject(parent),d(std::make_unique<Impl>(this)){
 connect(&d->method,&QWaylandClientExtension::activeChanged,this,[this]{if(!d->method.isActive()){d->clear();emit contextInvalidated();emit stateChanged();}});
}
PlasmaInputMethod::~PlasmaInputMethod()=default;
bool PlasmaInputMethod::initialize(){return d->method.start()&&d->panel.initialize(nullptr);}
bool PlasmaInputMethod::attachPanel(QWindow *window,bool top){
 if(d->panelWindow==window&&d->topPlacement==top)return true;
 const bool visible=window->isVisible();
 // Surface roles cannot change in place. Keep the QML window and input
 // context, but recreate the native surface when changing placement.
 if(d->panelWindow==window){window->hide();window->destroy();}
 if(d->topLayer)delete d->topLayer;
 window->create();auto wayland=dynamic_cast<QtWaylandClient::QWaylandWindow*>(window->handle());
 if(!wayland||!d->panel.panel.isActive())return false;
 if(top){
  d->topLayer=LayerShellQt::Window::get(window);
  d->topLayer->setScope("gaze-grid-keyboard");
  d->topLayer->setLayer(LayerShellQt::Window::LayerOverlay);
  d->topLayer->setAnchors(LayerShellQt::Window::AnchorTop);
  d->topLayer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
  d->topLayer->setActivateOnShow(false);
  d->topLayer->setExclusiveZone(-1);
  d->topLayer->setDesiredSize(window->size());
 }else wayland->setShellIntegration(&d->panel);
 d->panelWindow=window;d->topPlacement=top;
 if(visible)window->show();
 return true;
}
bool PlasmaInputMethod::ready()const{return d->context&&d->committed&&d->method.isActive();}
bool PlasmaInputMethod::sensitive()const{return d->privateField;}
DesktopTextContext PlasmaInputMethod::textContext()const{return d->lastText;}
bool PlasmaInputMethod::replaceText(const DesktopTextContext &expected,const QString &candidate,const QString &language,bool space){
 if(!ready()||sensitive()||!expected.available||d->lastText!=expected)return false;
 const auto edit=PredictionLanguage::edit(expected.before,expected.after,candidate,language,space);
 if(!edit.valid||edit.removeAfter>=4096)return false;
 const auto before=expected.before.right(edit.removeBefore).toUtf8();
 const auto after=expected.after.left(edit.removeAfter).toUtf8();
 if(!before.isEmpty()||!after.isEmpty())zwp_input_method_context_v1_delete_surrounding_text(d->context,-int(before.size()),uint32_t(before.size()+after.size()));
 const bool result=commitText(edit.text);
 if(result)d->lastText.available=false;
 return result;
}
bool PlasmaInputMethod::commitText(const QString &text){
 if(!ready()||text.contains(QChar(0)))return false;
 zwp_input_method_context_v1_commit_string(d->context,d->serial,text.toUtf8().constData());return true;
}
bool PlasmaInputMethod::sendKey(const QString &name,bool down){
 // This path is for momentary nontext keys. Held physical actions use PortalInput.
 const auto code=KeyNames::linuxCode(name);if(!ready()||!d->state||code<0)return false;
 const auto symbol=xkb_state_key_get_one_sym(d->state,code+8);if(symbol==XKB_KEY_NoSymbol)return false;
 zwp_input_method_context_v1_keysym(d->context,d->serial,uint32_t(QDateTime::currentMSecsSinceEpoch()),symbol,down?WL_KEYBOARD_KEY_STATE_PRESSED:WL_KEYBOARD_KEY_STATE_RELEASED,0);return true;
}
