#include "WindowsTextContext.h"
#include "WindowsAutomationContext.h"
#include <vector>
#include <imm.h>
#include "PredictionLanguage.h"
namespace {
bool message(HWND window,UINT message,WPARAM first,LPARAM second,DWORD_PTR &result){
 return SendMessageTimeoutW(window,message,first,second,SMTO_ABORTIFHUNG|SMTO_BLOCK,40,&result)!=0;
}
}
DesktopTextContext WindowsTextContext::readControl(HWND control){
 DesktopTextContext result;result.identity=QString::number(quintptr(control),16);
 if(!control||!IsWindow(control))return result;
 wchar_t name[128]{};if(!GetClassNameW(control,name,128))return result;
 const auto type=QString::fromWCharArray(name).toLower();
 if((type!="edit"&&!type.startsWith("richedit"))||!IsWindowUnicode(control))return result;
 result.supported=true;
 DWORD_PTR value=0;
 if((GetWindowLongPtrW(control,GWL_STYLE)&ES_PASSWORD)||!message(control,EM_GETPASSWORDCHAR,0,0,value)||value){
  result.privateField=true;return result;
 }
 const auto style=GetWindowLongPtrW(control,GWL_STYLE);
 if((style&ES_READONLY)||(type=="edit"&&(style&ES_NUMBER)))return result;
 if(const auto ime=ImmGetContext(control)){
  const auto composing=ImmGetCompositionStringW(ime,GCS_COMPSTR,nullptr,0);ImmReleaseContext(control,ime);
  if(composing>0)return result;
 }
 DWORD start=0,end=0;
 if(!message(control,EM_GETSEL,WPARAM(&start),LPARAM(&end),value)||start!=end)return result;
 if(!message(control,WM_GETTEXTLENGTH,0,0,value)||value>1024*1024||(type.startsWith("richedit")&&value>65535)||start>value)return result;
 std::vector<wchar_t> buffer(size_t(value)+1,0);
 if(!message(control,WM_GETTEXT,buffer.size(),LPARAM(buffer.data()),value)||value>=buffer.size())return result;
 auto text=QString::fromWCharArray(buffer.data(),int(value));
 if(type.startsWith("richedit")&&type!="richedit")text.replace("\r\n","\r");
 DWORD verifyStart=0,verifyEnd=0;
 if(start>DWORD(text.size())||!text.isValidUtf16()||!message(control,EM_GETSEL,WPARAM(&verifyStart),LPARAM(&verifyEnd),value)
    ||start!=verifyStart||end!=verifyEnd)return result;
 if(!text.isEmpty()){
  QTextBoundaryFinder boundary(QTextBoundaryFinder::Grapheme,text);boundary.setPosition(int(start));if(!boundary.isAtBoundary())return result;
 }
 result.before=text.left(int(start)).right(4096);result.after=text.mid(int(start),4096);
 if(!result.before.isEmpty()&&result.before.front().isLowSurrogate())result.before.remove(0,1);
 if(!result.after.isEmpty()&&result.after.back().isHighSurrogate())result.after.chop(1);
 result.cursor=int(start);result.available=true;return result;
}
bool WindowsTextContext::replaceControl(HWND control,const DesktopTextContext &expected,const QString &candidate,const QString &language,bool space){
 if(!expected.available||expected.privateField||readControl(control)!=expected)return false;
 const auto edit=PredictionLanguage::edit(expected.before,expected.after,candidate,language,space);
 if(!edit.valid||edit.removeAfter>=4096)return false;
 if(edit.text.isEmpty()&&!edit.removeBefore&&!edit.removeAfter)return true;
 DWORD_PTR result=0;
 if(!message(control,EM_SETSEL,expected.cursor-edit.removeBefore,expected.cursor+edit.removeAfter,result))return false;
 return message(control,EM_REPLACESEL,TRUE,LPARAM(edit.text.utf16()),result);
}
bool WindowsTextContext::replaceCurrent(const DesktopTextContext &expected,const QString &candidate,const QString &language,bool space){
 if(expected.identity.startsWith("uia:"))return WindowsAutomationContext::replaceCurrent(expected,candidate,language,space);
 if(current()!=expected)return false;
 bool valid=false;const auto handle=expected.identity.toULongLong(&valid,16);if(!valid)return false;
 return replaceControl(reinterpret_cast<HWND>(quintptr(handle)),expected,candidate,language,space);
}
DesktopTextContext WindowsTextContext::current(){
 if(!WindowsDesktop::inputDesktopActive())return {};
 const auto window=GetForegroundWindow();if(!window)return {};
 DWORD process=0;const auto thread=GetWindowThreadProcessId(window,&process);
 if(process==GetCurrentProcessId())return {};
 GUITHREADINFO info{};info.cbSize=sizeof(info);
 if(!GetGUIThreadInfo(thread,&info)||!info.hwndFocus)return {};
 const auto focus=info.hwndFocus;auto result=readControl(focus);
 if(!result.supported){const auto automation=WindowsAutomationContext::current();if(automation.supported||!automation.identity.isEmpty())result=automation;}
 GUITHREADINFO verify{};verify.cbSize=sizeof(verify);
 if(GetForegroundWindow()!=window||!GetGUIThreadInfo(thread,&verify)||verify.hwndFocus!=focus)return {};
 return result;
}
void WindowsTextContext::run(){
 while(!isInterruptionRequested()){
  const auto context=enabled?current():DesktopTextContext{};
  // The controller also performs synchronous reads. Repeat stable snapshots so
  // a transient result from one of those reads cannot leave it out of sync.
  emit contextChanged(context);
  msleep(150);
 }
}
