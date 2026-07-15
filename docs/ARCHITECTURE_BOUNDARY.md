# Architecture Boundary

## Purpose

The runtime core provides neutral loaded-host resolution and package fingerprinting, deferred callback delivery, a deterministic lifecycle gate, and a fail-closed Skyrim engine-observation contract. It selects one compatible already-loaded host, classifies on-disk host and game identities, moves callback notifications across an allocation-free boundary, validates adapter-supplied engine symbol evidence, answers whether a lifecycle transition is valid, and journals explicitly gated reversible property patches. It does not install hooks, resolve live relocation symbols, or own product rendering.

## Owned responsibilities

- The explicit lifecycle states `InactiveBootstrap`, `Active`, `QuiesceRequested`, `BaselineRestored`, `SaveInProgress`, `ReapplyPending`, `Shutdown`, `Stopped`, and `Failed`.
- Typed lifecycle and save events.
- First-save acknowledgement validation.
- Nested-save depth accounting.
- Application generation accounting.
- Stable transition status and diagnostic enums.
- No-change rejection of invalid events.
- Injectable enumeration of already-loaded modules.
- Exact resolution of the eight ENB SDK exports into `SdkExports`.
- Fail-closed candidate selection and retryable render-info readiness.
- Canonical path lookup for an already-resolved module handle.
- Streaming SHA-256, file-size, PE-machine, file-version, and original-name evidence.
- Immutable wrapper-build admission with strict and audit-only policy results.
- One explicitly bootstrapped process-wide callback session.
- Ordered translation of all eight SDK callback IDs into neutral records.
- Bounded queue, unknown-ID, and overflow diagnostics.
- Idempotent callback unregistration and publication teardown.
- Exact Skyrim executable identity admission, currently limited to the captured
  `1.6.1170.0` fingerprint.
- Runtime- and version-constrained relocation-provider and engine-adapter
  symbol descriptors.
- Exact relocation-artifact admission. Address Library is the first provider;
  an embedded product-owned manifest is a distinct reserved provider kind.
- Module-bound, protection-aware prologue and vtable/RTTI validation results.
- Six observer capabilities that remain unavailable when any assigned contract
  is missing or invalid.
- Transactional, read-back-verified property patch journaling with reverse
  rollback and explicit recovery state.

The `BeginSave` transition is synchronous. When accepted from `Active`, it records the quiesce and baseline-restored phases before returning `SaveInProgress`; callers never observe a partially accepted save entry.

## Caller responsibilities

- Serialize calls to `LifecycleGate`; the class contains no internal synchronization.
- Quiesce product mutation before acknowledging restoration.
- Restore the complete baseline before setting `baseline_restoration_acknowledged`.
- Deliver one explicit outcome for each accepted save depth.
- Choose and verify the barrier before dispatching `ReapplyAtVerifiedBarrier`.
- Invoke host resolution only from deferred bootstrap, outside the Windows loader entry point.
- Serialize session bootstrap, retry, drain, stop, and failure operations.
- Treat the host callback thread as the queue's single producer and one integration thread as its single consumer.
- Drain callback records outside callback context and perform every lifecycle dispatch explicitly.
- Apply package or wrapper fingerprint admission before enabling product integration.
- Choose strict or audit-only wrapper policy explicitly and retain unknown-build diagnostics.
- Perform all host-specific mutation, restoration, logging, and recovery work.
- Collect loaded `SkyrimSE.exe` file and image evidence without loading another
  module.
- Resolve relocation IDs through the selected provider and implement
  memory-region, byte-read, byte-write, and MSVC RTTI inspection adapters.
- Install and remove hooks only after the core reports a hook-ready contract.
- Keep every engine mutation behind both an explicit product feature gate and
  the lifecycle mutation gate.
- Stop product work whenever a transition is rejected or mutation permission is false.

## Excluded concerns

The library contains no module loading, loader entry point, hook installer,
SKSE/CommonLib dependency, D3D11 proxy, graphics implementation, shader
replacement, creative controls, artistic values, profiles, packaged assets,
endpoint configuration, or product-specific policy. The patch journal writes
only through an injected adapter after all gates validate; this repository has
no platform memory writer. Callback context contains no lifecycle dispatch,
product mutation, allocation, blocking, or logging. Those concerns belong in
integration layers outside this repository.

The Skyrim bridge is ENB-compatible lineage: ENB remains the host. RAW's proxy
ownership, nine-phase replacement pipeline, depth capture, DXBC patching, and
renderer stack are explicitly outside this boundary. See
[Skyrim Engine Bridge Contract](SKYRIM_ENGINE_BRIDGE.md).

## Transition summary

