# Lifecycle Gate Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a dependency-free C++23 lifecycle and save mutation gate with deterministic, fail-closed transitions for later host integration.

**Architecture:** A single `LifecycleGate` owns lifecycle state, save nesting depth, and application generation. Callers submit typed events plus a minimal transition context; each dispatch returns a value-only transition result with fixed numeric status and diagnostic enums. The gate owns no host objects and performs no product work.

**Tech Stack:** C++23, CMake 4.x, Visual Studio 18 2026 x64, MSVC static runtime, CTest, and a dependency-free assertion executable.

## Global Constraints

- Create exactly one independent repository at `C:\dev\enb-runtime-core` on `chore/bootstrap-runtime`; do not create a worktree.
- Keep the shared source mechanically neutral and free of product identifiers, rendering code, creative values, profiles, and assets.
- Do not inspect, copy, or modify recovered product source.
- Use no third-party runtime or test dependencies.
- Write tests before production behavior and preserve RED/GREEN command evidence in `.superpowers/sdd/task-01-report.md`.
- Configure with the Visual Studio 18 2026 x64 generator and the static MSVC runtime.
- Make exactly one final commit with message `feat: bootstrap neutral lifecycle gate`; do not add a remote or push.

---

### Task 1: Bootstrap and activation contract

**Files:**
- Create: `CMakeLists.txt`
- Create: `CMakePresets.json`
- Create: `tests/LifecycleGateTests.cpp`
- Create: `include/enbcore/runtime/LifecycleGate.hpp`
- Create: `src/runtime/LifecycleGate.cpp`

**Interfaces:**
- Produces: fixed-value `State`, `Event`, `TransitionStatus`, and `DiagnosticCode` enums; `TransitionContext`; `TransitionResult`; and `LifecycleGate::dispatch(Event, TransitionContext)`.

- [ ] Write a dependency-free test for the inactive initial state and activation transition.
- [ ] Configure and build to prove RED because the production contract does not exist.
- [ ] Add the minimal public contract and implementation for activation.
- [ ] Build and run CTest to prove GREEN.

### Task 2: Save quiesce and outcome contract

**Files:**
- Modify: `tests/LifecycleGateTests.cpp`
- Modify: `src/runtime/LifecycleGate.cpp`

**Interfaces:**
- Consumes: `LifecycleGate::dispatch` and `TransitionContext::baselineRestorationAcknowledged`.
- Produces: acknowledged first-save entry, rejected unacknowledged entry, and the three explicit save outcomes.

- [ ] Add failing tests for acknowledgement gating and every save outcome.
- [ ] Run CTest and confirm the new tests fail for missing behavior.
- [ ] Implement the minimum save-entry and outcome transitions.
- [ ] Run CTest and confirm all tests pass.

### Task 3: Nested save and verified-barrier contract

**Files:**
- Modify: `tests/LifecycleGateTests.cpp`
- Modify: `src/runtime/LifecycleGate.cpp`

**Interfaces:**
- Consumes: first-save entry and save outcome handling.
- Produces: nested save depth behavior and the sole reactivation event with one generation increment.

- [ ] Add failing tests for nested depth draining and verified-barrier reapplication.
- [ ] Run CTest and confirm the new tests fail for missing behavior.
- [ ] Implement minimum nested-depth and reapplication logic.
- [ ] Run CTest and confirm all tests pass.

### Task 4: Terminal and invalid-event contract

**Files:**
- Modify: `tests/LifecycleGateTests.cpp`
- Modify: `src/runtime/LifecycleGate.cpp`

**Interfaces:**
- Consumes: all preceding state transitions.
- Produces: interruption, shutdown, stopped, failed, and no-change invalid-event behavior.

- [ ] Add failing tests for terminal transitions and invalid events.
- [ ] Run CTest and confirm the new tests fail for missing behavior.
- [ ] Implement the minimum terminal and rejection logic.
- [ ] Run CTest and confirm all tests pass.

### Task 5: Boundary documentation and release evidence

**Files:**
- Create: `README.md`
- Create: `docs/ARCHITECTURE_BOUNDARY.md`
- Create: `LICENSE`
- Create: `.gitignore`
- Create: `.gitattributes`
- Create: `.superpowers/sdd/task-01-report.md`

**Interfaces:**
- Consumes: the final public contract and verified command output.
- Produces: build/use guidance, explicit ownership boundaries, RED/GREEN evidence, and reproducibility notes.

- [ ] Document the public state machine, caller responsibilities, and excluded concerns.
- [ ] Run a fresh preset configure, build, and `ctest --output-on-failure --no-tests=error`.
- [ ] Scan library sources for prohibited identifiers and rendering/asset extensions.
- [ ] Inspect status and the complete diff, then record exact verification evidence.
- [ ] Commit once with the required message and verify repository state and commit metadata.
