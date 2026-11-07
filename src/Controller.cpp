#ifdef _WIN32
#include "WindowsSignIn.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <cmath>
#include <algorithm>
#include "Controller.h"
#include "DwellGroups.h"
#include "KeyboardPack.h"
#include "KeyboardLayout.h"
#include "KeyNames.h"
#include "ActionCatalog.h"
#include "GazeClock.h"
#include "WordPrediction.h"
#include "PredictionLanguage.h"
#include "ShortcutMappings.h"
#include "MagnifierGeometry.h"
#include <QGuiApplication>
#include <QScreen>
#include <QWindow>
#include <QRegion>
#include "ProfileStore.h"
#include <QTimer>
#include <QSettings>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
#include "AppPaths.h"
#include <QTextBoundaryFinder>
namespace {
bool bypassPlatformMappings(bool literal){
#ifdef _WIN32
 return true;
#else
 return literal;
#endif
}
QVariantMap command(QString tag,QString value){return {{"tag",tag},{"text",value}};}
QString configFile(const QString &name){auto dir=appConfigDirectory();QDir().mkpath(dir);return QDir(dir).filePath(name);}
QString learnedFile(const QString &language){return configFile(language=="eng"?QString("learned.json"):"learned-"+language+".json");}
QString panelPlacementReason(const QVariantMap &layout){

 if(layout.value("verticaloffset","0").toString().remove('%').toDouble()!=0||layout.value("horizontaloffset","0").toString().remove('%').toDouble()!=0)
  return "Moving the keyboard off screen is unavailable in Plasma keyboard mode. Use standalone mode for this layout.";
 return {};
}
}
Controller::Controller(bool signIn,const QJsonObject &signInProfile,TobiiInput *tracker):tobii(tracker?*tracker:ownedTracker),signInMode(signIn){
 if(localTestBuild())gameController.output=[](int,int,int){return false;};
 sensitiveInput=signInMode;
 zoomTimer.setInterval(33);connect(&zoomTimer,&QTimer::timeout,this,&Controller::updateMagnifierZoom);
 connect(&speech,&SpeechOutput::status,this,[this](QString message){actionStatus=message;emit changed();});
 clock.start();auto profile=signInMode?ProfileStore::Loaded{ProfileStore::valid(signInProfile)?signInProfile:ProfileStore::defaults(),{}}:ProfileStore(configFile("profile.json")).load();
 if(signInMode){profile.values["predictionMode"]="off";profile.values["learning"]=false;}
 runner.physicalText=!signInMode;
 localIme=!signInMode;
 wantedImeLocale=profile.values["localImeLocale"].toString();
 connect(this,&Controller::layoutChanged,this,&Controller::refreshLocalIme);
 connect(&runner,&ActionRunner::cancelled,this,[this]{localInput.cancelComposition();refreshLocalIme();});
 runner.canEditLocal=[this]{return scratchpadEditable()&&localInput.attached();};
 runner.sendLocalText=[this](QString text,QStringList held){
  if(systemKeyboard()&&localInput.locale().startsWith("ko_")&&runner.capsLock&&!runner.shifted())text=text.toLower();
  return desktopActive&&!paused&&(!sensitiveInput||signInMode)&&localInput.text(text,held,systemKeyboard()&&!signInMode);
 };
 runner.sendLocalKey=[this](QString name,QStringList held){return desktopActive&&!paused&&(!sensitiveInput||signInMode)&&localInput.key(name,held,systemKeyboard()&&!signInMode);};
 runner.localInputError=[this]{return localInput.error();};
 connect(this,&Controller::changed,this,&Controller::refreshKeyboardLabels);
 connect(this,&Controller::layoutChanged,this,&Controller::refreshKeyboardLabels);
 systemLayout.refresh();
 runner.physicalKeyText=[this](QString name){
  if(name=="SPACE")return QString(" ");
  if(name=="TAB")return QString("\t");
  if(name=="RETURN")return QString("\n");
  return systemLayout.label(KeyNames::linuxCode(name),{runner.shifted(),systemCaps,runner.isHeld("RMENU")});
 };
 runner.physicalTextReady=[this]{return systemLayout.available()&&portal.ready();};
#ifndef _WIN32
 connect(&portal,&PortalInput::keyboardKeymapChanged,this,[this](const QByteArray &source){
  if(systemLayout.setKeymap(source)){
   if(systemKeyboard()){runner.cancel();actionStatus=systemLayout.available()?"System keyboard enabled":"System keyboard layout unavailable";}
   updateSuggestions();emit changed();
  }
 });
 connect(&portal,&PortalInput::keyboardModifiersChanged,this,[this](uint depressed,uint latched,uint locked,uint group){
  const bool wasAvailable=systemLayout.available();
  if(!systemLayout.setModifiers(depressed,latched,locked,group))return;
  if(systemKeyboard()){
   dwell.reset();
   if(wasAvailable&&!systemLayout.available()){runner.cancel();endButtonHold();clearOverlap();}
   if(wasAvailable!=systemLayout.available())actionStatus=systemLayout.available()?"System keyboard enabled":"System keyboard layout unavailable";
   emit changed();
  }
 });
#endif
 auto layoutTimer=new QTimer(this);layoutTimer->setInterval(250);
 connect(layoutTimer,&QTimer::timeout,this,[this]{
  if(!desktopActive||!systemLayout.refresh())return;
  if(systemKeyboard()){
  dwell.reset();
  if(!systemLayout.available()){runner.cancel();endButtonHold();clearOverlap();actionStatus="System keyboard layout unavailable";}
  else if(actionStatus=="System keyboard layout unavailable")actionStatus="System keyboard enabled";
  }
  emit changed();
 });
 layoutTimer->start();
 speechProfiles=profile.values.value("speechProfiles").toArray().toVariantList();
 if(speechProfiles.isEmpty())speechProfiles=ProfileStore::defaults()["speechProfiles"].toArray().toVariantList();
 speechProfileIndex=profile.values["activeSpeechProfile"].toInt(0);
 setProcessPriority(profile.values["processPriority"].toInt(0));
 gazeEnabled=profile.values["gazeEnabled"].toBool(true);
 typedWordMode=profile.values["typedWordSuggestionMode"].toString("fallback");
 predictionModeValue=profile.values["predictionMode"].toString("context");
 predictionUnicodeModeValue=profile.values["predictionUnicodeMode"].toString("nonAscii");
 nextWordEnabled=profile.values["predictNextWord"].toBool(true);
 completionSpaceEnabled=profile.values["addSpaceAfterCompletion"].toBool(true);
 removeCompletionSpaceEnabled=profile.values["removeCompletionSpaceBeforePunctuation"].toBool(true);
 sentenceStartersEnabled=profile.values["suggestSentenceStarters"].toBool(true);
 latchStyle=profile.values.value("latchStyle").toString("outline");
 lockStyle=profile.values.value("lockStyle").toString("fill");
 dwellIndicatorStyle=profile.values.value("dwellIndicatorStyle").toString("bar");
 groupDwellValues=profile.values["groupDwellTimes"].toObject().toVariantMap();
 // Retain older profiles while bringing their slider values within the new limit.
 for(auto it=groupDwellValues.begin();it!=groupDwellValues.end();++it)it.value()=qMin(it.value().toInt(),2500);
 dwellMs=qMin(profile.values["dwellMs"].toInt(),2500);literal=profile.values["literal"].toBool();learning=profile.values["learning"].toBool();profileStatus=profile.message;
 transitionDurationMs=profile.values["transitionDurationMs"].toInt(180);
 keyboardTransition=profile.values["keyboardTransition"].toString("instant");
 scalePercent=profile.values["keyboardScale"].toInt(100);keyboardTheme=profile.values["highContrast"].toBool()?QStringLiteral("high-contrast"):profile.values["keyboardTheme"].toString("system");
 shortLabelScale=profile.values.value("shortLabelScale").toInt(profile.values.value("enlargeShortLabels").toBool(true)?150:100);
 longLabelScale=profile.values["longLabelScale"].toInt(100);
 predictionTextScale=profile.values["predictionTextScale"].toInt(100);
 scratchpadTextScale=profile.values["scratchpadTextScale"].toInt(100);
 magnifierEnabled=profile.values.value("magnifierEnabled").toBool(false);
 clickDurationMs=profile.values["clickDurationMs"].toInt(0);
 magneticMouseRelative=profile.values["magneticMouseRelative"].toBool(false);
#ifndef _WIN32
 magneticMouseRelative=false; // The portal cannot report the global cursor position.
#endif
 showScrollZones=profile.values["showScrollZones"].toBool(false);
 scrollZoneOpacity=profile.values["scrollZoneOpacity"].toInt(30);
 scrollZoneColor=profile.values["scrollZoneColor"].toString("#00bfff");
 clampScrollGaze=profile.values["clampScrollGaze"].toBool(true);cancelScrollOffscreen=profile.values["cancelScrollOffscreen"].toBool(false);
 scrollSpeed=profile.values["scrollSpeed"].toInt(100);scrollDeadzone=profile.values["scrollDeadzone"].toInt(50);scrollAxis=profile.values["scrollAxis"].toInt(0);
 magnifierRefreshHz=profile.values["magnifierRefreshHz"].toInt(30);
 magnifierZoom=profile.values["magnifierZoom"].toInt(4);
 progressiveZoom=profile.values.value("progressiveZoom").toBool(profile.values.value("liveZoom").toBool(false));
 zoomDurationMs=profile.values.value("zoomDurationMs").toInt(1500);
 targetDwellEnabled=profile.values.value("targetDwellEnabled").toBool(true);
 targetStartDwellMs=qMin(profile.values.value("targetStartDwellMs").toInt(750),2500);
 targetConfirmDwellMs=qMin(profile.values.value("targetConfirmDwellMs").toInt(1000),2500);
 targetProgressStyle=profile.values.value("targetProgressStyle").toString("ring");
 targetProgressSize=profile.values.value("targetProgressSize").toInt(56);
 targetProgressThickness=profile.values.value("targetProgressThickness").toInt(5);
 targetProgressOpacity=profile.values.value("targetProgressOpacity").toInt(100);
 targetProgressColor=profile.values.value("targetProgressColor").toString("#00bfff");
 targetDwellRadius=profile.values.value("targetDwellRadius").toInt(40);
 restoreOpacity=profile.values.value("restoreOpacity").toInt(100);
 recoveryCorner=profile.values["restoreCorner"].toInt(2);
 recoveryWidth=profile.values.value("restoreWidth").toInt(profile.values.value("restoreSize").toInt(64));
 recoveryHeight=profile.values.value("restoreHeight").toInt(profile.values.value("restoreSize").toInt(64));
 rememberDesktop=profile.values["rememberDesktop"].toBool();
 gazeIndicatorSize=profile.values["gazeIndicatorSize"].toInt(18);
 gazeIndicatorOpacity=profile.values["gazeIndicatorOpacity"].toInt(100);
 gazeIndicatorColor=profile.values["gazeIndicatorColor"].toString("#00bfff");
 repeatIntervalMs=profile.values["repeatIntervalMs"].toInt(750);
 if(profile.values.contains("keyCycleModes"))setKeyCycleModes(profile.values["keyCycleModes"].toVariant().toStringList());
 const auto savedKeyMode=profile.values["keyMode"].toString("Tap");
 if(keyCycleModesValue.contains(savedKeyMode))keyModeValue=savedKeyMode;
 buttonModeValue=profile.values["buttonMode"].toString("Tap");
 setCacheKeyboards(profile.values["cacheKeyboards"].toBool(false));
 preserveTogglesBetweenLayouts=profile.values["preserveTogglesBetweenLayouts"].toBool();
 cycleModifiersOnHover=profile.values["cycleModifiersOnHover"].toBool();
 hideKeyboardDuringMouseActions=profile.values["hideKeyboardDuringMouseActions"].toBool();
 gazeIndicatorMorph=profile.values["gazeIndicatorMorph"].toBool();
 filterType=profile.values["gazeFilterType"].toInt(0);
 filterGain=profile.values["gazeFilterGain"].toDouble(0.2);
 saccadeDistance=profile.values["gazeSaccadeDistance"].toDouble(0.1);
 smoothingLevel=profile.values["gazeSmoothingLevel"].toInt(1);smoothTargetChanges=profile.values["smoothWhenChangingGazeTarget"].toBool();
 dwellGraceValue=profile.values["dwellGraceMs"].toInt(0);
 showPausedDwellProgress=profile.values["showPausedDwellProgress"].toBool(true);
 gazeToleranceMs=profile.values["invalidGazeToleranceMs"].toInt(500);
 clampGazeForKeyboard=profile.values["clampGazeForKeyboard"].toBool(false);
 clampGazeForPointSelection=profile.values["clampGazeForPointSelection"].toBool(true);
 layoutNames=QDir(keyboardPackDirectory()).entryList({"*.xml"},QDir::Files,QDir::Name);for(auto &name:layoutNames)name.chop(4);
 for(const auto &name:layoutNames){
  auto item=KeyboardLayout::fromXml(KeyboardPack::load(QDir(keyboardPackDirectory()).filePath(name+".xml")));
  if(!item.contains("error")&&item["hidefromkeyboardmenu"].toString().compare("true",Qt::CaseInsensitive)!=0)menuLayoutNames<<name;
 }
 if(!signInMode){
 predictionLanguageValue=profile.values["predictionLanguage"].toString("eng");
 if(!WordPrediction::languages().contains(predictionLanguageValue))predictionLanguageValue="eng";
 dictionary=WordPrediction::dictionary(predictionLanguageValue);
 WordPrediction::prepare(dictionary,predictionLanguageValue);
 QFile learned(learnedFile(predictionLanguageValue));if(learned.open(QIODevice::ReadOnly)){auto obj=QJsonDocument::fromJson(learned.readAll()).object();for(auto it=obj.begin();it!=obj.end();++it)frequencies[it.key()]=qBound(0,it.value().toInt(),1000000);}
 }
#ifndef _WIN32
 QString mappingError;QFile mappings(configFile("linux-mappings.json"));
 if(mappings.exists()){
  if(mappings.open(QIODevice::ReadOnly))mappingProfile=ShortcutMappings::parse(mappings.readAll(),mappingError);
  else mappingError="Could not read saved mappings";
 }
 if(mappingProfile.isEmpty()){
  QFile defaults(":/resources/linux-mappings.json");QString defaultError;
  if(defaults.open(QIODevice::ReadOnly))mappingProfile=ShortcutMappings::parse(defaults.readAll(),defaultError);
  else defaultError="Could not read default mappings";
  if(!defaultError.isEmpty())actionStatus=defaultError;
 }
 if(!mappingError.isEmpty())profileStatus+=(profileStatus.isEmpty()?QString():QString(" "))+mappingError+". Using default mappings; saved file was preserved.";
#endif
 pointer.move=[this](double x,double y){return desktopActive&&portal.moveAbsolute(x,y);};
 pointer.moveRelative=[this](double x,double y){return desktopActive&&portal.move(x,y);};
 pointer.moveRelativeTo=[this](double x,double y){return desktopActive&&portal.moveRelativeTo(x,y);};
 pointer.button=[this](int code,bool down){return (!down||desktopActive)&&portal.button(code,down);};
 pointer.scroll=[this](double amount){return desktopActive&&portal.scroll(amount);};
 pointer.scrollAxes=[this](double x,double y){return desktopActive&&portal.scrollAxes(x,y);};
 pointer.releaseAll=[this]{portal.releaseButtons();};
 connect(&pointer,&PointerActions::changed,this,[this]{targeting=pointer.targeting();if(!targeting)clearCapture();emit changed();});
 connect(&screenshot,&ScreenshotPortal::failed,this,[this](QString error){
  clearCapture();runner.cancel();endButtonHold();gameController.reset();activeJoystick=-1;pointer.cancel();actionStatus=error;emit changed();
 });
 connect(&screenshot,&ScreenshotPortal::liveFrame,this,&Controller::receiveLiveMagnifierFrame);
 connect(&screenshot,&ScreenshotPortal::captured,this,[this](QImage image){
  receiveMagnifierFrame(image);
  if(magnifierReady&&magnifierRefreshHz>0)screenshot.startLive(magnifierRefreshHz);
 });
 connect(&pointer,&PointerActions::completed,this,[this](bool ok){actionStatus=pointer.prompt();runner.resume(ok);emit changed();});
 runner.sendKey=[this](QString name,bool down){return (!down||desktopActive)&&portal.key(KeyNames::linuxCode(name),down);};
#ifdef _WIN32
 runner.sendText=[this](QString text){return desktopActive&&!paused&&!sensitiveInput&&portal.text(text);};
 runner.canCommitText=[this]{return desktopActive&&!paused&&!sensitiveInput&&portal.ready();};
#endif
 runner.supportsPhysicalKey=[this](QString){return desktopActive&&portal.ready()&&(!systemKeyboard()||systemLayout.available());};
 connect(&gameController,&GameController::changed,this,&Controller::changed);
 connect(&gameController,&GameController::failed,this,[this](QString error){activeJoystick=-1;actionStatus=error;emit changed();});
 runner.performAction=[this](QString action){return performAction(action);};
 runner.speakLabel=[this](QString label){if(desktopActive&&!sensitiveInput)speech.speak(label);};
 runner.supportsAction=ActionCatalog::supported;
 runner.navigate=[this](QString name,bool back){if(desktopActive)openLayout(name,back);};
 connect(&runner,&ActionRunner::failed,this,[this](QString m){endButtonHold();gameController.reset();activeJoystick=-1;actionStatus=m.contains("Gamepad")&&!gameController.error().isEmpty()?gameController.error():m;emit changed();});
 connect(&runner,&ActionRunner::stateChanged,this,&Controller::changed);
 connect(&runner,&ActionRunner::cancelled,this,&Controller::clearOverlap);
 connect(&runner,&ActionRunner::failed,this,[this]{clearOverlap();});
 connect(&runner,&ActionRunner::finished,this,&Controller::dispatchOverlap,Qt::QueuedConnection);
 connect(&runner,&ActionRunner::finished,this,[this]{
  if(externalContextPending){externalContextPending=false;updateDesktopTextContext(desktopContextProvider?desktopContextProvider():pendingExternalContext);}
 },Qt::QueuedConnection);
 connect(&runner,&ActionRunner::textCommitted,this,&Controller::updateText);
 runner.punctuationSpacing=[this](QString text){return takeCompletionSpace(text);};
 connect(&runner,&ActionRunner::punctuationPrepared,this,[this](QString text){
  if(text.endsWith(' '))pendingCompletionText=text;
 });
 connect(&runner,&ActionRunner::keyPressed,this,[this](QString name){
  if(name=="CAPITAL")systemCaps=!systemCaps;
  if(name!="LSHIFT"&&name!="RSHIFT"&&name!="CAPITAL")completionSpaceContext.clear();
 });
 connect(&runner,&ActionRunner::failed,this,[this]{pendingCompletionText.clear();completionSpaceContext.clear();});
 connect(&runner,&ActionRunner::cancelled,this,[this]{pendingCompletionText.clear();});
 connect(&portal,&PortalInput::status,this,[this](QString m){portalStatus=m;emit changed();});
 connect(&portal,&PortalInput::readyChanged,this,[this]{dwell.reset();if(!ready()){runner.cancel();endButtonHold();gameController.reset();activeJoystick=-1;pointer.cancel();}emit changed();});
 trackerRetryTimer.setSingleShot(true);
 connect(&trackerRetryTimer,&QTimer::timeout,this,&Controller::connectTobii);
 auto timer=new QTimer(this);timer->setInterval(25);connect(timer,&QTimer::timeout,this,[this]{
  if(!overlapDwells.isEmpty()&&clock.elapsed()-lastHoldHover>200)clearOverlap();
  if(!buttonHoldTarget.isEmpty()&&clock.elapsed()-lastHoldHover>200){endButtonHold();dwell.reset();}
  if(!heldDwellTarget.isEmpty()&&clock.elapsed()-lastHoldHover>200){runner.endDwellHold();heldDwellTarget.clear();dwell.reset();}
  if(gazeAge.isValid()&&gazeLossStarted)receiveGaze(0,0,false,0);
 });timer->start();
 auto start=profile.values["startLayout"].toString();
 if(!layoutNames.contains(start)){start=ProfileStore::defaults()["startLayout"].toString();profileStatus="Saved keyboard is unavailable. Opened the default keyboard.";}
 startupLayout=ProfileStore::defaults()["startLayout"].toString();
 if(!signInMode&&start!=startupLayout&&!menuLayoutNames.contains(start)){
  start=startupLayout;profileStatus="Saved startup keyboard is hidden. Opened the default keyboard.";
 }
 openLayout(start,false);
}
Controller::~Controller(){desktopActive=false;stopTracker();runner.cancel();endButtonHold();gameController.reset();activeJoystick=-1;pointer.cancel();portal.releaseAll();portal.close();
 if(signInMode)return;
 QJsonObject obj;for(auto it=frequencies.begin();it!=frequencies.end();++it)obj[it.key()]=it.value();QSaveFile file(learnedFile(predictionLanguageValue));if(file.open(QIODevice::WriteOnly)){file.write(QJsonDocument(obj).toJson());file.commit();}
}
void Controller::connectDesktop(){
 if(localTestBuild()){actionStatus="Local test: desktop output disabled";emit changed();return;}
 desktopOutputRequested=true;
 if(!desktopActive)return;
 portal.setRememberPermission(rememberDesktop);portal.connectPortal();
}
void Controller::setDesktopActive(bool active){
 if(desktopActive==active)return;
 if(!active)localInput.cancelComposition();
 desktopActive=active;
 if(!active){
  clearCapture();clearOverlap();runner.cancel();endButtonHold();heldDwellTarget.clear();
  gameController.reset();activeJoystick=-1;pointer.cancel();portal.releaseAll();portal.close();speech.stop();
  dwell.reset();pointDwell.reset();canvasDwell.reset();resetCanvasPlacement();
  pendingCompletionText.clear();completionSpaceContext.clear();stopTracker();
  tobiiStatus="Desktop inactive; eye tracking suspended";
  actionStatus="Desktop inactive; input suspended";
 }else{
  // Windows may reject key-up events after the input desktop changes. Retry
  // retained releases on the active desktop before allowing new output.
  portal.releaseAll();
  if(desktopOutputRequested)connectDesktop();
  if(gazeEnabled)connectTobii();
  else tobiiStatus="Gaze input is off";
  actionStatus="Desktop active";
 }
 emit changed();
}
void Controller::stopTracker(){
 trackerRetryTimer.stop();++trackerGeneration;
 disconnect(&tobii,nullptr,this,nullptr);
 if(tobii.isRunning())tobii.requestInterruption();
 tobii.wait();
 gazeAge.invalidate();gazeSmoothing.reset();gazeLossStarted=0;
 if(trackerConnected){trackerConnected=false;emit trackerConnectionChanged();}
 emit gazePoint(0,0,false);
}
void Controller::connectTobii(){
 if(localTestBuild()){tobiiStatus="Local test: tracker disabled";emit changed();return;}
 if(!desktopActive||tobii.isRunning())return;
 trackerRetryTimer.stop();disconnect(&tobii,nullptr,this,nullptr);
 const auto generation=++trackerGeneration;
 // Queued samples/status from an interrupted worker must not reach a newly
 // resumed desktop or replace the new worker's connection state.
 connect(&tobii,&TobiiInput::status,this,[this,generation](QString message){
  if(generation!=trackerGeneration||!desktopActive)return;
  tobiiStatus=message;emit changed();
 });
 connect(&tobii,&TobiiInput::gaze,this,[this,generation](double x,double y,bool valid,qint64 sampledAt){
  if(generation==trackerGeneration&&desktopActive)receiveGaze(x,y,valid,sampledAt);
 });
 connect(&tobii,&TobiiInput::connectionChanged,this,[this,generation](bool connected){
  if(generation!=trackerGeneration||!desktopActive)return;
  if(connected){trackerRetryDelayMs=1000;trackerRetryTimer.stop();}
  setTrackerConnected(connected);
 });
 connect(&tobii,&QThread::finished,this,[this,generation]{
  if(generation!=trackerGeneration)return;
  setTrackerConnected(false);scheduleTrackerRetry();
 });
 tobiiStatus="Connecting to eye tracker";emit changed();tobii.start();
}
void Controller::scheduleTrackerRetry(){
 if(!gazeEnabled||!desktopActive||tobii.isRunning()||trackerRetryTimer.isActive())return;
 const auto delay=trackerRetryDelayMs;
 trackerRetryDelayMs=qMin(trackerRetryDelayMs*2,30000);
 tobiiStatus=QString("Retrying eye tracker in %1 s. %2").arg((delay+999)/1000).arg(tobiiStatus);
 trackerRetryTimer.start(delay);emit changed();
}
void Controller::configureInputMethod(std::function<bool(QString)> text,std::function<bool(QString,bool)> key){
 nativeMode=true;sensitiveInput=true;runner.physicalText=false;
 runner.sendText=[this,text=std::move(text)](QString value){return desktopActive&&nativeActive&&text(value);};
 runner.canCommitText=[this]{return desktopActive&&nativeActive;};
 runner.sendKey=[this,key=std::move(key)](QString name,bool down){return (!down||desktopActive)&&(portal.ready()?portal.key(KeyNames::linuxCode(name),down):(!systemKeyboard()&&key(name,down)));};
 runner.supportsPhysicalKey=[this](QString){return desktopActive&&portal.ready()&&(!systemKeyboard()||systemLayout.available());};
 openLayout("keyboard-alphabet-14x4-bottom",false);
 actionStatus="Native text input enabled; authorize desktop control for pointer actions and held shortcuts";emit changed();
}
void Controller::invalidateInputContext(){
 externalContext={};externalContextObserved=false;externalContextPending=false;
 pendingCompletionText.clear();completionSpaceContext.clear();
 runner.cancel();endButtonHold();gameController.reset();activeJoystick=-1;pointer.cancel();portal.releaseAll();dwell.reset();speech.stop();scratchpad.clear();predictionContext.clear();suggestions.clear();emit changed();
}
void Controller::updateDesktopTextContext(const DesktopTextContext &context){
 if(signInMode)return;
 if(runner.busy()){pendingExternalContext=context;externalContextPending=true;return;}
 if(externalContextObserved&&externalContext==context)return;
 const bool changedField=!externalContextObserved||externalContext.identity!=context.identity;
 externalContextObserved=true;externalContext=context;
 if(context.available)predictionContext=context.before;
 else if(changedField||context.supported||context.privateField)predictionContext.clear();
 pendingCompletionText.clear();
 if(changedField||!context.available||!PredictionLanguage::atLineEnd(context.after)||completionSpaceContext!=context.before)completionSpaceContext.clear();
 if(!hasScratchpad())updateSuggestions();
}
void Controller::inputContextChanged(bool active,bool sensitive){
 nativeActive=active;sensitiveInput=signInMode||sensitive;
 if(!active||sensitive)invalidateInputContext();
 else updateSuggestions();
 actionStatus=active?(sensitive?"Native input ready — private field":"Native text input ready"):"Select a text field to use native input";emit changed();
}
void Controller::setKeyCycleModes(const QStringList &modes){
 QStringList selected;
 for(const auto &mode:QStringList{"Tap","Repeat","Hold","Toggle"})
  if(modes.contains(mode))selected.append(mode);
 if(selected.isEmpty()||selected==keyCycleModesValue)return;
 keyCycleModesValue=selected;
 if(!selected.contains(keyModeValue)){
  clearOverlap();runner.endDwellHold();heldDwellTarget.clear();runner.releaseToggledKeys();
  keyModeValue=selected.first();
  actionStatus="Key mode: "+keyModeValue;
 }
 emit changed();
}

