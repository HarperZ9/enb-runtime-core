# Task 02 — SDK 1002 Contract and Official ABI Evidence

Date: 2026-07-15

## Starting state and custody controls

- Starting commit: `5105701080d4952a29037ed840ba6e603164922b` on `chore/bootstrap-runtime`.
- The starting tree was clean.
- Captured archive SHA-256: `B014F5B4EE28BF6AE97ECF558EDB9F391171B26C1FB2FCBAB6709C74226A8E80`.
- Exact official header SHA-256: `19B3E6A6F6A9D36357682DA321FBD53F0001B88D3011FE553BCADF4E66453288`.
- The archive was extracted only under an external cache root. No official header, binary, archive, or example is present in the authored tree.
- `config/enb-sdk-1002.lock.json` contains hashes and ABI facts without filesystem paths.

## RED/GREEN record

| Cycle | RED evidence | GREEN evidence |
| --- | --- | --- |
| Fixed-width contract and parameter validation | MSVC build exited 1 with `C1083` because `enbcore/enb/SdkContract.hpp` did not exist. | The contract library built and CTest passed 2/2. |
| Fake host readiness validation | MSVC build exited 1 on absent `SdkExports`, `ValidateSdkHost`, and stable readiness codes. | All required-export, version, game-ID, and render-readiness cases passed; CTest passed 2/2. |
| Official-header ABI probe | MSVC build exited 1 with `C1083` for `enbseries.h` because no SDK include path or fallback existed. | With one explicit absolute external root, the hash gate ran before probe compilation and CTest passed 4/4. |

One initial GREEN attempt in the first cycle exposed `C4002` in the local assertion macro when parsing templated array literals. The test syntax was corrected without changing production behavior, then the same cycle passed.

## Admitted host contract

- All eight typed exports are required and checked in a stable order.
- Reported SDK versions are admitted only when `1002 <= reported_version < 2000`.
- Game identifier must equal `0x10000006`.
- Null render info returns `RenderInfoNotReady`; it is a retryable readiness result, not permanent incompatibility.
- Invalid parameter payload length, kind, and kind/size pair have distinct stable codes.
- Export parameter strings are mutable `char*`, matching the official ABI.
- The callback pointer uses the official Windows callback calling convention.
- Package and wrapper fingerprint admission remains a later host-layer gate. An admitted SDK report does not admit a package identity.

## Official ABI facts proved

- SDK macro: `1002`; game ID macro: `0x10000006`.
- SDK integer, SDK boolean, and all mirrored enum storage: size 4, alignment 4.
- Parameter kind IDs: 0–7; sizes by ID: 0, 4, 4, 4, 4, 12, 16, 12.
- Callback IDs: 1–8.
- Required state IDs: 8–23.
- Parameter layout: size 24, alignment 4; payload/size/type offsets 0/16/20.
- Render-info layout: size 32, alignment 8; pointer offsets 0/8/16 and screen offsets 24/28.
- Export function pointers: size 8, alignment 8.
- `SdkExports`: size 64, alignment 8; eight pointer slots at offsets 0 through 56 in eight-byte increments.
- The probe does not construct the official render-info type, so the captured constructor's repeated X-dimension assignment cannot affect verification. Layout is checked directly without copying or modifying the header.

## Final verification evidence

1. A brand-new build directory configured with Visual Studio 18 2026 x64 and the explicit external SDK root — exit 0.
2. Clean Debug build — exit 0; the archive/header hash gate completed before the official probe compilation.
3. `ctest --output-on-failure --no-tests=error -C Debug` — 4/4 passed, 0 failed.
4. A separate configure without `ENBCORE_ENB_SDK_ROOT` exited 1 with the required-root diagnostic, proving no fallback.
5. Generated project verified `MultiThreadedDebug` and package integration disabled.
6. Prohibited product-name scan — 0 matches.
7. Library rendering/asset extension path and content scans — 0 matches.
8. Official/generated authored-tree artifact scan — 0 matches.
9. Lock absolute-path and external path-fragment scans — 0 matches; JSON parse passed.

This report is included in the requested commit, `feat: validate ENB SDK 1002 contract`.
