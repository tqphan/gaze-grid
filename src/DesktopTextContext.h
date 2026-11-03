#pragma once
#include <QString>
#include <QByteArray>
#include <QMetaType>
#include <QTextBoundaryFinder>
struct DesktopTextContext {
 QString before,after,identity;
 int cursor=-1;
 bool supported=false,available=false,privateField=false;
 bool operator==(const DesktopTextContext &)const=default;
 static DesktopTextContext fromUtf8(const QByteArray &text,int cursor,int anchor){
  DesktopTextContext result;result.supported=true;
  if(cursor<0||anchor<0||cursor>text.size()||anchor>text.size()||cursor!=anchor||text.contains('\0'))return result;
  const auto before=QString::fromUtf8(text.left(cursor)),after=QString::fromUtf8(text.mid(cursor));
  if(before.toUtf8()!=text.left(cursor)||after.toUtf8()!=text.mid(cursor))return result;
  QTextBoundaryFinder boundary(QTextBoundaryFinder::Grapheme,before+after);boundary.setPosition(before.size());
  if(!boundary.isAtBoundary())return result;
  result.before=before.right(4096);result.after=after.left(4096);
  if(!result.before.isEmpty()&&result.before.front().isLowSurrogate())result.before.remove(0,1);
  if(!result.after.isEmpty()&&result.after.back().isHighSurrogate())result.after.chop(1);
  result.cursor=before.size();result.available=true;return result;
 }
};
Q_DECLARE_METATYPE(DesktopTextContext)
