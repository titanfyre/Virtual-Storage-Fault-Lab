# Virtual-Storage-Fault-Lab C / JSON SDK

Direct bridge to the original SDK. `virtual_storage_fault_lab_json_open(uint64_t*)`, `call(uint64_t,const char*,uint32_t)`, `close(uint64_t)`, `free(char*)`. Call takes UTF-8 JSON `{operation,arguments}` and returns allocated `{ok:true,result}` or `{ok:false,code,message}`. Free using this DLL. Requests32MiB maximum; responses16MiB. [Operations](docs/API.md).

## Standalone C interface

Include `VirtualStorageFaultLabJsonSDK.h` from C. This header uses C integer types, exported C symbols and explicit buffer ownership. `examples/c_consumer.c` is compiled as C, linked to the real DLL import library and tested with inert open/close plus malformed JSON rejection. Build this C ABI directory with CMake against the canonical `packaging/native` source; link its generated `VirtualStorageFaultLabJsonSDK.lib` and deploy `VirtualStorageFaultLabJsonSDK.dll` beside the executable. No driver is installed or started by the test. This C interface is distributed separately from the existing C++ source-package recipes.
