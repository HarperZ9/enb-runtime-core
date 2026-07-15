# Task 01 — Neutral Lifecycle Gate Evidence

Date: 2026-07-15

## Repository and provenance controls

- Target path was absent before initialization.
- One independent Git repository was initialized directly at `C:\dev\enb-runtime-core`.
- Branch: `chore/bootstrap-runtime`.
- No worktree or remote was created.
- Implementation was authored from the task contract; no neighboring product source was read, copied, or modified.
- `index map --root C:/dev --dry-run` completed without writing an artifact and reported 218 repositories, 129 dirty.

## Toolchain

- CMake 4.2.0.
- Visual Studio 18 2026 generator.
- MSVC 19.50.35721.0, x64.
- Windows SDK 10.0.26100.0.
- Generated project contains `<RuntimeLibrary>MultiThreadedDebug</RuntimeLibrary>`.
- Generated project contains `<VcpkgEnabled>false</VcpkgEnabled>`; no external package integration participates in the build.

## RED/GREEN record

All production behavior followed a failing test. Commands were run from the repository unless a build directory is stated.

| Cycle | RED evidence | Minimal implementation | GREEN evidence |
| --- | --- | --- | --- |
| 1 — bootstrap/activation | `cmake --build --preset vs18-x64-static-debug` exited 1 with MSVC `C1083`: public lifecycle header did not exist. | Added the typed bootstrap/active contract and activation transition. | Build exited 0; CTest reported 1/1 passed. |
| 2 — save acknowledgement/outcomes | Build exited 1 on absent `BeginSave`, transition context, save states, three outcome events, and acknowledgement diagnostic. | Added acknowledged first-save entry through quiesce/baseline phases and outcome convergence on reapply-pending. | Build exited 0; CTest reported 1/1 passed. |
| 3a — nested saves | CTest exited 1 with four expected assertions: nested begin rejected, first outcome reapplied early, and the outer outcome was rejected. | Added save-depth increment/decrement and outermost-outcome gating. | Build exited 0; CTest reported 1/1 passed. |
| 3b — verified barrier | Build exited 1 because `ReapplyAtVerifiedBarrier` did not exist. | Added the sole reapply transition, activation-state validation, and one generation increment. | Build exited 0; CTest reported 1/1 passed. |
| 4 — shutdown/interruption/invalid events | Build exited 1 because terminal states and events did not exist. | Added acknowledged active shutdown, baseline-safe save interruption, pending shutdown, stopped/failed terminals, and no-change rejection. | Build exited 0; CTest reported 1/1 passed. |

## Verified behavior

- Initial state is inactive/bootstrap, generation zero, with mutation denied.
- Activation is explicit and increments generation to one.
- Mutation permission is true only while state is active.
- Unacknowledged first-save entry is rejected without state or generation change.
- Acknowledged first-save entry records quiesce requested, baseline restored, then save in progress.
- Nested saves remain in progress until each accepted depth has one outcome.
- Success, failure, and cancellation outcomes converge on reapply-pending.
- Only the verified-barrier event returns active after a save and increments generation once.
- Shutdown and interruption never re-enable mutation; terminal states reject reapply.
- Invalid events retain state and generation and return a stable rejection diagnostic.

## Final verification evidence

1. `cmake --fresh --preset vs18-x64-static` — exit 0; configure and generation completed.
2. `cmake --build --preset vs18-x64-static-debug` — exit 0; static library and assertion executable built without warnings or injected package steps.
3. From `out/build/vs18-x64-static`, `ctest --output-on-failure --no-tests=error -C Debug` — exit 0; 1/1 test passed, 0 failed.
4. Entire authored repository product-name token scan — 0 matches for both prohibited tokens.
5. Library source path scan for rendering/asset extension families — 0 matches.
6. Library source content scan for rendering/asset extension references — 0 matches.

This report is included in the single requested commit, `feat: bootstrap neutral lifecycle gate`.