void Controller::refreshKeyboardLabels(){
 const SystemKeyboard::Modifiers modifiers{runner.shifted(),systemCaps,runner.isHeld("RMENU")};
 const bool active=systemKeyboard()&&!runner.textOnly&&systemLayout.available();
 auto labels=SystemKeyboard::labels(layout["keys"].toList(),modifiers,[this,active](int scan,SystemKeyboard::Modifiers state){
  if(localImeActive()&&localInput.locale().startsWith("ko_")){
   const auto name=KeyNames::fromLinuxCode(scan);
   if(name.size()==1&&name.front().isLetter())return localInput.inputLabel(name.toLower(),state.shift);
  }
  return active?systemLayout.label(scan,state):QString();
 });
 if(labels!=keyboardLabelValues){keyboardLabelValues=labels;emit keyboardLabelsChanged();}
}
bool Controller::savePreferences(){
 if(signInMode)return false;
 QJsonObject values{{"cacheKeyboards",cacheKeyboards()},{"transitionDurationMs",transitionDurationMs},{"keyboardTransition",keyboardTransition},{"clickDurationMs",clickDurationMs},{"clampScrollGaze",clampScrollGaze},{"cancelScrollOffscreen",cancelScrollOffscreen},{"showScrollZones",showScrollZones},{"scrollZoneOpacity",scrollZoneOpacity},{"scrollZoneColor",scrollZoneColor},{"keyCycleModes",QJsonArray::fromStringList(keyCycleModesValue)},{"magneticMouseRelative",magneticMouseRelative},{"scrollSpeed",scrollSpeed},{"scrollDeadzone",scrollDeadzone},{"scrollAxis",scrollAxis},{"gazeEnabled",gazeEnabled},{"repeatIntervalMs",repeatIntervalMs},{"preserveTogglesBetweenLayouts",preserveTogglesBetweenLayouts},{"cycleModifiersOnHover",cycleModifiersOnHover},{"hideKeyboardDuringMouseActions",hideKeyboardDuringMouseActions},{"gazeIndicatorSize",gazeIndicatorSize},{"gazeIndicatorOpacity",gazeIndicatorOpacity},{"gazeIndicatorColor",gazeIndicatorColor},{"gazeIndicatorMorph",gazeIndicatorMorph},{"version",1},{"dwellMs",dwellMs},{"latchStyle",latchStyle},{"lockStyle",lockStyle},{"dwellIndicatorStyle",dwellIndicatorStyle},{"literal",literal},{"learning",learning},{"typedWordSuggestionMode",typedWordMode},{"predictionMode",predictionModeValue},{"predictNextWord",nextWordEnabled},{"addSpaceAfterCompletion",completionSpaceEnabled},{"suggestSentenceStarters",sentenceStartersEnabled},{"startLayout",startupLayout},{"keyboardScale",scalePercent},{"keyboardTheme",keyboardTheme},{"shortLabelScale",shortLabelScale},{"longLabelScale",longLabelScale},{"magnifierRefreshHz",magnifierRefreshHz},{"magnifierZoom",magnifierZoom},{"magnifierEnabled",magnifierEnabled},{"progressiveZoom",progressiveZoom},{"zoomDurationMs",zoomDurationMs},{"targetDwellEnabled",targetDwellEnabled},{"targetStartDwellMs",targetStartDwellMs},{"targetConfirmDwellMs",targetConfirmDwellMs},{"targetProgressStyle",targetProgressStyle},{"targetProgressSize",targetProgressSize},{"targetProgressThickness",targetProgressThickness},{"targetProgressOpacity",targetProgressOpacity},{"targetProgressColor",targetProgressColor},{"targetDwellRadius",targetDwellRadius},{"rememberDesktop",rememberDesktop},{"gazeFilterType",filterType},{"gazeFilterGain",filterGain},{"gazeSaccadeDistance",saccadeDistance},{"gazeSmoothingLevel",smoothingLevel},{"smoothWhenChangingGazeTarget",smoothTargetChanges},{"restoreCorner",recoveryCorner},{"restoreWidth",recoveryWidth},{"restoreHeight",recoveryHeight},{"restoreOpacity",restoreOpacity}};
 values["keyMode"]=keyModeValue;
 values["buttonMode"]=buttonModeValue;
 values["systemKeyboard"]=systemKeyboard();
 values["localImeLocale"]=wantedImeLocale;
 values["predictionLanguage"]=predictionLanguageValue;
 values["predictionUnicodeMode"]=predictionUnicodeModeValue;
 values["groupDwellTimes"]=QJsonObject::fromVariantMap(groupDwellValues);
 values["speechProfiles"]=QJsonArray::fromVariantList(speechProfiles);
 values["activeSpeechProfile"]=speechProfileIndex;
 values["processPriority"]=processPriorityValue;
 values["invalidGazeToleranceMs"]=gazeToleranceMs;
 values["dwellGraceMs"]=dwellGraceValue;
 values["showPausedDwellProgress"]=showPausedDwellProgress;
 values["predictionTextScale"]=predictionTextScale;
 values["scratchpadTextScale"]=scratchpadTextScale;
 values["removeCompletionSpaceBeforePunctuation"]=removeCompletionSpaceEnabled;
 values["clampGazeForKeyboard"]=clampGazeForKeyboard;
 values["clampGazeForPointSelection"]=clampGazeForPointSelection;
 QString error;bool ok=ProfileStore(configFile("profile.json")).save(values,error);
 profileStatus=ok?QString():error;emit changed();return ok;
}
bool Controller::cacheKeyboards()const{return KeyboardPack::cacheEnabled();}
void Controller::setCacheKeyboards(bool enabled){if(enabled==cacheKeyboards())return;KeyboardPack::setCacheEnabled(enabled);emit changed();}
void Controller::resetPreferences(){
 localInput.cancelComposition();
 wantedImeLocale.clear();
 runner.physicalText=!signInMode;systemCaps=false;
 pendingCompletionText.clear();completionSpaceContext.clear();
 if(signInMode)return;
 QString error;auto values=ProfileStore::defaults();
 if(!ProfileStore(configFile("profile.json")).save(values,error,true)){profileStatus=error;emit changed();return;}
 if(!paused)togglePause();
 speechProfiles=values["speechProfiles"].toArray().toVariantList();speechProfileIndex=0;emit speechProfilesChanged();
 setProcessPriority(0);
 setMagnifierEnabled(false);
 setGazeEnabled(values["gazeEnabled"].toBool());
 setInvalidGazeToleranceMs(500);
 setDwellGraceMs(0);
 showPausedDwellProgress=true;
 setRemoveCompletionSpaceBeforePunctuation(true);
 predictionTextScale=values["predictionTextScale"].toInt();
 scratchpadTextScale=values["scratchpadTextScale"].toInt();
 clampGazeForKeyboard=false;clampGazeForPointSelection=true;
 latchStyle="outline";lockStyle="fill";
 dwellIndicatorStyle="bar";
 groupDwellValues.clear();
 setPredictionLanguage("eng");
 predictionUnicodeModeValue="nonAscii";
 dwellMs=values["dwellMs"].toInt();literal=false;learning=false;predictionModeValue="context";typedWordMode="fallback";nextWordEnabled=true;completionSpaceEnabled=true;sentenceStartersEnabled=true;backStack.clear();
 repeatIntervalMs=750;
 setKeyCycleModes({"Tap","Repeat","Hold","Toggle"});
 keyModeValue="Tap";buttonModeValue="Tap";
 setCacheKeyboards(false);
 preserveTogglesBetweenLayouts=false;
 cycleModifiersOnHover=false;
 hideKeyboardDuringMouseActions=false;
 clickDurationMs=0;magneticMouseRelative=false;
 scrollSpeed=100;scrollDeadzone=50;scrollAxis=0;clampScrollGaze=true;cancelScrollOffscreen=false;
 showScrollZones=false;scrollZoneOpacity=30;scrollZoneColor="#00bfff";
 gazeIndicatorSize=18;gazeIndicatorOpacity=100;gazeIndicatorColor="#00bfff";gazeIndicatorMorph=false;
 setRestoreCorner(2);setRestoreWidth(64);setRestoreHeight(64);restoreOpacity=100;
 setKeyboardScale(100);keyboardTheme="system";keyboardTransition="instant";transitionDurationMs=180;shortLabelScale=values["shortLabelScale"].toInt();longLabelScale=values["longLabelScale"].toInt();magnifierZoom=values["magnifierZoom"].toInt();setMagnifierRefreshHz(30);progressiveZoom=false;zoomDurationMs=1500;targetDwellEnabled=true;targetStartDwellMs=750;targetConfirmDwellMs=1000;targetDwellRadius=40;targetProgressStyle="ring";targetProgressSize=56;targetProgressThickness=5;targetProgressOpacity=100;targetProgressColor="#00bfff";rememberDesktop=false;
 setGazeFilterType(values["gazeFilterType"].toInt(0));
 setGazeFilterGain(values["gazeFilterGain"].toDouble(0.2));
 setGazeSaccadeDistance(values["gazeSaccadeDistance"].toDouble(0.1));
 setGazeSmoothingLevel(values["gazeSmoothingLevel"].toInt());setSmoothWhenChangingGazeTarget(false);gazeSmoothing.reset();
 openLayout(values["startLayout"].toString(),false);
 profileStatus="Preferences reset. Input remains paused.";emit changed();
}
void Controller::setProcessPriority(int priority){
 if(priority<0||priority>2)return;
#ifdef _WIN32
 const DWORD classes[]={NORMAL_PRIORITY_CLASS,ABOVE_NORMAL_PRIORITY_CLASS,HIGH_PRIORITY_CLASS};
 if(!SetPriorityClass(GetCurrentProcess(),classes[priority])){
  profileStatus=QString("Could not change process priority (Windows error %1).").arg(GetLastError());
  emit changed();return;
 }
#else
 if(priority!=0)return;
#endif
 processPriorityValue=priority;emit changed();
}
void Controller::setRestoreCorner(int corner){
 corner=qBound(0,corner,3);if(corner==recoveryCorner)return;
 recoveryCorner=corner;dwell.reset();emit geometryPreferencesChanged();
}
void Controller::setRestoreWidth(int width){
 width=qBound(40,width,128);if(width==recoveryWidth)return;
 recoveryWidth=width;dwell.reset();emit geometryPreferencesChanged();
}
void Controller::setRestoreHeight(int height){
 height=qBound(40,height,128);if(height==recoveryHeight)return;
 recoveryHeight=height;dwell.reset();emit geometryPreferencesChanged();
}
void Controller::setKeyboardScale(int scale){
 scale=qBound(50,scale,150);if(scale==scalePercent)return;
 runner.cancel();endButtonHold();gameController.reset();activeJoystick=-1;pointer.cancel();dwell.reset();scalePercent=scale;emit geometryPreferencesChanged();emit changed();
}
QString Controller::mappingJson()const{return QString::fromUtf8(QJsonDocument(mappingProfile).toJson(QJsonDocument::Indented));}
bool Controller::saveMappings(QString json){
 if(signInMode)return false;
 QString error;auto profile=ShortcutMappings::parse(json.toUtf8(),error);
 if(!error.isEmpty()){actionStatus=error;emit changed();return false;}
 const auto data=QJsonDocument(profile).toJson();QSaveFile file(configFile("linux-mappings.json"));
 if(!file.open(QIODevice::WriteOnly)||file.write(data)!=data.size()||!file.commit()){actionStatus="Could not save mappings";emit changed();return false;}
 mappingProfile=profile;actionStatus="Platform mappings saved";emit changed();return true;
}
QVariantList Controller::mapped(const QVariantList &commands,bool &ok){
 auto result=ShortcutMappings::resolve(commands,mappingProfile,bypassPlatformMappings(literal));ok=result.allowed;actionStatus=result.message;return result.commands;
}
void Controller::setTrackerConnected(bool connected){
 connected=connected&&desktopActive;
 if(trackerConnected==connected)return;
 trackerConnected=connected;
 if(!connected)receiveGaze(0,0,false,0);
 emit trackerConnectionChanged();
}
void Controller::receiveGaze(double x,double y,bool valid,qint64 /*sampledAt*/){
 if(!desktopActive)return;
 const auto now=GazeClock::now();
 valid=valid&&std::isfinite(x)&&std::isfinite(y);
 if(valid){
  if(gazeLossStarted){
   if(now-gazeLossStarted>=qint64(gazeToleranceMs)*1000){
    receiveGaze(0,0,false,0);
   }else pauseGazeDwell();
   gazeLossStarted=0;emit changed();
  }
  // Screen bounds do not determine tracker validity. Clamp only for the
  // selected use; otherwise preserve valid off-screen coordinates.
  const bool clamp=targeting?clampGazeForPointSelection:clampGazeForKeyboard;
  // Do not carry an off-screen smoothing estimate into a clamped context.
  if(clamp!=lastGazeClamped){gazeSmoothing.reset();lastGazeClamped=clamp;}
  if(clamp){
   x=qBound(0.,x,1.);y=qBound(0.,y,1.);
  }
  if(!gazeAge.isValid())gazeSmoothing.reset();
  gazeAge.restart();
  if(filterType==1){
   const auto point=gazeSmoothing.updateExponential({x,y},filterGain,saccadeDistance);
   x=point.x();y=point.y();
  }else{
  const auto point=gazeSmoothing.update({x*gazeDisplaySize.width(),y*gazeDisplaySize.height()},smoothingLevel,smoothTargetChanges);
  x=point.x()/gazeDisplaySize.width();y=point.y()/gazeDisplaySize.height();
  }
 }else{
  if(gazeAge.isValid()&&gazeEnabled&&trackerConnected&&gazeToleranceMs>0){
   if(!gazeLossStarted){
    gazeLossStarted=now;pauseGazeDwell();
    endButtonHold();runner.endDwellHold();heldDwellTarget.clear();gameController.reset();activeJoystick=-1;portal.releaseButtons();
    // A tolerated dropout must not release physical modifiers behind the
    // runner's back: recovery would leave a latched badge without Ctrl held.
    // Full tracking loss below also preserves latched and locked modifiers.
    // Retain target selection, but stop already-running pointer actions and
    // reset scroll timing so recovery cannot accumulate a scroll jump.
    if((pointer.busy()&&!pointer.targeting())||pointer.magnetic()||pointer.lookScrolling()||!pointer.heldButtons().isEmpty())pointer.trackingLost();
    emit changed();
   }
   if(now-gazeLossStarted<qint64(gazeToleranceMs)*1000){pauseGazeDwell();return;}
  }
  // Cancel gaze work once on tracking loss. Further invalid samples must not
  // continually reset mouse dwell or cancel actions started by the fallback.
  const bool wasValid=gazeAge.isValid();gazeAge.invalidate();gazeSmoothing.reset();
  const bool wasPaused=gazeLossStarted>0;gazeLossStarted=0;
  if(wasValid){dwell.reset();pointDwell.reset();canvasDwell.reset();runner.cancel(true);endButtonHold();gameController.reset();activeJoystick=-1;portal.releaseButtons();pointer.trackingLost();}
  if(wasPaused)emit changed();
 }
 emit gazePoint(x,y,valid);
}
void Controller::setDisplaySize(double width,double height){
 if(!std::isfinite(width)||!std::isfinite(height)||width<=0||height<=0)return;
 const QSizeF size(width,height);if(size!=gazeDisplaySize){gazeDisplaySize=size;gazeSmoothing.reset();}
 pointer.setDisplaySize(size);
}
void Controller::pauseGazeDwell(){
 if(!gazeLossStarted)return;
 const auto now=clock.elapsed();dwell.pause(now);pointDwell.pause(now);canvasDwell.pause(now);
 for(auto &item:overlapDwells)item.pause(now);
 lastHoldHover=now;
}
void Controller::setDwellGraceMs(int value){
 value=qBound(0,value,1000);if(value==dwellGraceValue)return;
 dwellGraceValue=value;dwell.reset();clearOverlap();emit changed();
}
void Controller::setInvalidGazeToleranceMs(int value){
 value=qBound(0,value,1000);if(value==gazeToleranceMs)return;
 gazeToleranceMs=value;
 if(gazeLossStarted)receiveGaze(0,0,false,0);
 emit changed();
}
void Controller::setGazeSmoothingLevel(int level){
 level=qBound(0,level,4);if(level==smoothingLevel)return;
 smoothingLevel=level;gazeSmoothing.reset();dwell.reset();runner.cancel();endButtonHold();gameController.reset();activeJoystick=-1;pointer.cancel();emit changed();
}
void Controller::setSmoothWhenChangingGazeTarget(bool enabled){
 if(enabled==smoothTargetChanges)return;
 smoothTargetChanges=enabled;gazeSmoothing.reset();dwell.reset();runner.cancel();endButtonHold();gameController.reset();activeJoystick=-1;pointer.cancel();emit changed();
}
void Controller::togglePause(){if(!desktopActive)return;localInput.cancelComposition();paused=!paused;dwell.reset();runner.cancel();endButtonHold();gameController.reset();activeJoystick=-1;portal.releaseAll();pointer.cancel();emit changed();}
void Controller::setGazeEnabled(bool enabled){
 const bool preferenceChanged=gazeEnabled!=enabled;
 gazeEnabled=enabled;dwell.reset();gazeSmoothing.reset();
 if(!enabled&&gazeLossStarted)receiveGaze(0,0,false,0);
 if(preferenceChanged&&!signInMode){
  ProfileStore store(configFile("profile.json"));auto saved=store.load().values;
  saved["gazeEnabled"]=enabled;QString error;
  if(!store.save(saved,error))profileStatus=error;
 }
 if(enabled)connectTobii();
 if(!enabled){stopTracker();tobiiStatus="Gaze input is off";runner.cancel();endButtonHold();gameController.reset();activeJoystick=-1;pointer.cancel();}
 if(preferenceChanged)emit gazeEnabledChanged();
 emit changed();
}
void Controller::trackGaze(double x,double y,bool overDesktop){
 if(!desktopActive)return;
 pointer.setMagneticRelative(magneticMouseRelative);
 pointer.configureScroll(scrollSpeed,scrollDeadzone,scrollAxis);
 pointer.setScrollGazePolicy(clampScrollGaze,cancelScrollOffscreen);
 if(gazeEnabled&&!paused&&ready()&&gazeAge.isValid())pointer.follow({x,y},overDesktop);
}
void Controller::displayChanged(){
 if(!paused)togglePause();else{runner.cancel();endButtonHold();gameController.reset();activeJoystick=-1;pointer.cancel();dwell.reset();}
 gazeAge.invalidate();gazeSmoothing.reset();emit gazePoint(0,0,false);
 actionStatus="Display configuration changed. Check tracker calibration and resume when ready.";emit changed();
}
bool Controller::isSleepKey(const QString &key) const{
 if(!key.startsWith("key:"))return false;
 bool valid=false;const int index=key.mid(4).toInt(&valid);
 const auto keys=layout["keys"].toList();
 if(!valid||index<0||index>=keys.size())return false;
 const auto entry=keys[index].toMap();
 if(!entry["unavailableReason"].toString().isEmpty())return false;
 const auto commands=entry["commands"].toList();
 if(commands.size()!=1)return false;
 const auto action=commands.front().toMap();
 return action["tag"]=="Action"&&action["text"].toString().trimmed()=="Sleep";
}
void Controller::endButtonHold(){
 const auto action=buttonHoldAction;buttonHoldAction.clear();buttonHoldTarget.clear();
 if(!action.isEmpty())gameController.perform(action);
}
void Controller::setAllGroupDwellTimes(int milliseconds){
 if(milliseconds<250||milliseconds>2500)return;
 dwellMs=milliseconds;
 for(const auto &group:dwellGroupNames())groupDwellValues[group]=milliseconds;
 dwell.reset();clearOverlap();emit changed();
}
void Controller::setGroupDwellTime(QString group,int milliseconds){
 if(!dwellGroupNames().contains(group)||milliseconds<250||milliseconds>2500)return;
 if(groupDwellValues.value(group).toInt()==milliseconds)return;
 groupDwellValues[group]=milliseconds;dwell.reset();clearOverlap();emit changed();
}
int Controller::dwellDuration(QString target)const{
 if(target.startsWith("suggest:")||target.startsWith("ime:"))return groupDwellValues.value("Predictions",dwellMs).toInt();
 if(target.startsWith("key:")){
  bool ok=false;const int index=target.mid(4).toInt(&ok);const auto keys=layout["keys"].toList();
  if(ok&&index>=0&&index<keys.size()){
   const auto entry=keys[index].toMap();const int custom=entry["dwell"].toInt();
   if(entry["individualDwell"].toBool())return custom>0?custom:dwellMs;
   const auto group=entry["dwellGroup"].toString();
   if(groupDwellValues.contains(group))return groupDwellValues[group].toInt();
   if(custom>0)return custom;
  }
 }
 return dwellMs;
}
double Controller::hover(QString key){
 if(!desktopActive)return 0;
 if(gazeLossStarted){pauseGazeDwell();return dwell.progress(clock.elapsed(),sizeMoveMode?dwellMs:dwellDuration(key),repeatIntervalMs);}
 if(!collectingOverlap)clearOverlap();
 if(sizeMoveMode){
  if(paused||runner.busy()){dwell.inhibit(key,clock.elapsed());return 0;}
  // Editing has its own dwell path: never execute hold, repeat or typing behavior.
  if(dwell.update(key,clock.elapsed(),dwellMs)){
   activate(key);dwell.consume(key,clock.elapsed());
  }
  return dwell.progress(clock.elapsed(),dwellMs);
 }
 if(!collectingOverlap){
  if(key!=buttonHoldTarget||paused)endButtonHold();
  if(key!=heldDwellTarget||paused){runner.endDwellHold();heldDwellTarget.clear();}
 }
 lastHoldHover=clock.elapsed();
 if(key.startsWith("suggest:")){
  auto target=key.mid(8);
  if(target.contains(':')){
   bool valid=false;const auto revision=target.section(':',0,0).toULongLong(&valid);
   if(!valid||revision!=predictionRevision){dwell.reset();return 0;}
   target=target.section(':',1);
  }
  bool valid=false;const int i=target.toInt(&valid);
  if(showingImeCandidates()||!valid||i<0||i>=suggestions.size()||suggestions[i].trimmed().isEmpty()){dwell.reset();return 0;}
 }
 if(key.startsWith("ime:")){int index=0;if(!imeTarget(key,index)){dwell.reset();return 0;}}
 const bool recovery=key=="Pause"||key=="Recover"||key=="CancelTarget"||key=="Connect"||key=="Tobii"||key=="Settings"||key=="Speak"||key=="StopSpeech"||isSleepKey(key);
 if(!recovery&&(paused||runner.busy())){dwell.inhibit(key,clock.elapsed());return 0;}
 int duration=dwellDuration(key);bool repeatKey=false;QVariantList holdCommands;QString buttonHold;
 if(key.startsWith("key:")){
  bool valid=false;const int i=key.mid(4).toInt(&valid);const auto keys=layout["keys"].toList();
  if(valid&&i>=0&&i<keys.size()){
   const auto entry=keys[i].toMap();
   const auto commands=entry["commands"].toList();
   const auto fixedMode=entry["mode"].toString();
   const auto keyMode=fixedMode.isEmpty()?keyModeValue:fixedMode;
   const auto buttonMode=fixedMode.isEmpty()?buttonModeValue:fixedMode;
   if(!runner.textOnly&&entry["unavailableReason"].toString().isEmpty()&&commands.size()==1){
    const auto resolved=ShortcutMappings::resolve(commands,mappingProfile,bypassPlatformMappings(literal));
    if(resolved.allowed&&resolved.commands.size()==1){
     const auto cmd=resolved.commands.front().toMap();
     const auto button=cmd["tag"]=="Action"?GameController::buttonAction(cmd["text"].toString(),"Press"):QString();
     if(!button.isEmpty()){
      repeatKey=buttonMode=="Repeat";
      if(buttonMode=="Hold")buttonHold=button;
     }
    }
   }
   if(ready()&&entry["unavailableReason"].toString().isEmpty()&&commands.size()==1){
    const bool modeKey=!ActionRunner::dwellKey(commands).first.isEmpty();
    if(modeKey&&keyMode=="Hold"&&!runner.textOnly){
     const auto resolved=ShortcutMappings::resolve(commands,mappingProfile,bypassPlatformMappings(literal));
     if(resolved.allowed&&!ActionRunner::dwellKey(resolved.commands).first.isEmpty())holdCommands=resolved.commands;
    }
    repeatKey=repeatKey||(cycleModifiersOnHover&&!ActionRunner::modifierName(commands).isEmpty())
     ||(modeKey&&keyMode=="Repeat");
   }
  }
 }
 if(dwell.update(key,clock.elapsed(),duration,repeatKey,repeatIntervalMs,gazeEnabled&&trackerConnected?dwellGraceValue:0)){
  if(collectingOverlap){
   bool pending=false;for(const auto &entry:overlapPending)if(entry.target==key)pending=true;
   if(!pending)overlapPending.append({key,holdCommands,buttonHold});
  }else if(!buttonHold.isEmpty()){
   if(gameController.perform(buttonHold)){buttonHoldTarget=key;buttonHoldAction=GameController::buttonAction(buttonHold,"Release");}
   else {actionStatus=gameController.error();emit changed();}
  }else if(!holdCommands.isEmpty()){if(runner.beginDwellHold(holdCommands))heldDwellTarget=key;}else activate(key);
  dwell.consume(key,clock.elapsed());
 }
 return dwell.progress(clock.elapsed(),duration,repeatIntervalMs);
}
void Controller::releaseOverlapTarget(const QString &target){
 runner.endDwellHold(target);
 const auto release=overlapButtons.take(target);
 if(!release.isEmpty()&&!overlapButtons.values().contains(release))gameController.perform(release);
}
QVariantMap Controller::pausedDwellProgress()const{
 QVariantMap result;
 if(!showPausedDwellProgress||!desktopActive||paused||sizeMoveMode||targeting||!gazeEnabled||!trackerConnected)return result;
 const auto now=clock.elapsed();
 auto collect=[&](const Dwell &timer){
  const auto saved=timer.savedElapsed(now,dwellGraceValue);
  for(auto it=saved.cbegin();it!=saved.cend();++it)
   result[it.key()]=qBound(0.0,double(it.value())/qMax(1,dwellDuration(it.key())),1.0);
 };
 collect(dwell);
 for(auto it=overlapDwells.cbegin();it!=overlapDwells.cend();++it)collect(it.value());
 return result;
}
void Controller::clearOverlap(){
 overlapPending.clear();
 const auto targets=overlapDwells.keys();overlapDwells.clear();
 for(const auto &target:targets)releaseOverlapTarget(target);
}
QVariantMap Controller::hoverKeys(const QStringList &targets){
 if(!desktopActive)return {};
 if(layout["layoutMode"]!="canvas"||!layout["activateoverlappingkeys"].toBool()||sizeMoveMode){
  const auto target=targets.value(0);return {{target,hover(target)}};
 }
 const auto keys=layout["keys"].toList();QStringList selected;
 for(const auto &target:targets){
  bool ok=false;const int i=target.mid(4).toInt(&ok);
  if(target.startsWith("key:")&&ok&&i>=0&&i<keys.size()&&keys[i].toMap()["kind"]=="DynamicKey"&&!selected.contains(target))selected<<target;
 }
 // Pause/resume must remain usable during a macro and must never resume input
 // accidentally by activating a second overlapping Sleep key.
 for(const auto &target:selected){
  const auto commands=keys[target.mid(4).toInt()].toMap()["commands"].toList();
  const auto command=commands.size()==1?commands.front().toMap():QVariantMap();
  const auto action=command["text"].toString().trimmed();
  if(isSleepKey(target)||isSizeMoveKey(target)||(command["tag"]=="Action"&&(action=="CycleKeyMode"||action=="CycleButtonMode")))
   return {{target,hover(target)}};
 }
 dwell.reset();
 if(paused||targeting){clearOverlap();return {};}
 endButtonHold();
 if(!heldDwellTarget.isEmpty()){runner.endDwellHold();heldDwellTarget.clear();}
 const auto previous=overlapDwells.keys();
 for(const auto &target:previous)if(!selected.contains(target)){
  releaseOverlapTarget(target);
  auto &timer=overlapDwells[target];
  timer.update("",clock.elapsed(),dwellDuration(target),false,-1,gazeEnabled&&trackerConnected?dwellGraceValue:0);
  if(!timer.hasSavedProgress())overlapDwells.remove(target);
 }
 for(auto it=overlapPending.begin();it!=overlapPending.end();)if(!selected.contains(it->target))it=overlapPending.erase(it);else ++it;
 std::sort(selected.begin(),selected.end(),[&](const QString &a,const QString &b){
  const int ai=a.mid(4).toInt(),bi=b.mid(4).toInt();
  const bool am=!ActionRunner::modifierName(keys[ai].toMap()["commands"].toList()).isEmpty();
  const bool bm=!ActionRunner::modifierName(keys[bi].toMap()["commands"].toList()).isEmpty();
  return am!=bm?am:ai<bi;
 });
 QVariantMap progress;collectingOverlap=true;
 for(const auto &target:selected){
  dwell=overlapDwells.value(target);
  progress[target]=hover(target);
  overlapDwells[target]=dwell;
 }
 collectingOverlap=false;dwell.reset();lastHoldHover=clock.elapsed();
 dispatchOverlap();return progress;
}
void Controller::dispatchOverlap(){
 if(!desktopActive){clearOverlap();return;}
 if(gazeLossStarted)return;
 if(paused||sizeMoveMode||targeting){clearOverlap();return;}
 while(!runner.busy()&&!overlapPending.isEmpty()){
  const auto next=overlapPending.takeFirst();
  if(!overlapDwells.contains(next.target))continue;
  if(!next.buttonHold.isEmpty()){
   const auto release=GameController::buttonAction(next.buttonHold,"Release");
   if(overlapButtons.values().contains(release)||gameController.perform(next.buttonHold))overlapButtons[next.target]=release;
   else {actionStatus=gameController.error();clearOverlap();emit changed();return;}
  }else if(!next.holdCommands.isEmpty()){
   if(!runner.beginDwellHold(next.holdCommands,next.target)){clearOverlap();return;}
  }else{
   dispatchingOverlap=true;activate(next.target);dispatchingOverlap=false;
  }
 }
}
void Controller::clickCommand(QString key){
 if(!desktopActive)return;
 activate(key);dwell.consume(key,clock.elapsed());
 if(!sizeMoveMode&&layout["layoutMode"]=="canvas"&&layout["activateoverlappingkeys"].toBool()&&key.startsWith("key:"))
  overlapDwells[key].consume(key,clock.elapsed());
}
void Controller::activate(QString key){
 if(!desktopActive)return;
 if(!dispatchingOverlap)clearOverlap();
 if(sizeMoveMode&&(key.startsWith("key:")||key.startsWith("suggest:")||key.startsWith("ime:"))&&!isSizeMoveKey(key)){
  if(paused)return;
  bool ok=false;const int index=key.mid(4).toInt(&ok);
  if(key.startsWith("key:")&&ok&&index>=0&&index<layout["keys"].toList().size()){
   resetCanvasPlacement();sizeMoveSelection=index;emit changed();
  }
  return;
 }
 if(!dispatchingOverlap){endButtonHold();runner.endDwellHold();heldDwellTarget.clear();}
 if(key=="MoveModeToggle"||key=="SizeModeToggle"||key=="CycleKeyMode"||key=="CycleButtonMode"){performAction(key);return;}
 if(key=="Settings"){if(!signInMode)emit settingsRequested();return;}
 if(key=="Speak"){speak();return;}
 if(key=="StopSpeech"){stopSpeech();return;}
 if(key=="Pause"||isSleepKey(key)){togglePause();return;}
 if(key=="Recover"){recover();return;}
 if(key=="Connect"){connectDesktop();return;}
 if(key=="Tobii"){connectTobii();return;}
 if(key=="CancelTarget"){targeting=false;runner.cancel();endButtonHold();gameController.reset();activeJoystick=-1;portal.releaseAll();pointer.cancel();emit changed();return;}
 if(paused)return;
 if(key.startsWith("ime:")){
  int index=0;if(runner.busy()||!imeTarget(key,index))return;
  if(!runner.heldKeys().isEmpty()){actionStatus="Release held keys before selecting a candidate";emit changed();return;}
  const auto expected=imeCandidateSnapshot[index];
  if(!localInput.selectCandidate(index,expected)){actionStatus=localInput.error();emit changed();}
  refreshLocalIme();return;
 }
 if(key.startsWith("suggest:")){
  auto target=key.mid(8);
  if(target.contains(':')){
   bool ok=false;const auto revision=target.section(':',0,0).toULongLong(&ok);
   if(!ok||revision!=predictionRevision)return;
   target=target.section(':',1);
  }
  bool valid=false;int i=target.toInt(&valid);if(!valid||i<0||i>=suggestions.size()||suggestions[i].trimmed().isEmpty()||(!runner.textOnly&&!ready()))return;
  if(!runner.heldKeys().isEmpty()){actionStatus="Release held keys before accepting a suggestion";emit changed();return;}
  if(showingImeCandidates())return;
  if(!hasScratchpad()&&desktopContextProvider){
   const auto current=desktopContextProvider();
   if(current!=externalContext){updateDesktopTextContext(current);return;}
  }
  const auto word=suggestions[i];
  if(!hasScratchpad()&&desktopCompletionProvider&&externalContext.available){
   if(runner.busy())return;
   const auto expected=externalContext;
   const auto edit=PredictionLanguage::edit(expected.before,expected.after,word,predictionLanguageValue,completionSpaceEnabled);
   const bool accepted=edit.valid&&desktopCompletionProvider(expected,word,predictionLanguageValue,completionSpaceEnabled);
   if(accepted&&learning&&!externalContext.privateField&&!WordPrediction::characterLanguage(predictionLanguageValue))
    for(const auto &token:PredictionLanguage::tokens(word,predictionLanguageValue))frequencies[token]=qMin(1000000,frequencies.value(token)+1);
   if(desktopContextProvider)updateDesktopTextContext(desktopContextProvider());
   if(accepted&&edit.text.endsWith(' ')&&PredictionLanguage::atLineEnd(expected.after.mid(edit.removeAfter)))
    completionSpaceContext=expected.before.left(expected.before.size()-edit.removeBefore)+edit.text;
   return;
  }
  if(scratchpadEditable()&&localInput.attached()){
   QString document;int cursor=0;
   if(!localInput.predictionContext(document,cursor))return;
   if(runner.busy())return;
   if(!localInput.complete(word,predictionLanguageValue,completionSpaceEnabled,document,cursor))actionStatus=localInput.error();
   else if(cursor==document.size()&&localInput.atEnd()&&scratchpad.endsWith(' '))completionSpaceContext=scratchpad;
   updateSuggestions();return;
  }
  const auto edit=PredictionLanguage::edit(predictionText(),!hasScratchpad()&&externalContext.available?externalContext.after:QString(),word,predictionLanguageValue,completionSpaceEnabled);
  if(!edit.valid)return;
  const auto recase=predictionText().right(edit.removeBefore);
  int removePrefix=0,removeSuffix=0;QTextBoundaryFinder boundaries(QTextBoundaryFinder::Grapheme,recase);
  while(boundaries.toNextBoundary()>0)++removePrefix;
  QTextBoundaryFinder suffixBoundaries(QTextBoundaryFinder::Grapheme,externalContext.after.left(edit.removeAfter));
  while(suffixBoundaries.toNextBoundary()>0)++removeSuffix;
  const auto text=edit.text;const bool space=text.endsWith(' ');
  if(text.isEmpty())return;
  auto completion=command("Text",text);completion["literalCompletion"]=!nativeMode;completion["alwaysUnicode"]=predictionUnicodeModeValue=="always";completion["removePrefix"]=removePrefix;completion["removeSuffix"]=removeSuffix;
  if(runner.start({completion}))pendingCompletionText=space?QString(removePrefix,'\b')+text:QString();
  return;
 }
 if(!key.startsWith("key:"))return;
 bool valid=false;int i=key.mid(4).toInt(&valid);auto keys=layout["keys"].toList();if(!valid||i<0||i>=keys.size())return;
 const auto unavailable=keys[i].toMap()["unavailableReason"].toString();
 if(!unavailable.isEmpty()){actionStatus=unavailable;emit changed();return;}
 const auto fixedMode=keys[i].toMap()["mode"].toString();
 const auto keyMode=fixedMode.isEmpty()?keyModeValue:fixedMode;
 const auto buttonMode=fixedMode.isEmpty()?buttonModeValue:fixedMode;
 auto commands=keys[i].toMap()["commands"].toList();bool ok=false;commands=mapped(commands,ok);
 if(!ok){emit changed();return;}
 // Navigation, sleep, and scratchpad clearing work without a portal session.
 bool needsOutput=false;for(auto v:commands){auto c=v.toMap();auto tag=c["tag"].toString(),value=c["text"].toString().trimmed();if(tag!="ChangeKeyboard"&&tag!="Wait"&&!(tag=="Action"&&(value=="MoveModeToggle"||value=="SizeModeToggle"||value=="CycleButtonMode"||value=="CycleKeyMode"||value=="BackFromKeyboard"||value=="MenuKeyboard"||value=="Sleep"||value=="ClearScratchpad"||value=="Speak"||value=="SpeakLabel"||value=="NextSpeechProfile"||value=="PreviousSpeechProfile"||value=="NextSuggestions"||value=="PreviousSuggestions"||GameController::supports(value))))needsOutput=true;}
 if(needsOutput&&!runner.textOnly&&!ready()){actionStatus="Authorize desktop control before sending input";emit changed();return;}
 if(!runner.textOnly&&commands.size()==1){
  auto cmd=commands.front().toMap();
  const auto button=cmd["tag"]=="Action"?GameController::buttonAction(cmd["text"].toString(),buttonMode=="Toggle"?"Toggle":"Tap"):QString();
  if(!button.isEmpty()){cmd["text"]=button;commands={cmd};}
 }
 if(keyMode=="Toggle"&&!runner.textOnly&&!ActionRunner::dwellKey(commands).first.isEmpty())
  runner.toggleDwellKey(commands);
 else runner.start(commands);
 emit changed();
}
void Controller::openLayout(QString name,bool backReturnsHere){
 if(name.endsWith(".xml"))name.chop(4);
 if(!layoutNames.contains(name)){actionStatus="Unknown keyboard: "+name;emit changed();return;}
 const auto sourcePath=QDir(keyboardPackDirectory()).filePath(name+".xml");
 QByteArray sourceBytes;
 auto compiled=KeyboardLayout::fromXml(KeyboardPack::load(sourcePath,&sourceBytes));
 if(compiled.contains("error")){actionStatus=compiled["error"].toString();emit changed();return;}
 if(nativeMode){
  const auto reason=panelPlacementReason(compiled);
  if(!reason.isEmpty()){actionStatus=reason;emit changed();return;}
  auto keys=compiled["keys"].toList();
  for(auto &value:keys){
   auto key=value.toMap();
   for(const auto &entry:key["commands"].toList()){
    const auto command=entry.toMap();if(command["tag"]!="ChangeKeyboard")continue;
    auto destination=command["text"].toString().trimmed();if(destination.endsWith(".xml"))destination.chop(4);
    if(!layoutNames.contains(destination))continue;
    const auto target=KeyboardLayout::fromXml(KeyboardPack::load(QDir(keyboardPackDirectory()).filePath(destination+".xml")));
    if(target.contains("error"))continue;
    const auto unavailable=panelPlacementReason(target);
    if(!unavailable.isEmpty()){key["unavailableReason"]=unavailable;break;}
   }
   value=key;
  }
  compiled["keys"]=keys;
 }
 const bool textOnly=compiled["simulatekeystrokes"].toString().compare("false",Qt::CaseInsensitive)==0;
 const bool preserve=preserveTogglesBetweenLayouts&&!textOnly&&!runner.textOnly;
 localInput.cancelComposition();runner.cancel(textOnly==runner.textOnly,preserve);endButtonHold();gameController.reset(preserve);activeJoystick=-1;runner.textOnly=textOnly;runner.capsLock=false;pointer.cancel();dwell.reset();
 if(backReturnsHere&&!layoutName.isEmpty()&&layoutName!=name){backStack<<layoutName;if(backStack.size()>64)backStack.removeFirst();}
 canvasSourcePath=sourcePath;canvasSource=sourceBytes;
 saveCanvasChanges=!signInMode&&compiled["persistkeygeometry"].toString().compare("true",Qt::CaseInsensitive)==0;canvasSaveStatus.clear();
 // Toolbars and other hidden layouts must not replace the startup keyboard.
 // The built-in default is allowed even though its XML hides it from the menu.
 if(menuLayoutNames.contains(name)||name==ProfileStore::defaults()["startLayout"].toString())startupLayout=name;
 layoutTransitionDirection=backReturnsHere?1:-1;
 resetCanvasPlacement();canvasEditMode.clear();sizeMoveMode=false;layout=compiled;layoutName=name;applySpeechProfile();updateSuggestions();emit layoutChanged();emit changed();
}
void Controller::recover(){if(!desktopActive)return;paused=false;backStack.clear();openLayout("main-14x1-bottom",false);}
void Controller::joystick(int index,double x,double y,bool active){
 if(!desktopActive)active=false;
 const auto keys=layout["keys"].toList();
 if(!active||sizeMoveMode||paused||runner.textOnly||index<0||index>=keys.size()){
  if(activeJoystick>=0&&activeJoystick<keys.size())gameController.stick(keys[activeJoystick].toMap()["style"].toMap().value("stick","Left").toString(),0,0);
  activeJoystick=-1;return;
 }
 const auto entry=keys[index].toMap();if(entry["kind"]!="Joystick")return;
 if(activeJoystick>=0&&activeJoystick!=index)joystick(-1,0,0,false);
 activeJoystick=index;
 if(!gameController.stick(entry["style"].toMap().value("stick","Left").toString(),x,y)){
  if(actionStatus!=gameController.error()){actionStatus=gameController.error();emit changed();}
 }
}
bool Controller::performAction(QString action){
 if(!desktopActive)return false;
 completionSpaceContext.clear();
 if(action=="MoveModeToggle"||action=="SizeModeToggle"){
  if(layout["layoutMode"]!="canvas"){actionStatus="Size/move mode requires a Canvas layout";emit changed();return true;}
  const QString mode=action=="MoveModeToggle"?"move":"size";
  clearOverlap();
  canvasEditMode=canvasEditMode==mode?QString():mode;sizeMoveMode=!canvasEditMode.isEmpty();resetCanvasPlacement();
  runner.endDwellHold();runner.releaseToggledKeys();heldDwellTarget.clear();endButtonHold();
  gameController.reset();activeJoystick=-1;
  // Keep the activating key consumed until the pointer leaves it.
  actionStatus=sizeMoveMode?"Dwell on a key, then dwell at its new "+QString(canvasEditMode=="move"?"center":"bottom-right corner"):"Canvas editing off";
  emit changed();return true;
 }

 if(action=="CycleButtonMode"){
  clearOverlap();
  endButtonHold();gameController.reset();activeJoystick=-1;
  buttonModeValue=buttonModeValue=="Tap"?"Repeat":buttonModeValue=="Repeat"?"Hold":buttonModeValue=="Hold"?"Toggle":"Tap";
  actionStatus="Button mode: "+buttonModeValue;emit changed();return true;
 }
 if(action=="CycleKeyMode"){
  clearOverlap();
  runner.endDwellHold();heldDwellTarget.clear();
  runner.releaseToggledKeys();
  keyModeValue=keyCycleModesValue[(keyCycleModesValue.indexOf(keyModeValue)+1)%keyCycleModesValue.size()];
  actionStatus="Key mode: "+keyModeValue;emit changed();return true;
 }
 if(GameController::supports(action))return runner.textOnly?true:gameController.perform(action);
 if(action=="MouseMagnifier"){
  setMagnifierEnabled(!magnifierEnabled);actionStatus=magnifierEnabled?"Magnifier enabled: target selection will request desktop capture":"Magnifier disabled";emit changed();return true;
 }
 if(action=="Speak"){speak();return true;}
 if(action=="NextSpeechProfile"||action=="PreviousSpeechProfile"){
  const int count=speechProfiles.size();
  if(count>0)setActiveSpeechProfile((speechProfileIndex+(action=="NextSpeechProfile"?1:count-1))%count);
  return true;
 }
 if(action=="BackMany"){
  if(!runner.textOnly)return false;
  if(scratchpadEditable()&&localInput.attached())return localInput.key("BACK",{"LCONTROL"},systemKeyboard()&&!signInMode);
  auto &text=typingContext();
  int end=text.size();while(end>0&&text[end-1].isSpace())--end;
  while(end>0&&!text[end-1].isSpace())--end;
  text.truncate(end);updateSuggestions();return true;
 }
 if(action=="NextSuggestions"||action=="PreviousSuggestions"){
  if(sensitiveInput){suggestions.clear();emit changed();return true;}
  int pageSize=4;for(const auto &v:layout["keys"].toList()){auto k=v.toMap();if(k["kind"]=="SuggestionRow")pageSize=qMax(1,k.value("suggestionCount",k["width"].toInt()/2).toInt());}
  if(showingImeCandidates()){
   const int localPageSize=suggestionPageSize();
   const int next=imeCandidateOffset+(action=="NextSuggestions"?localPageSize:-localPageSize);
   if(next>=0&&next<imeCandidateSnapshot.size()){imeCandidateOffset=next;++imeRevision;emit changed();}
   return true;
  }
  const auto all=predictionSuggestions(1000);
  int next=suggestionOffset+(action=="NextSuggestions"?pageSize:-pageSize);
  if(next>=0&&next<all.size())suggestionOffset=next;
  suggestions=all.mid(suggestionOffset,pageSize);++predictionRevision;emit changed();return true;
 }
 if(action=="ClearScratchpad"){
  if(scratchpadEditable()&&localInput.attached()&&!localInput.clear())return false;
  scratchpad.clear();predictionContext.clear();updateSuggestions();return true;
 }
 if(action=="Sleep"){togglePause();return true;}
 if(action=="MenuKeyboard"){recover();return true;}
 if(action=="BackFromKeyboard"){openLayout(backStack.isEmpty()?"main-14x1-bottom":backStack.takeLast(),false);return true;}
 if(PointerActions::supports(action)){
  if(action=="MouseMagneticCursor"&&magneticMouseRelative&&!portal.relativeTargetAvailable()){
   actionStatus="Relative magnetic mouse requires cursor-position feedback. Select Absolute movement on this desktop.";emit changed();return false;
  }
  if(PointerActions::needsAbsolute(action)&&!(action=="MouseMagneticCursor"&&magneticMouseRelative)&&!portal.absoluteAvailable()){
   actionStatus="Pointer targeting requires an input region mapped to the primary display";emit changed();return false;
  }
  pointer.setClickDuration(clickDurationMs);
  runner.suspend();
  if(!pointer.start(action,hideKeyboardDuringMouseActions&&action!="MouseMagneticCursor"?100:0)){actionStatus="Pointer action unavailable: release held buttons or complete an action before repeating";emit changed();return false;}
  return true;
 }
 actionStatus="Not implemented yet: "+action;emit changed();return false;
}
void Controller::targetAt(double x,double y){
 if(!desktopActive)return;
 if(gazeLossStarted)return;
 if(paused||!ready()||!targeting||capturePending||zoomAnimating||!MagnifierGeometry::validPoint({x,y}))return;
 if(magnifierReady){
  const auto point=MagnifierGeometry::map(magnifierCrop,captureSize,{x,y});clearCapture();pointer.select(point);emit changed();return;
 }
 if(magnifierEnabled){
  pointDwell.reset();coarseTarget={x,y};capturePending=true;const auto epoch=++captureEpoch;emit changed();
  QTimer::singleShot(150,this,[this,epoch]{if(epoch==captureEpoch&&capturePending&&!paused&&targeting)screenshot.capture();});return;
 }
 pointer.select(QPointF(x,y));
}
double Controller::dwellAt(double x,double y,bool valid){
 if(!desktopActive)return 0;
 if(gazeLossStarted){pauseGazeDwell();return pointDwell.progress(clock.elapsed(),magnifierReady?targetConfirmDwellMs:targetStartDwellMs);}
 if(!valid||!targetDwellEnabled||!targeting||paused||!ready()||capturePending||zoomAnimating||!MagnifierGeometry::validPoint({x,y})){
  pointDwell.reset();return 0;
 }
 const int duration=magnifierReady?targetConfirmDwellMs:targetStartDwellMs;
 if(pointDwell.update({x*gazeDisplaySize.width(),y*gazeDisplaySize.height()},clock.elapsed(),duration,targetDwellRadius)){
  pointDwell.reset();targetAt(x,y);return 0;
 }
 return pointDwell.progress(clock.elapsed(),duration);
}
void Controller::receiveLiveMagnifierFrame(QImage image){
 if(!targeting||paused||!magnifierReady||magnifierRefreshHz==0)return;
 if(image.size()!=captureSize){clearCapture();pointer.cancel();actionStatus="Display size changed; target selection cancelled";emit changed();return;}
 capturedImage=std::move(image);updateMagnifierZoom();
}
void Controller::receiveMagnifierFrame(QImage image){
 if(!targeting||paused||!capturePending)return;
 auto screen=QGuiApplication::primaryScreen();
 // Both capture backends now return only the primary display.
 if(image.isNull()||!screen||qAbs(double(image.width())/image.height()-double(screen->size().width())/screen->size().height())>.01){
  clearCapture();runner.cancel();endButtonHold();gameController.reset();activeJoystick=-1;pointer.cancel();actionStatus="Capture does not match the target display; target selection cancelled";emit changed();return;
 }
 capturedImage=image;captureSize=image.size();capturePending=false;magnifierReady=true;
 activeZoom=qBound(2,magnifierZoom,8);activeZoomDurationMs=qBound(500,zoomDurationMs,5000);
 zoomAnimating=progressiveZoom;zoomClock.start();
 updateMagnifierZoom();
 if(zoomAnimating)zoomTimer.start();
 emit changed();
}
void Controller::updateMagnifierZoom(){
 if(!magnifierReady||capturedImage.isNull())return;
 const double progress=zoomAnimating?qMin(1.,double(zoomClock.elapsed())/activeZoomDurationMs):1.;
 const double zoom=1.+(activeZoom-1)*progress;
 magnifierCrop=MagnifierGeometry::crop(captureSize,coarseTarget,zoom);
 magnifiedImage=capturedImage.copy(magnifierCrop);++magnifierRevision;
 const bool finished=zoomAnimating&&progress>=1.;
 if(progress>=1.){if(zoomAnimating)pointDwell.reset();zoomAnimating=false;zoomTimer.stop();}
 emit magnifierFrameChanged();
 if(finished)emit changed();
}
void Controller::clearCapture(){
 pointDwell.reset();
 zoomTimer.stop();zoomAnimating=false;capturedImage={};
 ++captureEpoch;screenshot.cancel();capturePending=false;magnifierReady=false;magnifiedImage={};magnifierCrop={};captureSize={};
}
void Controller::setMagnifierRefreshHz(int hz){
 if(hz!=0&&hz!=15&&hz!=30&&hz!=45&&hz!=60)return;
 if(magnifierRefreshHz==hz)return;
 magnifierRefreshHz=hz;
 if(magnifierReady){if(hz>0)screenshot.startLive(hz);else screenshot.cancel();}
 emit changed();
}
void Controller::setMagnifierEnabled(bool enabled){
 if(magnifierEnabled==enabled)return;
 magnifierEnabled=enabled;
 // Save only the toggle; other preferences retain their explicit Save behavior.
 // Secure-desktop toggles apply only to this instance.
 if(!signInMode){
 ProfileStore store(configFile("profile.json"));auto saved=store.load().values;
 saved["magnifierEnabled"]=enabled;QString error;
 if(!store.save(saved,error))profileStatus="Could not remember magnifier state. "+error;
 }
 if(!enabled&&(capturePending||magnifierReady)){clearCapture();runner.cancel();endButtonHold();gameController.reset();activeJoystick=-1;pointer.cancel();}
 emit changed();
}
bool Controller::hasScratchpad()const{
 for(const auto &entry:layout.value("keys").toList())
  if(entry.toMap().value("kind").toString()=="Scratchpad")return true;
 return false;
}
QString &Controller::typingContext(){return hasScratchpad()?scratchpad:predictionContext;}
const QString &Controller::typingContext()const{return hasScratchpad()?scratchpad:predictionContext;}
bool Controller::setScratchpadText(const QString &text){
 if(!scratchpadEditable()||!desktopActive||paused||(sensitiveInput&&!signInMode)||!text.isValidUtf16()||text.contains(QChar(0)))return false;
 if(scratchpad==text)return true;
 const bool completion=!pendingCompletionText.isEmpty()&&text==scratchpad+pendingCompletionText;
 if(learning&&!WordPrediction::characterLanguage(predictionLanguageValue)&&text.startsWith(scratchpad)){
  auto observed=scratchpad;
  for(auto ch:text.mid(scratchpad.size())){
   if(ch.isSpace()){
    const auto word=WordPrediction::completedWord(observed,predictionLanguageValue);
    if(!word.isEmpty()&&word.size()<64)frequencies[word]=qMin(1000000,frequencies.value(word)+1);
   }
   observed+=ch;
  }
 }
 scratchpad=text;pendingCompletionText.clear();completionSpaceContext.clear();
 if(completion&&text.endsWith(' '))completionSpaceContext=text;
 updateSuggestions();return true;
}
void Controller::attachScratchpad(QObject *editor){
 if(!editor||!editor->property("committedText").isValid())return;
 for(const auto &connection:editorConnections)disconnect(connection);editorConnections.clear();
 localInput.setEditor(editor);
 for(const auto *signal:{SIGNAL(cursorPositionChanged()),SIGNAL(selectionStartChanged()),SIGNAL(selectionEndChanged()),SIGNAL(inputMethodComposingChanged())})
  if(editor->metaObject()->indexOfSignal(signal+1)>=0)editorConnections<<connect(editor,signal,this,SLOT(refreshScratchpadContext()));
 editorConnections<<connect(editor,&QObject::destroyed,this,[this]{QTimer::singleShot(0,this,&Controller::refreshScratchpadContext);});
 refreshScratchpadContext();refreshLocalIme();
}
int Controller::suggestionPageSize()const{
 for(const auto &entry:layout["keys"].toList()){
  const auto key=entry.toMap();if(key["kind"]=="SuggestionRow")return qMax(1,key.value("suggestionCount",key["width"].toInt()/2).toInt());
 }
 return 4;
}
bool Controller::showingImeCandidates()const{return localImeActive()&&(!imeCandidateSnapshot.isEmpty()||localInput.composing());}
void Controller::refreshScratchpadContext(){
 completionSpaceContext.clear();pendingCompletionText.clear();++predictionRevision;
 if(scratchpadRefreshPending)return;
 scratchpadRefreshPending=true;
 QTimer::singleShot(0,this,[this]{scratchpadRefreshPending=false;updateSuggestions();});
}
QString Controller::predictionText()const{
 if(!hasScratchpad()&&externalContextObserved&&externalContext.available)return externalContext.before;
 if(scratchpadEditable()&&hasScratchpad()&&localInput.attached()){
  QString text;int cursor=0;if(localInput.predictionContext(text,cursor))return text.left(cursor);
 }
 return typingContext();
}
QStringList Controller::displaySuggestions()const{return showingImeCandidates()?imeCandidateSnapshot.mid(imeCandidateOffset,suggestionPageSize()):suggestions;}
QString Controller::suggestionPrefix()const{
 if(showingImeCandidates())return QString("ime:%1:").arg(imeRevision);
 return (scratchpadEditable()&&localInput.attached())||(!hasScratchpad()&&externalContextObserved)?QString("suggest:%1:").arg(predictionRevision):QString("suggest:");
}
bool Controller::imeTarget(const QString &target,int &index)const{
 if(!showingImeCandidates()||!target.startsWith(suggestionPrefix()))return false;
 bool valid=false;const int slot=target.mid(suggestionPrefix().size()).toInt(&valid);
 if(!valid||slot<0||slot>=suggestionPageSize())return false;
 index=imeCandidateOffset+slot;
 return index<imeCandidateSnapshot.size()&&!imeCandidateSnapshot[index].trimmed().isEmpty();
}
void Controller::refreshLocalIme(){
 if(localImeActive()&&!localInput.composing()&&localInput.locale()!=lastPredictionImeLocale){
  lastPredictionImeLocale=localInput.locale();
  const auto language=PredictionLanguage::fromLocale(lastPredictionImeLocale);
  if(!language.isEmpty())setPredictionLanguage(language);
 }
 imeCandidateSnapshot=localImeActive()?localInput.candidates():QStringList();
 // Candidate commands carry the new revision, so their dwell restarts naturally.
 // Resetting all dwell here would re-arm a consumed letter or paging key.
 imeCandidateOffset=0;++imeRevision;emit changed();
}
void Controller::attachScratchpadInputEngine(QObject *engine){
 if(localInput.inputEngine()==engine)return;
 for(const auto &connection:imeConnections)disconnect(connection);imeConnections.clear();
 localInput.setEngine(engine);
 if(engine){
  imeConnections<<connect(engine,SIGNAL(candidatesChanged()),this,SLOT(refreshLocalIme()));
  imeConnections<<connect(engine,&QObject::destroyed,this,[this]{localInput.setEngine(nullptr);QTimer::singleShot(0,this,&Controller::refreshLocalIme);});
  if(!wantedImeLocale.isEmpty()&&!localInput.setLocale(wantedImeLocale)){actionStatus=localInput.error();wantedImeLocale.clear();}
 }
 if(scratchpadEditable()&&systemKeyboard()&&localInput.error().isEmpty())actionStatus=engine?"Local input method enabled":"Local input method unavailable";
 refreshLocalIme();
}
void Controller::setLocalImeLocale(const QString &locale){
 if(signInMode||!localInput.availableLocales().contains(locale))return;
 runner.cancel();
 if(localInput.setLocale(locale))wantedImeLocale=locale;else actionStatus=localInput.error();
 refreshLocalIme();
}
void Controller::updateText(QString text){
 if(!desktopActive)return;
 const bool completion=!pendingCompletionText.isEmpty()&&text==pendingCompletionText;
 pendingCompletionText.clear();completionSpaceContext.clear();
 if((sensitiveInput&&!(signInMode&&scratchpadEditable()))||(!hasScratchpad()&&externalContext.privateField))return;
 auto &context=typingContext();
 for(auto ch:text){if(ch=='\b'){if(!context.isEmpty()){QTextBoundaryFinder boundary(QTextBoundaryFinder::Grapheme,context);boundary.toEnd();context.truncate(qMax(0,boundary.toPreviousBoundary()));}}else{if(ch.isSpace()&&learning){QString word=WordPrediction::characterLanguage(predictionLanguageValue)?QString():WordPrediction::completedWord(context,predictionLanguageValue);if(!word.isEmpty()&&word.size()<64)frequencies[word]=qMin(1000000,frequencies.value(word)+1);}context+=ch;}}
 if(context.size()>4096)context=context.right(4096);
 if(completion&&context.endsWith(' '))completionSpaceContext=context;
 updateSuggestions();
}
PredictionLanguage::PunctuationSpacing Controller::takeCompletionSpace(const QString &text){
 const auto context=completionSpaceContext;completionSpaceContext.clear();
 // Best-effort targets use the last known context; caret-aware targets are
 // checked again before deleting the completion space.
 if(!removeCompletionSpaceEnabled||sensitiveInput||context.isEmpty()
    ||context!=predictionText()||text.size()!=1)return {};
 if(scratchpadEditable()&&localInput.attached()&&!localInput.atEnd())return {};
 QString after;
 if(!hasScratchpad()&&externalContextObserved){
  if(externalContext.privateField||(externalContext.supported&&!externalContext.available))return {};
  if(externalContext.available){
   if(desktopContextProvider&&desktopContextProvider()!=externalContext)return {};
   after=externalContext.after;
  }
 }
 auto spacing=PredictionLanguage::punctuationSpacing(text,predictionLanguageValue,context,after);
 if(!completionSpaceEnabled)spacing.after.clear();
 return spacing;
}
QStringList Controller::predictionSuggestions(int limit)const{
 QString document;int cursor=0;
 if(scratchpadEditable()&&localInput.attached()&&!localInput.predictionContext(document,cursor))return {};
 const auto context=predictionText();
 if(!hasScratchpad()&&externalContextObserved&&(externalContext.privateField||(externalContext.supported&&!externalContext.available)))return {};
 if(sensitiveInput||predictionModeValue=="off"||(!context.isEmpty()&&!nextWordEnabled&&WordPrediction::prefix(context,predictionLanguageValue).isEmpty()))return {};
 return WordPrediction::suggest(context,dictionary,learning?frequencies:QMap<QString,int>{},limit,predictionModeValue=="context",sentenceStartersEnabled,typedWordMode,predictionLanguageValue);
}
QStringList Controller::predictionLanguages()const{return WordPrediction::languages();}
void Controller::setPredictionLanguage(const QString &language){
 if(signInMode||language==predictionLanguageValue||!WordPrediction::languages().contains(language))return;
 runner.cancel();dwell.reset();
 QJsonObject counts;for(auto it=frequencies.cbegin();it!=frequencies.cend();++it)counts[it.key()]=it.value();
 QSaveFile saved(learnedFile(predictionLanguageValue));if(saved.open(QIODevice::WriteOnly)){saved.write(QJsonDocument(counts).toJson());saved.commit();}
 predictionLanguageValue=language;frequencies.clear();
 QFile learned(learnedFile(language));if(learned.open(QIODevice::ReadOnly)){const auto obj=QJsonDocument::fromJson(learned.readAll()).object();for(auto it=obj.begin();it!=obj.end();++it)frequencies[it.key()]=qBound(0,it.value().toInt(),1000000);}
 dictionary=WordPrediction::dictionary(language);WordPrediction::prepare(dictionary,language);
 predictionContext.clear();completionSpaceContext.clear();pendingCompletionText.clear();
 updateSuggestions();
}
void Controller::setPredictionUnicodeMode(const QString &mode){
 if(signInMode||predictionUnicodeModeValue==mode||!QStringList{"nonAscii","always"}.contains(mode))return;
 predictionUnicodeModeValue=mode;emit changed();
}
void Controller::setPredictionMode(QString mode){
 if(signInMode)return;
 if(!QStringList{"context","frequency","off"}.contains(mode)||predictionModeValue==mode)return;
 predictionModeValue=mode;dwell.reset();updateSuggestions();
}
void Controller::setSuggestSentenceStarters(bool enabled){
 if(sentenceStartersEnabled==enabled)return;
 sentenceStartersEnabled=enabled;dwell.reset();updateSuggestions();
}
void Controller::setAddSpaceAfterCompletion(bool enabled){
 if(completionSpaceEnabled==enabled)return;
 completionSpaceEnabled=enabled;dwell.reset();emit changed();
}
void Controller::setRemoveCompletionSpaceBeforePunctuation(bool enabled){
 if(removeCompletionSpaceEnabled==enabled)return;
 removeCompletionSpaceEnabled=enabled;completionSpaceContext.clear();emit changed();
}
void Controller::setPredictNextWord(bool enabled){
 if(nextWordEnabled==enabled)return;
 nextWordEnabled=enabled;dwell.reset();updateSuggestions();
}
void Controller::setLearning(bool enabled){
 if(signInMode)return;
 if(learning==enabled)return;
 learning=enabled;dwell.reset();updateSuggestions();
}
void Controller::updateSuggestions(){
 ++predictionRevision;suggestionOffset=0;
 suggestions=predictionSuggestions();emit changed();
}
void Controller::resetLearning(){if(signInMode)return;frequencies.clear();QFile::remove(learnedFile(predictionLanguageValue));updateSuggestions();}
void Controller::speak(){if(desktopActive&&!sensitiveInput)speech.speak(scratchpad);}
void Controller::stopSpeech(){speech.stop();}

