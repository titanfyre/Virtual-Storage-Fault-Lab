# Validation and scope

Application/SDK 0.1.1; Microsoft-signed Storport driver 0.1.0.0, Windows x64. The returned INF matches the submission, the protected PE image matches the qualified private candidate, and Microsoft catalog trust plus INF/SYS membership pass. Driver source remains private.

## Signed bundle checks

- Both self-contained installers embed the exact returned INF, CAT and SYS, verified as data without executing installer code on the host.
- The public application, CLI, example and both installers build with MSVC warnings as errors, CFG and a static runtime. Both CTest checks pass.
- Twelve private installer-policy checks accept the exact signed product/catalog and reject missing files, an altered product identity and an invalid catalog. They do not install anything on the host.
- The returned signed package completed real installation, runtime and removal campaigns on Windows 10 build 19045 x64, normally and under standard Driver Verifier. Test-signing remained disabled.

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

The signed evaluation bundle includes the actual matching driver and both setup executables. Platform and production qualification remain bounded by the evidence recorded here. A screenshot or synthetic status report is not evidence of a mounted kernel disk.

Qualification must cover actual read/write integrity, deterministic error behavior and recovery, invalid controls, nonadministrator access denial, concurrent I/O, reset, cancellation and removal, uninstall/reinstall, memory pressure, supported Windows versions, Secure Boot/HVCI and long-duration workloads. Tests must target only an independently verified owned virtual adapter and preserve the host and VM baselines.

No guarantee of universal compatibility, zero defects, immunity to bypass or loss prevention is made. This component intentionally introduces I/O errors into a disposable disk; it is not a security boundary against administrators or kernel code.

## Microsoft-signed package VM qualification

The returned Microsoft-signed driver 0.1.0.0 was installed using the bundled console setup on Windows 10 build 19045 x64. The complete campaign passed normally and again under standard Driver Verifier targeted at both storage components. Test-signing was disabled, no guest test certificate was used, and the installed Driver Store SYS hash matched the exact signed return package.

Each campaign passed 1,290 fault-lab runtime checks, including 1,024 concurrent transfers, deterministic read/write error and recovery behavior, malformed controls/SCSI bounds, restricted-token access denial, a real adapter bus reset and policy recovery after an abruptly terminated owner's 30-second lease expired. The actual 64 MiB Storport disk was detected by Windows. No physical disk was accessed or formatted.

The optional recorder was also attached, exercised and detached on that same synthetic disk. The bundled console uninstallers completed removal, both drivers unloaded and unrelated disk filters were preserved. Neither campaign produced a new minidump; the VM returned to its original powered-off baseline with all nine historical checkpoints preserved.

These are bounded Windows 10 VM results. Windows 11, Secure Boot/HVCI, physical-controller, power-transition, broader storage coexistence and long-duration qualification remain pending. A Microsoft signature does not imply that those configurations have been tested.

## Earlier submission-image campaigns

Before submission, the final driver image passed corresponding normal and Driver Verifier campaigns with a temporary guest-only test signature. Those earlier results are separate from the signed-package campaigns above. The original unsigned package remains the CAB submission; guest certificates, lab-signed copies and initial failed startup experiments stay in private evidence.
