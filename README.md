# ENB Runtime Core

`enb-runtime-core` is a small C++23 state machine for coordinating host lifecycle, save quiescence, baseline restoration, and mutation reapplication. It has no third-party dependencies and owns no host or product objects.

## Build and test

Requirements:

- CMake 4.0 or newer
- Visual Studio 18 2026 with the x64 C++ toolchain

```powershell
cmake --preset vs18-x64-static
cmake --build --preset vs18-x64-static-debug
ctest --output-on-failure --no-tests=error -C Debug --test-dir out/build/vs18-x64-static
```

The preset selects the x64 generator and the static MSVC runtime for every configuration.

## Contract

`LifecycleGate::dispatch` accepts a typed `Event` and optional `TransitionContext`, then returns a value-only `TransitionResult`. Status and diagnostic enums have explicit numeric values. Every result reports the previous and current state, application generation, and whether product mutation is currently permitted.

Mutation is permitted only in `State::Active`. A first `BeginSave` from `Active` requires the caller to acknowledge that baseline restoration completed. The gate then atomically records quiesce requested, baseline restored, and save in progress. Nested saves retain the save state until every matching outcome has arrived.

`SaveSucceeded`, `SaveFailed`, and `SaveCancelled` all converge on `ReapplyPending` after the outermost save outcome. Only `ReapplyAtVerifiedBarrier` returns the gate to `Active`, incrementing the generation exactly once. The caller supplies explicit outcomes; no host post-save callback is assumed.

Shutdown and interruption paths retain mutation denial. Starting either path from `Active` requires the same restoration acknowledgement; starting from a save or reapply-pending state uses the already-restored baseline.

See [Architecture Boundary](docs/ARCHITECTURE_BOUNDARY.md) for ownership and integration constraints.