bool Controller::keyToggled(const QVariantList &commands)const{
 const auto resolved=ShortcutMappings::resolve(commands,mappingProfile,bypassPlatformMappings(literal));
 if(!resolved.allowed)return false;
 const auto isCaps=[](const QVariant &entry,const QString &tag){
  const auto command=entry.toMap();
  return command["tag"]==tag&&KeyNames::canonical(command["text"].toString().trimmed())=="CAPITAL";
 };
 const auto &keys=resolved.commands;
 const bool caps=(keys.size()==1&&(isCaps(keys.front(),"Action")||isCaps(keys.front(),"KeyToggle")))
  ||(keys.size()==2&&isCaps(keys.front(),"KeyDown")&&isCaps(keys.back(),"KeyUp"));
 if(caps)return runner.textOnly?runner.capsLock:systemLayout.capsLocked(systemCaps);
 return runner.isToggled(resolved.commands);
}

bool Controller::isSizeMoveKey(const QString &key) const{
 if(!key.startsWith("key:"))return false;
 bool ok=false;const int index=key.mid(4).toInt(&ok);const auto keys=layout["keys"].toList();
 if(!ok||index<0||index>=keys.size())return false;
 const auto commands=keys[index].toMap()["commands"].toList();
 return commands.size()==1&&commands[0].toMap()["tag"]=="Action"&&(commands[0].toMap()["text"].toString().trimmed()=="MoveModeToggle"||commands[0].toMap()["text"].toString().trimmed()=="SizeModeToggle");
}
bool Controller::setCanvasKeyRect(int index,double x,double y,double width,double height){
 if(!desktopActive)return false;
 auto keys=layout["keys"].toList();
 if(!sizeMoveMode||paused||layout["layoutMode"]!="canvas"||index<0||index>=keys.size()||
    !std::isfinite(x)||!std::isfinite(y)||!std::isfinite(width)||!std::isfinite(height)||
    x<0||y<0||width<=0||height<=0||x+width>layout["cols"].toDouble()||y+height>layout["rows"].toDouble())return false;
 auto entry=keys[index].toMap();entry["col"]=x;entry["row"]=y;entry["width"]=width;entry["height"]=height;
 keys[index]=entry;layout["keys"]=keys;dwell.reset();resetCanvasPlacement();
 canvasSaveStatus="Position and size changes are temporary until saved.";
 if(saveCanvasChanges)saveCanvasGeometry();
 emit layoutChanged();emit changed();return true;
}

