#include <ntddk.h>
#include <storport.h>
#include <ntddscsi.h>
#include <ntstrsafe.h>
#include "Policy.h"

#define IMAGE_BYTES ((ULONG)SF_SECTOR_BYTES * SF_SECTOR_COUNT)
#define MAX_TRANSFER (64u * 1024u)
#define LEASE_TICKS ((ULONGLONG)SF_LEASE_SECONDS * 10000000ull)

C_ASSERT(sizeof(SF_POLICY)==48);
C_ASSERT(sizeof(SF_STATS)==88);
C_ASSERT(sizeof(SF_CONTROL)==168);
C_ASSERT(sizeof(SRB_IO_CONTROL)==28);

typedef struct _ADAPTER {
    PUCHAR Image;
    BOOLEAN OwnsSlot, Running;
    SF_POLICY Policy;
    SF_STATS Stats;
    ULONGLONG Owner, Deadline, Generation, ClaimSequence;
    ULONG TraceId, FindCalls;
} ADAPTER, *PADAPTER;
static volatile LONG AdapterSlot=0;
static volatile LONG TraceSequence=0;
static REGHANDLE InitProvider=0;
/* Init-only support diagnostics. No SRB/data/addresses are emitted. */
static const GUID InitGuid={0xb1f010b5,0xad9a,0x4982,{0x8f,0xb3,0x68,0x1a,0x8e,0xaa,0x59,0xf2}};

DRIVER_INITIALIZE DriverEntry;
VIRTUAL_HW_FIND_ADAPTER FindAdapter;
HW_INITIALIZE Initialize;
HW_STARTIO StartIo;
HW_RESET_BUS ResetBus;
HW_ADAPTER_CONTROL AdapterControl;
HW_FREE_ADAPTER_RESOURCES FreeAdapter;
HW_INITIALIZE_TRACING InitializeTracing;
HW_CLEANUP_TRACING CleanupTracing;

static void TraceInit(ULONG stage,ULONG status,PADAPTER a,PPORT_CONFIGURATION_INFORMATION c){
    WCHAR text[384];
    if(!InitProvider)return;
    if(NT_SUCCESS(RtlStringCchPrintfW(text,RTL_NUMBER_OF(text),
        L"Stage=%lu Status=0x%08X Adapter=%lu Slot=%ld Owned=%lu Calls=%lu ConfigLen=%lu SrbType=%lu Virtual=%lu Bus=%lu Target=%lu Lun=%lu MaxTransfer=%lu Breaks=%lu Map=%lu NeedPhys=%lu Initiator=%ld Master=%lu Scatter=%lu",
        stage,status,a?a->TraceId:0,InterlockedCompareExchange(&AdapterSlot,0,0),a?(ULONG)a->OwnsSlot:0,a?a->FindCalls:0,
        c?c->Length:0,c?(ULONG)c->SrbType:0,c?(ULONG)c->VirtualDevice:0,c?(ULONG)c->NumberOfBuses:0,
        c?(ULONG)c->MaximumNumberOfTargets:0,c?(ULONG)c->MaximumNumberOfLogicalUnits:0,
        c?c->MaximumTransferLength:0,c?c->NumberOfPhysicalBreaks:0,c?(ULONG)c->MapBuffers:0,
        c?(ULONG)c->NeedPhysicalAddresses:0,c?(LONG)c->InitiatorBusId[0]:0,c?(ULONG)c->Master:0,c?(ULONG)c->ScatterGather:0)))
        EtwWriteString(InitProvider,4,1,NULL,text);
}
_Use_decl_annotations_
VOID InitializeTracing(PVOID Arg1,PVOID Arg2){
    UNREFERENCED_PARAMETER(Arg1);UNREFERENCED_PARAMETER(Arg2);
    if(!NT_SUCCESS(EtwRegister(&InitGuid,NULL,NULL,&InitProvider)))InitProvider=0;
    TraceInit(10,0,NULL,NULL);
}
_Use_decl_annotations_
VOID CleanupTracing(PVOID Arg1){
    UNREFERENCED_PARAMETER(Arg1);TraceInit(11,0,NULL,NULL);
    if(InitProvider){EtwUnregister(InitProvider);InitProvider=0;}
}

