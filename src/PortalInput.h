#ifdef GAZE_GRID_WINDOWS
#include "WindowsDesktopInput.h"
#else
#pragma once
#include <QObject>
#include <QVariantMap>
#include <QDBusMessage>
#include <QDBusContext>
#include <QSet>
#include <QHash>
#include "DisplayMapping.h"
#include <functional>
struct ei;struct ei_device;class QSocketNotifier;
class PortalInput:public QObject,protected QDBusContext {
 Q_OBJECT
public:
 explicit PortalInput(QObject *parent=nullptr):QObject(parent){}
 ~PortalInput() override;
 static QVariantMap probe();
 void connectPortal();
 void setRememberPermission(bool enabled){rememberPermission=enabled;}
 void forgetPermission();
 void close();
 bool key(int code,bool down);
 bool move(double dx,double dy);
 bool click();
 bool button(int code,bool down);
 bool moveAbsolute(double x,double y);
 // RemoteDesktop/libei do not expose the global cursor position needed for feedback.
 bool relativeTargetAvailable()const{return false;}
 bool moveRelativeTo(double,double){return false;}
 bool absoluteAvailable() const;
 bool scroll(double dy);
 bool scrollAxes(double dx,double dy);
 bool ready()const{return available;}
 void releaseAll();
 void releaseButtons();
signals:
 void status(QString message);
 void readyChanged();
 void keyboardKeymapChanged(QByteArray keymap);
 void keyboardModifiersChanged(uint depressed,uint latched,uint locked,uint group);
private slots:
 void response(uint code,const QVariantMap &results);
 void sessionClosed();
private:
 void request(const QString &method,const QVariantList &args,QVariantMap options,std::function<void(QVariantMap)> next);
 void connectEis();
 void selectDevices();
 void beginSelection(QVariantMap options);
 void dispatch();
 bool notify(const QString &method,const QVariantList &args);
 void setReady(bool value);
 QString session,requestPath; bool pending=false,available=false,eisMode=false;
 bool rememberPermission=false,persistenceRequested=false;
 QString permissionNotice;
 quint64 generation=0;
 std::function<void(QVariantMap)> continuation;
 ei *context=nullptr; QSocketNotifier *notifier=nullptr;
 QSet<ei_device*> devices;
 QSet<int> held,heldButtons;
 ei_device *device(int capability);
 void updateKeyboardKeymap();
 ei_device *keymapDevice=nullptr;
 QHash<ei_device*,QList<uint>> keyboardStates;
 DisplayMapping::Mapping absoluteMapping(QList<ei_device*> &owners)const;
};

#endif
