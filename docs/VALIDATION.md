# Validation and scope

Engineering preview. Compilation and offline checks do not establish loaded-driver stability or Windows compatibility. The kernel source is private, and the public source build never installs it.

## Implemented boundary

- One 64 MiB RAM-backed synthetic disk; 512-byte sectors and no physical backing path.
- Read/write error rules apply only inside the owned synthetic adapter. The SDK validates its exact hardware ID and service through SetupAPI before opening its storage-port interface.
- Matching combines the configured read/write commands whose validated transfer intersects the range; a transfer starting below sector 2048 is exempt. Every Nth matching command fails in full. The sequence resets on policy replacement/clear/expiry/release/reset/stop. Other counters accumulate for this adapter instance; byte counters count successful transfers, while request counters include injected failures.
- Fault ranges exclude the first 1 MiB, but this does not protect all filesystem metadata. Use disposable filesystems and fixtures.
- One administrator/SYSTEM policy owner at a time, with a 30-second lease. The lease is checked on I/O/control requests; there is no claim of instantaneous process-exit detection. The SDK renews it every five seconds, and clears it on normal scope exit.
- No artificial latency scheduler, physical device filtering, filesystem-content inspection, encryption, persistence or cross-reboot recovery is implemented.
- The counters describe virtual-disk requests observed by this miniport, not application operations, physical-media faults or driver-only overhead. Windows caching, retries and request splitting can alter application-visible outcomes.

## User-mode checks

The native Release build uses warnings-as-errors, CFG, ASLR, NX and a static C++ runtime. Offline tests validate protocol packing, range boundaries, flags, reserved fields, lease-response bounds and one million reproducible malformed-policy mutations. A synthetic preview path exercises the CLI without opening the driver. AddressSanitizer is available through `Build.ps1 -Asan`.

The installer's public source verifies the embedded driver identity, trusted Microsoft catalog and INF/SYS catalog membership. Source-only builds contain no driver payload; no unsigned driver or lab certificate belongs in a public download.

## Kernel and deployment qualification

The private driver's build, loaded-driver campaigns, signing return and platform matrix are recorded separately. Until those gates are completed, the public release remains an application/SDK preview, not an installable production package. A screenshot or synthetic status report is not evidence of a mounted kernel disk.

Qualification must cover actual read/write integrity, deterministic error behavior and recovery, invalid controls, nonadministrator access denial, concurrent I/O, reset, cancellation and removal, uninstall/reinstall, memory pressure, supported Windows versions, Secure Boot/HVCI and long-duration workloads. Tests must target only an independently verified owned virtual adapter and preserve the host and VM baselines.

No guarantee of universal compatibility, zero defects, immunity to bypass or loss prevention is made. This component intentionally introduces I/O errors into a disposable disk; it is not a security boundary against administrators or kernel code.

## Completed final-candidate VM campaigns

The final unsigned submission image was exercised with a temporary guest-only test signature on Windows 10 build 19045 x64, normally and under standard Driver Verifier targeted at both storage components. The actual Storport disk was detected by Windows; no physical drive was targeted or formatted. The fault lab passed deterministic error/recovery tests, 1,024 concurrent transfers, malformed controls/SCSI bounds, restricted-token access checks, a real adapter bus reset and abrupt-owner-exit lease expiry. The recorder passed data/status integrity, completion metadata, 600 overflow writes, 1,024 concurrent transfers, owner cleanup and a PnP restart with its control owner still open. Filter association was removed without changing unrelated entries; both drivers unloaded. Both VM baselines and all nine historical checkpoints were preserved; no new minidump was observed.

These are bounded VM qualification results, not Microsoft-signed return-package tests or Windows 11, Secure Boot/HVCI, physical-controller, power-transition, storage coexistence or long-duration qualification. The original unsigned package is the CAB submission; guest certificates and signed lab copies stay private. Initial failed startup experiments and corrections remain in private evidence.
