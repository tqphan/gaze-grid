#pragma once
#include "DesktopTextContext.h"
#include "WindowsDesktopSession.h"
#include <QThread>
#include <atomic>
class WindowsTextContext:public QThread {
 Q_OBJECT
public:
 explicit WindowsTextContext(QObject *parent=nullptr):QThread(parent){qRegisterMetaType<DesktopTextContext>();}
 ~WindowsTextContext()override{requestInterruption();wait();}
 void setEnabled(bool value){enabled=value;}
 static DesktopTextContext readControl(HWND control);
 static DesktopTextContext current();
 static bool replaceControl(HWND control,const DesktopTextContext &expected,const QString &candidate,const QString &language,bool space);
 static bool replaceCurrent(const DesktopTextContext &expected,const QString &candidate,const QString &language,bool space);
signals:
 void contextChanged(const DesktopTextContext &context);
protected:
 void run()override;
private:
 std::atomic<bool> enabled{true};
};