static ULONG Be16(const UCHAR* p){return ((ULONG)p[0]<<8)|p[1];}
static ULONG Be32(const UCHAR* p){return ((ULONG)p[0]<<24)|((ULONG)p[1]<<16)|((ULONG)p[2]<<8)|p[3];}
static ULONGLONG Be64(const UCHAR* p){return ((ULONGLONG)Be32(p)<<32)|Be32(p+4);}
static void Put32(UCHAR* p,ULONG n){p[0]=(UCHAR)(n>>24);p[1]=(UCHAR)(n>>16);p[2]=(UCHAR)(n>>8);p[3]=(UCHAR)n;}
static void Put64(UCHAR* p,ULONGLONG n){Put32(p,(ULONG)(n>>32));Put32(p+4,(ULONG)n);}

static void ClearPolicy(PADAPTER a){
    RtlZeroMemory(&a->Policy,sizeof(a->Policy));
    a->Policy.Size=sizeof(a->Policy);a->Policy.Version=SF_VERSION;
    a->Owner=0;a->Deadline=0;a->Stats.Matching=0;++a->Generation;
}
static void Expire(PADAPTER a,ULONGLONG Now){if(a->Owner&&Now>=a->Deadline)ClearPolicy(a);}
static void Sense(PSCSI_REQUEST_BLOCK s,UCHAR key,UCHAR asc){
    UCHAR sense[18]={0};ULONG n=s->SenseInfoBufferLength;
    sense[0]=0x70;sense[2]=key;sense[7]=10;sense[12]=asc;
    s->SrbStatus=SRB_STATUS_ERROR;s->ScsiStatus=SCSISTAT_CHECK_CONDITION;s->DataTransferLength=0;
    if(s->SenseInfoBuffer&&n){if(n>sizeof(sense))n=sizeof(sense);RtlCopyMemory(s->SenseInfoBuffer,sense,n);s->SrbStatus|=SRB_STATUS_AUTOSENSE_VALID;}
}
static void Reply(PSCSI_REQUEST_BLOCK s,const UCHAR* bytes,ULONG count,ULONG allocation){
    ULONG n=count;if(n>allocation)n=allocation;if(n>s->DataTransferLength)n=s->DataTransferLength;
    if(n&&!s->DataBuffer){s->SrbStatus=SRB_STATUS_INVALID_REQUEST;s->DataTransferLength=0;return;}
    if(n)RtlCopyMemory(s->DataBuffer,bytes,n);
    s->DataTransferLength=n;s->SrbStatus=SRB_STATUS_SUCCESS;s->ScsiStatus=SCSISTAT_GOOD;
}

