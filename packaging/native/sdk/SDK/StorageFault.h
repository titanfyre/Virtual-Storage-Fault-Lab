#pragma once

typedef unsigned int SF_U32;
typedef unsigned long long SF_U64;

enum {
    SF_VERSION = 1,
    SF_STATUS = 0, SF_CLAIM = 1, SF_SET = 2, SF_RELEASE = 3,
    SF_READ = 1, SF_WRITE = 2,
    SF_SECTOR_BYTES = 512, SF_SECTOR_COUNT = 131072,
    SF_FIRST_FAULT_BLOCK = 2048, SF_LEASE_SECONDS = 30,
    SF_CONTROL_CODE = 0x53464c01
};

typedef struct SF_POLICY {
    SF_U32 Size, Version, Flags, Every;
    SF_U64 FirstBlock, BlockCount;
    SF_U32 Reserved[4];
} SF_POLICY;

typedef struct SF_STATS {
    SF_U64 Reads, Writes, ReadBytes, WriteBytes, Injected, Rejected, Matching, Resets;
    SF_U64 LeaseRemaining100ns;
    SF_U32 SectorBytes, SectorCount, PolicyActive, Reserved;
} SF_STATS;

typedef struct SF_CONTROL {
    SF_U32 Size, Version, Operation, Result;
    SF_U64 Generation, Owner;
    SF_POLICY Policy;
    SF_STATS Stats;
} SF_CONTROL;

#ifdef __cplusplus
static_assert(sizeof(SF_POLICY) == 48, "Policy wire size");
static_assert(sizeof(SF_STATS) == 88, "Stats wire size");
static_assert(sizeof(SF_CONTROL) == 168, "Control wire size");
#endif

#ifndef _KERNEL_MODE
#include <windows.h>
#include <memory>
#include <stdexcept>
#include <string>
namespace StorageFault {
bool Valid(const SF_POLICY&) noexcept;
bool Valid(const SF_CONTROL&) noexcept;
SF_POLICY Policy(SF_U64 firstBlock,SF_U64 blockCount,unsigned every,unsigned flags);
std::string Utf8(const std::wstring&);
SF_CONTROL Inspect(); // Read owned adapter status without claiming its policy.
class Session {
 struct State;
 std::unique_ptr<State> state_;
public:
 Session();
 ~Session();
 Session(const Session&)=delete;
 Session& operator=(const Session&)=delete;
 void Start();
 void SetPolicy(const SF_POLICY&);
 SF_CONTROL Status();
 std::string LastHeartbeatError();
 void Stop() noexcept;
};
}
#endif
