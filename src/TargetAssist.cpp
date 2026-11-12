#include "TargetAssist.h"
#include <cmath>
#include <limits>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <uiautomationclient.h>
namespace {
template<class T> struct Com {
 T *p=nullptr;
 ~Com(){if(p)p->Release();}
 T **out(){return &p;}
 T *operator->()const{return p;}
};
QList<QRectF> discover(QPointF gaze,int range){
 QList<QRectF> result;
 const HRESULT apartment=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
 if(FAILED(apartment))return result;
 {
 Com<IUIAutomation2> api;
 if(SUCCEEDED(CoCreateInstance(CLSID_CUIAutomation8,nullptr,CLSCTX_INPROC_SERVER,IID_IUIAutomation2,reinterpret_cast<void**>(api.out())))){
  api->put_ConnectionTimeout(50);api->put_TransactionTimeout(50);
  // Point queries respect stacking/occlusion, unlike a desktop-wide tree scan.
  const int axis=qRound(range*0.8),diagonal=qRound(range*0.6);
  for(const QPoint offset: {QPoint(0,0),QPoint(-axis,0),QPoint(axis,0),QPoint(0,-axis),QPoint(0,axis),QPoint(-diagonal,-diagonal),QPoint(diagonal,-diagonal),QPoint(-diagonal,diagonal),QPoint(diagonal,diagonal)}){
   Com<IUIAutomationElement> element;
   POINT point{LONG(std::lround(gaze.x()+offset.x())),LONG(std::lround(gaze.y()+offset.y()))};
   if(FAILED(api->ElementFromPoint(point,element.out()))||!element.p)continue;
   BOOL enabled=FALSE,offscreen=TRUE;CONTROLTYPEID type=0;RECT rect{};
   if(FAILED(element->get_CurrentIsEnabled(&enabled))||!enabled||FAILED(element->get_CurrentIsOffscreen(&offscreen))||offscreen)continue;
   if(FAILED(element->get_CurrentControlType(&type)))continue;
   if(type!=UIA_ButtonControlTypeId&&type!=UIA_HyperlinkControlTypeId&&type!=UIA_CheckBoxControlTypeId&&type!=UIA_RadioButtonControlTypeId&&type!=UIA_ComboBoxControlTypeId&&type!=UIA_TabItemControlTypeId&&type!=UIA_MenuItemControlTypeId&&type!=UIA_ListItemControlTypeId&&type!=UIA_EditControlTypeId)continue;
   if(FAILED(element->get_CurrentBoundingRectangle(&rect)))continue;
   QRectF bounds(rect.left,rect.top,rect.right-rect.left,rect.bottom-rect.top);
   if(bounds.width()>=4&&bounds.height()>=4&&bounds.width()<=600&&bounds.height()<=200&&!result.contains(bounds))result.append(bounds);
  }
 }
 }
 CoUninitialize();return result;
}
}
#endif

void TargetAssist::configure(int strength,int range,int retention){
 strength=qBound(0,strength,100);range=qBound(10,range,200);retention=qBound(0,retention,40);
 if(strength==strengthValue&&range==rangeValue&&retention==retentionValue)return;
 strengthValue=strength;rangeValue=range;retentionValue=retention;reset();
}

QPointF TargetAssist::resolve(QPointF gaze,const QList<QRectF> &controls,QRectF &selected,int strength,int range,int retention){
 strength=qBound(0,strength,100);range=qBound(10,range,200);retention=qBound(0,retention,40);
 QRectF best;double score=std::numeric_limits<double>::infinity(),runner=score;
 for(const auto &rect:controls){
  if(!rect.isValid())continue;
  QPointF nearest(qBound(rect.left(),gaze.x(),rect.right()),qBound(rect.top(),gaze.y(),rect.bottom()));
  const double distance=std::hypot(gaze.x()-nearest.x(),gaze.y()-nearest.y());
  if(distance>range)continue;
  double candidate=distance+0.12*std::hypot(gaze.x()-rect.center().x(),gaze.y()-rect.center().y());
  if(rect==selected)candidate-=retention;
  if(candidate<score){runner=score;score=candidate;best=rect;}else runner=qMin(runner,candidate);
 }
 if(!best.isValid()||runner-score<4){selected={};return gaze;}
 selected=best;
 const QPointF nearest(qBound(best.left(),gaze.x(),best.right()),qBound(best.top(),gaze.y(),best.bottom()));
 const double distance=std::hypot(gaze.x()-nearest.x(),gaze.y()-nearest.y());
 const double pullStrength=(strength/100.)*(1-distance/range);
 QPointF pull=(best.center()-gaze)*pullStrength;
 const double length=std::hypot(pull.x(),pull.y());
 if(length>24)pull*=24/length;
 return gaze+pull;
}

QPointF TargetAssist::adjust(QPointF normalized){
#ifdef _WIN32
 const QSizeF screen(GetSystemMetrics(SM_CXSCREEN)-1,GetSystemMetrics(SM_CYSCREEN)-1);
 if(screen.width()<=0||screen.height()<=0)return normalized;
 QPointF gaze(qBound(0.,normalized.x(),1.)*screen.width(),qBound(0.,normalized.y(),1.)*screen.height());
 const auto now=clock.elapsed();
 const auto foreground=reinterpret_cast<quintptr>(GetForegroundWindow());
 if(sourceWindow!=foreground){reset();sourceWindow=foreground;}
 if(!pending&&now-requested>=100){
  if(!worker){worker=new QObject;worker->moveToThread(&thread);connect(&thread,&QThread::finished,worker,&QObject::deleteLater);thread.start();}
  pending=true;requested=now;const int requestGeneration=generation,requestRange=rangeValue;
  QMetaObject::invokeMethod(worker,[this,gaze,requestGeneration,requestRange]{
   const auto result=discover(gaze,requestRange);
   QMetaObject::invokeMethod(this,[this,result,requestGeneration]{pending=false;if(requestGeneration==generation&&clock.elapsed()-requested<=250){bounds=result;received=clock.elapsed();}},Qt::QueuedConnection);
  },Qt::QueuedConnection);
 }
 if(now-received>250){selected={};return normalized;}
 const QPointF target=resolve(gaze,bounds,selected,strengthValue,rangeValue,retentionValue);
 return {qBound(0.,target.x()/screen.width(),1.),qBound(0.,target.y()/screen.height(),1.)};
#else
 return normalized;
#endif
}
