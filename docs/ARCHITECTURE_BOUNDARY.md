# Architecture Boundary

## Purpose

The runtime core is a deterministic lifecycle gate. It answers whether a transition is valid and whether product mutation is permitted after that transition. It does not perform the mutation or baseline restoration itself.

## Owned responsibilities

- The explicit lifecycle states `InactiveBootstrap`, `Active`, `QuiesceRequested`, `BaselineRestored`, `SaveInProgress`, `ReapplyPending`, `Shutdown`, `Stopped`, and `Failed`.
- Typed lifecycle and save events.
- First-save acknowledgement validation.
- Nested-save depth accounting.
- Application generation accounting.
- Stable transition status and diagnostic enums.
- No-change rejection of invalid events.

The `BeginSave` transition is synchronous. When accepted from `Active`, it records the quiesce and baseline-restored phases before returning `SaveInProgress`; callers never observe a partially accepted save entry.

## Caller responsibilities

- Serialize calls to `LifecycleGate`; the class contains no internal synchronization.
- Quiesce product mutation before acknowledging restoration.
- Restore the complete baseline before setting `baseline_restoration_acknowledged`.
- Deliver one explicit outcome for each accepted save depth.
- Choose and verify the barrier before dispatching `ReapplyAtVerifiedBarrier`.
- Perform all host-specific mutation, restoration, logging, and recovery work.
- Stop product work whenever a transition is rejected or mutation permission is false.

## Excluded concerns

The library contains no host discovery, callback hooking, graphics implementation, memory patching, creative controls, artistic values, profiles, packaged assets, endpoint configuration, or product-specific policy. Those concerns belong in integration layers outside this repository.

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
