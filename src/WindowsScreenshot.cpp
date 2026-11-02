#include "ScreenshotPortal.h"
#include <QGuiApplication>
#include <QScreen>
#include <QWindow>
#include <qt_windows.h>
#include <cstring>
namespace {
QImage captureDesktop(){
 // CAPTUREBLT (used by QScreen::grabWindow) can make the hardware cursor
 // blink on every capture. Copy the composed desktop without that flag.
 const auto screen=QGuiApplication::primaryScreen();
 if(!screen)return {};
 const auto desktop=GetDC(nullptr);
 if(!desktop)return {};
 const int width=GetSystemMetrics(SM_CXSCREEN),height=GetSystemMetrics(SM_CYSCREEN);
 QImage image(width,height,QImage::Format_RGB32);
 const auto memory=CreateCompatibleDC(desktop);
 BITMAPINFO info{};
 info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
 info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;
 info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
 info.bmiHeader.biCompression=BI_RGB;
 void *pixels=nullptr;
 const auto bitmap=CreateDIBSection(desktop,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
 bool copied=false;
 if(memory&&bitmap&&!image.isNull()){
  const auto previous=SelectObject(memory,bitmap);
  if(previous&&previous!=HGDI_ERROR){
   copied=BitBlt(memory,0,0,width,height,desktop,0,0,SRCCOPY);
   GdiFlush();
   if(copied)std::memcpy(image.bits(),pixels,image.sizeInBytes());
   SelectObject(memory,previous);
  }
 }
 if(bitmap)DeleteObject(bitmap);
 if(memory)DeleteDC(memory);
 ReleaseDC(nullptr,desktop);
 return copied?image:QImage();
}
}
ScreenshotPortal::ScreenshotPortal(QObject *parent):QObject(parent){
 liveTimer.setTimerType(Qt::PreciseTimer);
 connect(&liveTimer,&QTimer::timeout,this,[this]{
  const auto image=captureDesktop();
  if(image.isNull()){cancel();emit failed("Windows desktop capture failed");}
  else emit liveFrame(image);
 });
}
ScreenshotPortal::~ScreenshotPortal(){cancel();}
void ScreenshotPortal::cancel(){++generation;liveTimer.stop();}
void ScreenshotPortal::startLive(int hz){
 if(hz!=15&&hz!=30&&hz!=45&&hz!=60)return;
 if(QGuiApplication::platformName()!="windows")return;
 for(auto window:QGuiApplication::allWindows()){
  if(window->objectName()!="targetWindow")continue;
  DWORD affinity=0;
  if(!GetWindowDisplayAffinity(reinterpret_cast<HWND>(window->winId()),&affinity)||affinity!=0x00000011){
   emit failed("Cannot exclude magnifier from live desktop capture");return;
  }
 }
 liveTimer.start(qRound(1000.0/hz));
}
void ScreenshotPortal::capture(){
 cancel();const auto epoch=generation;
 QTimer::singleShot(0,this,[this,epoch]{
  if(epoch!=generation)return;
  const auto image=captureDesktop();
  if(image.isNull())emit failed("Windows desktop capture failed");else emit captured(image);
 });
}
void ScreenshotPortal::response(uint,const QVariantMap &){}
