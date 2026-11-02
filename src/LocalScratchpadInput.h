#pragma once
#include <QPointer>
#include <QStringList>
#include <QObject>
#include <QVariant>

// Delivers events only to the explicitly registered, app-owned text control.
// No desktop input, native keyboard messages, or layout activation.
class LocalScratchpadInput {
public:
 void setEditor(QObject *value){editor=value;failure.clear();}
 void setEngine(QObject *value){engine=value;failure.clear();}
 QObject *inputEngine()const{return engine;}
 QStringList availableLocales()const;
 QString locale()const;
 QString inputLabel(const QString &text,bool shifted=false)const;
 QStringList candidates()const;
 bool setLocale(const QString &locale);
 bool selectCandidate(int index,const QString &expected);
 bool attached()const{return !editor.isNull();}
 bool atEnd()const;
 bool composing()const{return editor&&editor->property("inputMethodComposing").toBool();}
 bool predictionContext(QString &text,int &cursor)const;
 bool complete(const QString &candidate,const QString &language,bool space,const QString &expected,int cursor);
 bool clear();
 void cancelComposition();
 bool text(const QString &text,const QStringList &held,bool useEngine);
 bool key(const QString &name,const QStringList &held,bool useEngine);
 QString error()const{return failure;}
private:
 bool ready();
 bool dispatch(int key,const QString &text,int modifiers,bool useEngine);
 QPointer<QObject> editor,engine;
 QString failure;
};
