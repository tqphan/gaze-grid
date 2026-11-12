#include "TargetAssist.h"
#include <cmath>
#include <limits>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objbase.h>
#include <uiautomationclient.h>
namespace {
constexpr int discoveryBudgetMs=500,resultLifetimeMs=750,maxRequestAgeMs=1000;
template<class T> struct Com {
 T *p=nullptr;
 Com()=default;Com(const Com &)=delete;Com &operator=(const Com &)=delete;
 ~Com(){if(p)p->Release();}
 T **out(){return &p;}
 T *operator->()const{return p;}
};
QRectF rectangle(IUIAutomationElement *element){
 RECT rect{};
 if(FAILED(element->get_CachedBoundingRectangle(&rect)))return {};
 return {double(rect.left),double(rect.top),double(rect.right-rect.left),double(rect.bottom-rect.top)};
}
bool actionable(IUIAutomationElement *element){
 BOOL enabled=FALSE,offscreen=TRUE;CONTROLTYPEID type=0;
 if(FAILED(element->get_CachedIsEnabled(&enabled))||!enabled||FAILED(element->get_CachedIsOffscreen(&offscreen))||offscreen)return false;
 if(FAILED(element->get_CachedControlType(&type)))return false;
 return type==UIA_ButtonControlTypeId||type==UIA_HyperlinkControlTypeId||type==UIA_CheckBoxControlTypeId||type==UIA_RadioButtonControlTypeId||type==UIA_ComboBoxControlTypeId||type==UIA_TabItemControlTypeId||type==UIA_MenuItemControlTypeId||type==UIA_ListItemControlTypeId||type==UIA_EditControlTypeId;
}
struct Apartment {
 HRESULT initialized=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
 ~Apartment(){if(SUCCEEDED(initialized))CoUninitialize();}
};
struct DiscoveryContext {
 Apartment apartment;
 Com<IUIAutomation2> api;
 Com<IUIAutomationCacheRequest> cache;
 DiscoveryContext(){
  if(FAILED(apartment.initialized)||FAILED(CoCreateInstance(CLSID_CUIAutomation8,nullptr,CLSCTX_INPROC_SERVER,IID_IUIAutomation2,reinterpret_cast<void**>(api.out()))))return;
  api->put_ConnectionTimeout(100);api->put_TransactionTimeout(100);
  if(FAILED(api->CreateCacheRequest(cache.out())))return;
  for(PROPERTYID property:{UIA_BoundingRectanglePropertyId,UIA_IsEnabledPropertyId,UIA_IsOffscreenPropertyId,UIA_ControlTypePropertyId})cache->AddProperty(property);
  cache->put_TreeScope(static_cast<TreeScope>(TreeScope_Element|TreeScope_Children));
 }
};
QList<QRectF> discover(QPointF gaze,int range){
 // Reuse the worker's COM connection so provider startup does not consume every
 // request's deadline. All cached element snapshots are still refreshed below.
 thread_local DiscoveryContext context;
 auto &api=context.api;auto &cache=context.cache;
 QList<QRectF> result;
 if(api.p&&cache.p){
   // Search only intersecting containers in the windows under the gaze area.
   // Sparse point samples miss small controls, and often hit a button's label.
   const QRectF area(gaze.x()-range,gaze.y()-range,2*range,2*range);
   QElapsedTimer budget;budget.start();int visited=0;QList<HWND> windows;
   for(const QPoint offset:{QPoint(0,0),QPoint(-range,0),QPoint(range,0),QPoint(0,-range),QPoint(0,range)}){
    if(budget.elapsed()>=discoveryBudgetMs||visited>=256)break;
    POINT point{LONG(std::lround(gaze.x()+offset.x())),LONG(std::lround(gaze.y()+offset.y()))};
    const HWND window=GetAncestor(WindowFromPoint(point),GA_ROOT);
    if(!window||windows.contains(window))continue;
    windows.append(window);
    Com<IUIAutomationElement> root;
    if(FAILED(api->ElementFromHandle(window,root.out()))||!root.p)continue;
    const auto visit=[&](auto &&self,IUIAutomationElement *element,int depth)->void{
     if(depth>16||visited++>=256||budget.elapsed()>=discoveryBudgetMs)return;
     Com<IUIAutomationElement> snapshot;
     if(FAILED(element->BuildUpdatedCache(cache.p,snapshot.out()))||!snapshot.p)return;
     const auto bounds=rectangle(snapshot.p);
     if(!bounds.intersects(area))return;
     if(actionable(snapshot.p)&&bounds.width()>=4&&bounds.height()>=4&&bounds.width()<=600&&bounds.height()<=200){
      // Do not attract to controls covered by another top-level window.
      POINT nearest{LONG(std::lround(qBound(bounds.left()+1,gaze.x(),bounds.right()-1))),LONG(std::lround(qBound(bounds.top()+1,gaze.y(),bounds.bottom()-1)))};
      if(GetAncestor(WindowFromPoint(nearest),GA_ROOT)==window&&!result.contains(bounds))result.append(bounds);
      return; // Labels inside an actionable control are not separate targets.
     }
     Com<IUIAutomationElementArray> children;int count=0;
     if(FAILED(snapshot->GetCachedChildren(children.out()))||!children.p||FAILED(children->get_Length(&count)))return;
     for(int i=0;i<count&&visited<256&&budget.elapsed()<discoveryBudgetMs;++i){
      Com<IUIAutomationElement> child;
      if(SUCCEEDED(children->GetElement(i,child.out()))&&child.p&&rectangle(child.p).intersects(area))self(self,child.p,depth+1);
     }
    };
    visit(visit,root.p,0);
   }
 }
 return result;
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
   QMetaObject::invokeMethod(this,[this,result,requestGeneration]{pending=false;if(requestGeneration==generation&&clock.elapsed()-requested<=maxRequestAgeMs){bounds=result;received=clock.elapsed();}},Qt::QueuedConnection);
  },Qt::QueuedConnection);
 }
 if(now-received>resultLifetimeMs){selected={};return normalized;}
 const QPointF target=resolve(gaze,bounds,selected,strengthValue,rangeValue,retentionValue);
 return {qBound(0.,target.x()/screen.width(),1.),qBound(0.,target.y()/screen.height(),1.)};
#else
 return normalized;
#endif
}