static void PolicyControl(PADAPTER a,PSCSI_REQUEST_BLOCK s,ULONGLONG now){
    SRB_IO_CONTROL header;SF_CONTROL input,output;ULONG result=0;
    if(s->DataTransferLength!=sizeof(header)+sizeof(input)||!s->DataBuffer){s->SrbStatus=SRB_STATUS_INVALID_REQUEST;s->DataTransferLength=0;++a->Stats.Rejected;return;}
    RtlCopyMemory(&header,s->DataBuffer,sizeof(header));
    if(header.HeaderLength!=sizeof(header)||RtlCompareMemory(header.Signature,"TITNSFL1",8)!=8||header.ControlCode!=SF_CONTROL_CODE||header.Timeout!=10||header.ReturnCode||header.Length!=sizeof(input)){
        s->SrbStatus=SRB_STATUS_INVALID_REQUEST;s->DataTransferLength=0;++a->Stats.Rejected;return;
    }
    RtlCopyMemory(&input,(PUCHAR)s->DataBuffer+sizeof(header),sizeof(input));
    Expire(a,now);
    if(input.Size!=sizeof(input)||input.Version!=SF_VERSION)result=1306;
    else if(input.Result||input.Generation||!SfZero(&input.Stats,sizeof(input.Stats))||input.Operation>SF_RELEASE)result=87;
    else if(input.Operation!=SF_SET&&!SfZero(&input.Policy,sizeof(input.Policy)))result=87;
    else if(input.Operation==SF_SET&&!SfPolicyValid(&input.Policy))result=87;
    else if(input.Operation==SF_CLAIM&&input.Owner)result=87;
    else if(input.Operation==SF_CLAIM){
        if(a->Owner)result=170;
        else {++a->ClaimSequence;a->Owner=(a->ClaimSequence<<32)^(ULONGLONG)KeQueryPerformanceCounter(NULL).QuadPart;if(!a->Owner)a->Owner=a->ClaimSequence|1;a->Deadline=now+LEASE_TICKS;}
    }else if(input.Operation==SF_SET||input.Operation==SF_RELEASE){
        if(!input.Owner||input.Owner!=a->Owner)result=5;
        else if(input.Operation==SF_RELEASE)ClearPolicy(a);
        else {a->Policy=input.Policy;a->Stats.Matching=0;++a->Generation;a->Deadline=now+LEASE_TICKS;}
    }else if(input.Owner){if(input.Owner!=a->Owner)result=5;else a->Deadline=now+LEASE_TICKS;}
    if(result)++a->Stats.Rejected;
    RtlZeroMemory(&output,sizeof(output));output.Size=sizeof(output);output.Version=SF_VERSION;
    output.Operation=input.Operation;output.Result=result;output.Generation=a->Generation;
    if(!result&&a->Owner&&(input.Operation==SF_CLAIM||input.Owner==a->Owner))output.Owner=a->Owner;
    output.Policy=a->Policy;output.Stats=a->Stats;
    output.Stats.SectorBytes=SF_SECTOR_BYTES;output.Stats.SectorCount=SF_SECTOR_COUNT;
    output.Stats.PolicyActive=a->Owner&&a->Policy.Flags?1:0;
    output.Stats.LeaseRemaining100ns=a->Owner&&a->Deadline>now?a->Deadline-now:0;
    header.ReturnCode=result;RtlCopyMemory(s->DataBuffer,&header,sizeof(header));
    RtlCopyMemory((PUCHAR)s->DataBuffer+sizeof(header),&output,sizeof(output));
    s->SrbStatus=SRB_STATUS_SUCCESS;s->ScsiStatus=SCSISTAT_GOOD;s->DataTransferLength=sizeof(header)+sizeof(output);
}

static void Transfer(PADAPTER a,PSCSI_REQUEST_BLOCK s,BOOLEAN write,ULONGLONG block,ULONG count){
    ULONG bytes;ULONGLONG offset;
    if(block>=SF_SECTOR_COUNT||count>SF_SECTOR_COUNT-block){++a->Stats.Rejected;Sense(s,SCSI_SENSE_ILLEGAL_REQUEST,0x21);return;}
    if(count>MAX_TRANSFER/SF_SECTOR_BYTES){++a->Stats.Rejected;s->SrbStatus=SRB_STATUS_DATA_OVERRUN;s->DataTransferLength=0;return;}
    bytes=count*SF_SECTOR_BYTES;offset=block*SF_SECTOR_BYTES;
    if(s->DataTransferLength<bytes||(bytes&&(!s->DataBuffer||
        (write?((s->SrbFlags&SRB_FLAGS_DATA_OUT)==0||(s->SrbFlags&SRB_FLAGS_DATA_IN)!=0):((s->SrbFlags&SRB_FLAGS_DATA_IN)==0||(s->SrbFlags&SRB_FLAGS_DATA_OUT)!=0))))){
        ++a->Stats.Rejected;s->SrbStatus=SRB_STATUS_INVALID_REQUEST;s->DataTransferLength=0;return;
    }
    if(write)++a->Stats.Writes;else ++a->Stats.Reads;
    if(a->Owner&&(a->Policy.Flags&(write?SF_WRITE:SF_READ))&&SfOverlap(block,count,&a->Policy)){
        ++a->Stats.Matching;
        if(a->Stats.Matching%a->Policy.Every==0){++a->Stats.Injected;Sense(s,SCSI_SENSE_MEDIUM_ERROR,write?0x0c:0x11);return;}
    }
    if(bytes){if(write)RtlCopyMemory(a->Image+(SIZE_T)offset,s->DataBuffer,bytes);else RtlCopyMemory(s->DataBuffer,a->Image+(SIZE_T)offset,bytes);}
    if(write)a->Stats.WriteBytes+=bytes;else a->Stats.ReadBytes+=bytes;
    s->SrbStatus=SRB_STATUS_SUCCESS;s->ScsiStatus=SCSISTAT_GOOD;s->DataTransferLength=bytes;
}

