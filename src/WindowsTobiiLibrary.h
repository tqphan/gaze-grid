#pragma once
#include "TobiiRuntimePaths.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>

class WindowsTobiiLibrary {
 HMODULE handle=nullptr;
 QString path,error;
public:
 ~WindowsTobiiLibrary(){if(handle)FreeLibrary(handle);}
 WindowsTobiiLibrary()=default;
 WindowsTobiiLibrary(const WindowsTobiiLibrary &)=delete;
 WindowsTobiiLibrary &operator=(const WindowsTobiiLibrary &)=delete;
 bool load(){
  QStringList roots;
  for(const auto *folder:{&FOLDERID_ProgramFiles,&FOLDERID_ProgramFilesX86}){
   PWSTR location=nullptr;
   if(SUCCEEDED(SHGetKnownFolderPath(*folder,0,nullptr,&location))){
    const QString base=QString::fromWCharArray(location);CoTaskMemFree(location);
    roots<<base+"/Tobii"<<base+"/Tobii Gaming";
   }
  }
  return loadFromRoots(roots);
 }
 bool loadFromRoots(const QStringList &roots){
  if(handle)return true;
  QStringList failures;
  for(const auto &candidate:tobiiRuntimePaths(roots)){
   // Dependencies come from the vendor DLL directory and protected system
   // locations, rather than the current directory or user-controlled PATH.
   handle=LoadLibraryExW(reinterpret_cast<LPCWSTR>(candidate.utf16()),nullptr,
                         LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
   if(handle){path=candidate;return true;}
   const auto loadError=GetLastError();
   failures<<QString("%1 (Windows error %2)").arg(candidate).arg(loadError);
  }
  error="Install or repair the Tobii Experience driver package and app for your tracker from https://gaming.tobii.com/getstarted/, then restart Gaze Grid. A compatible 64-bit Tobii Stream Engine DLL is required. Mouse dwell remains available.";
  if(!failures.isEmpty())error+=" Could not load: "+failures.join("; ");
  return false;
 }
 FARPROC resolve(const char *symbol){return GetProcAddress(handle,symbol);}
 QString errorString()const{return error;}
 QString fileName()const{return path;}
};