void Controller::resetCanvasPlacement(){
 sizeMoveSelection=-1;canvasEditPreview.clear();canvasAnchorValid=false;canvasPlacementArmed=false;canvasPlacementBlocked=false;canvasDwell.reset();
}
double Controller::canvasEditAt(double x,double y,bool valid){
 if(!desktopActive)return 0;
 if(gazeLossStarted){pauseGazeDwell();return canvasDwell.progress(clock.elapsed(),dwellMs);}
 if(runner.busy()){canvasDwell.reset();return 0;}
 if(!sizeMoveMode||paused||!valid||!std::isfinite(x)||!std::isfinite(y)||x<0||y<0||x>1||y>1){
  canvasDwell.reset();canvasAnchorValid=false;dwell.reset();return 0;
 }
 const QPointF point(x,y);
 const auto distance=[](QPointF a,QPointF b){return std::hypot(a.x()-b.x(),a.y()-b.y());};
 if(canvasPlacementBlocked){
  if(distance(point,canvasAnchor)<.03)return 0;
  canvasPlacementBlocked=false;dwell.reset();
 }
 const auto keys=layout["keys"].toList();const double cols=layout["cols"].toDouble(),rows=layout["rows"].toDouble();
 QString target;
 for(int i=keys.size()-1;i>=0;--i){const auto k=keys[i].toMap();
  if(x*cols>=k["col"].toDouble()&&x*cols<k["col"].toDouble()+k["width"].toDouble()&&
     y*rows>=k["row"].toDouble()&&y*rows<k["row"].toDouble()+k["height"].toDouble()){
   const QString candidate="key:"+QString::number(i);
   if(target.isEmpty()||isSizeMoveKey(candidate))target=candidate;
   if(isSizeMoveKey(candidate))break;
  }
 }
 if(sizeMoveSelection<0||isSizeMoveKey(target)){
  canvasDwell.reset();
  const int before=sizeMoveSelection;const double progress=hover(target);
  if(before<0&&sizeMoveSelection>=0){canvasAnchor=point;canvasAnchorValid=true;}
  return progress;
 }
 dwell.reset();
 if(!canvasAnchorValid){canvasAnchor=point;canvasAnchorValid=true;canvasDwell.reset();return 0;}
 if(!canvasPlacementArmed){
  if(distance(point,canvasAnchor)<.03)return 0;
  canvasPlacementArmed=true;canvasAnchor=point;canvasDwell.reset();
 }
 if(distance(point,canvasAnchor)>.015){canvasAnchor=point;canvasDwell.reset();}
 auto preview=keys[sizeMoveSelection].toMap();
 if(canvasEditMode=="move"){
  preview["col"]=qBound(0.0,x*cols-preview["width"].toDouble()/2,cols-preview["width"].toDouble());
  preview["row"]=qBound(0.0,y*rows-preview["height"].toDouble()/2,rows-preview["height"].toDouble());
 }else{
  preview["width"]=qBound(qMin(cols/100,cols-preview["col"].toDouble()),x*cols-preview["col"].toDouble(),cols-preview["col"].toDouble());
  preview["height"]=qBound(qMin(rows/100,rows-preview["row"].toDouble()),y*rows-preview["row"].toDouble(),rows-preview["row"].toDouble());
 }
 canvasEditPreview=preview;emit changed();
 if(canvasDwell.update("place",clock.elapsed(),dwellMs)){
  setCanvasKeyRect(sizeMoveSelection,preview["col"].toDouble(),preview["row"].toDouble(),preview["width"].toDouble(),preview["height"].toDouble());
  resetCanvasPlacement();canvasPlacementBlocked=true;canvasAnchor=point;emit changed();return 0;
 }
 return canvasDwell.progress(clock.elapsed(),dwellMs);
}

