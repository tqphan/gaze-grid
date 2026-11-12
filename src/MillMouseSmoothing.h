#pragma once
#include <QPointF>
#include <QSizeF>
#include <algorithm>
#include <cmath>
#include <optional>

// Independent implementation of the cursor interpolation recovered from
// Mill Mouse 8.39. Response selects the recovered denominator branches;
// application-specific mode flags are deliberately not reproduced.
class MillMouseSmoothing {
public:
 struct Settings {
  int speed=0,response=0;
  bool adaptive=true,snap=true,cursorFeedback=true;
  double jitterRadius=0,snapDistance=50;
  bool operator==(const Settings &)const=default;
 };
 void reset(){initialized=false;estimate={};}
 void configure(Settings value){
  value.speed=std::clamp(value.speed,-5,5);value.response=std::clamp(value.response,0,1);
  if(!std::isfinite(value.jitterRadius)||!std::isfinite(value.snapDistance))return;
  value.jitterRadius=std::clamp(value.jitterRadius,0.,40.);
  value.snapDistance=std::clamp(value.snapDistance,1.,100.);
  if(value==settings)return;
  settings=value;reset();
 }
 QPointF update(QPointF incoming,QSizeF screen,std::optional<QPointF> cursor=std::nullopt){
  if(!finite(incoming)||!std::isfinite(screen.width())||!std::isfinite(screen.height())||screen.width()<=0||screen.height()<=0){reset();return incoming;}
  if(cursor&&!finite(*cursor))cursor.reset();
  if(!settings.cursorFeedback)cursor.reset();
  if(!initialized){initialized=true;estimate=cursor.value_or(incoming);if(!cursor)return incoming;}
  const auto reference=cursor.value_or(estimate),delta=incoming-reference;
  const double distance=std::max(std::abs(delta.x()),std::abs(delta.y()));
  const double snapRadius=std::min(screen.width(),screen.height())*settings.snapDistance/100.;
  if(settings.snap&&distance>=snapRadius){estimate=incoming;return estimate;}
  int step=settings.speed+3;
  const double jitter=settings.jitterRadius>0?settings.jitterRadius:(settings.speed+1)*.5;
  if(settings.adaptive&&settings.speed>0&&distance<=jitter)step=settings.speed-5;
  double denominator;
  if(settings.response==1&&step>=4)denominator=20.-.5*(step-4);
  else if(step<=6)denominator=50.-7.5*step;
  else denominator=5.-(step-6);
  estimate=denominator>1?reference+delta/denominator:incoming;
  return estimate;
 }
private:
 static bool finite(QPointF value){return std::isfinite(value.x())&&std::isfinite(value.y());}
 Settings settings;
 bool initialized=false;
 QPointF estimate;
};
