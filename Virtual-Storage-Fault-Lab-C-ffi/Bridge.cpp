#include "SDK/StorageFault.h"
#include "WireTypes.h"
#include <map>
struct ProductSession{

std::unique_ptr<StorageFault::Session> session;
StorageFault::Session& Get(){if(!session)session=std::make_unique<StorageFault::Session>();return *session;}

Json Invoke(const std::string& op,const Json& a){

if(op=="check"){auto policy=FromWire<SF_POLICY>(a.at("policy"));return {{"valid",StorageFault::Valid(policy)},{"value",EncodeWire(policy)}};}
if(op=="make_policy")return EncodeWire(StorageFault::Policy(Identifier(a,"first_block"),Identifier(a,"block_count"),Number(a,"every",1,1000000),Number(a,"flags",3,3)));
if(op=="inspect")return EncodeWire(StorageFault::Inspect());
if(op=="start"){Get().Start();return Json::object();}
if(op=="set_policy"){auto policy=FromWire<SF_POLICY>(a.at("policy"));Require(StorageFault::Valid(policy),"Invalid fault policy");Get().SetPolicy(policy);return Json::object();}
if(op=="stop"){if(session)session->Stop();return Json::object();}
if(op=="status")return EncodeWire(Get().Status());
if(op=="heartbeat_error")return {{"message",Get().LastHeartbeatError()}};

throw SdkError(50,"Unknown operation");}
};
static Runtime<ProductSession> runtime;
extern "C" __declspec(dllexport) unsigned virtual_storage_fault_lab_json_open(std::uint64_t* id)noexcept{return runtime.Open(id);}
extern "C" __declspec(dllexport) unsigned virtual_storage_fault_lab_json_close(std::uint64_t id)noexcept{return runtime.Close(id);}
extern "C" __declspec(dllexport) char* virtual_storage_fault_lab_json_call(std::uint64_t id,const char* request,std::uint32_t length)noexcept{return runtime.Call(id,request,length);}
extern "C" __declspec(dllexport) void virtual_storage_fault_lab_json_free(char* response)noexcept{delete[] response;}
