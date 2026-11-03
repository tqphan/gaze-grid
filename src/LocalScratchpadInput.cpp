#include "LocalScratchpadInput.h"
#include "KeyNames.h"
#include "PredictionLanguage.h"
#include <QTextBoundaryFinder>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QInputMethod>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QVariant>
#include <QMap>
#include <QThread>
#include <QAbstractItemModel>

namespace {
QAbstractItemModel *candidateModel(QObject *engine){
 return engine?qobject_cast<QAbstractItemModel*>(engine->property("candidateModel").value<QObject*>()):nullptr;
}
int modifiers(const QStringList &held){
 int result=Qt::NoModifier;
 for(const auto &name:held){
  if(name=="LSHIFT"||name=="RSHIFT")result|=Qt::ShiftModifier;
  if(name=="LCONTROL"||name=="RCONTROL")result|=Qt::ControlModifier;
  if(name=="LMENU"||name=="RMENU")result|=Qt::AltModifier;
  if(name=="LWIN"||name=="RWIN")result|=Qt::MetaModifier;
 }
 return result;
}
}
QStringList LocalScratchpadInput::availableLocales()const{return engine?engine->property("availableLocales").toStringList():QStringList();}
QString LocalScratchpadInput::locale()const{return engine?engine->property("locale").toString():QString();}
QString LocalScratchpadInput::inputLabel(const QString &text,bool shifted)const{
 if(!locale().startsWith("ko_")||text.size()!=1)return text;
 const QString latin="qwertyuiopasdfghjklzxcvbnm";
 const QString jamo=QString::fromUtf8("\u3142\u3148\u3137\u3131\u3145\u315b\u3155\u3151\u3150\u3154\u3141\u3134\u3147\u3139\u314e\u3157\u3153\u314f\u3163\u314b\u314c\u314a\u314d\u3160\u315c\u3161");
 const auto ch=text.front().toLower();const int index=latin.indexOf(ch);if(index<0)return text;
 if(shifted||text.front().isUpper()){
  const QString doubles="qwertop";
  const QString tense=QString::fromUtf8("\u3143\u3149\u3138\u3132\u3146\u3152\u3156");
  const int doubled=doubles.indexOf(ch);if(doubled>=0)return QString(tense[doubled]);
 }
 return QString(jamo[index]);
}
QStringList LocalScratchpadInput::candidates()const{
 QStringList result;auto model=candidateModel(engine);if(!model)return result;
 for(int i=0;i<qMin(model->rowCount(),2048);++i)result<<model->data(model->index(i,0),Qt::DisplayRole).toString();
 return result;
}
bool LocalScratchpadInput::setLocale(const QString &value){
 failure.clear();
 if(!engine||engine->thread()!=QThread::currentThread()||!availableLocales().contains(value)){failure="Local input language unavailable";return false;}
 QVariant accepted;
 const bool invoked=QMetaObject::invokeMethod(engine,"setLocale",Qt::DirectConnection,Q_RETURN_ARG(QVariant,accepted),Q_ARG(QVariant,QVariant(value)));
 if(!invoked||!accepted.toBool()){failure="Local input language could not be selected";return false;}
 return true;
}
bool LocalScratchpadInput::selectCandidate(int index,const QString &expected){
 if(!ready())return false;
 if(!engine||engine->thread()!=QThread::currentThread()||(engine->property("requiresFocus").toBool()&&QGuiApplication::focusObject()!=editor)){failure="Focus the scratchpad before selecting a candidate";return false;}
 auto model=candidateModel(engine);
 if(!model||index<0||index>=model->rowCount()||expected.isEmpty()||model->data(model->index(index,0),Qt::DisplayRole).toString()!=expected){failure="Candidate is no longer available";return false;}
 QVariant accepted;
 const bool invoked=QMetaObject::invokeMethod(engine,"selectCandidate",Qt::DirectConnection,Q_RETURN_ARG(QVariant,accepted),Q_ARG(QVariant,QVariant(index)));
 if(!invoked||!accepted.toBool()){failure="Candidate could not be selected";return false;}
 return true;
}
bool LocalScratchpadInput::atEnd()const{
 return editor&&editor->property("cursorPosition").toInt()==editor->property("text").toString().size()
  &&editor->property("selectionStart")==editor->property("selectionEnd");
}
bool LocalScratchpadInput::predictionContext(QString &text,int &cursor)const{
 if(!editor||composing()||!editor->property("cursorPosition").isValid())return false;
 text=editor->property("text").toString();cursor=editor->property("cursorPosition").toInt();
 if(cursor<0||cursor>text.size()||!text.isValidUtf16()
    ||editor->property("selectionStart")!=editor->property("selectionEnd"))return false;
 QTextBoundaryFinder boundary(QTextBoundaryFinder::Grapheme,text);boundary.setPosition(cursor);
 return boundary.isAtBoundary();
}
bool LocalScratchpadInput::complete(const QString &candidate,const QString &language,bool space,const QString &expected,int cursor){
 if(!ready())return false;
 QString text;int position=0;
 if(!predictionContext(text,position)||text!=expected||position!=cursor){failure="Prediction context changed";return false;}
 const auto edit=PredictionLanguage::edit(text.left(cursor),text.mid(cursor),candidate,language,space);
 if(!edit.valid){failure="Prediction no longer matches";return false;}
 QInputMethodEvent event;event.setCommitString(edit.text,-edit.removeBefore,edit.removeBefore+edit.removeAfter);QCoreApplication::sendEvent(editor,&event);
 return true;
}
bool LocalScratchpadInput::clear(){
 if(!ready())return false;
 cancelComposition();
 const bool cleared=QMetaObject::invokeMethod(editor,"clear",Qt::DirectConnection);
 if(!cleared)failure="Local scratchpad could not be cleared";
 return cleared;
}
void LocalScratchpadInput::cancelComposition(){
 if(!editor||editor->thread()!=QThread::currentThread())return;
 const bool focused=QGuiApplication::focusObject()==editor;
 if(focused)QGuiApplication::inputMethod()->reset();
 if(engine&&engine->thread()==QThread::currentThread()&&(focused||!engine->property("requiresFocus").toBool())&&engine->metaObject()->indexOfMethod("cancel()")>=0)
  QMetaObject::invokeMethod(engine,"cancel",Qt::DirectConnection);
 QInputMethodEvent cancel;QCoreApplication::sendEvent(editor,&cancel);
}
bool LocalScratchpadInput::ready(){
 failure.clear();
 if(!editor||editor->thread()!=QThread::currentThread()||editor->property("readOnly").toBool()){
  failure="Local scratchpad is unavailable";return false;
 }
 QMetaObject::invokeMethod(editor,"prepareInput",Qt::DirectConnection);
 return true;
}
bool LocalScratchpadInput::dispatch(int key,const QString &text,int mods,bool useEngine){
 if(useEngine){
  if(!engine||engine->thread()!=QThread::currentThread()){failure="Local input method unavailable; launch with --local-ime and Qt Virtual Keyboard installed";return false;}
  if(engine->property("requiresFocus").toBool()&&QGuiApplication::focusObject()!=editor){failure="Focus the scratchpad before using the local input method";return false;}
  QVariant accepted;
  const auto effective=mods&(Qt::ControlModifier|Qt::AltModifier|Qt::MetaModifier)?text:inputLabel(text,mods&Qt::ShiftModifier);
  const bool invoked=QMetaObject::invokeMethod(engine,"dispatch",Qt::DirectConnection,Q_RETURN_ARG(QVariant,accepted),Q_ARG(QVariant,QVariant(key)),Q_ARG(QVariant,QVariant(effective)),Q_ARG(QVariant,QVariant(mods)));
  if(!invoked||!accepted.toBool()){failure="Local input method did not accept the key";return false;}
  return true;
 }
 if(editor->property("inputMethodComposing").toBool()){
  failure="Finish the current composition before using personal keys";return false;
 }
 QKeyEvent press(QEvent::KeyPress,key,Qt::KeyboardModifiers(mods),text);
 QCoreApplication::sendEvent(editor,&press);
 QKeyEvent release(QEvent::KeyRelease,key,Qt::KeyboardModifiers(mods),text);
 QCoreApplication::sendEvent(editor,&release);return true;
}
bool LocalScratchpadInput::text(const QString &value,const QStringList &held,bool useEngine){
 if(!ready())return false;
 if(!value.isValidUtf16()||value.contains(QChar(0))){failure="Invalid scratchpad text";return false;}
 const int mods=modifiers(held);
 if(!useEngine&&!(mods&(Qt::ControlModifier|Qt::AltModifier|Qt::MetaModifier))&&!value.contains('\r')&&!value.contains('\n')&&!value.contains('\t')){
  if(editor->property("inputMethodComposing").toBool()){failure="Finish the current composition before using personal keys";return false;}
  QInputMethodEvent event;event.setCommitString(value);QCoreApplication::sendEvent(editor,&event);return true;
 }
 // Virtual keystrokes need actual key identities; reject an unsupported macro before any part is sent.
 for(const auto ch:value)if(KeyNames::usCharacter(ch).first.isEmpty()){failure="This text requires a committed-text input method";return false;}
 for(const auto ch:value){
  int code=ch=='\r'||ch=='\n'?Qt::Key_Return:ch=='\t'?Qt::Key_Tab:ch.toUpper().unicode();
  if(!dispatch(code,ch=='\r'?QString("\n"):QString(ch),mods,useEngine))return false;
 }
 return true;
}
bool LocalScratchpadInput::key(const QString &name,const QStringList &held,bool useEngine){
 if(!ready())return false;
 static const QMap<QString,int> keys{{"BACK",Qt::Key_Backspace},{"DELETE",Qt::Key_Delete},{"RETURN",Qt::Key_Return},{"TAB",Qt::Key_Tab},{"ESCAPE",Qt::Key_Escape},{"LEFT",Qt::Key_Left},{"RIGHT",Qt::Key_Right},{"UP",Qt::Key_Up},{"DOWN",Qt::Key_Down},{"HOME",Qt::Key_Home},{"END",Qt::Key_End},{"PRIOR",Qt::Key_PageUp},{"NEXT",Qt::Key_PageDown},{"SPACE",Qt::Key_Space}};
 const auto canonical=KeyNames::canonical(name);
 int code=keys.value(canonical,0);
 if(!code&&canonical.size()==1)code=canonical.front().unicode();
 if(!code){failure="Unsupported local key: "+name;return false;}
 return dispatch(code,canonical=="RETURN"?QString("\n"):canonical=="TAB"?QString("\t"):canonical=="SPACE"?QString(" "):QString(),modifiers(held),useEngine);
}
