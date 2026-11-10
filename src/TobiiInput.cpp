#include "TobiiInput.h"
#include <QLibrary>
#ifdef _WIN32
#include "WindowsTobiiLibrary.h"
#endif
#include <tobii/tobii.h>
#include <tobii/tobii_streams.h>
#include <vector>
#include <string>
#include <cmath>
#include "GazeClock.h"
#include "TobiiTimeSync.h"
namespace {
struct Api {
#ifdef _WIN32
 WindowsTobiiLibrary library;
#else
 QLibrary library{QStringLiteral(TOBII_LIBRARY_PATH)};
#endif
 QString error;
#define FN(name) decltype(&::name) name=nullptr;
 FN(tobii_get_api_version) FN(tobii_api_create) FN(tobii_api_destroy)
 FN(tobii_enumerate_local_device_urls) FN(tobii_device_create) FN(tobii_device_destroy)
 FN(tobii_get_device_info) FN(tobii_gaze_point_subscribe) FN(tobii_gaze_point_unsubscribe)
 FN(tobii_wait_for_callbacks) FN(tobii_update_timesync) FN(tobii_device_clear_callback_buffers) FN(tobii_stream_supported) FN(tobii_device_process_callbacks) FN(tobii_device_reconnect) FN(tobii_error_message)
#undef FN
 bool load() {
  if(!library.load()) {error=library.errorString();return false;}
#define LOAD(name) name=reinterpret_cast<decltype(name)>(library.resolve(#name)); if(!name){error="Missing SDK symbol: " #name;return false;}
  LOAD(tobii_get_api_version)
#ifdef _WIN32
  // Windows uses the Stream Engine 4 interactive device_create ABI.
  // Do not call newer incompatible exports merely because their names match.
  tobii_version_t version{};
  if(tobii_get_api_version(&version)!=TOBII_ERROR_NO_ERROR||version.major!=4){
   error="The installed Tobii Gaming Stream Engine is incompatible with this build (requires API major 4). Install a compatible Gaze Grid build; mouse dwell remains available.";
   return false;
  }
#endif
  LOAD(tobii_api_create) LOAD(tobii_api_destroy)
  LOAD(tobii_enumerate_local_device_urls) LOAD(tobii_device_create) LOAD(tobii_device_destroy)
  LOAD(tobii_get_device_info) LOAD(tobii_gaze_point_subscribe) LOAD(tobii_gaze_point_unsubscribe)
  LOAD(tobii_wait_for_callbacks) LOAD(tobii_update_timesync) LOAD(tobii_device_clear_callback_buffers) LOAD(tobii_stream_supported) LOAD(tobii_device_process_callbacks) LOAD(tobii_device_reconnect) LOAD(tobii_error_message)
#undef LOAD
  return true;
 }
 QString message(tobii_error_t e){return QString::fromUtf8(tobii_error_message(e));}
};
void url(const char *value,void *data) {static_cast<std::vector<std::string>*>(data)->emplace_back(value);}
}
TobiiInput::~TobiiInput(){requestInterruption();wait();}
QVariantMap TobiiInput::probe(){
 Api a; QVariantMap result{{"compiled",true},{"library",QStringLiteral(TOBII_LIBRARY_PATH)}};
 if(!a.load()){result["error"]=a.error;return result;}
 result["library"]=a.library.fileName();
 tobii_version_t v{}; auto e=a.tobii_get_api_version(&v);
 if(e!=TOBII_ERROR_NO_ERROR){result["error"]=a.message(e);return result;}
 result["version"]=QString("%1.%2.%3.%4").arg(v.major).arg(v.minor).arg(v.revision).arg(v.build);
 tobii_api_t *api=nullptr; e=a.tobii_api_create(&api,nullptr,nullptr);
 if(e!=TOBII_ERROR_NO_ERROR){result["error"]=a.message(e);return result;}
 std::vector<std::string> urls; e=a.tobii_enumerate_local_device_urls(api,url,&urls);
 result["deviceCount"]=int(urls.size());
 if(e!=TOBII_ERROR_NO_ERROR) result["error"]=a.message(e);
 a.tobii_api_destroy(api); return result;
}
void TobiiInput::run(){
 Api a;if(!a.load()){emit status(a.error);return;}
 tobii_api_t *api=nullptr;auto e=a.tobii_api_create(&api,nullptr,nullptr);
 if(e!=TOBII_ERROR_NO_ERROR){emit status("Tobii API: "+a.message(e));return;}
 std::vector<std::string> urls;e=a.tobii_enumerate_local_device_urls(api,url,&urls);
 if(e!=TOBII_ERROR_NO_ERROR || urls.empty()){
  emit status(e==TOBII_ERROR_NO_ERROR?"No Tobii device found. Check connection and Tobii services, then retry.":a.message(e));
  a.tobii_api_destroy(api);return;
 }
 if(urls.size()>1){emit status("Multiple Tobii devices found; device selection is not implemented yet.");a.tobii_api_destroy(api);return;}
 tobii_device_t *device=nullptr;
#ifdef _WIN32
 // Public integration reference: labstreaminglayer/App-TobiiStreamEngine,
 // mainwindow.cpp, device_connect(). See docs/windows.md.
 e=a.tobii_device_create(api,urls.front().c_str(),TOBII_FIELD_OF_USE_INTERACTIVE,&device);
#else
 e=a.tobii_device_create(api,urls.front().c_str(),&device);
#endif
 if(e!=TOBII_ERROR_NO_ERROR){emit status(a.message(e));a.tobii_api_destroy(api);return;}
 tobii_supported_t supported=TOBII_NOT_SUPPORTED;
 e=a.tobii_stream_supported(device,TOBII_STREAM_GAZE_POINT,&supported);
 if(e!=TOBII_ERROR_NO_ERROR||supported!=TOBII_SUPPORTED){emit status("This device does not expose gaze points through the selected library");a.tobii_device_destroy(device);a.tobii_api_destroy(api);return;}
 e=a.tobii_gaze_point_subscribe(device,[](const tobii_gaze_point_t *p,void *data){
  auto *self=static_cast<TobiiInput*>(data);
  const bool valid=p->validity==TOBII_VALIDITY_VALID && std::isfinite(p->position_xy[0]) && std::isfinite(p->position_xy[1]);
  emit self->gaze(p->position_xy[0],p->position_xy[1],valid,GazeClock::now());
 },this);
 if(e!=TOBII_ERROR_NO_ERROR) emit status("Gaze subscription: "+a.message(e));
 else {
  emit connectionChanged(true);
  emit status("Tobii connected — gaze stream active");
  TobiiTimeSync timeSync;
  bool syncWarning=false;
  while(!isInterruptionRequested()) {
   e=TOBII_ERROR_NO_ERROR;
   timeSync.update(GazeClock::now(),[&]{
    const auto syncError=a.tobii_update_timesync(device);
    if(syncError!=TOBII_ERROR_NO_ERROR){emit status("Tobii clock synchronization: "+a.message(syncError));syncWarning=true;}
    else if(syncWarning){emit status("Tobii connected — gaze stream active");syncWarning=false;}
    if(syncError==TOBII_ERROR_CONNECTION_FAILED)e=syncError;
    return syncError==TOBII_ERROR_NO_ERROR;
   });
   if(e==TOBII_ERROR_NO_ERROR)e=a.tobii_wait_for_callbacks(1,&device);
   if(isInterruptionRequested())break;
   if(e==TOBII_ERROR_TIMED_OUT)continue;
   if(e==TOBII_ERROR_NO_ERROR)e=a.tobii_device_process_callbacks(device);
   if(e==TOBII_ERROR_CONNECTION_FAILED){
    emit connectionChanged(false);emit gaze(0,0,false,0);emit status("Tobii disconnected — reconnecting");
    for(int i=0;i<50 && !isInterruptionRequested();++i) msleep(20);
    if(isInterruptionRequested()) break;
    e=a.tobii_device_reconnect(device);
    if(e==TOBII_ERROR_NO_ERROR){a.tobii_device_clear_callback_buffers(device);timeSync.reset();emit connectionChanged(true);emit status("Tobii reconnected");}
   }else if(e!=TOBII_ERROR_NO_ERROR){emit status(a.message(e));break;}
  }
  a.tobii_gaze_point_unsubscribe(device);
 }
 emit connectionChanged(false);emit gaze(0,0,false,0);a.tobii_device_destroy(device);a.tobii_api_destroy(api);
}
