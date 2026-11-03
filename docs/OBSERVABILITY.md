# Injected requests and application outcomes

The current status exposes aggregate adapter counters: reads/writes, transferred
bytes, matching commands, injected failures and rejected commands. It does not
label individual requests as retries or map them to an application operation.

For example, injecting a failure into a miniport command does not guarantee that
one `ReadFile` call fails. Windows can retry, split or cache I/O. An application
operation and an adapter command are different measurement boundaries.

For a disposable workload on this product's synthetic disk:

1. Take a status snapshot before the workload.
2. Log your program's operation ID, requested offset/length, final completion
   status and returned byte count.
3. Take another status snapshot after completion and retain the active policy.
4. Compare these observations while keeping their boundaries explicit.

An illustrative result could be "workload read 12 succeeded; the adapter's
injected count increased by 1 during this interval." It is **not** evidence that
read 12 was retried exactly once: other commands may have contributed. Concurrent
traffic and policy changes make this comparison still less specific.

A per-operation attempt history would require additional driver/workload
correlation, timestamps and a documented retry identity. That feature is not
implemented or promised by the current counters. This documentation update adds
no kernel tracing or physical-disk monitoring.

Use only disposable lab data. This tool affects its own volatile synthetic disk;
it does not select a physical disk or filesystem path. See the [README](../README.md)
and [validation](VALIDATION.md) for the tested scope.
