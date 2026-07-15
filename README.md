# ENB Runtime Core

`enb-runtime-core` is a small C++23 runtime library for resolving and fingerprinting an already-loaded ENB host, deferring callback work onto a bounded neutral event queue, coordinating save quiescence and reapplication, and validating a fail-closed ENB-compatible Skyrim engine bridge. It has no third-party dependencies and owns no host, SKSE, CommonLib, renderer, or product objects.

## Build and test

Requirements:

- CMake 4.0 or newer
- Visual Studio 18 2026 with the x64 C++ toolchain

```powershell
cmake --preset vs18-x64-static -DENBCORE_ENB_SDK_ROOT="$env:ENB_SDK_ROOT"
cmake --build --preset vs18-x64-static-debug
ctest --output-on-failure --no-tests=error -C Debug --test-dir out/build/vs18-x64-static
```

The preset selects the x64 generator and the static MSVC runtime for every configuration.

`ENB_SDK_ROOT` must expand to an absolute external cache root with exactly this input layout:

```text
enbseries-sdk-1002.zip
enbseries_sdk/enbseries.h
```

The build has no SDK search-path, network, or vendored fallback. It verifies both inputs against `config/enb-sdk-1002.lock.json` before compiling the private official-header ABI probe, and CTest repeats the hash verification before running the probe.

An optional, read-only wrapper integration test is enabled only when an absolute external path is supplied:

```powershell
cmake --preset vs18-x64-static `
  -DENBCORE_ENB_SDK_ROOT="$env:ENB_SDK_ROOT" `
  -DENBCORE_ENB_WRAPPER_PATH="C:/external/evidence/d3d11.dll"
```

The wrapper is opened in place and is never copied, vendored, or loaded by the library.

## Contract

`LifecycleGate::dispatch` accepts a typed `Event` and optional `TransitionContext`, then returns a value-only `TransitionResult`. Status and diagnostic enums have explicit numeric values. Every result reports the previous and current state, application generation, and whether product mutation is currently permitted.

Mutation is permitted only in `State::Active`. A first `BeginSave` from `Active` requires the caller to acknowledge that baseline restoration completed. The gate then atomically records quiesce requested, baseline restored, and save in progress. Nested saves retain the save state until every matching outcome has arrived.

`SaveSucceeded`, `SaveFailed`, and `SaveCancelled` all converge on `ReapplyPending` after the outermost save outcome. Only `ReapplyAtVerifiedBarrier` returns the gate to `Active`, incrementing the generation exactly once. The caller supplies explicit outcomes; no host post-save callback is assumed.

Shutdown and interruption paths retain mutation denial. Starting either path from `Active` requires the same restoration acknowledgement; starting from a save or reapply-pending state uses the already-restored baseline.

`ValidateSdkHost` is an ABI readiness check. It requires all eight typed exports, admits reported SDK versions from 1002 through 1999, requires game identifier `0x10000006`, and returns `RenderInfoNotReady` when the host has not published render information yet. An admitted SDK report does not admit any package identity by itself.

`ResolveLoadedEnbHost` examines only modules returned by an injected `LoadedModulePlatform`. The Windows adapter snapshots modules already present in the current process and resolves the eight SDK names directly from those handles. Anchorless modules are ignored; partial, incompatible, truncated, and ambiguous snapshots fail closed. A single compatible host with unavailable render information returns `NotReady` together with its complete typed export table so deferred bootstrap can retry without treating readiness as incompatibility.

`EvaluateLoadedEnbWrapper` is the separate package-identity gate. From an already-resolved module it obtains the full module path, streams the existing file through Windows CNG SHA-256, reads its byte size and PE machine, and reads the authored four-part `FileVersion` plus `OriginalFilename` from the version resource. It never loads a module. The immutable allowlist currently admits only the captured x64 `0.5.0.4` wrapper whose SHA-256 is `87583E85CE993E6338486F6D5DE09F9A74661BA245E820DC75E3A7A82C9888C7`, size is `4665344`, and original filename is `d3d11.dll`.

A complete but unlisted observation is `UnknownBuild`. Missing or malformed evidence remains `EvidenceUnavailable`. Strict policy rejects both; audit-only policy admits them with the explicit `AdmittedAuditOnly` result. Audit-only never relabels unknown evidence as a known build.

`CallbackSession` accepts either that retained `NotReady` result or a `Ready` result from explicit bootstrap outside loader lock. It probes render readiness once per explicit attempt, publishes one process-wide target before registering the SDK callback, and rejects a concurrent second session. Stop and failure paths unregister and unpublish once; a saved late thunk is safe after teardown.

The SDK thunk only validates the callback ID and writes an ordered record into a fixed-capacity lock-free SPSC queue. Unknown IDs and overflow are counted. `drain` runs outside callback context and exposes neutral records with explicit lifecycle handoff descriptions. It never dispatches `LifecycleGate` or invokes product mutation. In particular, `PostLoad` is notification-only, `PreSave` requests an explicit save-entry handoff, and `BeginFrame` only reports a possible verified barrier; callers still provide the save outcome and decide whether the barrier is valid. No post-save callback is invented.

Host resolution must run from deferred bootstrap after the Windows loader entry point has returned. The library contains no loader entry point and never loads an ENB module.

The Skyrim engine bridge admits only an exact executable fingerprint, then
validates each adapter-supplied relocation and engine-symbol contract
independently. Address Library artifacts are the first admitted relocation
provider; the API also reserves a provider kind for Truth's future embedded
relocation/signature manifest, so Address Library is not a permanent rule.
Camera, depth, weather/time, render-phase, sky-shader, and lighting-shader
capabilities all begin unavailable. Complete prologue bytes or an exact
vtable-slot/RTTI contract are required before a hook can be reported ready.
The core installs no hook.

An injected `PatchJournal` can apply narrowly scoped data-property changes only
when the product feature gate, lifecycle gate, exact runtime evaluation, and a
validated capability all agree. It preflights the complete transaction,
verifies every write, compensates partial application, and supports idempotent
rollback. No Windows memory writer is included.

This path retains SkyrimBridge's ENB-as-host architecture. It does not import
RAW's replacement D3D11 proxy, phase classifier, DXBC patching, depth ownership,
or renderer pipeline.

See [Architecture Boundary](docs/ARCHITECTURE_BOUNDARY.md) for ownership and integration constraints.
See [Skyrim Engine Bridge Contract](docs/SKYRIM_ENGINE_BRIDGE.md) for runtime,
symbol, capability, evidence, and reversible-mutation requirements.
