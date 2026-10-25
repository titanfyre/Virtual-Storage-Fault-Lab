#include "StorageFault.h"
#include <setupapi.h>
#include <winioctl.h>
#include <ntddscsi.h>
#include <array>
#include <algorithm>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

namespace StorageFault {
namespace {
constexpr GUID StoragePort={0x2accfe60,0xc130,0x11d2,{0xb0,0x82,0,0xa0,0xc9,0x1e,0xfb,0x8b}};
constexpr wchar_t Hardware[]=L"ROOT\\TitanStorageFaultLab";
constexpr wchar_t Service[]=L"TitanStorageFaultLab";
constexpr char Signature[8]={'T','I','T','N','S','F','L','1'};
constexpr ULONG Control=0x53464c01u;
constexpr unsigned Protocol=1,StatusOp=0,ClaimOp=1,SetOp=2,ReleaseOp=3;

[[noreturn]] void Fail(DWORD code,const char* stage,bool policyResponse=false) {
 wchar_t* value=nullptr;
 FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
  nullptr,code,MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_US),reinterpret_cast<LPWSTR>(&value),0,nullptr);
 std::string text=value?Utf8(value):"The operation failed.";
 if(value)LocalFree(value);
 if(code==ERROR_FILE_NOT_FOUND||code==ERROR_NO_MORE_ITEMS)text="The signed Titan Storage Fault Lab adapter is not installed or running. See README.md.";
 if(code==ERROR_ACCESS_DENIED)text="Access denied. Run as administrator; only SYSTEM and administrators may control the adapter.";
 if(code==ERROR_BUSY)text=policyResponse?"Another session owns the fault policy. Stop it or wait for its 30-second lease to expire.":"The storage adapter is temporarily busy, for example while recovering from a reset or failed command. Retry after the adapter becomes ready.";
 throw std::runtime_error(std::string(stage)+": "+text+" (Windows error "+std::to_string(code)+")");
}
bool Property(HDEVINFO set,SP_DEVINFO_DATA& device,DWORD property,std::vector<wchar_t>& value,DWORD kind) {
 DWORD bytes=0,type=0;
 SetupDiGetDeviceRegistryPropertyW(set,&device,property,&type,nullptr,0,&bytes);
 if(type!=kind||bytes<sizeof(wchar_t)||bytes>65536||bytes%sizeof(wchar_t))return false;
 value.assign(bytes/sizeof(wchar_t)+2,L'\0');
 return SetupDiGetDeviceRegistryPropertyW(set,&device,property,&type,reinterpret_cast<BYTE*>(value.data()),bytes,nullptr)!=FALSE&&type==kind;
}
bool Owned(HDEVINFO set,SP_DEVINFO_DATA& device) {
 std::vector<wchar_t> values;
 if(!Property(set,device,SPDRP_HARDWAREID,values,REG_MULTI_SZ))return false;
 bool matched=false;
 for(const wchar_t* p=values.data();*p;p+=wcslen(p)+1)if(_wcsicmp(p,Hardware)==0)matched=true;
 if(!matched||!Property(set,device,SPDRP_SERVICE,values,REG_SZ))return false;
 return _wcsicmp(values.data(),Service)==0;
}
HANDLE Open() {
 HDEVINFO set=SetupDiGetClassDevsW(&StoragePort,nullptr,nullptr,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
 if(set==INVALID_HANDLE_VALUE)Fail(GetLastError(),"Enumerate storage adapter");
 struct Cleanup{HDEVINFO value;~Cleanup(){SetupDiDestroyDeviceInfoList(value);}} cleanup{set};
 std::wstring path;
 for(DWORD n=0;;++n) {
  SP_DEVICE_INTERFACE_DATA item{};item.cbSize=sizeof(item);
  if(!SetupDiEnumDeviceInterfaces(set,nullptr,&StoragePort,n,&item)) {
   if(GetLastError()!=ERROR_NO_MORE_ITEMS)Fail(GetLastError(),"Enumerate adapter interface");break;
  }
  DWORD bytes=0;SP_DEVINFO_DATA device{};device.cbSize=sizeof(device);
  SetupDiGetDeviceInterfaceDetailW(set,&item,nullptr,0,&bytes,nullptr);
  if(bytes<sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W)||bytes>65536)Fail(ERROR_INVALID_DATA,"Adapter interface length");
  std::vector<unsigned char> storage(bytes+sizeof(wchar_t),0);
  auto detail=reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(storage.data());detail->cbSize=sizeof(*detail);
  if(!SetupDiGetDeviceInterfaceDetailW(set,&item,detail,bytes,nullptr,&device))Fail(GetLastError(),"Read adapter identity");
  if(!Owned(set,device))continue;
  if(!path.empty())Fail(ERROR_DUP_NAME,"More than one owned test adapter is present");
  path=detail->DevicePath;
 }
 if(path.empty())Fail(ERROR_FILE_NOT_FOUND,"Locate owned virtual adapter");
 HANDLE handle=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(handle==INVALID_HANDLE_VALUE)Fail(GetLastError(),"Open owned virtual adapter");return handle;
}
SF_CONTROL Exchange(HANDLE handle,unsigned operation,unsigned __int64 owner,const SF_POLICY* policy) {
 std::array<unsigned char,sizeof(SRB_IO_CONTROL)+sizeof(SF_CONTROL)> packet{};
 SRB_IO_CONTROL header{};header.HeaderLength=sizeof(header);header.Timeout=10;header.ControlCode=Control;header.Length=sizeof(SF_CONTROL);
 std::memcpy(header.Signature,Signature,sizeof(Signature));
 SF_CONTROL body{};body.Size=sizeof(body);body.Version=Protocol;body.Operation=operation;body.Owner=owner;
 if(policy)body.Policy=*policy;
 std::memcpy(packet.data(),&header,sizeof(header));std::memcpy(packet.data()+sizeof(header),&body,sizeof(body));
 DWORD got=0;
 if(!DeviceIoControl(handle,IOCTL_SCSI_MINIPORT,packet.data(),static_cast<DWORD>(packet.size()),packet.data(),static_cast<DWORD>(packet.size()),&got,nullptr))Fail(GetLastError(),"Storage fault control request");
 if(got!=packet.size())Fail(ERROR_INVALID_DATA,"Storage fault response length");
 std::memcpy(&header,packet.data(),sizeof(header));std::memcpy(&body,packet.data()+sizeof(header),sizeof(body));
 if(header.HeaderLength!=sizeof(header)||header.Length!=sizeof(body)||header.ControlCode!=Control||std::memcmp(header.Signature,Signature,sizeof(Signature)))Fail(ERROR_INVALID_DATA,"Storage fault response header");
 if(header.ReturnCode!=body.Result)Fail(ERROR_INVALID_DATA,"Storage fault result consistency");
 if(header.ReturnCode)Fail(header.ReturnCode,"Storage fault miniport response",true);
 if(body.Result)Fail(body.Result,"Storage fault policy response",true);
 if(body.Operation!=operation||!Valid(body))Fail(ERROR_INVALID_DATA,"Storage fault response validation");
 if((operation==ClaimOp&&!body.Owner)||((operation==StatusOp||operation==SetOp)&&owner&&body.Owner!=owner))Fail(ERROR_ACCESS_DENIED,"Fault policy lease expired or changed");
 if((operation==StatusOp&&!owner&&body.Owner)||(operation==ReleaseOp&&body.Owner))Fail(ERROR_INVALID_DATA,"Unexpected policy ownership in response");
 return body;
}
}
std::string Utf8(const std::wstring& text) {
 if(text.empty())return {};
 if(text.size()>0x7fffffff)throw std::invalid_argument("Text is too long.");
 int bytes=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),nullptr,0,nullptr,nullptr);
 if(!bytes)throw std::invalid_argument("Invalid UTF-16 text.");
 std::string output(static_cast<size_t>(bytes),0);
 if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),output.data(),bytes,nullptr,nullptr))throw std::invalid_argument("Could not encode text.");return output;
}
bool Valid(const SF_POLICY& p) noexcept {
 if(p.Size!=sizeof(p)||p.Version!=Protocol||(p.Flags&~3u))return false;
 for(auto value:p.Reserved)if(value)return false;
 if(!p.Flags)return !p.Every&&!p.FirstBlock&&!p.BlockCount;
 return p.Every>=1&&p.Every<=1000000&&p.FirstBlock>=2048&&p.FirstBlock<131072&&p.BlockCount&&p.BlockCount<=131072-p.FirstBlock;
}
bool Valid(const SF_CONTROL& c) noexcept {
 return c.Size==sizeof(c)&&c.Version==Protocol&&c.Operation<=ReleaseOp&&!c.Result&&Valid(c.Policy)&&c.Stats.SectorBytes==512&&c.Stats.SectorCount==131072&&c.Stats.PolicyActive<=1&&!c.Stats.Reserved&&c.Stats.LeaseRemaining100ns<=300000000ULL&&c.Stats.PolicyActive==(c.Policy.Flags?1u:0u);
}
SF_POLICY Policy(unsigned __int64 first,unsigned __int64 count,unsigned every,unsigned flags) {
 SF_POLICY p{};p.Size=sizeof(p);p.Version=Protocol;p.Flags=flags;p.Every=every;p.FirstBlock=first;p.BlockCount=count;
 if(!Valid(p))throw std::invalid_argument("Use read/write flags, every 1..1000000, and a nonempty sector range inside [2048, 131072). The first 1 MiB is excluded from faults.");return p;
}
struct Session::State {
 HANDLE device=INVALID_HANDLE_VALUE;
 unsigned __int64 owner=0;
 std::mutex lifecycle,lock;std::condition_variable changed;std::thread worker;bool stopping=false;std::string failure;
};
Session::Session():state_(std::make_unique<State>()){}
Session::~Session(){Stop();}
void Session::Start() {
 std::lock_guard<std::mutex> lifecycle(state_->lifecycle);
 std::lock_guard<std::mutex> guard(state_->lock);
 if(state_->device!=INVALID_HANDLE_VALUE)throw std::runtime_error("The session is already started.");
 state_->device=Open();state_->stopping=false;state_->failure.clear();
 try {
  state_->owner=Exchange(state_->device,ClaimOp,0,nullptr).Owner;
  state_->worker=std::thread([this]{
   std::unique_lock<std::mutex> held(state_->lock);
   while(!state_->changed.wait_for(held,std::chrono::seconds(5),[this]{return state_->stopping;})) {
    try {Exchange(state_->device,StatusOp,state_->owner,nullptr);}
    catch(const std::exception& e){state_->failure=e.what();break;}
   }
  });
 }catch(...) {
  if(state_->owner)try{Exchange(state_->device,ReleaseOp,state_->owner,nullptr);}catch(...){}
  CloseHandle(state_->device);state_->device=INVALID_HANDLE_VALUE;state_->owner=0;throw;
 }
}
void Session::SetPolicy(const SF_POLICY& policy) {
 if(!Valid(policy))throw std::invalid_argument("Invalid fault policy.");
 std::lock_guard<std::mutex> held(state_->lock);
 if(state_->device==INVALID_HANDLE_VALUE)throw std::runtime_error("Start the session before setting a fault policy.");
 if(!state_->failure.empty())throw std::runtime_error(state_->failure);
 Exchange(state_->device,SetOp,state_->owner,&policy);
}
SF_CONTROL Session::Status() {
 std::lock_guard<std::mutex> held(state_->lock);
 if(state_->device==INVALID_HANDLE_VALUE)throw std::runtime_error("Start the session before reading owned status.");
 if(!state_->failure.empty())throw std::runtime_error(state_->failure);
 return Exchange(state_->device,StatusOp,state_->owner,nullptr);
}
std::string Session::LastHeartbeatError(){std::lock_guard<std::mutex> held(state_->lock);return state_->failure;}
void Session::Stop() noexcept {
 std::lock_guard<std::mutex> lifecycle(state_->lifecycle);
 {std::lock_guard<std::mutex> held(state_->lock);state_->stopping=true;state_->changed.notify_all();}
 if(state_->worker.joinable())state_->worker.join();
 std::lock_guard<std::mutex> held(state_->lock);
 if(state_->device!=INVALID_HANDLE_VALUE) {
  if(state_->owner)try{Exchange(state_->device,ReleaseOp,state_->owner,nullptr);}catch(...){}
  CloseHandle(state_->device);state_->device=INVALID_HANDLE_VALUE;state_->owner=0;
 }
}
SF_CONTROL Inspect(){HANDLE handle=Open();try{auto result=Exchange(handle,StatusOp,0,nullptr);CloseHandle(handle);return result;}catch(...){CloseHandle(handle);throw;}}
}
