#pragma once
#include <QObject>
#include <QVariantMap>
#include "WindowsInput.h"
#include "RelativePointer.h"
// Compatibility facade for Controller's desktop-output lifecycle.
class PortalInput:public QObject {
 Q_OBJECT
public:
 explicit PortalInput(QObject *parent=nullptr):QObject(parent){}
 ~PortalInput() override = default; // WindowsInput releases silently during destruction.
 static QVariantMap probe();
 void connectPortal(){available=true;emit status("Windows desktop input connected");emit readyChanged();}
 void setRememberPermission(bool){}
 void forgetPermission(){}
 void close(){relativePointer.reset();releaseAll();if(available){available=false;emit readyChanged();}}
 bool key(int code,bool down);
 bool text(const QString &text){return available&&input.text(text);}
 bool move(double dx,double dy){return available&&input.move(dx,dy);}
 bool click(){if(!button(272,true))return false;return button(272,false);}
 bool button(int code,bool down){return available&&input.button(code,down);}
 bool moveAbsolute(double x,double y);
 bool moveRelativeTo(double x,double y);
 bool relativeTargetAvailable()const{return available;}
 bool absoluteAvailable()const{return available;}
 bool scroll(double dy){return scrollAxes(0,dy);}
 bool scrollAxes(double dx,double dy){return available&&input.scrollAxes(dx/120.0,dy/120.0);}
 bool ready()const{return available;}
 void releaseAll(){if(!input.releaseAll())emit status("Windows rejected an input release; retry desktop connection");}
 void releaseButtons(){if(!input.releaseButtons())emit status("Windows rejected a pointer release");}
signals:
 void status(QString message);
 void readyChanged();
private:
 WindowsInput input;
 RelativePointer relativePointer;
 bool available=false;
};
