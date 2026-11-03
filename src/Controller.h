#pragma once
#include "DesktopTextContext.h"
#include <QObject>
#include <QElapsedTimer>
#include <QVariantList>
#include <QJsonObject>
#include "PortalInput.h"
#include "TobiiInput.h"
#include "Dwell.h"
#include "PointDwell.h"
#include "GazeSmoothing.h"
#include "ActionRunner.h"
#include "PointerActions.h"
#include "GameController.h"
#include "ScreenshotPortal.h"
#include "SpeechOutput.h"
#include "SystemKeyboard.h"
#include "LocalScratchpadInput.h"
class Controller:public QObject{
 Q_OBJECT
 Q_PROPERTY(QVariantList speechProfiles READ getSpeechProfiles NOTIFY speechProfilesChanged)
 Q_PROPERTY(int activeSpeechProfile READ activeSpeechProfile WRITE setActiveSpeechProfile NOTIFY speechProfilesChanged)
 Q_PROPERTY(bool cacheKeyboards READ cacheKeyboards WRITE setCacheKeyboards NOTIFY changed)
 Q_PROPERTY(int processPriority READ processPriority WRITE setProcessPriority NOTIFY changed)
 Q_PROPERTY(bool signInMode READ isSignInMode CONSTANT)
 Q_PROPERTY(bool startWithWindows READ startsWithWindows NOTIFY changed)
 Q_PROPERTY(bool signInEnabled READ isSignInEnabled NOTIFY changed)
 Q_PROPERTY(bool desktopActive READ isDesktopActive NOTIFY changed)
 Q_PROPERTY(bool trackerConnected READ isTrackerConnected NOTIFY trackerConnectionChanged)
 Q_PROPERTY(QString portalStatus MEMBER portalStatus NOTIFY changed)
 Q_PROPERTY(QString tobiiStatus MEMBER tobiiStatus NOTIFY changed)
 Q_PROPERTY(QString actionStatus MEMBER actionStatus NOTIFY changed)
 Q_PROPERTY(QString profileStatus MEMBER profileStatus NOTIFY changed)
 Q_PROPERTY(bool ready READ ready NOTIFY changed)
 Q_PROPERTY(bool paused MEMBER paused NOTIFY changed)
 Q_PROPERTY(bool gazeEnabled READ isGazeEnabled WRITE setGazeEnabled NOTIFY gazeEnabledChanged)
 Q_PROPERTY(int dwellGraceMs READ dwellGraceMs WRITE setDwellGraceMs NOTIFY changed)
 Q_PROPERTY(bool showPausedDwellProgress MEMBER showPausedDwellProgress NOTIFY changed)
 Q_PROPERTY(int invalidGazeToleranceMs READ invalidGazeToleranceMs WRITE setInvalidGazeToleranceMs NOTIFY changed)
 Q_PROPERTY(bool gazeDwellPaused READ isGazeDwellPaused NOTIFY changed)
 Q_PROPERTY(bool clampGazeForKeyboard MEMBER clampGazeForKeyboard NOTIFY changed)
 Q_PROPERTY(bool clampGazeForPointSelection MEMBER clampGazeForPointSelection NOTIFY changed)
 Q_PROPERTY(int gazeFilterType READ gazeFilterType WRITE setGazeFilterType NOTIFY changed)
 Q_PROPERTY(double gazeFilterGain READ gazeFilterGain WRITE setGazeFilterGain NOTIFY changed)
 Q_PROPERTY(double gazeSaccadeDistance READ gazeSaccadeDistance WRITE setGazeSaccadeDistance NOTIFY changed)
 Q_PROPERTY(int gazeSmoothingLevel READ gazeSmoothingLevel WRITE setGazeSmoothingLevel NOTIFY changed)
 Q_PROPERTY(bool smoothWhenChangingGazeTarget READ smoothWhenChangingGazeTarget WRITE setSmoothWhenChangingGazeTarget NOTIFY changed)
 Q_PROPERTY(int gazeIndicatorSize MEMBER gazeIndicatorSize NOTIFY changed)
 Q_PROPERTY(int gazeIndicatorOpacity MEMBER gazeIndicatorOpacity NOTIFY changed)
 Q_PROPERTY(QString gazeIndicatorColor MEMBER gazeIndicatorColor NOTIFY changed)
 Q_PROPERTY(bool gazeIndicatorMorph MEMBER gazeIndicatorMorph NOTIFY changed)
 Q_PROPERTY(QString buttonMode READ buttonMode NOTIFY changed)
 Q_PROPERTY(QString keyMode READ keyMode NOTIFY changed)
 Q_PROPERTY(QStringList keyCycleModes READ keyCycleModes WRITE setKeyCycleModes NOTIFY changed)
 Q_PROPERTY(int repeatIntervalMs MEMBER repeatIntervalMs NOTIFY changed)
 Q_PROPERTY(bool preserveTogglesBetweenLayouts MEMBER preserveTogglesBetweenLayouts NOTIFY changed)
 Q_PROPERTY(bool cycleModifiersOnHover MEMBER cycleModifiersOnHover NOTIFY changed)
 Q_PROPERTY(QString latchStyle MEMBER latchStyle NOTIFY changed)
 Q_PROPERTY(QString lockStyle MEMBER lockStyle NOTIFY changed)
 Q_PROPERTY(QString dwellIndicatorStyle MEMBER dwellIndicatorStyle NOTIFY changed)
 Q_PROPERTY(int dwellMs MEMBER dwellMs NOTIFY changed)
 Q_PROPERTY(QVariantMap groupDwellTimes READ groupDwellTimes NOTIFY changed)
 Q_PROPERTY(int restoreCorner READ restoreCorner WRITE setRestoreCorner NOTIFY geometryPreferencesChanged)
 Q_PROPERTY(int restoreOpacity MEMBER restoreOpacity NOTIFY changed)
 Q_PROPERTY(int restoreWidth READ restoreWidth WRITE setRestoreWidth NOTIFY geometryPreferencesChanged)
 Q_PROPERTY(int restoreHeight READ restoreHeight WRITE setRestoreHeight NOTIFY geometryPreferencesChanged)
 Q_PROPERTY(int keyboardScale READ keyboardScale WRITE setKeyboardScale NOTIFY geometryPreferencesChanged)
 Q_PROPERTY(QString keyboardTheme MEMBER keyboardTheme NOTIFY changed)
 Q_PROPERTY(QString keyboardTransition MEMBER keyboardTransition NOTIFY changed)
 Q_PROPERTY(int transitionDurationMs MEMBER transitionDurationMs NOTIFY changed)
 Q_PROPERTY(int layoutTransitionDirection MEMBER layoutTransitionDirection NOTIFY layoutChanged)
 Q_PROPERTY(int shortLabelScale MEMBER shortLabelScale NOTIFY changed)
 Q_PROPERTY(int longLabelScale MEMBER longLabelScale NOTIFY changed)
 Q_PROPERTY(int predictionTextScale MEMBER predictionTextScale NOTIFY changed)
 Q_PROPERTY(int scratchpadTextScale MEMBER scratchpadTextScale NOTIFY changed)
 Q_PROPERTY(bool rememberDesktop MEMBER rememberDesktop NOTIFY changed)
 Q_PROPERTY(int sizeMoveSelection MEMBER sizeMoveSelection NOTIFY changed)
 Q_PROPERTY(bool saveCanvasChanges MEMBER saveCanvasChanges NOTIFY changed)
 Q_PROPERTY(QString canvasSaveStatus MEMBER canvasSaveStatus NOTIFY changed)
 Q_PROPERTY(bool sizeMoveMode MEMBER sizeMoveMode NOTIFY changed)
 Q_PROPERTY(QString canvasEditMode MEMBER canvasEditMode NOTIFY changed)
 Q_PROPERTY(QVariantMap canvasEditPreview MEMBER canvasEditPreview NOTIFY changed)
 Q_PROPERTY(QVariantMap layout MEMBER layout NOTIFY layoutChanged)
 Q_PROPERTY(QString layoutName MEMBER layoutName NOTIFY layoutChanged)
 Q_PROPERTY(QStringList layoutNames MEMBER layoutNames CONSTANT)
 Q_PROPERTY(QStringList menuLayoutNames MEMBER menuLayoutNames CONSTANT)
 Q_PROPERTY(bool shifted READ shifted NOTIFY changed)
 Q_PROPERTY(QStringList heldKeys READ heldKeys NOTIFY changed)
 Q_PROPERTY(QVariantMap modifierStates READ modifierStates NOTIFY changed)
 Q_PROPERTY(bool literal MEMBER literal NOTIFY changed)
 Q_PROPERTY(bool systemKeyboard READ systemKeyboard WRITE setSystemKeyboard NOTIFY changed)
 Q_PROPERTY(QVariantList keyboardLabels READ keyboardLabels NOTIFY keyboardLabelsChanged)
 Q_PROPERTY(QString mappingJson READ mappingJson NOTIFY changed)
 Q_PROPERTY(QString scratchpad MEMBER scratchpad NOTIFY changed)
 Q_PROPERTY(bool scratchpadEditable READ scratchpadEditable NOTIFY changed)
 Q_PROPERTY(bool localImeEnabled READ localImeEnabled CONSTANT)
 Q_PROPERTY(bool localImeActive READ localImeActive NOTIFY changed)
 Q_PROPERTY(QStringList localImeLocales READ localImeLocales NOTIFY changed)
 Q_PROPERTY(QString localImeLocale READ localImeLocale WRITE setLocalImeLocale NOTIFY changed)
 Q_PROPERTY(QStringList displaySuggestions READ displaySuggestions NOTIFY changed)
 Q_PROPERTY(QString suggestionPrefix READ suggestionPrefix NOTIFY changed)
 Q_PROPERTY(QStringList suggestions MEMBER suggestions NOTIFY changed)
 Q_PROPERTY(bool learning READ learningEnabled WRITE setLearning NOTIFY changed)
 Q_PROPERTY(QString typedWordSuggestionMode READ typedWordSuggestionMode WRITE setTypedWordSuggestionMode NOTIFY changed)
 Q_PROPERTY(QString predictionUnicodeMode READ predictionUnicodeMode WRITE setPredictionUnicodeMode NOTIFY changed)
 Q_PROPERTY(QString predictionLanguage READ predictionLanguage WRITE setPredictionLanguage NOTIFY changed)
 Q_PROPERTY(QStringList predictionLanguages READ predictionLanguages CONSTANT)
 Q_PROPERTY(QString predictionMode READ predictionMode WRITE setPredictionMode NOTIFY changed)
 Q_PROPERTY(bool suggestSentenceStarters READ suggestSentenceStarters WRITE setSuggestSentenceStarters NOTIFY changed)
 Q_PROPERTY(bool addSpaceAfterCompletion READ addSpaceAfterCompletion WRITE setAddSpaceAfterCompletion NOTIFY changed)
 Q_PROPERTY(bool removeCompletionSpaceBeforePunctuation READ removeCompletionSpaceBeforePunctuation WRITE setRemoveCompletionSpaceBeforePunctuation NOTIFY changed)
 Q_PROPERTY(bool predictNextWord READ predictNextWord WRITE setPredictNextWord NOTIFY changed)
 Q_PROPERTY(bool hideKeyboardDuringMouseActions MEMBER hideKeyboardDuringMouseActions NOTIFY changed)
 Q_PROPERTY(bool keyboardTemporarilyHidden READ keyboardTemporarilyHidden NOTIFY changed)
 Q_PROPERTY(bool targeting MEMBER targeting NOTIFY changed)
 Q_PROPERTY(QString targetPrompt READ targetPrompt NOTIFY changed)
 Q_PROPERTY(QStringList heldPointerButtons READ heldPointerButtons NOTIFY changed)
 Q_PROPERTY(bool magneticCursor READ magneticCursor NOTIFY changed)
 Q_PROPERTY(int clickDurationMs MEMBER clickDurationMs NOTIFY changed)
 Q_PROPERTY(bool magneticMouseRelative MEMBER magneticMouseRelative NOTIFY changed)
 Q_PROPERTY(int scrollSpeed MEMBER scrollSpeed NOTIFY changed)
 Q_PROPERTY(int scrollDeadzone MEMBER scrollDeadzone NOTIFY changed)
 Q_PROPERTY(int scrollAxis MEMBER scrollAxis NOTIFY changed)
 Q_PROPERTY(bool clampScrollGaze MEMBER clampScrollGaze NOTIFY changed)
 Q_PROPERTY(bool cancelScrollOffscreen MEMBER cancelScrollOffscreen NOTIFY changed)
 Q_PROPERTY(bool lookScrolling READ lookScrolling NOTIFY changed)
 Q_PROPERTY(QPointF scrollCentre READ scrollCentre NOTIFY changed)
 Q_PROPERTY(bool showScrollZones MEMBER showScrollZones NOTIFY changed)
 Q_PROPERTY(int scrollZoneOpacity MEMBER scrollZoneOpacity NOTIFY changed)
 Q_PROPERTY(QString scrollZoneColor MEMBER scrollZoneColor NOTIFY changed)
 Q_PROPERTY(bool magnifierEnabled READ isMagnifierEnabled WRITE setMagnifierEnabled NOTIFY changed)
 Q_PROPERTY(int magnifierRefreshHz READ getMagnifierRefreshHz WRITE setMagnifierRefreshHz NOTIFY changed)
 Q_PROPERTY(int magnifierZoom MEMBER magnifierZoom NOTIFY changed)
 Q_PROPERTY(bool progressiveZoom MEMBER progressiveZoom NOTIFY changed)
 Q_PROPERTY(int zoomDurationMs MEMBER zoomDurationMs NOTIFY changed)
 Q_PROPERTY(bool targetDwellEnabled MEMBER targetDwellEnabled NOTIFY changed)
 Q_PROPERTY(int targetStartDwellMs MEMBER targetStartDwellMs NOTIFY changed)
 Q_PROPERTY(int targetConfirmDwellMs MEMBER targetConfirmDwellMs NOTIFY changed)
 Q_PROPERTY(QString targetProgressStyle MEMBER targetProgressStyle NOTIFY changed)
 Q_PROPERTY(int targetProgressSize MEMBER targetProgressSize NOTIFY changed)
 Q_PROPERTY(int targetProgressThickness MEMBER targetProgressThickness NOTIFY changed)
 Q_PROPERTY(int targetProgressOpacity MEMBER targetProgressOpacity NOTIFY changed)
 Q_PROPERTY(QString targetProgressColor MEMBER targetProgressColor NOTIFY changed)
 Q_PROPERTY(int targetDwellRadius MEMBER targetDwellRadius NOTIFY changed)
 Q_PROPERTY(bool zoomAnimating MEMBER zoomAnimating NOTIFY changed)
 Q_PROPERTY(bool capturePending MEMBER capturePending NOTIFY changed)
 Q_PROPERTY(bool magnifierReady MEMBER magnifierReady NOTIFY changed)
 Q_PROPERTY(int magnifierRevision MEMBER magnifierRevision NOTIFY magnifierFrameChanged)
public:
 QVariantMap groupDwellTimes()const{return groupDwellValues;}
 Q_INVOKABLE void setGroupDwellTime(QString group,int milliseconds);
 Q_INVOKABLE void setAllGroupDwellTimes(int milliseconds);
 Q_INVOKABLE int dwellDuration(QString target)const;
 int processPriority()const{return processPriorityValue;}
 void setProcessPriority(int priority);
 // An injected tracker must outlive this controller; production uses the owned worker.
 explicit Controller(bool signIn=false,const QJsonObject &signInProfile={},TobiiInput *tracker=nullptr);~Controller()override;
 bool cacheKeyboards()const;
 void setCacheKeyboards(bool enabled);
 bool isSignInMode()const{return signInMode;}
 bool startsWithWindows()const;
 Q_INVOKABLE void configureWindowsStartup(bool enabled);
 Q_INVOKABLE void refreshWindowsStartup(){emit changed();}
 bool isSignInEnabled()const;
 bool isTrackerConnected()const{return trackerConnected;}
 bool isDesktopActive()const{return desktopActive;}
 void setDesktopActive(bool active);
 Q_INVOKABLE void configureSignIn(bool enabled);
 Q_INVOKABLE void syncSignInPreferences();
 Q_INVOKABLE void updateKeyboardMask(QObject *window,const QVariantList &rectangles,bool enabled);
 int getMagnifierRefreshHz()const{return magnifierRefreshHz;}
 void setMagnifierRefreshHz(int hz);
 bool isMagnifierEnabled()const{return magnifierEnabled;}
 void setMagnifierEnabled(bool enabled);
 PortalInput *output(){return &portal;}
 bool ready()const{return desktopActive&&(nativeActive||portal.ready());}
 void configureInputMethod(std::function<bool(QString)> text,std::function<bool(QString,bool)> key);
 void inputContextChanged(bool active,bool sensitive);
 void updateDesktopTextContext(const DesktopTextContext &context);
 void setDesktopContextProvider(std::function<DesktopTextContext()> provider){desktopContextProvider=std::move(provider);}
 void setDesktopCompletionProvider(std::function<bool(const DesktopTextContext &,const QString &,const QString &,bool)> provider){desktopCompletionProvider=std::move(provider);}
 void invalidateInputContext();
 int keyboardScale()const{return scalePercent;}
 void setKeyboardScale(int scale);
 int restoreCorner()const{return recoveryCorner;}
 int restoreWidth()const{return recoveryWidth;}
 int restoreHeight()const{return recoveryHeight;}
 void setRestoreCorner(int corner);
 void setRestoreWidth(int width);
 void setRestoreHeight(int height);
 bool shifted()const{return runner.shifted();}
 QStringList heldKeys()const{return runner.heldKeys();}
 QVariantMap modifierStates()const{return runner.modifierStates();}
 Q_INVOKABLE bool keyToggled(const QVariantList &commands)const;
 Q_INVOKABLE QString modifierName(const QVariantList &commands)const{return ActionRunner::modifierName(commands);}
 QString mappingJson()const;
 QString targetPrompt()const{return zoomAnimating?"Zooming in…":magnifierReady?"Choose the precise target in the magnified image":pointer.prompt();}
 QImage magnifierImage()const{return magnifiedImage;}
 QStringList heldPointerButtons()const{return pointer.heldButtons();}
 bool keyboardTemporarilyHidden()const{return hideKeyboardDuringMouseActions&&pointer.hidesKeyboard();}
 bool magneticCursor()const{return pointer.magnetic();}
 bool lookScrolling()const{return pointer.lookScrolling();}
 QPointF scrollCentre()const{return pointer.scrollCentre();}
 bool isGazeEnabled()const{return gazeEnabled;}
 int dwellGraceMs()const{return dwellGraceValue;}
 void setDwellGraceMs(int value);
 int invalidGazeToleranceMs()const{return gazeToleranceMs;}
 void setInvalidGazeToleranceMs(int value);
 bool isGazeDwellPaused()const{return gazeLossStarted>0;}
 Q_INVOKABLE void pauseGazeDwell();
 Q_INVOKABLE void setGazeEnabled(bool enabled);
 Q_INVOKABLE void trackGaze(double x,double y,bool overDesktop);
 void displayChanged();
 void setDisplaySize(double width,double height);
 int gazeFilterType()const{return filterType;}
 void setGazeFilterType(int value);
 double gazeFilterGain()const{return filterGain;}
 void setGazeFilterGain(double value);
 double gazeSaccadeDistance()const{return saccadeDistance;}
 void setGazeSaccadeDistance(double value);
 int gazeSmoothingLevel()const{return smoothingLevel;}
 bool smoothWhenChangingGazeTarget()const{return smoothTargetChanges;}
 void setGazeSmoothingLevel(int level);
 void setSmoothWhenChangingGazeTarget(bool enabled);
 Q_INVOKABLE bool saveMappings(QString json);
 Q_INVOKABLE bool savePreferences();
 bool systemKeyboard()const{return runner.physicalText;}
 void setSystemKeyboard(bool enabled);
 QVariantList keyboardLabels()const{return keyboardLabelValues;}
 Q_INVOKABLE void resetPreferences();
 Q_INVOKABLE void connectDesktop();
 Q_INVOKABLE void forgetDesktopPermission(){portal.forgetPermission();rememberDesktop=false;emit changed();}
 Q_INVOKABLE void connectTobii();
 Q_INVOKABLE void togglePause();
 Q_INVOKABLE void activate(QString key);
 Q_INVOKABLE bool saveCanvasGeometry();
 Q_INVOKABLE bool setCanvasKeyRect(int index,double x,double y,double width,double height);
 bool isSizeMoveKey(const QString &key) const;
 Q_INVOKABLE double canvasEditAt(double x,double y,bool valid);
 void resetCanvasPlacement();
 Q_INVOKABLE bool gamepadHeld(QString action)const{return gameController.held(action);}
 Q_INVOKABLE void joystick(int index,double x,double y,bool active);
 Q_INVOKABLE void clickCommand(QString key);
 Q_INVOKABLE double hover(QString key);
 Q_INVOKABLE QVariantMap hoverKeys(const QStringList &targets);
 Q_INVOKABLE QVariantMap pausedDwellProgress()const;
 QStringList keyCycleModes()const{return keyCycleModesValue;}
 void setKeyCycleModes(const QStringList &modes);
 QString keyMode()const{return keyModeValue;}
 QString buttonMode()const{return buttonModeValue;}
 Q_INVOKABLE double dwellAt(double x,double y,bool valid);
 Q_INVOKABLE void openLayout(QString name,bool backReturnsHere=true);
 Q_INVOKABLE void recover();
 Q_INVOKABLE void targetAt(double x,double y);
 bool learningEnabled()const{return learning;}
 void setLearning(bool enabled);
 QString typedWordSuggestionMode()const{return typedWordMode;}
 void setTypedWordSuggestionMode(const QString &mode);
 QString predictionUnicodeMode()const{return predictionUnicodeModeValue;}
 void setPredictionUnicodeMode(const QString &mode);
 QString predictionLanguage()const{return predictionLanguageValue;}
 QStringList predictionLanguages()const;
 void setPredictionLanguage(const QString &language);
 QString predictionMode()const{return predictionModeValue;}
 void setPredictionMode(QString mode);
 bool suggestSentenceStarters()const{return sentenceStartersEnabled;}
 void setSuggestSentenceStarters(bool enabled);
 bool addSpaceAfterCompletion()const{return completionSpaceEnabled;}
 bool removeCompletionSpaceBeforePunctuation()const{return removeCompletionSpaceEnabled;}
 void setRemoveCompletionSpaceBeforePunctuation(bool enabled);
 void setAddSpaceAfterCompletion(bool enabled);
 bool predictNextWord()const{return nextWordEnabled;}
 void setPredictNextWord(bool enabled);
 Q_INVOKABLE void resetLearning();
 QVariantList getSpeechProfiles()const{return speechProfiles;}
 int activeSpeechProfile()const{return speechProfileIndex;}
 void setActiveSpeechProfile(int index);
 Q_INVOKABLE bool createSpeechProfile(QString name);
 Q_INVOKABLE bool renameSpeechProfile(QString name);
 Q_INVOKABLE void deleteSpeechProfile();
 Q_INVOKABLE bool addSpeechItem(QString label,QString phrase);
 Q_INVOKABLE bool updateSpeechItem(int index,QString label,QString phrase);
 Q_INVOKABLE void removeSpeechItem(int index);
 Q_INVOKABLE void speak();
 Q_INVOKABLE void stopSpeech();
 // Used only by deterministic external-input integration checks.
 bool executeCommands(const QVariantList &commands){return desktopActive&&runner.start(commands);}
 bool executingCommands()const{return runner.busy();}
 bool scratchpadEditable()const{return runner.textOnly&&!nativeMode&&!signInMode;}
 Q_INVOKABLE bool setScratchpadText(const QString &text);
 bool localImeEnabled()const{return localIme;}
 Q_INVOKABLE void attachScratchpad(QObject *editor);
 Q_INVOKABLE void attachScratchpadInputEngine(QObject *engine);
 bool localImeActive()const{return systemKeyboard()&&scratchpadEditable()&&desktopActive&&!paused&&!sensitiveInput&&localInput.attached()&&localInput.inputEngine();}
 QStringList localImeLocales()const{return localInput.availableLocales();}
 QString localImeLocale()const{return wantedImeLocale.isEmpty()?localInput.locale():wantedImeLocale;}
 void setLocalImeLocale(const QString &locale);
 QStringList displaySuggestions()const;
 QString suggestionPrefix()const;
signals:
 void keyboardLabelsChanged();
 void speechProfilesChanged();
 void geometryPreferencesChanged();
 void settingsRequested();
 void changed();
 void gazeEnabledChanged();
 void trackerConnectionChanged();
 void magnifierFrameChanged();
 void layoutChanged();
 void gazePoint(double x,double y,bool valid);
private slots:
 void refreshLocalIme();
 void refreshScratchpadContext();
private:
 bool showingImeCandidates()const;
 int suggestionPageSize()const;
 bool imeTarget(const QString &target,int &index)const;
 QString wantedImeLocale,lastPredictionImeLocale;
 QStringList imeCandidateSnapshot;
 int imeCandidateOffset=0;
 quint64 imeRevision=0;
 QList<QMetaObject::Connection> imeConnections;
 QList<QMetaObject::Connection> editorConnections;
 quint64 predictionRevision=0;
 bool scratchpadRefreshPending=false;
 SystemKeyboard systemLayout;
 LocalScratchpadInput localInput;
 bool localIme=false;
 bool systemCaps=false;
 QVariantList keyboardLabelValues;
 void refreshKeyboardLabels();
 QVariantList speechProfiles;
 int speechProfileIndex=0;
 void applySpeechProfile();
 void speechProfileEdited();
 QJsonObject signInPreferences() const;
 void stopTracker();
 void scheduleTrackerRetry();
 bool isSleepKey(const QString &key) const;
 bool performAction(QString action);
 void clearCapture();
 void updateMagnifierZoom();
 void updateText(QString text);
 bool takeCompletionSpace(const QString &text);
 void updateSuggestions();
 QVariantList mapped(const QVariantList &commands,bool &ok);
 Q_SLOT void receiveGaze(double x,double y,bool valid,qint64 sampledAt);
 Q_SLOT void setTrackerConnected(bool connected);
 GazeSmoothing gazeSmoothing;
 QSizeF gazeDisplaySize{1,1};
 int gazeIndicatorSize=18,gazeIndicatorOpacity=100;
 QString gazeIndicatorColor="#00bfff";
 void endButtonHold();
 void clearOverlap();
 void releaseOverlapTarget(const QString &target);
 void dispatchOverlap();
 struct OverlapActivation {QString target;QVariantList holdCommands;QString buttonHold;};
 QMap<QString,Dwell> overlapDwells;
 QMap<QString,QString> overlapButtons;
 QList<OverlapActivation> overlapPending;
 bool collectingOverlap=false,dispatchingOverlap=false;
 QString buttonModeValue="Tap",buttonHoldTarget,buttonHoldAction;
 QStringList keyCycleModesValue{"Tap","Repeat","Hold","Toggle"};
 QString keyModeValue="Tap",heldDwellTarget;
 qint64 lastHoldHover=0;
 int repeatIntervalMs=750;
 bool preserveTogglesBetweenLayouts=false;
 bool cycleModifiersOnHover=false;
 bool hideKeyboardDuringMouseActions=false;
 bool gazeIndicatorMorph=false;
 int filterType=0;
 double filterGain=0.2;
 double saccadeDistance=0.1;
 int smoothingLevel=1;
 bool smoothTargetChanges=false;
 GameController gameController;
 int activeJoystick=-1;
 PortalInput portal;TobiiInput ownedTracker;TobiiInput &tobii;Dwell dwell;ActionRunner runner;PointerActions pointer;QElapsedTimer clock,gazeAge;
 QTimer trackerRetryTimer;
 int trackerRetryDelayMs=1000;
 quint64 trackerGeneration=0;
 bool desktopActive=true,desktopOutputRequested=false;
 ScreenshotPortal screenshot;
 Q_SLOT void receiveMagnifierFrame(QImage image);
 Q_SLOT void receiveLiveMagnifierFrame(QImage image);
 QTimer zoomTimer;
 QElapsedTimer zoomClock;
 QImage capturedImage;
 SpeechOutput speech;
 QImage magnifiedImage;
 QRect magnifierCrop;
 QSize captureSize;
 QPointF coarseTarget;
 bool magnifierEnabled=false,capturePending=false,magnifierReady=false;
 int magnifierRevision=0;
 int clickDurationMs=0;
 bool magneticMouseRelative=false;
 int scrollSpeed=100,scrollDeadzone=50,scrollAxis=0;
 bool clampScrollGaze=true,cancelScrollOffscreen=false;
 bool showScrollZones=false;
 int scrollZoneOpacity=30;
 QString scrollZoneColor="#00bfff";
 int magnifierZoom=4,magnifierRefreshHz=30;
 bool progressiveZoom=false,zoomAnimating=false;
 int zoomDurationMs=1500,activeZoomDurationMs=1500,activeZoom=4;
 PointDwell pointDwell;
 bool targetDwellEnabled=true;
 QString targetProgressStyle="ring";
 int targetProgressSize=56;
 int targetProgressThickness=5;
 int targetProgressOpacity=100;
 QString targetProgressColor="#00bfff";
 int targetStartDwellMs=750,targetConfirmDwellMs=1000,targetDwellRadius=40;
 quint64 captureEpoch=0;
 QString portalStatus="Desktop control disconnected",tobiiStatus="Tobii ready to connect",actionStatus="Choose Authorize to enable desktop output";
 QString profileStatus;
 bool paused=false,literal=false,learning=false,targeting=false,gazeEnabled=false;
 QString latchStyle="outline",lockStyle="fill";
 QString dwellIndicatorStyle="bar";
 QVariantMap groupDwellValues;
 int dwellMs=750;
 int processPriorityValue=0;
 int scalePercent=100;
 int restoreOpacity=100;
 int recoveryCorner=2,recoveryWidth=64,recoveryHeight=64;
 QString keyboardTheme="system";
 QString keyboardTransition="instant";
 int transitionDurationMs=180,layoutTransitionDirection=1;
 int shortLabelScale=150;
 int longLabelScale=100;
 int predictionTextScale=100;
 int scratchpadTextScale=100;
 bool rememberDesktop=false;
 const bool signInMode;
 bool trackerConnected=false;
 bool nativeMode=false,nativeActive=false,sensitiveInput=false;
 qint64 gazeLossStarted=0;
 int gazeToleranceMs=500;
 int dwellGraceValue=0;
 bool showPausedDwellProgress=true;
 bool clampGazeForKeyboard=false,clampGazeForPointSelection=true;
 bool lastGazeClamped=true;
 int sizeMoveSelection=-1;
 bool sizeMoveMode=false;
 bool saveCanvasChanges=false;
 QString canvasSourcePath,canvasSaveStatus;QByteArray canvasSource;
 QString canvasEditMode;QVariantMap canvasEditPreview;
 QPointF canvasAnchor;bool canvasAnchorValid=false,canvasPlacementArmed=false,canvasPlacementBlocked=false;
 Dwell canvasDwell;
 QVariantMap layout;QString layoutName,startupLayout;QStringList layoutNames,menuLayoutNames,backStack;
 int suggestionOffset=0;
 QString typedWordMode="fallback";
 QString predictionUnicodeModeValue="nonAscii";
 QString predictionLanguageValue="eng";
 QString predictionModeValue="context";bool nextWordEnabled=true;bool completionSpaceEnabled=true;bool sentenceStartersEnabled=true;
 bool removeCompletionSpaceEnabled=true;
 QString pendingCompletionText,completionSpaceContext;
 QStringList predictionSuggestions(int limit=8)const;
 bool hasScratchpad()const;
 QString &typingContext();
 const QString &typingContext()const;
 QString predictionText()const;
 QString predictionContext;
 DesktopTextContext externalContext,pendingExternalContext;
 bool externalContextObserved=false,externalContextPending=false;
 std::function<DesktopTextContext()> desktopContextProvider;
 std::function<bool(const DesktopTextContext &,const QString &,const QString &,bool)> desktopCompletionProvider;
 QString scratchpad;QStringList suggestions,dictionary;QMap<QString,int> frequencies;
 QJsonObject mappingProfile;
};