void Controller::updateKeyboardMask(QObject *object,const QVariantList &rectangles,bool enabled){
 auto window=qobject_cast<QWindow*>(object);if(!window)return;
 QRegion region;
 if(enabled){
  for(const auto &value:rectangles){const auto r=value.toRectF();if(!r.isEmpty())region+=r.toAlignedRect();}
  // An empty QWindow mask means the full window; use an off-window region for an empty layout.
  if(region.isEmpty())region=QRegion(-1,-1,1,1);
 }
 if(window->mask()!=region)window->setMask(region);
}

bool Controller::saveCanvasGeometry(){
 if(signInMode)return false;
 if(layout["layoutMode"]!="canvas"){canvasSaveStatus="Only Canvas keyboards support saving key geometry.";emit changed();return false;}
 QString error;
 const bool saved=KeyboardLayout::saveCanvasGeometry(canvasSourcePath,canvasSource,layout["keys"].toList(),error);
 canvasSaveStatus=saved?"Positions and sizes saved to "+canvasSourcePath:"Could not save: "+error+" Changes remain temporary.";
 emit changed();return saved;
}

bool Controller::startsWithWindows()const{
#ifdef _WIN32
 QSettings run("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",QSettings::Registry64Format);
 if(run.value("gaze-grid").toString().isEmpty())return false;
 QSettings approval("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run",QSettings::Registry64Format);
 const auto state=approval.value("gaze-grid").toByteArray();
 return state.isEmpty()||(static_cast<unsigned char>(state[0])&1)==0;
#else
 return false;
#endif
}
void Controller::configureWindowsStartup(bool enabled){
 if(localTestBuild())return;
 if(signInMode||!desktopActive)return;
#ifdef _WIN32
 QSettings run("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",QSettings::Registry64Format);
 if(enabled)run.setValue("gaze-grid",QString("\"%1\"").arg(QDir::toNativeSeparators(QCoreApplication::applicationFilePath())));
 else run.remove("gaze-grid");
 run.sync();
 // An explicit app setting can re-enable an entry disabled in Windows.
 QSettings approval("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run",QSettings::Registry64Format);
 approval.remove("gaze-grid");approval.sync();
 if(run.status()!=QSettings::NoError||approval.status()!=QSettings::NoError)profileStatus="Could not change Windows startup.";
 emit changed();
#endif
}

