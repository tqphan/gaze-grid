#include <QApplication>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QScreen>
#include <QJsonDocument>
#include <QSaveFile>
#include <QTextStream>
#include <QTimer>
#include <QProcess>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QEnterEvent>
#include <QAbstractNativeEventFilter>
#include <QWheelEvent>
#include <memory>
#include <atomic>
#include <QJsonObject>
#include <QDir>
#include <QFileInfo>
#include <QPainter>
#ifdef GAZE_GRID_LINUX
#include <QDBusConnection>
#include <QDBusMessage>
#endif
#include "Controller.h"
#include "AppPaths.h"
#include "KeyboardPack.h"
#include "KeyboardEditor.h"
#include "KeyboardLayout.h"
#include "SymbolProvider.h"
#include "PointerCheck.h"
#ifdef _WIN32
#include "WindowsTheme.h"
#include "WindowsSignIn.h"
#include "WindowsDesktopSession.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>
#include <shobjidl.h>
#endif
#ifdef GAZE_GRID_LINUX
#include <LayerShellQt/Window>
#include "PlasmaInputMethod.h"
#endif
#ifdef _WIN32
// Both surfaces are topmost. Restore their relative order after the keyboard
// is shown or restacked, without activating either window or remapping capture.
class TargetStacking:public QObject,public QAbstractNativeEventFilter {
public:
 TargetStacking(QWindow *keyboard,QWindow *target):QObject(target),keyboard(keyboard),target(target){
  QCoreApplication::instance()->installNativeEventFilter(this);
  connect(target,&QWindow::visibleChanged,this,[this](bool visible){if(visible)scheduleRaise();});
 }
 ~TargetStacking()override{QCoreApplication::instance()->removeNativeEventFilter(this);}
 bool nativeEventFilter(const QByteArray &,void *message,qintptr *)override{
  const auto msg=static_cast<MSG*>(message);
  if(msg->message==WM_WINDOWPOSCHANGED&&msg->hwnd==reinterpret_cast<HWND>(keyboard->winId()))
   scheduleRaise();
  return false;
 }
private:
 void scheduleRaise(){
  if(pending||!target->isVisible())return;
  pending=true;
  QTimer::singleShot(0,this,[this]{
   pending=false;
   if(target->isVisible())SetWindowPos(reinterpret_cast<HWND>(target->winId()),HWND_TOPMOST,
                                    0,0,0,0,SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE);
  });
 }
 QWindow *keyboard,*target;
 bool pending=false;
};
#endif
class MagnifierProvider:public QQuickImageProvider {
public:
 explicit MagnifierProvider(Controller *controller):QQuickImageProvider(QQuickImageProvider::Image),controller(controller){}
 QImage requestImage(const QString &,QSize *size,const QSize &)override{
  auto result=controller->magnifierImage();if(size)*size=result.size();return result;
 }
private:
 Controller *controller;
};
class InputObserver:public QObject {
public:
 bool pointerEvents=false;
 bool eventFilter(QObject *object,QEvent *event)override{
  if(event->type()==QEvent::KeyPress){auto key=static_cast<QKeyEvent*>(event);QTextStream(stdout)<<QJsonDocument(QJsonObject{{"text",key->text()},{"key",key->key()}}).toJson(QJsonDocument::Compact)<<Qt::endl;}
  auto window=qobject_cast<QWindow*>(object);
  if(pointerEvents&&window&&window->width()>0&&window->height()>0){
   QJsonObject data;
   if(event->type()==QEvent::MouseButtonPress||event->type()==QEvent::MouseButtonDblClick||event->type()==QEvent::MouseButtonRelease||event->type()==QEvent::MouseMove){
    auto mouse=static_cast<QMouseEvent*>(event);
    data={{"pointer",event->type()==QEvent::MouseMove?"move":event->type()==QEvent::MouseButtonRelease?"release":"press"},{"double",event->type()==QEvent::MouseButtonDblClick},{"time",qint64(mouse->timestamp())},{"button",int(mouse->button())},{"buttons",int(mouse->buttons())},{"x",mouse->position().x()/window->width()},{"y",mouse->position().y()/window->height()}};
   }else if(event->type()==QEvent::Wheel){
    auto wheel=static_cast<QWheelEvent*>(event);const auto delta=wheel->pixelDelta().isNull()?wheel->angleDelta():wheel->pixelDelta();
    data={{"pointer","wheel"},{"dx",delta.x()},{"dy",delta.y()}};
   }
   if(!data.isEmpty())QTextStream(stdout)<<QJsonDocument(data).toJson(QJsonDocument::Compact)<<Qt::endl;
  }
  return false;
 }
};
// Inspect rendered scene alpha without the platform backing store's RGB conversion.
// Native window composition is checked separately during desktop acceptance.
static QImage grabSceneForCheck(QQuickWindow *window){
 auto result=window->contentItem()->grabToImage();
 if(!result)return {};
 QEventLoop loop;
 QObject::connect(result.data(),&QQuickItemGrabResult::ready,&loop,&QEventLoop::quit);
 QTimer::singleShot(2000,&loop,&QEventLoop::quit);
 loop.exec();return result->image();
}
int main(int argc,char **argv){
 bool signInMode=false;
#ifdef _WIN32
 const bool secureDesktop=WindowsSignIn::secureDesktop();
 if(localTestBuild())SetCurrentProcessExplicitAppUserModelID(L"org.gaze_grid.gaze_grid.LocalTest");
 if(localTestBuild()&&secureDesktop)return 2;
 bool signInCheck=false;
 for(int i=1;i<argc;++i){const auto argument=QString::fromLocal8Bit(argv[i]);signInMode|=argument=="--sign-in";signInCheck|=argument=="--sign-in-ui-check";}
 if(secureDesktop){
  // Only the keyboard may run on Winlogon; editor/diagnostic modes stay on the user's desktop.
  for(int i=1;i<argc;++i){
   const auto argument=QString::fromLocal8Bit(argv[i]);
   if(argument!="--sign-in"&&argument!="--accessibility")return 2;
  }
  signInMode=true;
  wchar_t executable[32768]{};if(!GetModuleFileNameW(nullptr,executable,32768))return 2;
  const auto directory=QFileInfo(QString::fromWCharArray(executable)).absolutePath();
  QCoreApplication::setLibraryPaths({directory});
  for(const auto *name:{"QT_PLUGIN_PATH","QT_QPA_PLATFORM_PLUGIN_PATH","QML_IMPORT_PATH","QML2_IMPORT_PATH","QT_QUICK_CONTROLS_CONF","GAZE_GRID_CONFIG_DIR"})qunsetenv(name);
  qputenv("QT_QPA_PLATFORM","windows");qputenv("QT_QUICK_BACKEND","software");
 }
 // A preview check exercises the same controller and QML without sending input.
 if(signInMode&&!secureDesktop)return 2;
 signInMode|=signInCheck;
 for(int i=1;i<argc;++i)if(QString::fromLocal8Bit(argv[i])=="--configure-sign-in"){
  if(localTestBuild())return 2;
  // The elevated settings helper needs no GUI or user-supplied Qt plugins.
  QCoreApplication helper(argc,argv);return WindowsSignIn::setup(helper.arguments());
 }
 // A broker-started duplicate must exit before Qt UI, tracking, or either
 // accessibility notification. Each desktop has its own independent owner.
 QStringList startupArguments;
 for(int i=0;i<argc;++i)startupArguments<<QString::fromLocal8Bit(argv[i]);
 std::unique_ptr<WindowsDesktop::Instance> keyboardInstance;
 if(WindowsDesktop::keyboardInvocation(startupArguments)){
  keyboardInstance=std::make_unique<WindowsDesktop::Instance>(localTestBuild()?L"gaze-grid.LocalTest":L"gaze-grid.Keyboard");
  if(keyboardInstance->duplicate())return 0;
  if(!keyboardInstance->owner()){
   fprintf(stderr,"Cannot acquire the keyboard desktop instance (Windows error %lu)\n",static_cast<unsigned long>(keyboardInstance->error()));return 2;
  }
 }
#endif
#ifdef _WIN32
 // Fusion consistently honors the dark Windows palette across Quick controls.
 qputenv("QT_QUICK_CONTROLS_STYLE","Fusion");
#endif
 bool inputMethodMode=false;for(int i=1;i<argc;++i)inputMethodMode|=QString::fromLocal8Bit(argv[i])=="--input-method";
 const bool localIme=!signInMode&&!inputMethodMode;
 if(localIme){qputenv("QT_IM_MODULE","qtvirtualkeyboard");qputenv("QT_VIRTUALKEYBOARD_DESKTOP_DISABLE","1");}
 if(inputMethodMode&&qEnvironmentVariableIsEmpty("WAYLAND_SOCKET")){QTextStream(stderr)<<"Input-method mode must be launched by KWin through Virtual Keyboard settings or a nested test session.\n";return 2;}
 bool diagnose=false,audit=false,smoke=false,portalCheck=false,inputCheck=false,inputTarget=false,pointerCheck=false,pointerTarget=false;
 for(int i=1;i<argc;++i){QString arg=argv[i];diagnose|=arg=="--diagnose";audit|=arg=="--validate-keyboards";smoke|=arg=="--smoke";portalCheck|=arg=="--portal-check";inputCheck|=arg=="--input-check";inputTarget|=arg=="--input-target";pointerCheck|=arg=="--pointer-check";pointerTarget|=arg=="--pointer-target";}
 if(diagnose||audit)qputenv("QT_QPA_PLATFORM","offscreen");
 QQuickWindow::setDefaultAlphaBuffer(true);
 QApplication app(argc,argv);app.setOrganizationName("gaze-grid");app.setApplicationName("gaze-grid");app.setApplicationDisplayName("gaze-grid");app.setDesktopFileName("org.gaze_grid.gaze_grid");
 if(localTestBuild()){app.setApplicationName("gaze-grid-local-test");app.setApplicationDisplayName("gaze-grid Local Test");}
#ifdef _WIN32
 app.setStyle("Fusion"); // QWidget menus/editor must honor the same system palette as QML.
 WindowsTheme windowsTheme;
#endif
 app.setWindowIcon(QIcon(localTestBuild()?":/resources/gaze-grid-local-test.svg":":/resources/gaze-grid.svg"));
 if(app.arguments().contains("--editor")){
  KeyboardEditor editor;
  int index=app.arguments().indexOf("--editor");
  if(index+1<app.arguments().size()&&!app.arguments()[index+1].startsWith("--"))editor.openFile(app.arguments()[index+1]);
  editor.show();return app.exec();
 }
 const bool magnifierCheck=app.arguments().contains("--magnifier-check");pointerCheck|=magnifierCheck;
 if(app.arguments().contains("--input-method-check")&&qgetenv("GAZE_GRID_ISOLATED_IM_TEST")!="1"){
  QTextStream(stderr)<<"Run tests/native_input_method.py to isolate the native input check\n";return 2;
 }
 if(app.arguments().contains("--input-method-target")){
  QTextStream(stderr)<<"Native receiver platform: "<<QGuiApplication::platformName()<<Qt::endl;
  QQmlApplicationEngine targetEngine;targetEngine.load(QUrl("qrc:/ui/InputMethodTarget.qml"));if(targetEngine.rootObjects().isEmpty())return 1;
  auto target=targetEngine.rootObjects().front();QTimer poll;
  QObject::connect(&poll,&QTimer::timeout,&app,[&]{
   if(target->property("focusLost").toBool()){QTextStream(stderr)<<"Native receiver lost focus\n";app.exit(2);return;}
   if(target->property("privateComplete").toBool()&&target->property("receivedText").toString()==QString::fromUtf8("gaze-grid é€😀\n")){
   QTextStream(stdout)<<"PASS: native Unicode commit, Backspace, Return and private field reached the Qt receiver"<<Qt::endl;app.quit();
  }});poll.start(100);QTimer::singleShot(20000,&app,[&]{QTextStream(stderr)<<"Native input-method receiver timed out\n";app.exit(2);});return app.exec();
 }
 if(inputTarget||pointerTarget){
  QQmlApplicationEngine targetEngine;targetEngine.load(QUrl(pointerTarget?"qrc:/ui/PointerTarget.qml":"qrc:/ui/InputTarget.qml"));
  if(targetEngine.rootObjects().isEmpty())return 1;
  auto target=qobject_cast<QQuickWindow*>(targetEngine.rootObjects().front());InputObserver observer;
  observer.pointerEvents=pointerTarget;
  target->installEventFilter(&observer);
  auto reportTarget=[target]{QTextStream(stdout)<<QJsonDocument(QJsonObject{{"active",target->isActive()},{"width",target->width()},{"height",target->height()}}).toJson(QJsonDocument::Compact)<<Qt::endl;};
  QObject::connect(target,&QWindow::activeChanged,&app,reportTarget);
  QObject::connect(target,&QWindow::widthChanged,&app,reportTarget);QObject::connect(target,&QWindow::heightChanged,&app,reportTarget);
  if(pointerTarget){auto screen=QGuiApplication::primaryScreen();target->setScreen(screen);if(screen)target->setGeometry(screen->geometry());target->showFullScreen();}else target->show();target->requestActivate();QTimer::singleShot(pointerTarget?120000:60000,&app,&QCoreApplication::quit);return app.exec();
 }
 if(diagnose||audit){
  QVariantMap report;
  if(diagnose){report["tobii"]=TobiiInput::probe();report["portal"]=PortalInput::probe();report["qtVersion"]=qVersion();}
  if(audit)report["keyboards"]=KeyboardPack::audit(keyboardPackDirectory());
  const auto json=QJsonDocument::fromVariant(report).toJson();
  // ShellExecute is required for UIAccess launches and cannot redirect stdout.
  const auto arguments=app.arguments();const int outputIndex=arguments.indexOf("--diagnostic-output");
  if(outputIndex>=0){
   if(outputIndex+1>=arguments.size()||arguments[outputIndex+1].isEmpty())return 2;
   QSaveFile output(arguments[outputIndex+1]);
   if(!output.open(QIODevice::WriteOnly)||output.write(json)!=json.size()||!output.commit())return 2;
  }else QTextStream(stdout)<<json;
  return audit&&!report["keyboards"].toMap()["errors"].toStringList().isEmpty()?1:0;
 }
 if(portalCheck){
  PortalInput portal;
  QObject::connect(&portal,&PortalInput::status,&app,[](QString message){QTextStream(stdout)<<message<<Qt::endl;});
  QObject::connect(&portal,&PortalInput::readyChanged,&app,[&]{if(portal.ready())QTimer::singleShot(200,&app,&QCoreApplication::quit);});
  QTimer::singleShot(0,&portal,&PortalInput::connectPortal);
  QTimer::singleShot(60000,&app,[&]{QTextStream(stdout)<<"Portal check timed out"<<Qt::endl;app.exit(2);});
  return app.exec();
 }
#ifdef GAZE_GRID_LINUX
 std::unique_ptr<PlasmaInputMethod> inputMethod;
 if(inputMethodMode){
  inputMethod=std::make_unique<PlasmaInputMethod>();
  if(!inputMethod->initialize()){QTextStream(stderr)<<"KWin input-method interfaces unavailable\n";return 2;}
  if(app.arguments().contains("--input-method-check"))QTextStream(stderr)<<"Native input-method interfaces bound"<<Qt::endl;
 }
 // Register before any portal request so remembered screenshot approval belongs
 // to gaze-grid rather than an unidentified host application. Older portals may
 // not implement Registry; their normal permission dialogs remain available.
 bool registerIdentity=!smoke;
 for(const auto &arg:app.arguments())if(arg.endsWith("-check"))registerIdentity=false;
 if(registerIdentity){
  auto registration=QDBusMessage::createMethodCall("org.freedesktop.portal.Desktop","/org/freedesktop/portal/desktop","org.freedesktop.host.portal.Registry","Register");
  registration.setArguments({QString("org.gaze_grid.gaze_grid"),QVariantMap{}});
  QDBusConnection::sessionBus().call(registration,QDBus::Block,2000);
 }
#endif
#ifdef _WIN32
 Controller controller(signInMode,signInMode?WindowsSignIn::profile():QJsonObject{});
 std::unique_ptr<WindowsDesktop::Monitor> desktopMonitor;
 if(keyboardInstance){
  desktopMonitor=std::make_unique<WindowsDesktop::Monitor>();
  QObject::connect(desktopMonitor.get(),&WindowsDesktop::Monitor::activeChanged,&controller,[&](bool active){
   controller.setDesktopActive(active);
   // The user desktop persists. A secure copy exits when its desktop loses
   // input, releasing the tracker before the user instance resumes.
   if(secureDesktop&&!active)app.quit();
  });
  desktopMonitor->start();
  if(secureDesktop&&!desktopMonitor->active())return 0;
 }
#else
 Controller controller;
#endif
 if(localTestBuild()&&app.arguments().contains("--local-isolation-check")){
  controller.connectDesktop();controller.connectTobii();
  controller.configureWindowsStartup(true);controller.configureWindowsStartup(false);
  controller.syncSignInPreferences();controller.configureSignIn(true);controller.configureSignIn(false);
  const bool isolated=!controller.ready()&&!controller.isTrackerConnected()
      &&appConfigDirectory()==QCoreApplication::applicationDirPath()+"/local-check-profile";
  QTextStream(stdout)<<(isolated?"PASS: local test isolation":"FAIL: local test isolation")<<Qt::endl;
  return isolated?0:2;
 }
#ifdef GAZE_GRID_LINUX
 if(inputMethod){
  controller.configureInputMethod([&](QString text){return inputMethod->commitText(text);},[&](QString name,bool down){return inputMethod->sendKey(name,down);});
  QObject::connect(inputMethod.get(),&PlasmaInputMethod::contextInvalidated,&controller,&Controller::invalidateInputContext);
  QObject::connect(inputMethod.get(),&PlasmaInputMethod::stateChanged,&controller,[&]{controller.inputContextChanged(inputMethod->ready(),inputMethod->sensitive());});
 }
 int inputMethodCheckStage=0;
 QQuickWindow *nativeCheckWindow=nullptr;
 if(inputMethod&&app.arguments().contains("--input-method-check")){
  QObject::connect(inputMethod.get(),&PlasmaInputMethod::stateChanged,&controller,[&]{
   QTextStream(stderr)<<"Native test context: ready="<<inputMethod->ready()<<" private="<<inputMethod->sensitive()<<Qt::endl;
   if(!inputMethod->ready())return;
   const bool sensitive=inputMethod->sensitive();
   if((inputMethodCheckStage==0&&!sensitive)||(inputMethodCheckStage==1&&sensitive)){
    ++inputMethodCheckStage;
    QTimer::singleShot(100,&controller,[&,sensitive]{
     if(!nativeCheckWindow||!inputMethod->ready()||inputMethod->sensitive()!=sensitive){app.exit(2);return;}
     controller.openLayout(sensitive?"main-14x1-top":"main-14x1-bottom",false);
     auto timer=new QTimer(&controller);
     timer->setInterval(100);
     // Odd count ends on the opposite edge. Each phase exercises five
     // round trips plus one move, through the rendered QML button.
     QObject::connect(timer,&QTimer::timeout,&controller,[&,sensitive,timer,clicks=0,expected=QString()]()mutable{
      if(!inputMethod->ready()||inputMethod->sensitive()!=sensitive){timer->stop();timer->deleteLater();app.exit(2);return;}
      if(!expected.isEmpty()&&controller.property("layoutName").toString()!=expected){
       QTextStream(stderr)<<"Placement button did not navigate to "<<expected<<Qt::endl;
       timer->stop();timer->deleteLater();app.exit(2);return;
      }
      if(clicks==11){
       timer->stop();timer->deleteLater();
       controller.openLayout(sensitive?"keyboard-alphabet-14x4-bottom":"keyboard-alphabet-14x4-top",false);
       QTextStream(stderr)<<"Native placement check: "<<(sensitive?"bottom":"top")<<" (11 UI clicks)"<<Qt::endl;
       QVariantList commands;
       if(sensitive)commands={QVariantMap{{"tag","Text"},{"text","secret42"}}};
       else commands={QVariantMap{{"tag","Text"},{"text",QString::fromUtf8("gaze-grid é€😀X")}},QVariantMap{{"tag","Action"},{"text","Backspace"}},QVariantMap{{"tag","Text"},{"text","\n"}}};
       if(!controller.executeCommands(commands)){QTextStream(stderr)<<"Native check could not start\n";app.exit(2);}
       return;
      }
      const auto layout=controller.property("layout").toMap();
      const auto key=layout["keys"].toList().front().toMap();
      QPointF point((key["col"].toDouble()+key["width"].toDouble()/2)*nativeCheckWindow->width()/layout["cols"].toDouble(),
                    (key["row"].toDouble()+key["height"].toDouble()/2)*nativeCheckWindow->height()/layout["rows"].toDouble());
      expected=layout["position"]=="Top"?"main-14x1-bottom":"main-14x1-top";
      QMouseEvent press(QEvent::MouseButtonPress,point,point,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
      QMouseEvent release(QEvent::MouseButtonRelease,point,point,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
      QCoreApplication::sendEvent(nativeCheckWindow,&press);
      QCoreApplication::sendEvent(nativeCheckWindow,&release);
      ++clicks;
     });
     timer->start();
    });
   }
  });
 }
#endif
 if(inputCheck||pointerCheck){controller.setKeyboardScale(100);controller.openLayout("keyboard-alphabet-14x4-bottom",false);}
 auto args=app.arguments();int layoutIndex=args.indexOf("--layout");if(layoutIndex>=0&&layoutIndex+1<args.size())controller.openLayout(args[layoutIndex+1],false);
 QQmlApplicationEngine engine;
#ifdef _WIN32
 if(secureDesktop)engine.setImportPathList({QCoreApplication::applicationDirPath()+"/qml","qrc:/qt/qml","qrc:/qt-project.org/imports"});
#endif
 engine.addImageProvider("symbols",new SymbolProvider);
 engine.addImageProvider("magnifier",new MagnifierProvider(&controller));
 engine.rootContext()->setContextProperty("controller",&controller);
 engine.load(QUrl("qrc:/ui/Main.qml"));if(engine.rootObjects().isEmpty())return 1;
 auto window=qobject_cast<QQuickWindow*>(engine.rootObjects().front());if(!window)return 1;
#ifdef GAZE_GRID_LINUX
 nativeCheckWindow=window;
#endif
 if(app.arguments().contains("--keyboard-transition-ui-check")){
  controller.setGazeEnabled(false);controller.setProperty("transitionDurationMs",600);
  controller.openLayout("keyboard-alphabet-14x4-bottom",false);window->show();
  const QStringList styles{"instant","fade","lift","slide","zoom","rows"};
  int index=0;bool waiting=false,sawIntermediate=false;
  QTimer step;step.setInterval(10);QElapsedTimer phase;
  auto keysLayer=window->findChild<QQuickItem*>("keyboardKeys");
  auto fail=[&](const QString &stage){
   step.stop();QTextStream(stderr)<<"FAIL: keyboard transition "<<styles.value(index)<<" "<<stage
    <<" running="<<window->property("transitioning").toBool()<<" progress="<<window->property("revealProgress").toDouble()
    <<" keysEnabled="<<(keysLayer&&keysLayer->isEnabled())<<Qt::endl;app.exit(2);
  };
  QObject::connect(&step,&QTimer::timeout,&app,[&]{
   const bool running=window->property("transitioning").toBool();
   const double progress=window->property("revealProgress").toDouble();
   if(waiting){
    if(running){
     if(!keysLayer||keysLayer->isEnabled()){fail("input enabled during animation");return;}
     sawIntermediate=sawIntermediate||(progress>0&&progress<1);
     if(phase.elapsed()>3000)fail("animation did not finish");
     return;
    }
    if(qAbs(progress-1)>=.001||!keysLayer||!keysLayer->isEnabled()||!sawIntermediate){fail("incomplete reveal or input recovery");return;}
    ++index;waiting=false;
   }
   if(index==styles.size()){
    step.stop();QTextStream(stderr)<<"PASS: keyboard transitions and input blocking"<<Qt::endl;app.exit(0);return;
   }
   controller.setProperty("keyboardTransition",styles[index]);
   controller.openLayout(index%2?"keyboard-alphabet-14x4-bottom":"keyboard-numeric-14x4-bottom",true);
   const bool animated=index>0;
   if(window->property("transitioning").toBool()!=animated||!keysLayer||keysLayer->isEnabled()==animated){fail("unexpected initial state");return;}
   if(animated){
    QVariant hit;QMetaObject::invokeMethod(window,"keyAt",Q_RETURN_ARG(QVariant,hit),Q_ARG(QVariant,10),Q_ARG(QVariant,10));
    if(!hit.toString().isEmpty()||window->property("revealProgress").toDouble()>=1){fail("hit testing active during reveal");return;}
    waiting=true;sawIntermediate=false;phase.start();
   }else ++index;
  });
  QTimer::singleShot(100,&step,qOverload<>(&QTimer::start));
  QTimer::singleShot(15000,&app,[&]{fail("check timed out");});
  return app.exec();
 }
 if(app.arguments().contains("--gaze-indicator-ui-check")){
  auto overlay=window->findChild<QQuickItem*>("keyboardGazeIndicator");
  if(!overlay)return 1;
  window->setProperty("gazeEnabled",true);window->setProperty("trackerConnected",true);controller.setProperty("gazeIndicatorMorph",true);window->show();
  auto sample=[overlay](double x,double y){return QMetaObject::invokeMethod(overlay,"updateMotion",Q_ARG(QVariant,x),Q_ARG(QVariant,y),Q_ARG(QVariant,true));};
  QTimer::singleShot(100,&app,[&,overlay,sample]{
  sample(.1,.1);
  QTimer::singleShot(30,&app,[&,overlay,sample]{
   sample(.11,.1);
   QTimer::singleShot(30,&app,[&,overlay,sample]{
    sample(.2,.1);
    const bool stretched=overlay->property("stretch").toDouble()>0&&overlay->property("stretch").toDouble()<=1;
    const bool horizontal=qAbs(overlay->property("heading").toDouble())<.001;
    fprintf(stderr,"indicator stretch=%f heading=%f visible=%d\n",overlay->property("stretch").toDouble(),overlay->property("heading").toDouble(),overlay->isVisible());
    QMetaObject::invokeMethod(overlay,"updateMotion",Q_ARG(QVariant,0.0),Q_ARG(QVariant,0.0),Q_ARG(QVariant,false));
    const bool reset=overlay->property("stretch").toDouble()==0;
    auto marker=window->findChild<QQuickItem*>("gazeMarker");
    window->setProperty("gazeValid",true);window->setProperty("trackerConnected",true);
    window->setProperty("gaze",QPointF(.5,.8));
    const bool inside=marker&&marker->isVisible()&&marker->parentItem()==overlay&&overlay->clip();
    window->setProperty("gaze",QPointF(.5,.1));
    const bool outside=marker&&!marker->isVisible();
    window->setProperty("gaze",QPointF(.5,.8));window->hide();
    const bool hidden=marker&&!marker->isVisible();
    app.exit(stretched&&horizontal&&reset&&inside&&outside&&hidden?0:1);
   });
  });
  });
  return app.exec();
 }
 auto settings=window->findChild<QQuickWindow*>("settingsWindow");
 if(!settings)return 1;
 settings->setTransientParent(nullptr);
#ifdef _WIN32
 windowsTheme.preserveWindowTitle(settings);
#endif
 app.setQuitOnLastWindowClosed(false);
 QSystemTrayIcon tray(app.windowIcon());
 tray.setToolTip(app.applicationDisplayName());
 QMenu trayMenu;
 auto openSettings=trayMenu.addAction(QIcon::fromTheme("configure"),"Settings…");
 QObject::connect(openSettings,&QAction::triggered,&controller,[&]{controller.activate("Settings");});
 auto openEditor=trayMenu.addAction("Keyboard editor…");
 QObject::connect(openEditor,&QAction::triggered,&app,[]{QProcess::startDetached(QCoreApplication::applicationFilePath(),{"--editor"});});
 trayMenu.addSeparator();
 auto quitAction=trayMenu.addAction(QIcon::fromTheme("application-exit"),"Quit gaze-grid");
 QObject::connect(quitAction,&QAction::triggered,&app,&QApplication::quit);
 tray.setContextMenu(&trayMenu);
 QObject::connect(&tray,&QSystemTrayIcon::activated,&controller,[&](QSystemTrayIcon::ActivationReason reason){
  if(reason==QSystemTrayIcon::Trigger||reason==QSystemTrayIcon::DoubleClick)controller.activate("Settings");
 });
 if(!signInMode)tray.show();
 auto target=window->findChild<QQuickWindow*>("targetWindow");
 auto scrollZones=window->findChild<QQuickWindow*>("scrollZonesWindow");
 if(scrollZones)scrollZones->setTransientParent(nullptr);
 // Target selection and cancellation must remain available while the panel is hidden.
 if(target)target->setTransientParent(nullptr);
#ifdef _WIN32
 if(target&&QGuiApplication::platformName()=="windows"){
  new TargetStacking(window,target);
  SetWindowDisplayAffinity(reinterpret_cast<HWND>(target->winId()),0x00000011 /* WDA_EXCLUDEFROMCAPTURE */);
  target->setProperty("waitForFreshFrame",true);
  target->setProperty("frameReady",false);
  struct Presentation {
   std::atomic<unsigned> epoch{0};
   unsigned synchronizedEpoch=0; // Accessed only by the render thread.
   bool active=false; // Accessed only by the GUI thread.
  };
  auto presentation=std::make_shared<Presentation>();
  QObject::connect(&controller,&Controller::changed,target,[target,presentation]{
   const bool active=target->property("presentationActive").toBool();
   if(active==presentation->active)return;
   presentation->active=active;++presentation->epoch;
   target->setProperty("frameReady",false);
   if(active)target->requestUpdate();
  });
  QObject::connect(target,&QQuickWindow::afterSynchronizing,target,[presentation]{
   presentation->synchronizedEpoch=presentation->epoch.load();
  },Qt::DirectConnection);
  QObject::connect(target,&QQuickWindow::afterFrameEnd,target,[target,presentation]{
   const auto epoch=presentation->synchronizedEpoch;
   QMetaObject::invokeMethod(target,[target,presentation,epoch]{
    if(epoch==presentation->epoch.load()&&presentation->active&&target->isVisible()
       &&!target->property("frameReady").toBool()){
     // Let the compositor consume the newly submitted transparent frame before
     // restoring native opacity; render completion alone need not mean display.
     DwmFlush();
     target->setProperty("frameReady",true);
    }
   },Qt::QueuedConnection);
  },Qt::DirectConnection);
 }
#endif
 bool hiddenForMouseAction=false,restoreAfterMouseAction=false;
 QObject::connect(&controller,&Controller::changed,&app,[&]{
  const bool hide=controller.keyboardTemporarilyHidden();
  if(hide==hiddenForMouseAction)return;
  hiddenForMouseAction=hide;
  if(hide){restoreAfterMouseAction=window->isVisible();window->hide();}
  else if(restoreAfterMouseAction){window->show();restoreAfterMouseAction=false;}
 });


 auto applyGeometry=[&]{
  auto screen=QGuiApplication::primaryScreen();
  if(!screen){controller.displayChanged();return;}
  auto size=screen->geometry().size();auto layout=controller.property("layout").toMap();
  if(scrollZones){scrollZones->setScreen(screen);scrollZones->setGeometry(screen->geometry());}
  controller.setDisplaySize(size.width(),size.height());
  auto dimension=[](QString value,double total){return value.endsWith('%')?value.chopped(1).toDouble()*total/100:value.toDouble();};
  int width=qMax(1,int(dimension(layout.value("width","100%").toString(),size.width())));
  int height=qMax(1,int(dimension(layout.value("height","46%").toString(),size.height())));
  // Full-screen layouts must use the whole output, including panel reservations.
  const bool fullScreen=layout.value("width").toString().trimmed()=="100%"
   &&layout.value("height").toString().trimmed()=="100%";
  if(!fullScreen&&controller.keyboardScale()!=100&&!controller.property("layoutName").toString().startsWith("minimized-off-screen-"))
   height=qBound(1,int(height*controller.keyboardScale()/100.0),qMax(1,size.height()-84));
  bool top=layout["position"].toString()=="Top";
  int offset=int(dimension(layout.value("verticaloffset","0").toString(),size.height()));
  const bool minimized=controller.property("layoutName").toString().startsWith("minimized-off-screen-");
  int x=(size.width()-width)/2+int(dimension(layout.value("horizontaloffset","0").toString(),size.width()));
  int y=(top?0:size.height()-height)+offset;
  if(fullScreen){width=size.width();height=size.height();x=0;y=0;offset=0;}
  // Linux needs an on-screen recovery surface; Windows uses the XML offsets.
#ifdef GAZE_GRID_LINUX
  if(minimized){
   width=qMin(controller.restoreWidth(),size.width());
   height=qMin(controller.restoreHeight(),size.height());
   const int corner=controller.restoreCorner();
   x=corner%2?size.width()-width:0;
   y=corner>=2?size.height()-height:0;
  }
#endif
#ifdef _WIN32
  // Match primary-display capture and normalized gaze coordinates on Windows.
  if(target){target->setScreen(screen);target->setGeometry(screen->geometry());}
#endif
  window->setScreen(screen);window->resize(width,height);
  window->setProperty("screenWidth",size.width());window->setProperty("screenHeight",size.height());window->setProperty("logicalX",x);window->setProperty("logicalY",y);
#ifdef GAZE_GRID_LINUX
  if(QGuiApplication::platformName().startsWith("wayland")){
   if(inputMethod&&!inputMethod->attachPanel(window,top||fullScreen)){QTextStream(stderr)<<"Unable to assign keyboard surface role\n";app.exit(2);return;}
   // Keep every keyboard layout above fullscreen clients without taking focus.
   if(!inputMethod||top||fullScreen){auto layer=LayerShellQt::Window::get(window);layer->setScope("gaze-grid-keyboard");layer->setScreen(screen);layer->setLayer(LayerShellQt::Window::LayerOverlay);
   layer->setAnchors(minimized?LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop)|LayerShellQt::Window::AnchorLeft:LayerShellQt::Window::Anchors(top?LayerShellQt::Window::AnchorTop:LayerShellQt::Window::AnchorBottom));
   layer->setMargins(minimized?QMargins(x,y,0,0):top?QMargins(0,offset,0,0):QMargins(0,0,0,-offset));layer->setDesiredSize(QSize(width,height));
   layer->setKeyboardInteractivity(controller.scratchpadEditable()?LayerShellQt::Window::KeyboardInteractivityOnDemand:LayerShellQt::Window::KeyboardInteractivityNone);layer->setActivateOnShow(false);layer->setExclusiveZone(inputMethod||minimized||fullScreen?-1:0);
   }
   if(target){target->setScreen(screen);target->resize(size);auto overlay=LayerShellQt::Window::get(target);overlay->setScope("gaze-grid-target");overlay->setScreen(screen);overlay->setLayer(LayerShellQt::Window::LayerOverlay);overlay->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop)|LayerShellQt::Window::AnchorLeft);overlay->setDesiredSize(size);overlay->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);overlay->setExclusiveZone(-1);}
   if(scrollZones){auto overlay=LayerShellQt::Window::get(scrollZones);overlay->setScope("gaze-grid-scroll-zones");overlay->setScreen(screen);overlay->setLayer(LayerShellQt::Window::LayerOverlay);overlay->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop)|LayerShellQt::Window::AnchorLeft);overlay->setDesiredSize(size);overlay->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);overlay->setActivateOnShow(false);overlay->setExclusiveZone(-1);}
  }else