| Current state | Event | Requirement | Next state | Generation |
| --- | --- | --- | --- | --- |
| `InactiveBootstrap` | `Activate` | none | `Active` | +1 |
| `Active` | `BeginSave` | baseline restoration acknowledged | `SaveInProgress` through quiesce and baseline-restored phases | unchanged |
| `SaveInProgress` | `BeginSave` | none; nested depth only | `SaveInProgress` | unchanged |
| `SaveInProgress` | any save outcome | one depth is consumed | `SaveInProgress` or `ReapplyPending` at depth zero | unchanged |
| `ReapplyPending` | `ReapplyAtVerifiedBarrier` | caller verified barrier | `Active` | +1 |
| active or baseline-restored path | `RequestShutdown` | acknowledgement is required from `Active` | `Shutdown` | unchanged |
| `Shutdown` | `CompleteShutdown` | none | `Stopped` | unchanged |
| active or baseline-restored path | `Interrupt` | acknowledgement is required from `Active` | `Failed` | unchanged |

Every other event/state pair is rejected without changing state or generation. Terminal states cannot re-enable mutation.

## Callback handoff boundary

`CallbackSession::bootstrap` is an explicit operation that must run after loader lock. A retained resolver `NotReady` export table remains available for deterministic one-probe retries. Activation reserves the process slot, resets a fixed-capacity target, publishes it, and only then registers the exact SDK thunk. A second session cannot publish or register while that slot is owned.

The thunk is `noexcept`, bounded, allocation-free, and lock-free on the supported x64 target. It is only the SPSC producer. `drain` is the consumer and preserves accepted callback order. Unknown IDs are rejected into diagnostics, and a full queue increments `overflow_events` rather than silently overwriting an older record.

Neutral handoffs describe, but never execute, lifecycle work:

| SDK callback | Neutral lifecycle handoff |
| --- | --- |
| `OnInit` | activation requested |
| `PreSave` | save entry requested; baseline restoration remains caller-owned |
| `OnExit` | shutdown requested |
| `BeginFrame` | possible verified frame barrier observed |
| `EndFrame`, `PostLoad`, `PreReset`, `PostReset` | notification only |

`PostLoad` is not treated as a save outcome. The integration caller supplies `SaveSucceeded`, `SaveFailed`, or `SaveCancelled` separately and verifies any later reapplication barrier. There is no `PostSave` callback in the contract.

## SDK ABI boundary

The SDK contract owns fixed-width public values, binary layouts, typed export pointers, parameter validation, and host ABI readiness. The loaded-host resolver builds that table through an injected platform boundary. Its Windows adapter enumerates the current process with `EnumProcessModules` and queries existing handles with `GetProcAddress`; it has no module-loading path.

`ENBGetSDKVersion` is the candidate anchor. Modules without that exact export are ignored even if they contain similarly named or partial decoy symbols. An anchored candidate must contain all seven remaining exact exports. Any partial candidate, incompatible complete candidate, incomplete enumeration, or more than one compatible complete candidate rejects the whole snapshot. Only one compatible complete host is retained. If its render info is null, the resolver returns `NotReady` with the same module handle and complete export table that a ready result would carry.

The admitted SDK report range is `1002 <= reported_version < 2000`. This range follows the official 1000-family compatibility rule and is not package admission. A null render-info result means the host is not ready yet; it is not a permanent incompatibility.

The private-build ABI probe includes the official header only from the explicit external cache root. Hash verification precedes compilation and test execution. The official render-info constructor is never used by the probe because the captured header assigns its X dimension twice; layout and function ABI are verified without copying or modifying that header.

## Wrapper fingerprint boundary

Wrapper fingerprinting starts only after loaded-host resolution has returned a module handle. `GetModuleFileNameW` is retried with bounded buffer growth; the resulting file is opened read-only and in place. Windows CNG hashes it in fixed-size chunks. The same inspection captures the byte size and PE machine, while the injected version reader supplies the authored `StringFileInfo` `FileVersion` and `OriginalFilename` values. The file is never copied or loaded.

The allowlist is an immutable exact record, not a version-range rule. SHA-256, byte size, four-part authored version, original filename, and x64 machine must all match one record. A mismatch produces `UnknownBuild` with field flags. An incomplete read produces `EvidenceUnavailable`. Strict policy rejects either result; audit-only policy permits continuation while preserving the non-known classification.

Module-path and version-resource access are injected interfaces. Synthetic-file tests exercise the streaming hash and PE reader without bundling an ENB artifact. A separately enabled CTest accepts only the cache variable `ENBCORE_ENB_WRAPPER_PATH` and reads that external file in place.
