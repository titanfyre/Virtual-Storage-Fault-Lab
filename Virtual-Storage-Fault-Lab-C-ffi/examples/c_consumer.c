#include "VirtualStorageFaultLabJsonSDK.h"
#include <string.h>
int main(void){uint64_t session=0;if(virtual_storage_fault_lab_json_open(&session))return 1;char *reply=virtual_storage_fault_lab_json_call(session,"{",1);if(!reply){virtual_storage_fault_lab_json_close(session);return 2;}int failed=strstr(reply,"\"ok\":false")==0;virtual_storage_fault_lab_json_free(reply);if(virtual_storage_fault_lab_json_close(session))return 3;return failed;}
