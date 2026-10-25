#include "StorageFault.h"
#include <iostream>

int main() {
 try {
  StorageFault::Session lab;
  lab.Start();
  lab.SetPolicy(StorageFault::Policy(4096,128,4,SF_READ));
  std::cout<<"Every fourth matching read on the lab disk now fails.\n"
   <<"Only sectors 4096..4223 are selected. Press Enter to clear the policy.\n";
  std::cin.get();
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
