# ENB SDK 1002 Contract Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a product-neutral, fixed-width SDK 1002 ABI contract, fail-closed host readiness validation, and an official-header probe whose inputs are hash-gated outside the repository.

**Architecture:** The public library independently defines only the required ABI-facing values, layouts, and function pointer types. Unit tests exercise validation with fake exports; a separate private-build executable includes the official external header and statically proves binary layout and signature facts. CMake receives one explicit absolute SDK cache root and verifies the locked archive/header hashes before compiling or running the probe.

**Tech Stack:** C++23, CMake 4.x, Visual Studio 18 2026 x64, MSVC static runtime, CTest, and PowerShell only for external artifact custody preparation.

## Global Constraints

- Continue from commit `5105701080d4952a29037ed840ba6e603164922b` on `chore/bootstrap-runtime`.
- Create one new commit named `feat: validate ENB SDK 1002 contract`; do not amend or push.
- Keep the committed tree free of official SDK headers, binaries, examples, absolute SDK paths, creative content, and prohibited product-name tokens.
- Read the verified official header only from an external cache; do not read example or binary contents.
- Admit reported versions only when `1002 <= reported_version < 2000`.
- Treat package fingerprint admission as a later host-layer concern; this slice validates the locked SDK inputs and ABI only.
- Preserve mutable `char*` export parameters and the official callback calling convention.
- Treat null render info as `RenderInfoNotReady`, not permanent incompatibility.
- Use strict RED/GREEN TDD and record evidence in `.superpowers/sdd/task-02-report.md`.

---

### Task 1: Fixed-width parameter and interface contract

**Files:**
- Create: `tests/SdkContractTests.cpp`
- Create: `include/enbcore/enb/SdkContract.hpp`
- Create: `src/enb/SdkContract.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**

```cpp
std::uint32_t ParameterSize(ParameterKind kind) noexcept;
ValidationResult ValidateParameter(const Parameter& parameter) noexcept;
```

- [ ] Add tests for enum values, expected parameter sizes, payload layout, and shape/type/size rejection.
- [ ] Build to prove RED because the public contract does not exist.
- [ ] Add the minimum fixed-width types and parameter validation implementation.
- [ ] Build and run CTest to prove GREEN.

### Task 2: Fail-closed host readiness validation

**Files:**
- Modify: `tests/SdkContractTests.cpp`
- Modify: `include/enbcore/enb/SdkContract.hpp`
- Modify: `src/enb/SdkContract.cpp`

**Interfaces:**

```cpp
ValidationResult ValidateSdkHost(const SdkExports& exports) noexcept;
```

- [ ] Add fake-export tests for every required null pointer, SDK family/minimum checks, required game ID, ready render info, and `RenderInfoNotReady`.
- [ ] Run CTest and confirm the new cases fail for missing host validation.
- [ ] Implement the minimum deterministic validation order and stable codes.
- [ ] Run CTest and confirm all unit tests pass.

### Task 3: Locked official-header ABI probe

**Files:**
- Create: `config/enb-sdk-1002.lock.json`
- Create: `cmake/VerifyEnbSdk1002.cmake`
- Create: `tests/abi/EnbSdk1002AbiProbe.cpp`
- Modify: `CMakeLists.txt`
- Modify: `README.md`
- Modify: `docs/ARCHITECTURE_BOUNDARY.md`

**Interfaces:**

The configure invocation must set `ENBCORE_ENB_SDK_ROOT` to a caller-supplied absolute cache root; no concrete filesystem path is committed.

- [ ] Add the official probe assertions and CMake target before the hash lock/gate exists, then build to prove RED.
- [ ] Add a path-free lock containing the exact archive/header SHA-256 values and expected ABI facts.
- [ ] Add a build dependency that runs the hash verifier before probe compilation and a CTest hash test that precedes probe execution.
- [ ] Configure, build, and run CTest with the explicit external root to prove GREEN.

### Task 4: Evidence and commit

**Files:**
- Create: `.superpowers/sdd/task-02-report.md`

- [ ] Run a fresh configure/build and `ctest --output-on-failure --no-tests=error`.
- [ ] Scan for prohibited names, secrets, absolute paths, and tracked official/generated SDK artifacts.
- [ ] Inspect complete status/diff and verify the existing commit remains the parent.
- [ ] Record RED/GREEN, hash, ABI, and scan evidence.
- [ ] Commit once with the requested message and verify the new commit without pushing.