bool Controller::isSignInEnabled()const{
#ifdef _WIN32
 return WindowsSignIn::enabled();
#else
 return false;
#endif
}
QJsonObject Controller::signInPreferences() const{
 QJsonObject values{{"groupDwellTimes",QJsonObject::fromVariantMap(groupDwellValues)},{"startLayout",layoutName},{"keyboardScale",scalePercent},{"keyboardTheme",keyboardTheme},{"dwellMs",dwellMs},{"latchStyle",latchStyle},{"lockStyle",lockStyle},{"dwellIndicatorStyle",dwellIndicatorStyle},{"gazeEnabled",gazeEnabled},{"gazeFilterType",filterType},{"gazeFilterGain",filterGain},{"gazeSaccadeDistance",saccadeDistance},{"gazeSmoothingLevel",smoothingLevel},{"smoothWhenChangingGazeTarget",smoothTargetChanges},{"shortLabelScale",shortLabelScale},{"longLabelScale",longLabelScale}};
 values["invalidGazeToleranceMs"]=gazeToleranceMs;
 values["dwellGraceMs"]=dwellGraceValue;
 values["showPausedDwellProgress"]=showPausedDwellProgress;
 values["predictionTextScale"]=predictionTextScale;
 values["scratchpadTextScale"]=scratchpadTextScale;
 values["clampGazeForKeyboard"]=clampGazeForKeyboard;
 values["clampGazeForPointSelection"]=clampGazeForPointSelection;
 values["magnifierEnabled"]=magnifierEnabled;
 values["magnifierRefreshHz"]=magnifierRefreshHz;
 values["magnifierZoom"]=magnifierZoom;
 values["progressiveZoom"]=progressiveZoom;
 values["zoomDurationMs"]=zoomDurationMs;
 values["targetDwellEnabled"]=targetDwellEnabled;
 values["targetStartDwellMs"]=targetStartDwellMs;
 values["targetConfirmDwellMs"]=targetConfirmDwellMs;
 values["targetDwellRadius"]=targetDwellRadius;
 return values;
}
void Controller::syncSignInPreferences(){
 if(localTestBuild())return;
 if(signInMode||!desktopActive)return;
#ifdef _WIN32
 profileStatus=WindowsSignIn::configure(true,signInPreferences(),true)?"Current preferences copied to login and secure desktop. Prediction is disabled there.":"Could not sync preferences. Install the trusted version of gaze-grid and accept the administrator prompt.";
 emit changed();
#endif
}
void Controller::configureSignIn(bool enabled){
 if(localTestBuild())return;
 if(signInMode||!desktopActive)return;
#ifdef _WIN32
 const auto values=signInPreferences();
 profileStatus=WindowsSignIn::configure(enabled,values)?(enabled?"This keyboard will start before Windows sign-in. Prediction is disabled there.":"Windows sign-in startup is disabled."):"Could not change sign-in startup. Install the trusted version of gaze-grid and accept the administrator prompt.";
 emit changed();
#endif
}

