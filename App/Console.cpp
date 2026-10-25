#include "../SDK/StorageFault.h"
#include <iostream>
#include <limits>

namespace {
volatile LONG interrupted=0;
BOOL WINAPI Control(DWORD type){if(type==CTRL_C_EVENT||type==CTRL_BREAK_EVENT){InterlockedExchange(&interrupted,1);return TRUE;}return FALSE;}
SF_U64 Number(const wchar_t* text,SF_U64 maximum) {
 if(!text||!*text)throw std::invalid_argument("A numeric argument is empty.");
 SF_U64 result=0;
 for(const wchar_t* p=text;*p;++p){if(*p<L'0'||*p>L'9'||result>(maximum-static_cast<unsigned>(*p-L'0'))/10)throw std::invalid_argument("Numeric argument is invalid or out of range.");result=result*10+static_cast<unsigned>(*p-L'0');}
 return result;
}
void Print(const SF_CONTROL& s) {
 std::cout<<"{\"version\":"<<s.Version<<",\"generation\":"<<s.Generation<<",\"sector_bytes\":"<<s.Stats.SectorBytes
  <<",\"sector_count\":"<<s.Stats.SectorCount<<",\"policy_active\":"<<(s.Stats.PolicyActive?"true":"false")
  <<",\"reads\":"<<s.Stats.Reads<<",\"writes\":"<<s.Stats.Writes<<",\"read_bytes\":"<<s.Stats.ReadBytes<<",\"write_bytes\":"<<s.Stats.WriteBytes
  <<",\"matching\":"<<s.Stats.Matching<<",\"injected\":"<<s.Stats.Injected<<",\"rejected\":"<<s.Stats.Rejected
  <<",\"lease_remaining_100ns\":"<<s.Stats.LeaseRemaining100ns<<"}\n";
}
}
int wmain(int argc,wchar_t** argv) {
 try {
  if(argc==2&&std::wstring(argv[1])==L"--preview") {
   std::cout<<"OFFLINE PREVIEW - synthetic status only; no disk opened or modified.\n";
   SF_CONTROL s{};s.Size=sizeof(s);s.Version=SF_VERSION;s.Policy.Size=sizeof(s.Policy);s.Policy.Version=SF_VERSION;s.Stats.SectorBytes=SF_SECTOR_BYTES;s.Stats.SectorCount=SF_SECTOR_COUNT;s.Stats.Reads=1200;s.Stats.Writes=300;Print(s);return 0;
  }
  if(argc==2&&std::wstring(argv[1])==L"--status"){Print(StorageFault::Inspect());return 0;}
  if(argc==7&&std::wstring(argv[1])==L"--policy") {
   auto first=Number(argv[2],SF_SECTOR_COUNT),count=Number(argv[3],SF_SECTOR_COUNT),every=Number(argv[4],1000000),seconds=Number(argv[6],3600);
   std::wstring mode=argv[5];unsigned flags=mode==L"read"?SF_READ:mode==L"write"?SF_WRITE:mode==L"both"?SF_READ|SF_WRITE:0;
   if(!flags||!seconds)throw std::invalid_argument("Use mode read, write or both and a duration from 1 to 3600 seconds.");
   auto policy=StorageFault::Policy(first,count,static_cast<unsigned>(every),flags);
   std::cout<<"Faults affect only the owned 64 MiB RAM-backed lab disk. No physical disk is opened.\nPress Ctrl+C to clear this session's faults.\n";
   SetConsoleCtrlHandler(Control,TRUE);StorageFault::Session session;session.Start();session.SetPolicy(policy);
   ULONGLONG until=GetTickCount64()+seconds*1000;
   while(!InterlockedCompareExchange(&interrupted,0,0)&&GetTickCount64()<until){Print(session.Status());Sleep(1000);}
   session.Stop();std::cout<<"Fault session stopped.\n";return 0;
  }
  std::cout<<"Virtual Storage Fault Lab\n"
   <<"StorageFaultLabCLI.exe --status\n"
   <<"StorageFaultLabCLI.exe --preview\n"
   <<"StorageFaultLabCLI.exe --policy FIRST_SECTOR SECTOR_COUNT EVERY read|write|both SECONDS\n\n"
   <<"Example: --policy 4096 128 4 read 30\n"
   <<"Fail every fourth matching read in sectors 4096..4223 for 30 seconds.\n"
   <<"Administrator rights and signed lab driver required for live mode. No formatting or physical-disk operations.\n";return 2;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