static void Scsi(PADAPTER a,PSCSI_REQUEST_BLOCK s){
    UCHAR reply[128]={0};UCHAR op=s->Cdb[0];ULONG allocation;
    if(s->PathId||s->TargetId||s->Lun){s->SrbStatus=SRB_STATUS_NO_DEVICE;s->DataTransferLength=0;return;}
    if(s->CdbLength<6||s->CdbLength>16){++a->Stats.Rejected;Sense(s,SCSI_SENSE_ILLEGAL_REQUEST,0x24);return;}
    switch(op){
    case 0x00: case 0x1b: case 0x1e:
        s->SrbStatus=SRB_STATUS_SUCCESS;s->DataTransferLength=0;return;
    case 0x03:
        reply[0]=0x70;reply[7]=10;Reply(s,reply,18,s->Cdb[4]);return;
    case 0x12:
        allocation=s->Cdb[4];
        if(s->Cdb[1]&~1){Sense(s,SCSI_SENSE_ILLEGAL_REQUEST,0x24);return;}
        if(!(s->Cdb[1]&1)){
            if(s->Cdb[2]){Sense(s,SCSI_SENSE_ILLEGAL_REQUEST,0x24);return;}
            reply[0]=DIRECT_ACCESS_DEVICE;reply[2]=6;reply[3]=2;reply[4]=31;reply[7]=2;
            RtlCopyMemory(reply+8,"TITANCSA",8);RtlCopyMemory(reply+16,"STORAGE FAULTLAB ",16);RtlCopyMemory(reply+32,"0001",4);
            Reply(s,reply,36,allocation);return;
        }
        reply[1]=s->Cdb[2];
        if(s->Cdb[2]==0){reply[3]=3;reply[4]=0;reply[5]=0x80;reply[6]=0x83;Reply(s,reply,7,allocation);return;}
        if(s->Cdb[2]==0x80){reply[3]=14;RtlCopyMemory(reply+4,"TITAN-SFL-0001",14);Reply(s,reply,18,allocation);return;}
        if(s->Cdb[2]==0x83){
            static const char id[]="TITANCSA:STORAGEFAULTLAB:0001";
            reply[3]=(UCHAR)(4+sizeof(id)-1);reply[4]=2;reply[5]=0;reply[7]=(UCHAR)(sizeof(id)-1);
            RtlCopyMemory(reply+8,id,sizeof(id)-1);Reply(s,reply,(ULONG)(8+sizeof(id)-1),allocation);return;
        }
        Sense(s,SCSI_SENSE_ILLEGAL_REQUEST,0x24);return;
    case 0x25:
        if(s->CdbLength!=10){Sense(s,SCSI_SENSE_ILLEGAL_REQUEST,0x24);return;}
        Put32(reply,SF_SECTOR_COUNT-1);Put32(reply+4,SF_SECTOR_BYTES);Reply(s,reply,8,8);return;
    case 0x9e:
        if(s->CdbLength!=16||(s->Cdb[1]&0x1f)!=0x10){Sense(s,SCSI_SENSE_ILLEGAL_REQUEST,0x24);return;}
        Put64(reply,SF_SECTOR_COUNT-1);Put32(reply+8,SF_SECTOR_BYTES);Reply(s,reply,32,Be32(s->Cdb+10));return;
    case 0xa0:
        if(s->CdbLength!=12){Sense(s,SCSI_SENSE_ILLEGAL_REQUEST,0x24);return;}
        reply[3]=8;Reply(s,reply,16,Be32(s->Cdb+6));return;
    case 0x23:
        if(s->CdbLength!=10){Sense(s,SCSI_SENSE_ILLEGAL_REQUEST,0x24);return;}
        reply[3]=8;Put32(reply+4,SF_SECTOR_COUNT);reply[8]=2;reply[10]=2;Reply(s,reply,12,Be16(s->Cdb+7));return;
    case 0x1a: case 0x5a: {
        ULONG header=op==0x1a?4:8,page=s->Cdb[2]&0x3f;
        if(s->CdbLength!=(op==0x1a?6:10)||s->Cdb[3]||(s->Cdb[2]&0xc0)||s->Cdb[1]&~8||!(page==8||page==0x3f)){
            Sense(s,SCSI_SENSE_ILLEGAL_REQUEST,0x24);return;
        }
        if(op==0x1a){reply[0]=23;reply[2]=0x10;allocation=s->Cdb[4];}
        else {reply[1]=26;reply[3]=0x10;allocation=Be16(s->Cdb+7);}
        reply[header]=8;reply[header+1]=18;Reply(s,reply,header+20,allocation);return;
    }
    case 0x28: case 0x2a:
        if(s->CdbLength!=10||(s->Cdb[1]&~0x18)){Sense(s,SCSI_SENSE_ILLEGAL_REQUEST,0x24);return;}
        Transfer(a,s,op==0x2a,Be32(s->Cdb+2),Be16(s->Cdb+7));return;
    case 0x88: case 0x8a:
        if(s->CdbLength!=16||(s->Cdb[1]&~0x18)){Sense(s,SCSI_SENSE_ILLEGAL_REQUEST,0x24);return;}
        Transfer(a,s,op==0x8a,Be64(s->Cdb+2),Be32(s->Cdb+10));return;
    case 0x35: case 0x91:
        if(s->CdbLength!=(op==0x35?10:16)){Sense(s,SCSI_SENSE_ILLEGAL_REQUEST,0x24);return;}
        s->SrbStatus=SRB_STATUS_SUCCESS;s->DataTransferLength=0;return;
    case 0x2f: case 0x8f: {
        ULONGLONG block;ULONG count;
        if(s->CdbLength!=(op==0x2f?10:16)||(s->Cdb[1]&6)){Sense(s,SCSI_SENSE_ILLEGAL_REQUEST,0x24);return;}
        block=op==0x2f?Be32(s->Cdb+2):Be64(s->Cdb+2);count=op==0x2f?Be16(s->Cdb+7):Be32(s->Cdb+10);
        if(block>=SF_SECTOR_COUNT||count>SF_SECTOR_COUNT-block){Sense(s,SCSI_SENSE_ILLEGAL_REQUEST,0x21);return;}
        s->SrbStatus=SRB_STATUS_SUCCESS;s->DataTransferLength=0;return;
    }
    default:Sense(s,SCSI_SENSE_ILLEGAL_REQUEST,0x20);return;
    }
}