#endif
  {window->setPosition(screen->geometry().topLeft()+QPoint(x,y));}
 };
 QObject::connect(&controller,&Controller::layoutChanged,&app,applyGeometry);
 QObject::connect(&controller,&Controller::geometryPreferencesChanged,&app,applyGeometry);
 auto displayChanged=[&]{controller.displayChanged();applyGeometry();};
 auto watchScreen=[&](QScreen *screen){
  QObject::connect(screen,&QScreen::geometryChanged,&app,displayChanged);
  QObject::connect(screen,&QScreen::logicalDotsPerInchChanged,&app,displayChanged);
  QObject::connect(screen,&QScreen::orientationChanged,&app,displayChanged);
 };
 for(auto screen:QGuiApplication::screens())watchScreen(screen);
 QObject::connect(&app,&QGuiApplication::screenAdded,&app,[&](QScreen *screen){watchScreen(screen);displayChanged();});
 QObject::connect(&app,&QGuiApplication::screenRemoved,&app,displayChanged);
 QObject::connect(&app,&QGuiApplication::primaryScreenChanged,&app,displayChanged);
 applyGeometry();window->show();
#ifdef _WIN32
 // Prepare the translucent target surface before its first use and suppress
 // Windows' show/hide fades, which otherwise flash over the selection point.
 if(target&&QGuiApplication::platformName()=="windows"){
  const auto handle=reinterpret_cast<HWND>(target->winId());
  const BOOL disabled=TRUE;
  DwmSetWindowAttribute(handle,DWMWA_TRANSITIONS_FORCEDISABLED,&disabled,sizeof(disabled));
 }
