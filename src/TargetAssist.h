#pragma once
#include <QObject>
#include <QThread>
#include <QElapsedTimer>
#include <QRectF>
#include <QList>

// Accessibility queries run away from the gaze/input thread.
class TargetAssist:public QObject {
public:
 explicit TargetAssist(QObject *parent=nullptr):QObject(parent){clock.start();}
 ~TargetAssist(){thread.quit();thread.wait();}
 QPointF adjust(QPointF normalized);
 void reset(){bounds.clear();selected={};++generation;}
 void configure(int strength,int range,int retention);
 static QPointF resolve(QPointF gaze,const QList<QRectF> &controls,QRectF &selected,int strength=65,int range=40,int retention=8);
private:
 QThread thread;
 QObject *worker=nullptr;
 QElapsedTimer clock;
 QList<QRectF> bounds;
 QRectF selected;
 qint64 requested=-1000,received=-1000;
 quintptr sourceWindow=0;
 int strengthValue=65,rangeValue=40,retentionValue=8;
 int generation=0;
 bool pending=false;
};
