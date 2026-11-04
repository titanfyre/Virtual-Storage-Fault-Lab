#pragma once
#include "StorageFault.h"

static int SfZero(const void* Data, unsigned int Bytes) {
    const unsigned char* p=(const unsigned char*)Data;
    unsigned int i;
    for(i=0;i<Bytes;++i)if(p[i])return 0;
    return 1;
}
static int SfPolicyValid(const SF_POLICY* p) {
    if(p->Size!=sizeof(*p)||p->Version!=SF_VERSION||!SfZero(p->Reserved,sizeof(p->Reserved)))return 0;
    if(!p->Flags)return !p->Every&&!p->FirstBlock&&!p->BlockCount;
    if((p->Flags&~(SF_READ|SF_WRITE))||!p->Every||p->Every>1000000)return 0;
    return p->FirstBlock>=SF_FIRST_FAULT_BLOCK&&p->FirstBlock<SF_SECTOR_COUNT&&p->BlockCount&&p->BlockCount<=SF_SECTOR_COUNT-p->FirstBlock;
}
static int SfOverlap(SF_U64 Start, SF_U32 Count, const SF_POLICY* p) {
    return Count&&Start>=SF_FIRST_FAULT_BLOCK&&Start<p->FirstBlock+p->BlockCount&&p->FirstBlock<Start+Count;
}
