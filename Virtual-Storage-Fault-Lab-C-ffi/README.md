# Virtual-Storage-Fault-Lab C / JSON SDK

Direct bridge to the original SDK. `virtual_storage_fault_lab_json_open(uint64_t*)`, `call(uint64_t,const char*,uint32_t)`, `close(uint64_t)`, `free(char*)`. Call takes UTF-8 JSON `{operation,arguments}` and returns allocated `{ok:true,result}` or `{ok:false,code,message}`. Free using this DLL. Requests32MiB maximum; responses16MiB. [Operations](docs/API.md).
