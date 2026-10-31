#pragma once
#include <QDir>
#include <QCoreApplication>
#include <QStandardPaths>

inline bool localTestBuild() {
#ifdef GAZE_GRID_LOCAL_TEST
 return true;
#else
 return false;
#endif
}

// Keep application data separate without hiding the desktop theme configuration.
inline QString appConfigDirectory() {
 const auto overridePath=qEnvironmentVariable("GAZE_GRID_CONFIG_DIR");
 if(localTestBuild()){
  bool check=false;
  for(const auto &arg:QCoreApplication::arguments())if(arg.endsWith("-check")||arg=="--smoke")check=true;
  return QCoreApplication::applicationDirPath()+(check?"/local-check-profile":"/local-profile");
 }
 return overridePath.isEmpty()?QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation):overridePath;
}

inline QString keyboardPackDirectory() {
 const auto installed=QCoreApplication::applicationDirPath()+"/keyboards";
 if(QDir(installed).exists())return installed;
#ifdef DEFAULT_PACK_PATH
 return QStringLiteral(DEFAULT_PACK_PATH);
#else
 return installed;
#endif
}
