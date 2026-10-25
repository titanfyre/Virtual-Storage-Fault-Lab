#include "../SDK/StorageFault.h"
#include <iostream>
#include <random>

void Check(bool result){if(!result)throw std::runtime_error("Offline protocol validation failed.");}
int main() {
 try {
  SF_POLICY off{};off.Size=sizeof(off);off.Version=SF_VERSION;Check(StorageFault::Valid(off));
  auto policy=StorageFault::Policy(4096,128,4,SF_READ|SF_WRITE);Check(StorageFault::Valid(policy));
  auto base=policy;policy.FirstBlock=2047;Check(!StorageFault::Valid(policy));
  policy=base;policy.BlockCount=SF_SECTOR_COUNT;Check(!StorageFault::Valid(policy));
  policy=base;policy.FirstBlock=~SF_U64(0);Check(!StorageFault::Valid(policy));
  policy=base;policy.Every=0;Check(!StorageFault::Valid(policy));
  policy=base;policy.Flags=4;Check(!StorageFault::Valid(policy));
  policy=base;policy.Reserved[3]=1;Check(!StorageFault::Valid(policy));
  policy=off;policy.BlockCount=1;Check(!StorageFault::Valid(policy));
  policy=off;policy.Version=2;Check(!StorageFault::Valid(policy));
  SF_CONTROL control{};control.Size=sizeof(control);control.Version=SF_VERSION;control.Policy=off;control.Stats.SectorBytes=SF_SECTOR_BYTES;control.Stats.SectorCount=SF_SECTOR_COUNT;Check(StorageFault::Valid(control));
  control.Stats.PolicyActive=1;Check(!StorageFault::Valid(control));
  control.Stats.PolicyActive=0;control.Stats.LeaseRemaining100ns=300000001ULL;Check(!StorageFault::Valid(control));
  control.Stats.LeaseRemaining100ns=0;control.Stats.Reserved=1;Check(!StorageFault::Valid(control));
  std::mt19937 generator(57241);unsigned accepted=0;
  for(unsigned n=0;n<1000000;++n){policy=base;auto data=reinterpret_cast<unsigned char*>(&policy);for(unsigned j=0;j<3;++j)data[generator()%sizeof(policy)]=static_cast<unsigned char>(generator());if(StorageFault::Valid(policy))++accepted;}
  bool rejected=false;try{StorageFault::Session session;session.SetPolicy(base);}catch(const std::exception&){rejected=true;}Check(rejected);
  std::cout<<"Offline boundary checks and one million reproducible policy mutations passed (accepted: "<<accepted<<"). No driver loaded; no disk opened.\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