void Controller::setGazeFilterType(int value){
 value=qBound(0,value,1);if(value==filterType)return;
 filterType=value;gazeSmoothing.reset();dwell.reset();runner.cancel();endButtonHold();gameController.reset();activeJoystick=-1;pointer.cancel();emit changed();
}

void Controller::setGazeFilterGain(double value){
 if(!std::isfinite(value))return;
 value=qBound(0.01,value,1.0);if(value==filterGain)return;
 filterGain=value;gazeSmoothing.reset();dwell.reset();runner.cancel();endButtonHold();gameController.reset();activeJoystick=-1;pointer.cancel();emit changed();
}

void Controller::setGazeSaccadeDistance(double value){
 if(!std::isfinite(value))return;
 value=qBound(0.01,value,1.0);if(value==saccadeDistance)return;
 saccadeDistance=value;gazeSmoothing.reset();dwell.reset();runner.cancel();endButtonHold();gameController.reset();activeJoystick=-1;pointer.cancel();emit changed();
}

void Controller::setTypedWordSuggestionMode(const QString &mode){
 if(!QStringList{"always","fallback","never"}.contains(mode)||typedWordMode==mode)return;
 typedWordMode=mode;dwell.reset();updateSuggestions();
}

void Controller::applySpeechProfile(){
 if((layoutName!="keyboard-conversation-bottom"&&layoutName!="keyboard-conversation-top")||speechProfiles.isEmpty())return;
 auto keys=layout["keys"].toList();
 const auto items=speechProfiles[speechProfileIndex].toMap()["items"].toList();int slot=0;
 for(auto &value:keys){
  auto key=value.toMap();auto commands=key["commands"].toList();bool speechKey=false;
  for(auto &entry:commands){auto command=entry.toMap();
   if(command["tag"]=="Action"&&command["text"].toString().trimmed()=="SpeakLabel"){
    speechKey=true;command["label"]=slot<items.size()?items[slot].toMap()["phrase"].toString():QString();entry=command;
   }
  }
  if(!speechKey)continue;
  key["label"]=slot<items.size()?items[slot].toMap()["label"].toString():QString();
  key["unavailableReason"]=slot<items.size()?QString():QString("No phrase configured");
  key["commands"]=commands;value=key;++slot;
 }
 layout["keys"]=keys;dwell.reset();emit layoutChanged();
}
void Controller::speechProfileEdited(){
 runner.cancel();speech.stop();applySpeechProfile();emit speechProfilesChanged();emit changed();
}
void Controller::setActiveSpeechProfile(int index){
 if(signInMode||index<0||index>=speechProfiles.size()||index==speechProfileIndex)return;
 speechProfileIndex=index;speechProfileEdited();
}
bool Controller::createSpeechProfile(QString name){
 name=name.trimmed();if(signInMode||name.isEmpty())return false;
 speechProfiles<<QVariantMap{{"name",name},{"items",QVariantList{}}};speechProfileIndex=speechProfiles.size()-1;speechProfileEdited();return true;
}
bool Controller::renameSpeechProfile(QString name){
 name=name.trimmed();if(signInMode||name.isEmpty())return false;
 auto profile=speechProfiles[speechProfileIndex].toMap();profile["name"]=name;speechProfiles[speechProfileIndex]=profile;speechProfileEdited();return true;
}
void Controller::deleteSpeechProfile(){
 if(signInMode||speechProfiles.size()<=1)return;
 speechProfiles.removeAt(speechProfileIndex);speechProfileIndex=qMin(speechProfileIndex,int(speechProfiles.size())-1);speechProfileEdited();
}
bool Controller::addSpeechItem(QString label,QString phrase){
 if(signInMode||label.trimmed().isEmpty()||phrase.trimmed().isEmpty())return false;
 auto profile=speechProfiles[speechProfileIndex].toMap();auto items=profile["items"].toList();if(items.size()>=12)return false;
 items<<QVariantMap{{"label",label.trimmed()},{"phrase",phrase.trimmed()}};profile["items"]=items;speechProfiles[speechProfileIndex]=profile;speechProfileEdited();return true;
}
bool Controller::updateSpeechItem(int index,QString label,QString phrase){
 if(signInMode||label.trimmed().isEmpty()||phrase.trimmed().isEmpty())return false;
 auto profile=speechProfiles[speechProfileIndex].toMap();auto items=profile["items"].toList();if(index<0||index>=items.size())return false;
 items[index]=QVariantMap{{"label",label.trimmed()},{"phrase",phrase.trimmed()}};profile["items"]=items;speechProfiles[speechProfileIndex]=profile;speechProfileEdited();return true;
}
void Controller::removeSpeechItem(int index){
 if(signInMode)return;
 auto profile=speechProfiles[speechProfileIndex].toMap();auto items=profile["items"].toList();if(index<0||index>=items.size())return;
 items.removeAt(index);profile["items"]=items;speechProfiles[speechProfileIndex]=profile;speechProfileEdited();
}
