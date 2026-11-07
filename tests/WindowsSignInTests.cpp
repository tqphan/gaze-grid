#include <QtTest>
#include "SignInRegistry.h"
#include "WindowsSignIn.h"
#include "ProfileStore.h"
#include <QUuid>
class WindowsSignInTests:public QObject {
 Q_OBJECT
private slots:
 void loginDefaultsEnableGazeWithoutReadingPersonalPreferences(){
  for(const QByteArray data:{QByteArray{},QByteArray("{broken"),QByteArray("[]"),QByteArray(16385,'x'),QByteArray("{}"),QByteArray("{\"gazeEnabled\":\"true\"}")}){
   const auto values=WindowsSignIn::profileFromData(data);
   QVERIFY(ProfileStore::valid(values));QVERIFY(values["gazeEnabled"].toBool());
   QCOMPARE(values["predictionMode"].toString(),QString("off"));QVERIFY(!values["learning"].toBool());
  }
  QVERIFY(ProfileStore::defaults()["gazeEnabled"].toBool());
 }
 void loginPreservesExplicitGazeChoiceAndKeyboard(){
  const auto disabled=WindowsSignIn::profileFromData("{\"gazeEnabled\":false,\"startLayout\":\"main-14x1-top\"}");
  QVERIFY(!disabled["gazeEnabled"].toBool());QCOMPARE(disabled["startLayout"].toString(),QString("main-14x1-top"));
  const auto enabled=WindowsSignIn::profileFromData("{\"gazeEnabled\":true,\"keyboardScale\":120}");
  QVERIFY(enabled["gazeEnabled"].toBool());QCOMPARE(enabled["keyboardScale"].toInt(),120);
 }
 void accessibilityNotificationWritesSignalBeforeSendingShortcut(){
  const auto path=(QStringLiteral("SOFTWARE\\gaze-grid\\Tests\\")+QUuid::createUuid().toString(QUuid::WithoutBraces)).toStdWString();
  struct Cleanup {const std::wstring &path;~Cleanup(){RegDeleteKeyW(HKEY_CURRENT_USER,path.c_str());}} cleanup{path};
  for(bool running:{true,false}){
   DWORD observed=0;UINT count=0;bool shortcut=false;
   const auto notified=SignInRegistry::notify(HKEY_CURRENT_USER,path.c_str(),running,[&](INPUT *inputs,UINT size){
    DWORD bytes=sizeof(observed);
    RegGetValueW(HKEY_CURRENT_USER,path.c_str(),SignInRegistry::id,RRF_RT_REG_DWORD,nullptr,&observed,&bytes);
    count=size;
    shortcut=size==4&&inputs[0].type==INPUT_KEYBOARD&&inputs[0].ki.wVk==VK_LWIN&&inputs[0].ki.dwFlags==0
       &&inputs[1].type==INPUT_KEYBOARD&&inputs[1].ki.wVk=='U'&&inputs[1].ki.dwFlags==0
       &&inputs[2].type==INPUT_KEYBOARD&&inputs[2].ki.wVk=='U'&&inputs[2].ki.dwFlags==KEYEVENTF_KEYUP
       &&inputs[3].type==INPUT_KEYBOARD&&inputs[3].ki.wVk==VK_LWIN&&inputs[3].ki.dwFlags==KEYEVENTF_KEYUP;
    return size;
   });
   QVERIFY(notified);QCOMPARE(count,UINT(4));QVERIFY(shortcut);QCOMPARE(observed,DWORD(running?3:2));
  }
 }
 void rejectedAccessibilityShortcutDoesNotLeavePendingSignal(){
  const auto path=(QStringLiteral("SOFTWARE\\gaze-grid\\Tests\\")+QUuid::createUuid().toString(QUuid::WithoutBraces)).toStdWString();
  struct Cleanup {const std::wstring &path;~Cleanup(){RegDeleteKeyW(HKEY_CURRENT_USER,path.c_str());}} cleanup{path};
  QVERIFY(!SignInRegistry::notify(HKEY_CURRENT_USER,path.c_str(),true,[](INPUT *,UINT){return UINT(0);}));
  DWORD value=0,bytes=sizeof(value);
  QCOMPARE(RegGetValueW(HKEY_CURRENT_USER,path.c_str(),SignInRegistry::id,RRF_RT_REG_DWORD,nullptr,&value,&bytes),LONG(ERROR_FILE_NOT_FOUND));
 }
 void partialAccessibilityShortcutReleasesKeysAndClearsSignal(){
  const auto path=(QStringLiteral("SOFTWARE\\gaze-grid\\Tests\\")+QUuid::createUuid().toString(QUuid::WithoutBraces)).toStdWString();
  struct Cleanup {const std::wstring &path;~Cleanup(){RegDeleteKeyW(HKEY_CURRENT_USER,path.c_str());}} cleanup{path};
  int calls=0;bool released=false,pending=true;
  QVERIFY(!SignInRegistry::notify(HKEY_CURRENT_USER,path.c_str(),true,[&](INPUT *inputs,UINT count){
   if(++calls==1)return UINT(2);
   DWORD value=0,bytes=sizeof(value);
   pending=RegGetValueW(HKEY_CURRENT_USER,path.c_str(),SignInRegistry::id,RRF_RT_REG_DWORD,nullptr,&value,&bytes)==ERROR_SUCCESS;
   released=count==2&&inputs[0].ki.wVk=='U'&&inputs[0].ki.dwFlags==KEYEVENTF_KEYUP
       &&inputs[1].ki.wVk==VK_LWIN&&inputs[1].ki.dwFlags==KEYEVENTF_KEYUP;
   return count;
  }));
  QCOMPARE(calls,2);QVERIFY(released);QVERIFY(!pending);
 }
 void retainsOtherAccessibilityTools(){
  using namespace SignInRegistry;
  const std::wstring before=L"osk,narrator";
  auto enabled=updated(before,true);QVERIFY(contains(enabled));
  QVERIFY(updated(enabled,true)==enabled);QVERIFY(updated(enabled,false)==before);
  QVERIFY(updated(L"osk,gaze-grid_gaze-grid_v1,narrator,GAZE-GRID_GAZE-GRID_V1",false)==before);
  QVERIFY(!contains(L"osk,,narrator"));QVERIFY(!contains(L"other_gaze-grid_gaze-grid_v1"));
  QVERIFY(contains(L"osk, gaze-grid_gaze-grid_v1 ,narrator"));
 }
 void sharedProfileOnlyContainsValidatedKeyboardPreferences(){
  auto values=ProfileStore::defaults();values["learning"]=true;values["predictionMode"]="context";values["magnifierEnabled"]=true;values["magnifierRefreshHz"]=0;
  values["scratchpadTextScale"]=175;values["keyboardScale"]=120;values["startLayout"]="main-14x1-top";values["invalidGazeToleranceMs"]=250;
  values["clampGazeForKeyboard"]=false;values["clampGazeForPointSelection"]=false;
  auto result=WindowsSignIn::validatedProfile(values);
  QCOMPARE(result["startLayout"].toString(),QString("main-14x1-top"));QCOMPARE(result["keyboardScale"].toInt(),120);
  QCOMPARE(result["predictionMode"].toString(),QString("off"));QVERIFY(!result["learning"].toBool());QVERIFY(result["magnifierEnabled"].toBool());
  QCOMPARE(result["magnifierRefreshHz"].toInt(-1),0);
  QCOMPARE(result["invalidGazeToleranceMs"].toInt(),250);QCOMPARE(result["scratchpadTextScale"].toInt(),175);
  QVERIFY(!result["clampGazeForKeyboard"].toBool());QVERIFY(!result["clampGazeForPointSelection"].toBool());
  values["startLayout"]="../user-keyboard";QVERIFY(WindowsSignIn::validatedProfile(values).isEmpty());
  values["startLayout"]="minimized-off-screen-bottom";QVERIFY(WindowsSignIn::validatedProfile(values).isEmpty());
  values["startLayout"]="main-14x1-bottom";values["dwellMs"]=-1;QVERIFY(WindowsSignIn::validatedProfile(values).isEmpty());
 }
};
QTEST_GUILESS_MAIN(WindowsSignInTests)
#include "WindowsSignInTests.moc"