#endif
#ifdef _WIN32
 if(signInCheck){
  window->setProperty("gazeCheck",true);window->setProperty("gazeEnabled",false);
  QTimer::singleShot(100,&app,[&]{
   auto status=window->findChild<QQuickItem*>("signInTrackerStatus");QVariant underStatus;
   const bool statusVisible=status&&status->isVisible();
   const bool statusBlocksKeys=statusVisible&&QMetaObject::invokeMethod(window,"keyAt",Q_RETURN_ARG(QVariant,underStatus),
      Q_ARG(QVariant,status->x()+status->width()/2),Q_ARG(QVariant,status->y()+status->height()/2))&&underStatus.toString().isEmpty();
   window->setProperty("trackerConnected",true);
   const bool ok=controller.isSignInMode()&&controller.predictionMode()=="off"
      &&controller.property("suggestions").toStringList().isEmpty()&&!settings->isVisible()
      &&statusBlocksKeys&&!status->isVisible();
   QTextStream(stdout)<<(ok?"PASS: sign-in keyboard privacy and tracker status":"FAIL: sign-in keyboard privacy or tracker status")<<Qt::endl;
   app.exit(ok?0:2);
  });return app.exec();
 }
#endif
 if(app.arguments().contains("--gaze-input-mode-ui-check")){
  window->setProperty("gazeCheck",true);window->setProperty("gazeEnabled",false);
  window->setProperty("gazeValid",false);window->setProperty("gazeEnabled",true);
  controller.setProperty("paused",true);
  auto layout=controller.property("layout").toMap();layout["layoutMode"]="canvas";layout["activateoverlappingkeys"]=true;
  controller.setProperty("layout",layout);emit controller.layoutChanged();
  struct Step {int delay;int connected;int valid;bool gaze;};
  const auto steps=std::make_shared<QList<Step>>(QList<Step>{
   {0,0,0,false},{0,1,0,true},{100,-1,1,true},
   {100,-1,0,true},{100,-1,1,true},{100,-1,0,true},{1500,-1,-1,true},
   {0,0,0,false},{100,-1,1,false},{0,1,0,true},{100,-1,1,true}
  });
  auto timer=new QTimer(&app);timer->setSingleShot(true);
  QObject::connect(timer,&QTimer::timeout,&app,[&,timer,steps,index=0]()mutable{
   const auto step=steps->at(index);
   if(step.connected>=0)QMetaObject::invokeMethod(&controller,"setTrackerConnected",Qt::DirectConnection,Q_ARG(bool,step.connected==1));
   if(step.valid>=0)emit controller.gazePoint(.5,.75,step.valid==1);
   const bool actual=window->property("usingGaze").toBool();
   auto status=window->findChild<QObject*>("inputModeStatus");
   const bool statusMatches=status&&status->property("text").toString().startsWith(step.gaze?"Gaze dwell":"Mouse dwell");
   const bool noStaleTarget=window->property("gazeValid").toBool()
      ||(window->property("gazeTarget").toString().isEmpty()&&window->property("overlapTargets").toList().isEmpty());
   if(actual!=step.gaze||!statusMatches||!noStaleTarget){
    QTextStream(stderr)<<"FAIL: unstable gaze/mouse source at stage "<<index<<" actual="<<actual<<" status="<<statusMatches<<" safe="<<noStaleTarget<<Qt::endl;app.exit(2);return;
   }
   if(++index==steps->size()){
    window->setProperty("gazeEnabled",false);
    if(window->property("usingGaze").toBool()){app.exit(2);return;}
    QTextStream(stdout)<<"PASS: gaze/mouse dwell and status follow connection state regardless of gaze validity"<<Qt::endl;app.quit();return;
   }
   timer->start(steps->at(index).delay);
  });timer->start(0);return app.exec();
 }
 if(app.arguments().contains("--keyboard-background-ui-check")){
  window->setProperty("gazeCheck",true);
  controller.openLayout("size-move-demo",false);controller.setProperty("saveCanvasChanges",false);
  QTimer::singleShot(250,&app,[&]{
   const auto image=window->grabWindow();
   bool transparent=!image.isNull();
#ifdef GAZE_GRID_WINDOWS
   // The Windows offscreen backing store returns RGB pixels. Check the actual
   // backdrop item and transparent window color there; Linux retains pixel coverage.
   if(QGuiApplication::platformName()=="offscreen"){
    auto backdrop=window->findChild<QQuickItem*>("keyboardBackdrop");
    transparent=backdrop&&!backdrop->isVisible()&&window->color().alpha()==0;
   }else
#endif
   transparent=transparent&&image.pixelColor(window->width()*99/100,window->height()/2).alpha()==0;
   const QPoint gap(window->width()*99/100,window->height()/2);
   if(!transparent||window->mask().isEmpty()||
      window->mask().contains(gap)||!window->mask().contains(QPoint(window->width()/10,window->height()/10))){
    QTextStream(stderr)<<"FAIL: transparent background or key input mask: alpha="<<(image.isNull()?-1:image.pixelColor(gap).alpha())<<" size="<<image.width()<<"x"<<image.height()<<" maskEmpty="<<window->mask().isEmpty()<<" gapInMask="<<window->mask().contains(gap)<<Qt::endl;app.exit(2);return;
   }
   controller.setProperty("sizeMoveMode",true);
   QTimer::singleShot(50,&app,[&]{
    if(!window->mask().isEmpty()){app.exit(3);return;}
    controller.setProperty("sizeMoveMode",false);
    controller.openLayout("main-14x1-bottom",false);
    QTimer::singleShot(50,&app,[&]{app.exit(window->mask().isEmpty()?0:4);});
   });
  });
  return app.exec();
 }
 if(app.arguments().contains("--overlap-activation-ui-check")){
  controller.openLayout("keyboard-conversation-bottom",false);
  controller.setProperty("paused",false);controller.setProperty("dwellMs",25);
  window->setProperty("gazeCheck",true);window->setProperty("gazeEnabled",false);
  QVariantList keys;
  for(int i=0;i<2;++i)keys<<QVariantMap{{"kind","DynamicKey"},{"col",i*.25},{"row",0},{"width",.75},{"height",1},
   {"label",i?"B":"A"},{"commands",QVariantList{QVariantMap{{"tag","Text"},{"text",i?"b":"a"}}}}};
  keys<<QVariantMap{{"kind","Scratchpad"},{"col",0},{"row",0},{"width",0.1},{"height",0.1}};
  controller.setProperty("layout",QVariantMap{{"layoutMode","canvas"},{"activateoverlappingkeys",true},{"rows",1},{"cols",1},{"keys",keys}});
  emit controller.layoutChanged();window->show();
  auto stage=std::make_shared<int>(0);auto timer=new QTimer(&app);timer->setInterval(200);
  QObject::connect(timer,&QTimer::timeout,&app,[&,stage]{
   if(*stage>0){
    const QString expected=(*stage==1||*stage==4)?"ab":"b";
    if(controller.property("scratchpad").toString()!=expected){
     QTextStream(stderr)<<"FAIL: overlap activation stage "<<*stage<<": "<<controller.property("scratchpad").toString()<<Qt::endl;app.exit(2);return;
    }
   }
   if(*stage==4){app.quit();return;}
   window->setProperty("gazeCheck",true);window->setProperty("gazeValid",false);
   controller.hover("");controller.setProperty("scratchpad",QString());
   auto layout=controller.property("layout").toMap();
   layout["activateoverlappingkeys"]=*stage!=1;layout["layoutMode"]=*stage==2?"grid":"canvas";
   controller.setProperty("layout",layout);emit controller.layoutChanged();
   const QPointF point(window->width()*.5,window->height()*.5);
   if(*stage==3){
    window->setProperty("gazeEnabled",false);
    QEnterEvent enter(point,point,point);QCoreApplication::sendEvent(window,&enter);
    QMouseEvent move(QEvent::MouseMove,point,point,Qt::NoButton,Qt::NoButton,Qt::NoModifier);QCoreApplication::sendEvent(window,&move);
   }else{
    window->setProperty("gazeEnabled",true);
    window->setProperty("gaze",QPointF((point.x()+window->property("logicalX").toDouble())/window->property("screenWidth").toDouble(),
     (point.y()+window->property("logicalY").toDouble())/window->property("screenHeight").toDouble()));
    window->setProperty("gazeValid",true);window->setProperty("trackerConnected",true);
   }
   window->setProperty("gazeCheck",false);++*stage;
  });timer->start();return app.exec();
 }
 if(app.arguments().contains("--keyboard-opacity-ui-check")){
  controller.openLayout("main-14x1-bottom",false);
  window->setProperty("gazeCheck",true);window->setProperty("gazeEnabled",false);
  controller.setProperty("paused",false);
  QVariantMap key{{"col",0},{"row",0},{"width",1},{"height",1},{"kind","DynamicKey"},
                  {"label","Test"},{"symbol",""},{"commands",QVariantList{}},
                  {"style",QVariantMap{{"opacity",0.5},{"foregroundopacity",0.25}}}};
  controller.setProperty("layout",QVariantMap{{"rows",1},{"cols",1},{"keys",QVariantList{key}}});
  emit controller.layoutChanged();
  QTimer::singleShot(250,&app,[&]{
   const auto image=grabSceneForCheck(window);
   std::function<QQuickItem*(QQuickItem*)> findKey=[&](QQuickItem *item)->QQuickItem*{
    if(item->objectName()=="keyMouseArea:0")return item->parentItem();
    for(auto child:item->childItems())if(auto found=findKey(child))return found;
    return nullptr;
   };
   auto item=findKey(window->contentItem());
   if(image.isNull()||qAbs(image.pixelColor(10,10).alpha()-128)>2||!item||
      qAbs(item->property("foregroundOpacity").toDouble()-0.25)>0.001){
    QTextStream(stderr)<<"FAIL: keyboard XML opacity rendering: alpha="<<(image.isNull()?-1:image.pixelColor(10,10).alpha())<<" foreground="<<(item?item->property("foregroundOpacity").toDouble():-1)<<Qt::endl;app.exit(2);return;
   }
   app.quit();
  });
  return app.exec();
 }
 if(app.arguments().contains("--restore-opacity-ui-check")){
  window->setProperty("gazeCheck",true);window->setProperty("gazeEnabled",false);
  controller.setProperty("paused",false);controller.setRestoreWidth(64);controller.setRestoreHeight(64);
  controller.openLayout("minimized-off-screen-bottom",false);
  auto poll=new QTimer(&app);poll->setInterval(60);
  QObject::connect(poll,&QTimer::timeout,&app,[&,stage=0]()mutable{
   poll->stop();
   if(stage>0){
    const auto image=grabSceneForCheck(window);const int opacity=window->property("minimizedRecovery").toBool()?((stage-1)/4==0?25:(stage-1)/4==1?50:100):75;
    if(image.isNull()||qAbs(image.pixelColor(2,2).alpha()-qRound(255*opacity/100.0))>2){
     QTextStream(stderr)<<"FAIL: unstable restore-button alpha at frame "<<stage<<Qt::endl;app.exit(2);return;
    }
   }
   if(stage==12){app.quit();return;}
   controller.setProperty("restoreOpacity",stage/4==0?25:stage/4==1?50:100);
   window->setProperty("hovered",stage%2?"key:0":"");window->setProperty("progress",stage%2?.5:0.0);
   window->update();++stage;poll->start();
  });
  poll->start();return app.exec();
 }

 if(app.arguments().contains("--canvas-ui-check")){
  QVariantMap layout{{"layoutMode","canvas"},{"cols",100.0},{"rows",100.0}};
  auto key=[](double x,double y,double w,double h,QString kind){return QVariantMap{
   {"col",x},{"row",y},{"width",w},{"height",h},{"kind",kind},
   {"label","A"},{"symbol",""},{"commands",QVariantList{}},{"suggestionCount",3}};};
  layout["keys"]=QVariantList{key(12.5,20,30,10.5,"DynamicKey"),key(20,22,5,5,"DynamicKey"),key(0,80,90,10,"SuggestionRow")};
  controller.setProperty("layout",layout);emit controller.layoutChanged();
  controller.setProperty("suggestions",QStringList{"one","two","three"});
  auto hit=[&](double x,double y){
   QVariant result;
   QMetaObject::invokeMethod(window,"keyAt",Q_RETURN_ARG(QVariant,result),Q_ARG(QVariant,x*window->width()/100),Q_ARG(QVariant,y*window->height()/100));
   window->setProperty("gazeEnabled",true);window->setProperty("gazeValid",true);window->setProperty("trackerConnected",true);
   window->setProperty("gaze",QPointF(
    (window->property("logicalX").toDouble()+x*window->width()/100)/window->property("screenWidth").toDouble(),
    (window->property("logicalY").toDouble()+y*window->height()/100)/window->property("screenHeight").toDouble()));
   if(window->property("gazeTarget").toString()!=result.toString())return QString("shared hit mismatch");
   return result.toString();
  };
  for(const QSize size:{QSize(1000,600),QSize(700,900)}){
   window->resize(size);QCoreApplication::processEvents();
   if(hit(13,21)!="key:0"||hit(22,24)!="key:1"||!hit(60,50).isEmpty()||hit(45,85)!="suggest:1"||!hit(90,85).isEmpty()){
    QTextStream(stderr)<<"FAIL: canvas targeting at "<<size.width()<<"x"<<size.height()<<" hits: "<<hit(13,21)<<","<<hit(22,24)<<","<<hit(60,50)<<","<<hit(45,85)<<","<<hit(90,85)<<Qt::endl;return 2;
   }
  }
  hit(45,85);
  controller.setProperty("suggestions",QStringList{});
  if(!window->property("gazeTarget").toString().isEmpty()){
   QTextStream(stderr)<<"FAIL: shared hit retained an empty suggestion"<<Qt::endl;return 2;
  }
  applyGeometry();QCoreApplication::processEvents();
  hit(22,24);
  auto replacement=layout["keys"].toList();replacement[1]=key(70,22,5,5,"DynamicKey");layout["keys"]=replacement;
  controller.setProperty("layout",layout);emit controller.layoutChanged();
  QCoreApplication::processEvents();
  if(window->property("gazeTarget").toString()!="key:0"){
   QTextStream(stderr)<<"FAIL: shared hit did not update after same-size layout replacement"<<Qt::endl;return 2;
  }
  window->setProperty("gazeValid",false);
  if(!window->property("gazeTarget").toString().isEmpty())return 2;
  controller.openLayout("size-move-demo",false);controller.setProperty("saveCanvasChanges",false);
  controller.activate("MoveModeToggle");
  window->setProperty("gazeEnabled",false);window->show();QCoreApplication::processEvents();
  auto drag=[&](double x,double y,double dx,double dy){
   const QPointF start(x*window->width()/100,y*window->height()/100);
   const QPointF end((x+dx)*window->width()/100,(y+dy)*window->height()/100);
   QMouseEvent press(QEvent::MouseButtonPress,start,start,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
   QCoreApplication::sendEvent(window,&press);
   QMouseEvent move(QEvent::MouseMove,end,end,Qt::NoButton,Qt::LeftButton,Qt::NoModifier);
   QCoreApplication::sendEvent(window,&move);
   QMouseEvent release(QEvent::MouseButtonRelease,end,end,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
   QCoreApplication::sendEvent(window,&release);QCoreApplication::processEvents();
  };
  drag(15,60,5,3);
  auto moved=controller.property("layout").toMap()["keys"].toList()[3].toMap();
  if(qAbs(moved["col"].toDouble()-10)>0.1||qAbs(moved["row"].toDouble()-51)>0.1){
   QTextStream(stderr)<<"FAIL: canvas key drag "<<moved["col"].toDouble()<<","<<moved["row"].toDouble()<<Qt::endl;return 2;
  }
  controller.activate("SizeModeToggle");
  drag(29.5,78.5,5,5);
  auto resized=controller.property("layout").toMap()["keys"].toList()[3].toMap();
  if(qAbs(resized["width"].toDouble()-25)>0.1||qAbs(resized["height"].toDouble()-33)>0.1){
   QTextStream(stderr)<<"FAIL: canvas key resize"<<Qt::endl;return 2;
  }
  if(hit(40,30)!="key:2"){
   QTextStream(stderr)<<"FAIL: scratchpad cannot be selected for dwell editing"<<Qt::endl;return 2;
  }
  window->setProperty("gazeCheck",true);
  controller.activate("MoveModeToggle");controller.setProperty("dwellMs",10);
  controller.canvasEditAt(.2,.6,true);
  {QEventLoop wait;QTimer::singleShot(20,&wait,&QEventLoop::quit);wait.exec();}controller.canvasEditAt(.2,.6,true);
  controller.canvasEditAt(.45,.45,true);
  {QEventLoop wait;QTimer::singleShot(20,&wait,&QEventLoop::quit);wait.exec();}controller.canvasEditAt(.45,.45,true);
  QCoreApplication::processEvents();
  if(controller.property("sizeMoveSelection").toInt()!=-1||hit(45,45)!="key:3"){
   QTextStream(stderr)<<"FAIL: dwell placement and gaze hit geometry"<<Qt::endl;return 2;
  }
  auto editSettings=window->findChild<QQuickWindow*>("settingsWindow");
  if(!editSettings)return 2;
  editSettings->show();QCoreApplication::processEvents();
  controller.setProperty("dwellMs",50);window->setProperty("gazeCheck",false);
  auto settle=[](){QEventLoop wait;QTimer::singleShot(160,&wait,&QEventLoop::quit);wait.exec();};
  hit(65,60);settle();
  if(controller.property("sizeMoveSelection").toInt()!=5){QTextStream(stderr)<<"FAIL: gaze dwell pickup"<<Qt::endl;return 2;}
  hit(60,45);settle();
  if(controller.property("sizeMoveSelection").toInt()!=-1||hit(60,45)!="key:5"){QTextStream(stderr)<<"FAIL: gaze dwell drop"<<Qt::endl;return 2;}
  window->setProperty("gazeEnabled",false);
  auto pointMouse=[&](double x,double y){
   const QPointF point(x*window->width()/100,y*window->height()/100);

   QMouseEvent move(QEvent::MouseMove,point,point,Qt::NoButton,Qt::NoButton,Qt::NoModifier);QCoreApplication::sendEvent(window,&move);
  };
  pointMouse(45,45);settle();
  if(controller.property("sizeMoveSelection").toInt()!=3){QTextStream(stderr)<<"FAIL: mouse dwell pickup"<<Qt::endl;return 2;}
  pointMouse(20,45);settle();
  const auto placed=controller.property("layout").toMap()["keys"].toList()[3].toMap();
  if(controller.property("sizeMoveSelection").toInt()!=-1||qAbs(placed["col"].toDouble()-7.5)>.1){QTextStream(stderr)<<"FAIL: mouse dwell drop"<<Qt::endl;return 2;}
  // Enter both modes through the actual XML keys, with Settings still open.
  // This matches selecting the demo from Settings and then using mouse dwell.
  controller.openLayout("size-move-demo",false);controller.setProperty("saveCanvasChanges",false);QCoreApplication::processEvents();
  pointMouse(15,10);settle();settle();
  if(controller.property("canvasEditMode").toString()!="move"){QTextStream(stderr)<<"FAIL: dwell move toggle did not stay enabled"<<Qt::endl;return 2;}
  pointMouse(15,60);settle();
  if(controller.property("sizeMoveSelection").toInt()!=3){QTextStream(stderr)<<"FAIL: pickup after dwelling on mode key"<<Qt::endl;return 2;}
  pointMouse(45,45);settle();
  const auto moveResult=controller.property("layout").toMap()["keys"].toList()[3].toMap();
  if(controller.property("sizeMoveSelection").toInt()!=-1||qAbs(moveResult["col"].toDouble()-35)>.1){QTextStream(stderr)<<"FAIL: move with Settings open"<<Qt::endl;return 2;}
  pointMouse(50,10);settle();settle();
  if(controller.property("canvasEditMode").toString()!="size"){QTextStream(stderr)<<"FAIL: dwell resize toggle did not stay enabled"<<Qt::endl;return 2;}
  pointMouse(45,45);settle();pointMouse(65,78);settle();
  const auto sizeResult=controller.property("layout").toMap()["keys"].toList()[3].toMap();
  if(controller.property("sizeMoveSelection").toInt()!=-1||qAbs(sizeResult["width"].toDouble()-30)>.1||qAbs(sizeResult["height"].toDouble()-47)>.1){QTextStream(stderr)<<"FAIL: resize with Settings open"<<Qt::endl;return 2;}
  editSettings->hide();
  QTextStream(stdout)<<"PASS: shared gaze targeting, fractional geometry, overlap order, gaps, suggestions, resizing and layout replacement"<<Qt::endl;
  return 0;
 }
 if(app.arguments().contains("--joystick-mouse-ui-check")||app.arguments().contains("--joystick-mouse-settings-ui-check")){
  controller.openLayout("game-controller",false);
  window->setProperty("gazeEnabled",true);window->setProperty("gazeValid",false);
  std::function<QQuickItem*(QQuickItem*)> findArea=[&](QQuickItem *item)->QQuickItem*{
   if(item->objectName()=="keyMouseArea:0")return item;
   for(auto child:item->childItems())if(auto found=findArea(child))return found;
   return nullptr;
  };
  auto area=findArea(window->contentItem());if(!area)return 2;
  auto key=area->parentItem();
  auto settings=window->findChild<QQuickWindow*>("settingsWindow");if(!settings)return 2;
  if(app.arguments().contains("--joystick-mouse-settings-ui-check"))settings->show();
  std::function<QQuickItem*(QQuickItem*)> findThumb=[&](QQuickItem *item)->QQuickItem*{
   if(item->objectName()=="joystickThumb:0")return item;
   for(auto child:item->childItems())if(auto found=findThumb(child))return found;
   return nullptr;
  };
  auto thumb=findThumb(key);if(!thumb)return 2;
  auto poll=new QTimer(&app);poll->setInterval(50);
  QObject::connect(poll,&QTimer::timeout,&app,[&,area,key,thumb,stage=0]()mutable{
   if(!window->isVisible())return;
   // Settings activation and layout polish can arrive after the first injected
   // mouse event. Observe the settled geometry and retry within the test timeout.
   const double centeredX=(thumb->parentItem()->width()-thumb->width())/2;
   if(stage==4){
    if(key->property("stickX").toDouble()<.5||thumb->x()<=centeredX+1)return;
    QTextStream(stdout)<<"PASS: mouse and gaze joystick input with Settings open"<<Qt::endl;app.quit();return;
   }
   const QPointF local(stage<2?area->width()*.8:area->width()/2,area->height()/2);
   const QPointF point=area->mapToItem(window->contentItem(),local);
   if(stage==0||stage==2){
    QEnterEvent enter(point,point,point);QCoreApplication::sendEvent(window,&enter);
    QMouseEvent move(QEvent::MouseMove,point,point,Qt::NoButton,Qt::NoButton,Qt::NoModifier);QCoreApplication::sendEvent(window,&move);
    ++stage;return;
   }
   if(stage==1){
    if(window->property("hovered").toString()!="key:0"||key->property("stickX").toDouble()<.5||thumb->x()<=centeredX+1){stage=0;return;}
    stage=2;return;
   }
   if(std::abs(key->property("stickX").toDouble())>.01||std::abs(thumb->x()-centeredX)>1){stage=2;return;}
   QMetaObject::invokeMethod(area,"exited");
   if(!window->property("hovered").toString().isEmpty()){app.exit(2);return;}
   if(settings->isVisible()){
    const auto gazePoint=area->mapToItem(window->contentItem(),QPointF(area->width()*.8,area->height()/2));
    window->setProperty("gaze",QPointF((window->property("logicalX").toDouble()+gazePoint.x())/window->property("screenWidth").toDouble(),(window->property("logicalY").toDouble()+gazePoint.y())/window->property("screenHeight").toDouble()));
    window->setProperty("gazeValid",true);window->setProperty("trackerConnected",true);stage=4;return;
   }
   QTextStream(stdout)<<"PASS: actual joystick dot moves and centers with gaze unavailable"<<Qt::endl;app.quit();
  });
  poll->start();QTimer::singleShot(3000,&app,[&]{QTextStream(stderr)<<"FAIL: joystick UI did not settle before timeout"<<Qt::endl;app.exit(2);});return app.exec();
 }

 if(app.arguments().contains("--prediction-hover-ui-check")){
  window->setProperty("gazeEnabled",false);
  controller.openLayout("keyboard-alphabet-14x4-bottom",false);
  std::function<QQuickItem*(QQuickItem*,const QString&)> findItem=[&](QQuickItem *item,const QString &name)->QQuickItem*{
   if(item->objectName()==name)return item;
   for(auto child:item->childItems())if(auto found=findItem(child,name))return found;
   return nullptr;
  };
  auto prediction=findItem(window->contentItem(),"predictionMouseArea:0");
  QObject *keyArea=nullptr;QString command;
  const auto keys=controller.property("layout").toMap()["keys"].toList();
  for(int i=0;i<keys.size();++i){
   if(keys[i].toMap()["kind"]=="SuggestionRow")continue;
   keyArea=findItem(window->contentItem(),"keyMouseArea:"+QString::number(i));
   if(keyArea){command="key:"+QString::number(i);break;}
  }
  if(!prediction||!keyArea){QTextStream(stderr)<<"Missing hover areas: prediction="<<(prediction!=nullptr)<<" key="<<(keyArea!=nullptr)<<Qt::endl;return 2;}
  controller.setProperty("suggestions",QStringList{"word"});
  if(!prediction->property("enabled").toBool()){QTextStream(stderr)<<"Prediction did not enable"<<Qt::endl;return 2;}
  QMetaObject::invokeMethod(prediction,"entered");
  controller.setProperty("suggestions",QStringList{});
  if(prediction->property("enabled").toBool()||!window->property("hovered").toString().isEmpty()){QTextStream(stderr)<<"Empty prediction enabled="<<prediction->property("enabled").toBool()<<" hover="<<window->property("hovered").toString()<<Qt::endl;return 2;}
  controller.setProperty("suggestions",QStringList{"word"});
  auto predictionProgress=findItem(window->contentItem(),"predictionProgress:0");
  if(!predictionProgress){QTextStream(stderr)<<"Missing prediction progress bar"<<Qt::endl;return 2;}
  QMetaObject::invokeMethod(prediction,"entered");window->setProperty("progress",.5);
  if(predictionProgress->width()<=0){QTextStream(stderr)<<"Mouse prediction progress missing"<<Qt::endl;return 2;}
  const QPointF centre=prediction->mapToItem(window->contentItem(),QPointF(prediction->width()/2,prediction->height()/2));
  window->setProperty("hovered","");window->setProperty("gazeEnabled",true);window->setProperty("gazeValid",true);window->setProperty("trackerConnected",true);
  window->setProperty("gaze",QPointF((centre.x()+window->property("logicalX").toDouble())/window->property("screenWidth").toDouble(),
                                   (centre.y()+window->property("logicalY").toDouble())/window->property("screenHeight").toDouble()));
  window->setProperty("progress",.5);
  if(predictionProgress->width()<=0){QTextStream(stderr)<<"Gaze prediction progress missing"<<Qt::endl;return 2;}
  window->setProperty("gazeValid",false);
  if(predictionProgress->width()!=0){QTextStream(stderr)<<"Invalid gaze shows prediction progress"<<Qt::endl;return 2;}
  window->setProperty("trackerConnected",false); // Connection-based switching has its own regression check.
  QMetaObject::invokeMethod(prediction,"entered");window->setProperty("progress",.5);
  if(window->property("usingGaze").toBool()||predictionProgress->width()<=0){QTextStream(stderr)<<"Mouse fallback prediction progress missing"<<Qt::endl;return 2;}
  window->setProperty("gazeValid",true);window->setProperty("trackerConnected",true);
  if(!window->property("usingGaze").toBool()||window->property("progress").toDouble()!=0){QTextStream(stderr)<<"Gaze return did not reset dwell"<<Qt::endl;return 2;}
  controller.setProperty("suggestions",QStringList{});
  if(predictionProgress->width()!=0){QTextStream(stderr)<<"Empty prediction shows gaze progress"<<Qt::endl;return 2;}
  controller.setProperty("suggestions",QStringList{"word"});window->setProperty("gazeEnabled",false);
  // Qt can deliver the new key's enter before the old prediction's exit.
  QMetaObject::invokeMethod(prediction,"entered");
  QMetaObject::invokeMethod(keyArea,"entered");
  QMetaObject::invokeMethod(prediction,"exited");
  if(window->property("hovered").toString()!=command){QTextStream(stderr)<<"Unexpected hover: "<<window->property("hovered").toString()<<Qt::endl;return 2;}
  QTimer::singleShot(150,&app,[&,keyArea,command]{
   const bool dwelling=window->property("hovered").toString()==command&&window->property("progress").toDouble()>0;
   QMetaObject::invokeMethod(keyArea,"exited");
   app.exit(dwelling&&window->property("hovered").toString().isEmpty()?0:2);
  });
  return app.exec();
 }

 if(app.arguments().contains("--hide-keyboard-mouse-ui-check")){
  controller.configureInputMethod([](QString){return true;},[](QString,bool){return true;});
  controller.inputContextChanged(true,false);
  controller.setProperty("hideKeyboardDuringMouseActions",true);
  QObject::connect(&controller,&Controller::changed,&app,[&]{
   if(!controller.keyboardTemporarilyHidden())return;
   QTimer::singleShot(0,&app,[&]{
    const bool hidden=!window->isVisible();
    controller.setProperty("targeting",true);
    const bool cancelAvailable=target&&target->isVisible();
    controller.clickCommand("CancelTarget");
    const bool restored=window->isVisible()&&!controller.keyboardTemporarilyHidden();
    QTextStream(stdout)<<"hidden="<<hidden<<" cancelAvailable="<<cancelAvailable<<" restored="<<restored<<Qt::endl;
    app.exit(hidden&&cancelAvailable&&restored?0:2);
   });
  });
  controller.executeCommands({QVariantMap{{"tag","Action"},{"text","MouseMoveToLeft"}}});
  QTimer::singleShot(2000,&app,[&]{app.exit(2);});
  return app.exec();
 }

#ifdef _WIN32
 if(app.arguments().contains("--target-stacking-check")){
  if(!target||QGuiApplication::platformName()!="windows")return 2;
  window->setProperty("gazeCheck",true);
  window->show();controller.setProperty("targeting",true);
  auto poll=new QTimer(&app);poll->setInterval(100);
  QObject::connect(poll,&QTimer::timeout,&app,[&,stage=0,focus=HWND(nullptr)]()mutable{
   const auto keyboardHandle=reinterpret_cast<HWND>(window->winId());
   const auto targetHandle=reinterpret_cast<HWND>(target->winId());
   bool above=false;
   for(auto handle=GetWindow(keyboardHandle,GW_HWNDPREV);handle;handle=GetWindow(handle,GW_HWNDPREV))
    if(handle==targetHandle){above=true;break;}
   if(!above||(stage>0&&GetForegroundWindow()!=focus)){
    QTextStream(stderr)<<"FAIL: target stacking or foreground focus changed at stage "<<stage<<Qt::endl;
    app.exit(2);return;
   }
   focus=GetForegroundWindow();
   if(stage==0){
    SetWindowPos(keyboardHandle,HWND_TOPMOST,0,0,0,0,SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE);
   }else if(stage==1){
    window->hide();window->show();
   }else if(stage==2){
    controller.setProperty("targeting",false);controller.setProperty("targeting",true);
   }else{
    controller.setProperty("targeting",false);
    QTextStream(stdout)<<"PASS: target stays above keyboard on show, restack and selection restart without taking focus"<<Qt::endl;
    app.quit();return;
   }
   ++stage;
  });
  poll->start();return app.exec();
 }
#endif
 if(app.arguments().contains("--target-overlay-frame-check")){
  if(!target||!target->property("waitForFreshFrame").toBool())return 2;
  window->setProperty("gazeCheck",true);
  controller.setProperty("targeting",true);controller.setProperty("capturePending",true);
  controller.setProperty("magnifierEnabled",true);controller.setProperty("progressiveZoom",false);
  QImage frame(QGuiApplication::primaryScreen()->size(),QImage::Format_RGB32);frame.fill(Qt::red);
  QMetaObject::invokeMethod(&controller,"receiveMagnifierFrame",Qt::DirectConnection,Q_ARG(QImage,frame));
  if(target->opacity()!=0){QTextStream(stderr)<<"FAIL: overlay exposed before its first frame"<<Qt::endl;return 2;}
  auto poll=new QTimer(&app);poll->setInterval(25);
  QObject::connect(poll,&QTimer::timeout,&app,[&,stage=0]()mutable{
   if(!target->property("frameReady").toBool())return;
   const auto image=target->grabWindow();
   if(image.isNull()){QTextStream(stderr)<<"FAIL: no overlay frame"<<Qt::endl;app.exit(2);return;}
   const auto color=image.pixelColor(image.width()/2,image.height()/2);
   if(stage==0){
    if(color!=QColor(Qt::red)){QTextStream(stderr)<<"FAIL: stale-frame fixture did not render"<<Qt::endl;app.exit(2);return;}
    controller.clickCommand("CancelTarget");controller.setProperty("targeting",true);
    if(target->opacity()!=0||target->property("frameReady").toBool()){
     QTextStream(stderr)<<"FAIL: previous magnified frame exposed on restart"<<Qt::endl;app.exit(2);return;
    }
    stage=1;
   }else{
    if(color==QColor(Qt::red)||target->opacity()!=1){QTextStream(stderr)<<"FAIL: stale magnified frame remained visible"<<Qt::endl;app.exit(2);return;}
    controller.clickCommand("CancelTarget");
    QTextStream(stdout)<<"PASS: overlay waits for a fresh rendered frame on initial show and restart"<<Qt::endl;app.quit();
   }
  });
  poll->start();QTimer::singleShot(5000,&app,[&]{QTextStream(stderr)<<"FAIL: overlay frame readiness timed out"<<Qt::endl;app.exit(2);});
  return app.exec();
 }
 if(app.arguments().contains("--target-dwell-ui-check")||app.arguments().contains("--pointer-dwell-ui-check")){
  // Synthetic gaze/pointer motion and screenshot, without mouse-button events.
  const bool gazeInput=app.arguments().contains("--target-dwell-ui-check");
  controller.inputContextChanged(true,false);
  controller.setProperty("targeting",true);controller.setProperty("magnifierEnabled",true);
  controller.setProperty("progressiveZoom",true);controller.setProperty("zoomDurationMs",500);
  controller.setProperty("targetDwellEnabled",true);controller.setProperty("targetStartDwellMs",250);controller.setProperty("targetConfirmDwellMs",300);
  window->setProperty("gazeEnabled",gazeInput);window->setProperty("gazeValid",gazeInput);window->setProperty("trackerConnected",gazeInput);window->setProperty("gaze",QPointF(.7,.7));
  auto poll=new QTimer(&app);poll->setInterval(25);
  QObject::connect(poll,&QTimer::timeout,&app,[&,gazeInput,stage=0]()mutable{
   if(!gazeInput&&target->isVisible()){
    const QPointF point(target->width()*.7,target->height()*.7);
    QEnterEvent enter(point,point,point);QCoreApplication::sendEvent(target,&enter);
    QMouseEvent move(QEvent::MouseMove,point,point,Qt::NoButton,Qt::NoButton,Qt::NoModifier);QCoreApplication::sendEvent(target,&move);
   }
   if(stage==0&&controller.property("capturePending").toBool()){
    auto ring=window->findChild<QQuickItem*>("targetProgressRing");
    const bool stable=QGuiApplication::platformName()!="windows"||(target->isVisible()&&target->opacity()==0);
    if(!stable||!ring||ring->isVisible()){
     QTextStream(stderr)<<"FAIL: target overlay flashed during capture"<<Qt::endl;app.exit(2);return;
    }
    QImage frame(QGuiApplication::primaryScreen()->size(),QImage::Format_RGB32);frame.fill(Qt::green);
    QMetaObject::invokeMethod(&controller,"receiveMagnifierFrame",Qt::DirectConnection,Q_ARG(QImage,frame));stage=1;
   }else if(stage==1&&!controller.property("zoomAnimating").toBool()){
    if(!controller.property("magnifierReady").toBool()){QTextStream(stderr)<<"FAIL: target selected during zoom"<<Qt::endl;app.exit(2);return;}
    stage=2;
   }else if(stage==2&&!controller.property("magnifierReady").toBool()){
    controller.clickCommand("CancelTarget");
    QTextStream(stdout)<<"PASS: "<<(gazeInput?"gaze":"pointer hover")<<" dwell started zoom and confirmed the target without mouse clicks"<<Qt::endl;app.quit();
   }
  });
  poll->start();QTimer::singleShot(6000,&app,[&]{QTextStream(stderr)<<"FAIL: dwell target selection timed out"<<Qt::endl;app.exit(2);});
 }
 if(app.arguments().contains("--progressive-zoom-ui-check")){
  // One synthetic screenshot; no screen sharing or desktop capture.
  controller.setProperty("targeting",true);controller.setProperty("capturePending",true);
  controller.setProperty("progressiveZoom",true);controller.setProperty("zoomDurationMs",500);controller.setProperty("magnifierZoom",4);
  QImage frame(QGuiApplication::primaryScreen()->size(),QImage::Format_RGB32);frame.fill(Qt::blue);
  {QPainter painter(&frame);painter.fillRect(QRect(0,0,frame.width()/4,frame.height()/4),Qt::red);}
  QMetaObject::invokeMethod(&controller,"receiveMagnifierFrame",Qt::DirectConnection,Q_ARG(QImage,frame));
  auto poll=new QTimer(&app);poll->setInterval(150);
  QObject::connect(poll,&QTimer::timeout,&app,[&]{
   if(controller.property("zoomAnimating").toBool())return;
   const auto image=target->grabWindow();
   if(image.isNull()||image.pixelColor(int(image.width()*.79),int(image.height()*.79))!=QColor(Qt::red)||controller.property("magnifierRevision").toInt()<2){
    QTextStream(stderr)<<"FAIL: progressive zoom did not render the enlarged source"<<Qt::endl;app.exit(2);return;
   }
   controller.clickCommand("CancelTarget");
   if(target->isVisible()){QTextStream(stderr)<<"FAIL: progressive zoom remained visible after cancel"<<Qt::endl;app.exit(2);return;}
   QTextStream(stdout)<<"PASS: progressive zoom rendered the enlarged source and closed on cancel"<<Qt::endl;app.quit();
  });
  poll->start();
 }
 if(app.arguments().contains("--dwell-startup-check")){
  // Synthetic gaze over a navigation key: no tracker, portal or desktop output.
  controller.setProperty("paused",false);controller.setProperty("dwellMs",250);
  controller.openLayout("main-14x1-bottom",false);
  window->setProperty("gazeEnabled",true);window->setProperty("trackerConnected",true);
  openSettings->trigger();
  if(!settings->isVisible()||controller.property("paused").toBool()||app.quitOnLastWindowClosed()){
   QTextStream(stderr)<<"FAIL: tray Settings action did not open settings with input active and background lifetime"<<Qt::endl;return 2;
  }
  if(window->findChild<QQuickWindow*>("controlWindow")){QTextStream(stderr)<<"FAIL: separate controls window still exists"<<Qt::endl;return 2;}
  auto poll=new QTimer(&app);poll->setInterval(25);
  QObject::connect(poll,&QTimer::timeout,&app,[&,started=false]()mutable{
   if(!settings->isActive()){
    if(started){QTextStream(stderr)<<"FAIL: settings lost focus during startup dwell check"<<Qt::endl;app.exit(2);}
    return;
   }
   started=true;
   if(controller.property("layoutName").toString()=="main-14x1-top"){
    QTextStream(stdout)<<"PASS: gaze dwell activated navigation with settings focused at startup"<<Qt::endl;app.quit();return;
   }
   const auto layout=controller.property("layout").toMap();
   const auto key=layout["keys"].toList().front().toMap();
   const double x=window->property("logicalX").toDouble()+(key["col"].toDouble()+key["width"].toDouble()/2)*window->width()/layout["cols"].toDouble();
   const double y=window->property("logicalY").toDouble()+(key["row"].toDouble()+key["height"].toDouble()/2)*window->height()/layout["rows"].toDouble();
   emit controller.gazePoint(x/window->property("screenWidth").toDouble(),y/window->property("screenHeight").toDouble(),true);
  });
  poll->start();QTimer::singleShot(4000,&app,[&]{QTextStream(stderr)<<"FAIL: startup dwell timed out"<<Qt::endl;app.exit(2);});
 }
 if(app.arguments().contains("--dwell-minimize-check")){
  controller.setRestoreCorner(0);controller.setRestoreWidth(40);controller.setRestoreHeight(128);
  controller.setProperty("paused",false);controller.setProperty("dwellMs",250);
  controller.openLayout("main-14x1-bottom",false);window->setProperty("gazeEnabled",true);window->setProperty("trackerConnected",true);
  auto poll=new QTimer(&app);poll->setInterval(25);
  QObject::connect(poll,&QTimer::timeout,&app,[&,stage=0,corner=0]()mutable{
   const QStringList expected{"minimized-off-screen-bottom","main-14x1-bottom","minimized-off-screen-top","main-14x1-top"};
   if(controller.property("layoutName").toString().startsWith("minimized-off-screen-")){
#ifdef GAZE_GRID_LINUX
    const int expectedX=corner%2?window->property("screenWidth").toInt()-window->width():0;
    const int expectedY=corner>=2?window->property("screenHeight").toInt()-window->height():0;
    if(window->property("logicalX").toInt()!=expectedX||window->property("logicalY").toInt()!=expectedY){
     QTextStream(stderr)<<"FAIL: restore button is not flush with its screen corner"<<Qt::endl;app.exit(2);return;
    }
#else
    const int screenWidth=window->property("screenWidth").toInt();
    const int screenHeight=window->property("screenHeight").toInt();
    const bool top=controller.property("layoutName").toString().endsWith("-top");
    const int expectedWidth=qMax(1,int(screenWidth*.2));
    const int expectedHeight=qMax(1,int(screenHeight*.2));
    const int expectedY=(top?0:screenHeight-expectedHeight)+int(screenHeight*(top?-.1995:.1995));
    if(window->width()!=expectedWidth||window->height()!=expectedHeight
       ||window->property("logicalX").toInt()!=(screenWidth-expectedWidth)/2
       ||window->property("logicalY").toInt()!=expectedY
       ||window->property("minimizedRecovery").toBool()){
     QTextStream(stderr)<<"FAIL: original off-screen minimized layout was overridden"<<Qt::endl;app.exit(2);return;
    }
#endif
   }
   if(controller.property("layoutName").toString()==expected[stage]){
    if(++stage==4){
     if(++corner==4){QTextStream(stdout)<<"PASS: gaze minimizes and restores both keyboard edges across recovery preferences"<<Qt::endl;app.quit();return;}
     stage=0;controller.setRestoreCorner(corner);controller.setRestoreWidth(corner%2?128:40);controller.setRestoreHeight(corner%2?40:128);controller.openLayout("main-14x1-bottom",false);
    }
    if(stage==2)controller.openLayout("main-14x1-top",false);
   }
   const auto layout=controller.property("layout").toMap();
   const auto keys=layout["keys"].toList();
   const auto key=keys[stage%2?0:keys.size()-1].toMap();
   const double x=window->property("logicalX").toDouble()+(key["col"].toDouble()+key["width"].toDouble()/2)*window->width()/layout["cols"].toDouble();
   const double keyTop=window->property("logicalY").toDouble()+key["row"].toDouble()*window->height()/layout["rows"].toDouble();
   const double keyBottom=keyTop+key["height"].toDouble()*window->height()/layout["rows"].toDouble();
   const double y=(qMax(0.0,keyTop)+qMin(window->property("screenHeight").toDouble(),keyBottom))/2;
   const double nx=x/window->property("screenWidth").toDouble(),ny=y/window->property("screenHeight").toDouble();
   if(nx<0||nx>1||ny<0||ny>1){QTextStream(stderr)<<"FAIL: recovery key is outside the display"<<Qt::endl;app.exit(2);return;}
   emit controller.gazePoint(nx,ny,true);
  });
  poll->start();QTimer::singleShot(15000,&app,[&]{QTextStream(stderr)<<"FAIL: minimize/restore dwell timed out"<<Qt::endl;app.exit(2);});
 }
 if(app.arguments().contains("--gaze")||controller.isGazeEnabled())controller.setGazeEnabled(true);
 if(app.arguments().contains("--gaze-check")){
  // Exercise the real worker, controller freshness filter and QML signal handler,
  // without authorizing desktop output or allowing dwell actions.
  controller.setProperty("paused",true);
  window->setProperty("gazeCheck",true);
  window->setProperty("gazeEnabled",true);
  auto counts=std::make_shared<QJsonObject>(QJsonObject{{"signals",0},{"controllerValid",0},{"qmlValid",0},{"overKeyboard",0}});
  QObject::connect(&controller,&Controller::gazePoint,&app,[&,counts](double x,double y,bool valid){
   auto increment=[&](const char *key){(*counts)[key]=(*counts)[key].toInt()+1;};
   increment("signals");if(!valid)return;increment("controllerValid");
   if(window->property("gazeValid").toBool())increment("qmlValid");
   double px=x*window->property("screenWidth").toDouble()-window->property("logicalX").toDouble();
   double py=y*window->property("screenHeight").toDouble()-window->property("logicalY").toDouble();
   if(px>=0&&py>=0&&px<window->width()&&py<window->height())increment("overKeyboard");
  });
  controller.setGazeEnabled(true);
  QTimer::singleShot(20000,&app,[&,counts]{
   (*counts)["tobiiStatus"]=controller.property("tobiiStatus").toString();
   (*counts)["screenWidth"]=window->property("screenWidth").toInt();
   (*counts)["screenHeight"]=window->property("screenHeight").toInt();
   QTextStream(stdout)<<QJsonDocument(*counts).toJson()<<Qt::endl;
   app.exit((*counts)["qmlValid"].toInt()>0?0:2);
  });
 }
 QProcess receiver;QByteArray receiverBuffer;QString received;bool sent=false,active=false,clickStage=false;
 std::unique_ptr<PointerCheck> pointerHarness;
 if(pointerCheck){pointerHarness=std::make_unique<PointerCheck>(&controller,magnifierCheck);pointerHarness->start();}
 if(inputCheck){
  QObject::connect(controller.output(),&PortalInput::status,&app,[](QString message){QTextStream(stdout)<<message<<Qt::endl;});
  QObject::connect(controller.output(),&PortalInput::readyChanged,&app,[&]{
   if(controller.ready()&&receiver.state()==QProcess::NotRunning){receiver.setProcessChannelMode(QProcess::SeparateChannels);receiver.start(QCoreApplication::applicationFilePath(),{"--input-target"});}
  });
  QObject::connect(&receiver,&QProcess::readyReadStandardOutput,&app,[&]{
   receiverBuffer+=receiver.readAllStandardOutput();
   while(receiverBuffer.contains('\n')){
    auto end=receiverBuffer.indexOf('\n');auto line=receiverBuffer.left(end);receiverBuffer.remove(0,end+1);
    auto obj=QJsonDocument::fromJson(line).object();
    if(obj.contains("active")){
     active=obj["active"].toBool();
     if(!active&&sent){
      if(!controller.property("paused").toBool())controller.togglePause();
      QTextStream(stdout)<<"Input check stopped: disposable receiver lost focus"<<Qt::endl;app.exit(7);return;
     }
     if(active&&!sent)QTimer::singleShot(300,&app,[&]{
      if(!active||sent||!controller.ready())return;sent=true;
      // Send only after the disposable receiver reports keyboard focus.
      auto layout=KeyboardLayout::fromXml(KeyboardPack::load(keyboardPackDirectory()+"/keyboard-alphabet-14x4-bottom.xml"));
      QVariantList sequence;
      for(auto letter:QString("gaze-grid")){
       if(letter==QLatin1Char('-')){sequence<<QVariantMap{{"tag","Text"},{"text","-"}};continue;}
       bool found=false;
       for(auto v:layout["keys"].toList()){auto key=v.toMap();if(key["label"].toString()==QString(letter)){sequence+=key["commands"].toList();found=true;break;}}
       if(!found){app.exit(4);return;}
      }
      sequence<<QVariantMap{{"tag","Wait"},{"text","50"}}<<QVariantMap{{"tag","Text"},{"text","\r"}};
      if(!controller.executeCommands(sequence))app.exit(4);
     });
    }
    if(obj.contains("text")){
     received+=obj["text"].toString();
     if(obj["key"].toInt()==Qt::Key_Return){
      bool passed=received=="gaze-grid\r";
      QTextStream(stdout)<<(passed?"PASS: imported XML actions delivered gaze-grid + Enter to separate Wayland receiver":"FAIL: unexpected input sequence")<<Qt::endl;
      if(!passed){app.exit(5);return;}
      if(!controller.output()->absoluteAvailable()){QTextStream(stdout)<<"Click/focus check unavailable: no unique absolute EIS region"<<Qt::endl;app.exit(6);return;}
      clickStage=true;
      QTimer::singleShot(100,&app,[&]{
       if(!active||!controller.ready()){app.exit(7);return;}
       auto layout=controller.property("layout").toMap();
       for(auto value:layout["keys"].toList()){
        auto key=value.toMap();if(key["label"]!="q")continue;
        double px=window->property("logicalX").toDouble()+(key["col"].toDouble()+key["width"].toDouble()/2)*window->width()/layout["cols"].toDouble();
        double py=window->property("logicalY").toDouble()+(key["row"].toDouble()+key["height"].toDouble()/2)*window->height()/layout["rows"].toDouble();
        if(!controller.output()->moveAbsolute(px/window->property("screenWidth").toDouble(),py/window->property("screenHeight").toDouble())){app.exit(6);return;}
        QTimer::singleShot(100,&app,[&]{if(!active||!controller.ready()){app.exit(7);return;}if(!controller.output()->click())app.exit(6);});return;
       }
       app.exit(6);
      });
     }else if(clickStage&&obj["text"].toString()=="q"){
      QTextStream(stdout)<<"PASS: clicking the XML q key preserved receiver focus and delivered q"<<Qt::endl;app.exit(0);
     }
    }
   }
  });
  QTimer::singleShot(0,controller.output(),&PortalInput::connectPortal);
  QTimer::singleShot(60000,&app,[&]{QTextStream(stdout)<<"Input check timed out; focus the disposable receiver if it appeared"<<Qt::endl;app.exit(2);});
 }
 if(!signInMode&&args.contains("--settings"))controller.activate("Settings");
 // Diagnostic modes manage their own sessions or must remain input-free.
 bool diagnosticMode=smoke;
 for(const auto &arg:args)if(arg.endsWith("-check"))diagnosticMode=true;
#ifdef _WIN32
 // Only the owning normal-desktop instance announces itself. Secure copies
 // and rejected broker duplicates never send Win+U or an exit notification.
 bool accessibilityAnnounced=false;
 if(!localTestBuild()&&keyboardInstance&&!secureDesktop&&WindowsSignIn::registered()){
  auto announce=[&]{
   if(!accessibilityAnnounced&&WindowsDesktop::inputDesktopActive())
    accessibilityAnnounced=WindowsSignIn::notify(true);
  };
  announce();
  QObject::connect(desktopMonitor.get(),&WindowsDesktop::Monitor::activeChanged,&app,[announce](bool active){if(active)announce();});
  QObject::connect(&app,&QCoreApplication::aboutToQuit,&app,[&]{
   controller.setDesktopActive(false);
   if(accessibilityAnnounced&&WindowsDesktop::inputDesktopActive())WindowsSignIn::notify(false);
  });
 }
#endif
 if(!diagnosticMode&&
#ifdef _WIN32
    true
#else
    controller.property("rememberDesktop").toBool()
#endif
 )
  QTimer::singleShot(0,&controller,&Controller::connectDesktop);
 if(smoke)QTimer::singleShot(1500,&app,[&]{
  auto output=qEnvironmentVariable("GAZE_GRID_SCREENSHOT");
  auto captureWindow=args.contains("--settings")?window->findChild<QQuickWindow*>("settingsWindow"):window;
  if(!output.isEmpty()&&(!captureWindow||!captureWindow->grabWindow().save(output))){QTextStream(stderr)<<"Screenshot failed"<<Qt::endl;app.exit(3);return;}
  app.quit();
 });
 int result=app.exec();
 if(receiver.state()!=QProcess::NotRunning){receiver.terminate();if(!receiver.waitForFinished(1500)){receiver.kill();receiver.waitForFinished();}}
 return result;
}
