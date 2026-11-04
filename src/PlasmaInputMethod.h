#pragma once
#include <QObject>
#include <memory>
#include "DesktopTextContext.h"
class QWindow;
// KWin launches this backend on its privileged input-method Wayland connection.
class PlasmaInputMethod:public QObject {
 Q_OBJECT
public:
 explicit PlasmaInputMethod(QObject *parent=nullptr);
 ~PlasmaInputMethod() override;
 bool initialize();
 bool attachPanel(QWindow *window, bool top = false);
 bool commitText(const QString &text);
 bool sendKey(const QString &name,bool down);
 bool ready()const;
 bool sensitive()const;
 DesktopTextContext textContext()const;
 bool replaceText(const DesktopTextContext &expected,const QString &candidate,const QString &language,bool space);
signals:
 void stateChanged();
 void contextInvalidated();
 void surroundingTextChanged(const DesktopTextContext &context);
private:
 struct Impl;
 std::unique_ptr<Impl> d;
};
