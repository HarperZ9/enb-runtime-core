# ENB Runtime Core

`enb-runtime-core` is a small C++23 runtime library for resolving an already-loaded ENB host and coordinating save quiescence, baseline restoration, and mutation reapplication. It has no third-party dependencies and owns no host or product objects.

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

## Contract

`LifecycleGate::dispatch` accepts a typed `Event` and optional `TransitionContext`, then returns a value-only `TransitionResult`. Status and diagnostic enums have explicit numeric values. Every result reports the previous and current state, application generation, and whether product mutation is currently permitted.

Mutation is permitted only in `State::Active`. A first `BeginSave` from `Active` requires the caller to acknowledge that baseline restoration completed. The gate then atomically records quiesce requested, baseline restored, and save in progress. Nested saves retain the save state until every matching outcome has arrived.

`SaveSucceeded`, `SaveFailed`, and `SaveCancelled` all converge on `ReapplyPending` after the outermost save outcome. Only `ReapplyAtVerifiedBarrier` returns the gate to `Active`, incrementing the generation exactly once. The caller supplies explicit outcomes; no host post-save callback is assumed.

Shutdown and interruption paths retain mutation denial. Starting either path from `Active` requires the same restoration acknowledgement; starting from a save or reapply-pending state uses the already-restored baseline.

`ValidateSdkHost` is an ABI readiness check. It requires all eight typed exports, admits reported SDK versions from 1002 through 1999, requires game identifier `0x10000006`, and returns `RenderInfoNotReady` when the host has not published render information yet. Package or wrapper fingerprint admission belongs to a later host integration gate; an admitted SDK report does not admit any package identity by itself.

`ResolveLoadedEnbHost` examines only modules returned by an injected `LoadedModulePlatform`. The Windows adapter snapshots modules already present in the current process and resolves the eight SDK names directly from those handles. Anchorless modules are ignored; partial, incompatible, truncated, and ambiguous snapshots fail closed. A single compatible host with unavailable render information returns `NotReady` together with its complete typed export table so deferred bootstrap can retry without treating readiness as incompatibility.

Host resolution must run from deferred bootstrap after the Windows loader entry point has returned. The library contains no loader entry point and never loads an ENB module.

See [Architecture Boundary](docs/ARCHITECTURE_BOUNDARY.md) for ownership and integration constraints.
