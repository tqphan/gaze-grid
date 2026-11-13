#include "WindowsAutomationContext.h"
#include "PredictionLanguage.h"
#include "WindowsInput.h"
#include <objbase.h>
#include <uiautomationclient.h>
#include <uiautomationcoreapi.h>
#include <oleauto.h>
namespace {
template<class T> struct Com {
 T *value=nullptr;
 Com()=default;Com(const Com &)=delete;Com &operator=(const Com &)=delete;
 ~Com(){if(value)value->Release();}
 T **out(){return &value;}
 T *operator->()const{return value;}
 explicit operator bool()const{return value!=nullptr;}
};
struct Apartment {
 HRESULT initialized=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
 ~Apartment(){if(SUCCEEDED(initialized))CoUninitialize();}
};
QString identity(IUIAutomationElement *element){
 SAFEARRAY *array=nullptr;if(FAILED(element->GetRuntimeId(&array))||!array){if(array)SafeArrayDestroy(array);return {};}
 LONG first=0,last=-1;LONG *values=nullptr;QString result;
 if(SafeArrayGetDim(array)==1&&SUCCEEDED(SafeArrayGetLBound(array,1,&first))&&SUCCEEDED(SafeArrayGetUBound(array,1,&last))
    &&last>=first&&last-first<64&&SUCCEEDED(SafeArrayAccessData(array,reinterpret_cast<void**>(&values)))){
  for(LONG i=0;i<=last-first;++i)result+=QString::number(values[i])+',';
  SafeArrayUnaccessData(array);
 }
 SafeArrayDestroy(array);return result.isEmpty()?QString():"uia:"+result;
}
bool rangeText(IUIAutomationTextRange *range,QString &text,bool *readFailed=nullptr){
 BSTR value=nullptr;const auto result=range->GetText(16384,&value);
 if(FAILED(result)){if(readFailed)*readFailed=true;if(value)SysFreeString(value);return false;}
 text=QString::fromWCharArray(value,int(SysStringLen(value)));SysFreeString(value);
 return text.size()<16384&&text.isValidUtf16();
}
struct Session {
 Apartment apartment;Com<IUIAutomation2> api;Com<IUIAutomationElement> element;
 Com<IUIAutomationTextPattern> pattern;Com<IUIAutomationTextRange> caret;
 HWND foreground=nullptr;bool focused=false;
 explicit Session(HWND window=nullptr){
  if(FAILED(CoCreateInstance(CLSID_CUIAutomation8,nullptr,CLSCTX_INPROC_SERVER,IID_IUIAutomation2,reinterpret_cast<void**>(api.out()))))return;
  api->put_ConnectionTimeout(100);api->put_TransactionTimeout(100);
  if(window){api->ElementFromHandle(window,element.out());}
  else {focused=true;foreground=GetForegroundWindow();api->GetFocusedElement(element.out());}
 }
 DesktopTextContext read(){
  DesktopTextContext result;if(!element){result.readFailed=true;return result;}
  int process=0;if(FAILED(element->get_CurrentProcessId(&process))){result.readFailed=true;return result;}
  if(focused&&DWORD(process)==GetCurrentProcessId())return result;
  result.identity=identity(element.value);if(result.identity.isEmpty()){result.readFailed=true;return result;}
  if(focused)result.identity+=':'+QString::number(quintptr(foreground),16);
  BOOL privateField=TRUE;
  if(FAILED(element->get_CurrentIsPassword(&privateField))){result.readFailed=true;return result;}
  if(privateField){result.supported=true;result.privateField=true;return result;}
  CONTROLTYPEID type=0;if(FAILED(element->get_CurrentControlType(&type))){result.readFailed=true;return result;}
  if(type!=UIA_EditControlTypeId){
   VARIANT editable;VariantInit(&editable);
   const auto status=element->GetCurrentPropertyValue(UIA_IsTextEditPatternAvailablePropertyId,&editable);
   const bool available=SUCCEEDED(status)&&editable.vt==VT_BOOL&&editable.boolVal==VARIANT_TRUE;
   VariantClear(&editable);if(FAILED(status))result.readFailed=true;if(!available)return result;
  }
  result.supported=true;
  BOOL hasFocus=FALSE;if(focused){
   if(FAILED(element->get_CurrentHasKeyboardFocus(&hasFocus))){result.readFailed=true;return result;}
   if(!hasFocus)return result;
  }
  VARIANT readOnly;VariantInit(&readOnly);
  const auto readOnlyStatus=element->GetCurrentPropertyValue(UIA_ValueIsReadOnlyPropertyId,&readOnly);
  const bool locked=SUCCEEDED(readOnlyStatus)&&readOnly.vt==VT_BOOL&&readOnly.boolVal==VARIANT_TRUE;
  VariantClear(&readOnly);if(FAILED(readOnlyStatus)){result.readFailed=true;return result;}if(locked)return result;
  Com<IUIAutomationTextEditPattern> edit;
  if(SUCCEEDED(element->GetCurrentPatternAs(UIA_TextEditPatternId,IID_IUIAutomationTextEditPattern,reinterpret_cast<void**>(edit.out())))&&edit){
   Com<IUIAutomationTextRange> composition;
   if(SUCCEEDED(edit->GetActiveComposition(composition.out()))&&composition)return result;
  }
  const auto patternStatus=element->GetCurrentPatternAs(UIA_TextPatternId,IID_IUIAutomationTextPattern,reinterpret_cast<void**>(pattern.out()));
  if(FAILED(patternStatus)||!pattern){result.readFailed=patternStatus==UIA_E_TIMEOUT;return result;}
  Com<IUIAutomationTextRange> document;
  if(FAILED(pattern->get_DocumentRange(document.out()))||!document){result.readFailed=true;return result;}
  QString className,accessibleName;BSTR value=nullptr;
  if(SUCCEEDED(element->get_CurrentClassName(&value)))className=QString::fromWCharArray(value,int(SysStringLen(value)));
  SysFreeString(value);value=nullptr;
  if(className=="native-edit-context"){
   if(SUCCEEDED(element->get_CurrentName(&value)))accessibleName=QString::fromWCharArray(value,int(SysStringLen(value)));
   SysFreeString(value);
  }
  Com<IUIAutomationTextRangeArray> ranges;int count=0;
  if(FAILED(pattern->GetSelection(ranges.out()))||!ranges||FAILED(ranges->get_Length(&count))){result.readFailed=true;return result;}
  if(count==1&&FAILED(ranges->GetElement(0,caret.out()))){result.readFailed=true;return result;}
  auto context=WindowsAutomationContext::readRange(caret.value,document.value,className,accessibleName);
  context.identity=result.identity;
  if(focused&&!stillFocused()){context.available=false;context.readFailed=true;}
  return context;
 }
 bool stillFocused(){
  if(!focused)return true;
  if(GetForegroundWindow()!=foreground)return false;
  Com<IUIAutomationElement> current;BOOL hasFocus=FALSE;
  return SUCCEEDED(api->GetFocusedElement(current.out()))&&current&&identity(current.value)==identity(element.value)
   &&SUCCEEDED(current->get_CurrentHasKeyboardFocus(&hasFocus))&&hasFocus;
 }
 bool replace(const DesktopTextContext &expected,const QString &candidate,const QString &language,bool space,const std::function<bool(const QString &)> &insert){
  auto automationExpected=expected;automationExpected.focusIdentity.clear();automationExpected.windowIdentity.clear();
  const auto current=read();if(current!=automationExpected||!current.available||current.privateField||!caret)return false;
  const auto plan=PredictionLanguage::edit(current.before,current.after,candidate,language,space);
  if(!plan.valid||plan.removeBefore>256||plan.removeAfter>256)return false;
  Com<IUIAutomationTextRange> selection;if(FAILED(caret->Clone(selection.out())))return false;
  const auto left=current.before.right(plan.removeBefore),right=current.after.left(plan.removeAfter);
  // UIA character units are provider-defined. Verify actual text at each move
  // rather than assuming a unit is a UTF-16 code unit or a grapheme.
  QString text;int moved=0;
  for(int i=0;text!=left&&i<256;++i){
   if(FAILED(selection->MoveEndpointByUnit(TextPatternRangeEndpoint_Start,TextUnit_Character,-1,&moved))||moved!=-1||!rangeText(selection.value,text)||!left.endsWith(text))return false;
  }
  if(text!=left)return false;
  for(int i=0;text!=left+right&&i<256;++i){
   if(FAILED(selection->MoveEndpointByUnit(TextPatternRangeEndpoint_End,TextUnit_Character,1,&moved))||moved!=1||!rangeText(selection.value,text)||!(left+right).startsWith(text))return false;
  }
  if(text!=left+right||!stillFocused())return false;
  if(focused&&WindowsAutomationContext::current()!=automationExpected)return false;
  if(FAILED(selection->Select())||!stillFocused())return false;
  return insert(plan.text);
 }
};
}
DesktopTextContext WindowsAutomationContext::readRange(IUIAutomationTextRange *caret,IUIAutomationTextRange *document,
                                                       const QString &className,const QString &accessibleName){
 DesktopTextContext result;result.supported=true;if(!document)return result;
 auto failed=[&](HRESULT status){if(FAILED(status)){result.readFailed=true;return true;}return false;};
 if(className=="native-edit-context"){
  QString text;
  if(rangeText(document,text,&result.readFailed)){
   while(text.endsWith('\r')||text.endsWith('\n'))text.chop(1);
   // Monaco's EditContext has no editor text until screen-reader content is
   // enabled. Chromium may expose its accessible label as the document instead.
   // Retain key-based prediction rather than treating this as an empty editor.
   if(text.isEmpty()||text==accessibleName){result.supported=false;return result;}
  }
 }
 if(!caret)return result;
 int difference=1;
 if(failed(caret->CompareEndpoints(TextPatternRangeEndpoint_Start,caret,TextPatternRangeEndpoint_End,&difference))||difference
    ||failed(caret->CompareEndpoints(TextPatternRangeEndpoint_Start,document,TextPatternRangeEndpoint_Start,&difference))||difference<0
    ||failed(caret->CompareEndpoints(TextPatternRangeEndpoint_End,document,TextPatternRangeEndpoint_End,&difference))||difference>0)return result;
 Com<IUIAutomationTextRange> before,after;int moved=0;QString left,right;
 if(failed(caret->Clone(before.out()))||!before||failed(caret->Clone(after.out()))||!after
    ||failed(before->MoveEndpointByUnit(TextPatternRangeEndpoint_Start,TextUnit_Character,-4096,&moved))
    ||failed(after->MoveEndpointByUnit(TextPatternRangeEndpoint_End,TextUnit_Character,4096,&moved)))return result;
 // Chromium ranges can navigate beyond an embedded editor into the surrounding
 // page. Clamp before reading, rather than accepting neighboring UI as text.
 if(failed(before->CompareEndpoints(TextPatternRangeEndpoint_Start,document,TextPatternRangeEndpoint_Start,&difference)))return result;
 if(difference<0&&failed(before->MoveEndpointByRange(TextPatternRangeEndpoint_Start,document,TextPatternRangeEndpoint_Start)))return result;
 if(failed(after->CompareEndpoints(TextPatternRangeEndpoint_End,document,TextPatternRangeEndpoint_End,&difference)))return result;
 if(difference>0&&failed(after->MoveEndpointByRange(TextPatternRangeEndpoint_End,document,TextPatternRangeEndpoint_End)))return result;
 if(!rangeText(before.value,left,&result.readFailed)||!rangeText(after.value,right,&result.readFailed))return result;
 return DesktopTextContext::fromUtf8((left+right).toUtf8(),left.toUtf8().size(),left.toUtf8().size());
}
DesktopTextContext WindowsAutomationContext::current(){
 DWORD process=0;GetWindowThreadProcessId(GetForegroundWindow(),&process);
 if(!process||process==GetCurrentProcessId())return {};
 Session session;return session.read();
}
DesktopTextContext WindowsAutomationContext::readWindow(HWND window){Session session(window);return session.read();}
bool WindowsAutomationContext::replaceWindow(HWND window,const DesktopTextContext &expected,const QString &candidate,const QString &language,bool space,const std::function<bool(const QString &)> &insert){
 Session session(window);return session.replace(expected,candidate,language,space,insert);
}
bool WindowsAutomationContext::replaceCurrent(const DesktopTextContext &expected,const QString &candidate,const QString &language,bool space){
 DWORD process=0;GetWindowThreadProcessId(GetForegroundWindow(),&process);
 if(!process||process==GetCurrentProcessId())return false;
 Session session;return session.replace(expected,candidate,language,space,[](const QString &text){WindowsInput input;return input.text(text);});
}
