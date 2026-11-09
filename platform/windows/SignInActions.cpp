#include "SignInRegistry.h"
#include <msiquery.h>
#include <wincrypt.h>
namespace {
std::wstring property(MSIHANDLE install,const wchar_t *name){
 DWORD length=0;wchar_t dummy=0;MsiGetPropertyW(install,name,&dummy,&length);
 std::wstring value(length+1,L'\0');++length;
 if(MsiGetPropertyW(install,name,value.data(),&length)!=ERROR_SUCCESS)return {};
 value.resize(length);return value;
}
UINT apply(MSIHANDLE install){
 const auto value=property(install,L"CustomActionData");if(value!=L"0"&&value!=L"1")return ERROR_INSTALL_FAILURE;
 return SignInRegistry::set(HKEY_LOCAL_MACHINE,SignInRegistry::key,value==L"1")==ERROR_SUCCESS?ERROR_SUCCESS:ERROR_INSTALL_FAILURE;
}
}
extern "C" __declspec(dllexport) UINT __stdcall PrepareSignIn(MSIHANDLE install){
 std::wstring previous;if(SignInRegistry::read(HKEY_LOCAL_MACHINE,SignInRegistry::key,previous)!=ERROR_SUCCESS)return ERROR_INSTALL_FAILURE;
 bool remove=property(install,L"REMOVE")==L"ALL";
 bool enable=!remove&&property(install,L"GAZE_GRID_TRUST_CERTIFICATE")==L"1"&&property(install,L"GAZE_GRID_SIGN_IN")==L"1";
 if(MsiSetPropertyW(install,L"ApplySignIn",enable?L"1":L"0")!=ERROR_SUCCESS)return ERROR_INSTALL_FAILURE;
 return MsiSetPropertyW(install,L"RollbackSignIn",SignInRegistry::contains(previous)?L"1":L"0");
}
extern "C" __declspec(dllexport) UINT __stdcall ApplySignIn(MSIHANDLE install){return apply(install);}
extern "C" __declspec(dllexport) UINT __stdcall RollbackSignIn(MSIHANDLE install){return apply(install);}

extern "C" __declspec(dllexport) UINT __stdcall CheckCertificateTrust(MSIHANDLE install){
 // This action only reads machine state. The WiX certificate actions perform
 // elevated, transactional changes after the component has been selected.
 const auto thumbprint=property(install,L"gaze_gridCertificateThumbprint");
 if(thumbprint.size()!=40)return ERROR_INSTALL_FAILURE;
 BYTE hash[20]{};
 for(size_t i=0;i<thumbprint.size();++i){
  const auto ch=thumbprint[i];
  const int digit=ch>=L'0'&&ch<=L'9'?ch-L'0':ch>=L'A'&&ch<=L'F'?ch-L'A'+10:ch>=L'a'&&ch<=L'f'?ch-L'a'+10:-1;
  if(digit<0)return ERROR_INSTALL_FAILURE;
  hash[i/2]=BYTE((hash[i/2]<<4)|digit);
 }
 const auto store=CertOpenStore(CERT_STORE_PROV_SYSTEM_W,0,0,
     CERT_SYSTEM_STORE_LOCAL_MACHINE|CERT_STORE_READONLY_FLAG|CERT_STORE_OPEN_EXISTING_FLAG,L"ROOT");
 if(!store)return ERROR_INSTALL_FAILURE;
 CRYPT_HASH_BLOB blob{sizeof(hash),hash};
 const auto certificate=CertFindCertificateInStore(store,X509_ASN_ENCODING|PKCS_7_ASN_ENCODING,0,CERT_FIND_SHA1_HASH,&blob,nullptr);
 const auto findError=certificate?ERROR_SUCCESS:GetLastError();
 const bool present=certificate!=nullptr;
 if(certificate)CertFreeCertificateContext(certificate);
 CertCloseStore(store,0);
 if(!present&&findError!=CRYPT_E_NOT_FOUND)return ERROR_INSTALL_FAILURE;
 DWORD owned=0,bytes=sizeof(owned);
 const auto key=L"Software\\gaze-grid\\Certificates\\Managed\\"+thumbprint;
 const auto result=RegGetValueW(HKEY_LOCAL_MACHINE,key.c_str(),L"Installed",
     RRF_RT_REG_DWORD|RRF_SUBKEY_WOW6464KEY,nullptr,&owned,&bytes);
 if(result!=ERROR_SUCCESS&&result!=ERROR_FILE_NOT_FOUND&&result!=ERROR_PATH_NOT_FOUND)return ERROR_INSTALL_FAILURE;
 return MsiSetPropertyW(install,L"GAZE_GRID_CERTIFICATE_PREEXISTING",present&&owned!=1?L"1":L"");
}