_Use_decl_annotations_
BOOLEAN StartIo(PVOID DeviceExtension,PSCSI_REQUEST_BLOCK Srb){
    PADAPTER a=(PADAPTER)DeviceExtension;STOR_LOCK_HANDLE lock={0};
    if(!Srb)return TRUE;
    StorPortAcquireSpinLock(a,StartIoLock,NULL,&lock);
    Expire(a,KeQueryInterruptTime());
    if(Srb->Function==SRB_FUNCTION_EXECUTE_SCSI||Srb->Function==SRB_FUNCTION_IO_CONTROL)Srb->ScsiStatus=SCSISTAT_GOOD;
    if(!a->Running||!a->Image){Srb->SrbStatus=SRB_STATUS_NO_DEVICE;Srb->DataTransferLength=0;}
    else if(Srb->Function==SRB_FUNCTION_IO_CONTROL)PolicyControl(a,Srb,KeQueryInterruptTime());
    else if(Srb->Function==SRB_FUNCTION_EXECUTE_SCSI)Scsi(a,Srb);
    else if(Srb->Function==SRB_FUNCTION_FLUSH||Srb->Function==SRB_FUNCTION_SHUTDOWN){Srb->SrbStatus=SRB_STATUS_SUCCESS;Srb->DataTransferLength=0;}
    else if(Srb->Function==SRB_FUNCTION_RESET_BUS||Srb->Function==SRB_FUNCTION_RESET_DEVICE||Srb->Function==SRB_FUNCTION_RESET_LOGICAL_UNIT){
        ++a->Stats.Resets;ClearPolicy(a);Srb->SrbStatus=SRB_STATUS_SUCCESS;Srb->DataTransferLength=0;
    }else if(Srb->Function==SRB_FUNCTION_PNP){
        PSCSI_PNP_REQUEST_BLOCK p=(PSCSI_PNP_REQUEST_BLOCK)Srb;
        if(p->PnPAction==StorQueryCapabilities&&p->DataBuffer&&p->DataTransferLength>=sizeof(STOR_DEVICE_CAPABILITIES)){
            PSTOR_DEVICE_CAPABILITIES caps=(PSTOR_DEVICE_CAPABILITIES)p->DataBuffer;RtlZeroMemory(caps,sizeof(*caps));caps->Version=1;caps->Removable=TRUE;caps->UniqueID=TRUE;caps->SurpriseRemovalOK=TRUE;p->DataTransferLength=sizeof(*caps);
        }else p->DataTransferLength=0;
        p->SrbStatus=SRB_STATUS_SUCCESS;
    }else if(Srb->Function==SRB_FUNCTION_POWER){Srb->SrbStatus=SRB_STATUS_SUCCESS;Srb->DataTransferLength=0;}
    else {Srb->SrbStatus=SRB_STATUS_INVALID_REQUEST;Srb->DataTransferLength=0;++a->Stats.Rejected;}
    StorPortReleaseSpinLock(a,&lock);
    StorPortNotification(RequestComplete,a,Srb);return TRUE;
}

_Use_decl_annotations_
ULONG FindAdapter(PVOID DeviceExtension,PVOID HwContext,PVOID BusInformation,PVOID LowerDevice,PCHAR ArgumentString,PPORT_CONFIGURATION_INFORMATION ConfigInfo,PBOOLEAN Again){
    PADAPTER a=(PADAPTER)DeviceExtension;ULONG result;
    UNREFERENCED_PARAMETER(HwContext);UNREFERENCED_PARAMETER(BusInformation);UNREFERENCED_PARAMETER(LowerDevice);UNREFERENCED_PARAMETER(ArgumentString);
    if(!a->TraceId)a->TraceId=(ULONG)InterlockedIncrement(&TraceSequence);
    ++a->FindCalls;TraceInit(20,0,a,ConfigInfo);
    *Again=FALSE;
    if(InterlockedCompareExchange(&AdapterSlot,1,0)!=0){TraceInit(21,SP_RETURN_NOT_FOUND,a,ConfigInfo);return SP_RETURN_NOT_FOUND;}
    a->OwnsSlot=TRUE;
    result=StorPortAllocatePool(a,IMAGE_BYTES,'LfST',(PVOID*)&a->Image);
    TraceInit(22,result,a,ConfigInfo);
    if(result!=STOR_STATUS_SUCCESS||!a->Image){a->OwnsSlot=FALSE;InterlockedExchange(&AdapterSlot,0);TraceInit(23,SP_RETURN_ERROR,a,ConfigInfo);return SP_RETURN_ERROR;}
    RtlZeroMemory(a->Image,IMAGE_BYTES);ClearPolicy(a);
    ConfigInfo->VirtualDevice=TRUE;ConfigInfo->NumberOfBuses=1;
    /* Preserve the port's target/LUN limits together with InitiatorBusId. We
       expose only (0,0,0); other addresses return SRB_STATUS_NO_DEVICE. */
    ConfigInfo->MaximumTransferLength=MAX_TRANSFER;ConfigInfo->NumberOfPhysicalBreaks=MAX_TRANSFER/PAGE_SIZE+1;
    ConfigInfo->AlignmentMask=0;
    ConfigInfo->CachesData=FALSE;ConfigInfo->MapBuffers=STOR_MAP_ALL_BUFFERS_INCLUDING_READ_WRITE;
    /* Storport owns NeedPhysicalAddresses, Master, ScatterGather, TaggedQueuing,
       AutoRequestSense and MultipleRequestPerLu. Do not change these defaults. */
    ConfigInfo->SynchronizationModel=StorSynchronizeFullDuplex;
    TraceInit(24,SP_RETURN_FOUND,a,ConfigInfo);
    return SP_RETURN_FOUND;
}
_Use_decl_annotations_
BOOLEAN Initialize(PVOID DeviceExtension){PADAPTER a=(PADAPTER)DeviceExtension;a->Running=TRUE;TraceInit(30,1,a,NULL);return TRUE;}
_Use_decl_annotations_
BOOLEAN ResetBus(PVOID DeviceExtension,ULONG PathId){
    PADAPTER a=(PADAPTER)DeviceExtension;UNREFERENCED_PARAMETER(PathId);
    /* Storport already holds StartIoLock before HW_RESET_BUS. */
    ++a->Stats.Resets;ClearPolicy(a);return TRUE;
}
_Use_decl_annotations_
SCSI_ADAPTER_CONTROL_STATUS AdapterControl(PVOID DeviceExtension,SCSI_ADAPTER_CONTROL_TYPE ControlType,PVOID Parameters){
    PADAPTER a=(PADAPTER)DeviceExtension;
    if(ControlType==ScsiQuerySupportedControlTypes){
        PSCSI_SUPPORTED_CONTROL_TYPE_LIST p=(PSCSI_SUPPORTED_CONTROL_TYPE_LIST)Parameters;
        if(p){if(p->MaxControlType>ScsiQuerySupportedControlTypes)p->SupportedTypeList[ScsiQuerySupportedControlTypes]=TRUE;if(p->MaxControlType>ScsiStopAdapter)p->SupportedTypeList[ScsiStopAdapter]=TRUE;if(p->MaxControlType>ScsiRestartAdapter)p->SupportedTypeList[ScsiRestartAdapter]=TRUE;}
        return ScsiAdapterControlSuccess;
    }
    if(ControlType!=ScsiStopAdapter&&ControlType!=ScsiRestartAdapter)return ScsiAdapterControlUnsuccessful;
    /* Stop/Restart run with Storport's InterruptLock, potentially at DIRQL.
       Storport quiesces/completes all I/O before Stop and dispatches none until
       Restart returns. Do not acquire a lower-IRQL lock in these callbacks. */
    a->Running=ControlType==ScsiRestartAdapter;ClearPolicy(a);return ScsiAdapterControlSuccess;
}
_Use_decl_annotations_
VOID FreeAdapter(PVOID DeviceExtension){
    PADAPTER a=(PADAPTER)DeviceExtension;
    TraceInit(40,0,a,NULL);
    a->Running=FALSE;a->Owner=0;
    if(a->Image){StorPortFreePool(a,a->Image);a->Image=NULL;}
    if(a->OwnsSlot){a->OwnsSlot=FALSE;InterlockedExchange(&AdapterSlot,0);}
}
_Use_decl_annotations_
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject,PUNICODE_STRING RegistryPath){
    HW_INITIALIZATION_DATA init={0};NTSTATUS status;
    init.HwInitializationDataSize=sizeof(init);init.AdapterInterfaceType=Internal;
    init.HwInitialize=Initialize;init.HwFindAdapter=(PVOID)FindAdapter;init.HwStartIo=StartIo;
    init.HwResetBus=ResetBus;init.HwAdapterControl=AdapterControl;init.HwFreeAdapterResources=FreeAdapter;
    init.DeviceExtensionSize=sizeof(ADAPTER);init.MapBuffers=STOR_MAP_ALL_BUFFERS_INCLUDING_READ_WRITE;
    init.TaggedQueuing=TRUE;init.AutoRequestSense=TRUE;init.MultipleRequestPerLu=TRUE;
    init.FeatureSupport=STOR_FEATURE_VIRTUAL_MINIPORT;init.SrbTypeFlags=SRB_TYPE_FLAG_SCSI_REQUEST_BLOCK;
    init.AddressTypeFlags=ADDRESS_TYPE_FLAG_BTL8;
    init.HwInitializeTracing=InitializeTracing;init.HwCleanupTracing=CleanupTracing;
    status=(NTSTATUS)StorPortInitialize(DriverObject,RegistryPath,&init,NULL);
    TraceInit(1,(ULONG)status,NULL,NULL);
    if(!NT_SUCCESS(status)&&InitProvider)CleanupTracing(DriverObject);
    return status;
}
