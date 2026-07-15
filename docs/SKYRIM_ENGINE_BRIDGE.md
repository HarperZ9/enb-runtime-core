# Skyrim Engine Bridge Contract

## Intent

This slice is the neutral safety boundary for an ENB-compatible SkyrimBridge
adapter. ENB remains the rendering host. The core can admit an exact Skyrim
runtime identity, validate adapter-supplied relocation and engine-symbol
contracts, publish fail-closed observation capabilities, and journal narrowly
scoped reversible property changes. It does not install an SKSE plugin, resolve
live relocation IDs, or own a renderer. The release adapter is loaded by ENB
and has no SKSE or CommonLib runtime/link dependency.

This is deliberately not RAW. It contains no D3D11 proxy, swap-chain ownership,
DXBC rewriting, render replacement, material pipeline, phase classifier, or
post-processing stack. Those are useful evidence about the old lineage, but
they are incompatible with an ENB-hosted bridge.

## Admission sequence

1. The product adapter observes the loaded `SkyrimSE.exe` module, hashes the
   file, reads its authored version metadata, and supplies its loaded image
   bounds.
2. `EvaluateRuntimeIdentity` requires the exact module name, x64 architecture,
   nonempty SHA-256 and version evidence, valid loaded image bounds, and an
   exact allowlist match. The only built-in identity is the captured
   `1.6.1170.0` executable. Runtime admission does not admit any symbol.
3. The ENB-loaded adapter supplies the Address Library artifact identity,
   selected runtime, and adapter build version. The only admitted relocation
   artifacts are the two captured Address Library v2 databases for 1.6.1170;
   size and SHA-256 must match exactly.
4. Every `SymbolDescriptor` supplies its own exact runtime and inclusive
   provider-version constraints. A relocation ID must resolve inside the
   loaded image.
5. A data observation requires a readable region. An inline hook additionally
   requires a complete expected prologue in readable executable memory. A
   vtable hook requires a declared slot, exact RTTI identity, a readable
   vtable entry, and a readable executable target inside the module.
6. `EvaluateObserverCapabilities` starts all six capabilities unavailable. A
   capability becomes ready only when every descriptor assigned to it passes.
   A failed or missing descriptor never degrades into a best-effort hook.

The core never arms a hook. `hook_arming_permitted` is a validation result for a
product adapter that owns installation and teardown. The adapter must also
prove instruction boundaries for a prologue and implement RTTI matching by
walking the MSVC complete-object-locator/type-descriptor chain; a name supplied
without that evidence is not sufficient.

SKSE detection is optional and passive. It may inform collision avoidance, but
SKSE absence never blocks runtime, symbol, capability, observation, or property
admission.

## Direct Address Library parser

`ParseAddressLibraryV2` independently implements the documented v2 header and
nibble/delta encoding. The format provenance is CommonLibSSE-NG's official
[`REL/ID.h`](https://github.com/CharmedBaryon/CommonLibSSE-NG/blob/main/include/REL/ID.h)
and [`REL/ID.cpp`](https://github.com/CharmedBaryon/CommonLibSSE-NG/blob/main/src/REL/ID.cpp);
no CommonLib implementation code is copied or linked.

Parsing requires an opaque `RelocationArtifactReceipt` issued only by
`AdmitRelocationArtifact`. The admission boundary first matches the artifact's
kind, runtime, variant, size, and expected SHA-256 against the built-in
allowlist, then calls the supplied `RelocationArtifactDigestVerifier` over the
exact bytes. That verifier is a security boundary: a production adapter must
compute SHA-256 from the bytes and compare it with the supplied digest; it must
not trust filename or caller metadata. The receipt is bound to that exact
immutable byte span and cannot be ordinarily constructed or retargeted.

The parser then checks format 2, runtime 1.6.1170.0, eight-byte pointer size,
positive bounded entry count, every variable-width read, delta/scaling overflow
and underflow, module offset bounds, trailing bytes, and duplicate IDs. Entries
are sorted into an owned database. The core creates no shared writable mapping.

## Capability boundary

| Capability | Recovered evidence | Initial live state |
| --- | --- | --- |
| Camera / inverse view-projection | Typed `PlayerCamera` and `NiCamera` CommonLib reads | Unavailable until an adapter descriptor validates |
| Depth SRV | The recovered path came from the replacement proxy | Unavailable; no RAW proxy path is admitted |
| Weather / time of day | Typed `Sky` and `TESWeather` CommonLib reads | Unavailable until an adapter descriptor validates |
| Render phases | The recovered nine-phase detector came from the replacement proxy | Unavailable; ENB-compatible observation still needs a contract |
| Sky shader observation | No complete `BSSkyShader` descriptor was recovered | Unavailable |
| Lighting shader observation | Recovered candidates used `BSLightingShader` slots 4 and 6 | Unavailable until runtime RTTI, slot, target, and protection all validate |

The supported-runtime and evidence record is
`config/skyrim-engine-bridge-evidence.json`. It records recovered observations
as evidence, not as trusted offsets. It also records the two exact Address
Library artifact fingerprints admitted for runtime 1.6.1170; the binaries
remain external and are not distributed by this repository.

Recovered CommonLibSSE-NG 3.7.0 metadata and vcpkg baselines remain historical
provenance for understanding the old typed access path. They are not part of
the release adapter. Each live symbol still needs its own validated descriptor.

## ENB adapter shapes and SDK boundary

Two distinct historical bridge shapes were recovered:

- the Truth bridge exposed a pull-style provider through the ENB module export
  surface, including `ENBGetParameter`; and
- the Playground adapter was a push-style host-callback experiment that
  resolved `ENBSetCallbackFunction` and `ENBSetParameter` during `DllMain`.

The neutral engine contracts may feed either shape, but the adapters have
different ownership and lifecycle rules and must not be conflated. The
recovered Truth binary reports ENB SDK1000, while the protected ENB 0.504 host
reports SDK1002. Those versions share the official 1000 major family, and the
SDK1002 example plugin itself reports version 1000, so the skew is not by
itself evidence of ABI incompatibility. This core nevertheless uses an
explicit SDK1002 floor because its present contract is validated against the
1002 layout and semantics; SDK1000 fails this local exact-contract policy, not
the major-family test. The old binary's behavior remains evidence rather than
reusable admitted code. The Playground binary has no export table and is
recorded as incomplete evidence. Neither binary is distributed here.

## Typed observation and reversible properties

The first property schema recognizes exactly seven names:

- `camera.world_fov` and `camera.first_person_fov` are observable and eligible
  for gated reversible object mutation;
- `camera.inverse_view_projection`, current/outgoing weather form IDs, weather
  transition, and game hour are observer-only.

Names are exact. Unknown or differently cased names reject; they never default
to a valid property. All observations require the exact admitted runtime, their
capability, the declared type, and finite/ranged values.

`PatchJournal` is off unless the caller supplies all of these at once:

- an explicit feature gate;
- lifecycle permission from the surrounding `LifecycleGate` integration;
- the complete runtime evaluation for the same loaded image;
- a capability result with no validation diagnostic;
- a nonempty exact baseline and same-sized replacement;
- a readable, writable, non-executable target inside the admitted image.

Apply is a transaction. Every entry is preflighted before the first write.
Each write is read back. A later failure restores earlier entries in reverse
order; failed compensation moves the journal to `RecoveryRequired`. Rollback
first verifies that all replacement bytes are still present, then restores in
reverse order. Repeated apply and rollback are idempotent. Rollback does not
depend on the feature gate because baseline restoration must remain possible
after a feature is disabled.

The static `PatchJournal` memory adapter's `Write` contract is all-or-nothing:
`false` means no byte was changed. A Windows implementation must provide that
guarantee around page protection changes and instruction-cache handling before
executable patches can ever be considered. This first slice rejects executable
regions for property patches entirely.

`PatchJournal` remains restricted to static data inside the admitted
`SkyrimSE.exe` image. Heap-backed fields such as `PlayerCamera::worldFOV` use a
separate `ObjectPropertyJournal`; module containment is not weakened. An object
transaction additionally requires a current owner address and generation
token, the adapter's declared property-to-field binding, a readable/writable
non-executable region, an allowed main-thread phase, and a matching capability
in `HookReady` state; `ObserveReady` is insufficient for mutation. It captures
and verifies the exact baseline before writing and must roll back while the
owner token is still current. If the owner invalidates first, the journal enters
`RecoveryRequired` and performs no unsafe write.
Every write attempt is read back. A writer that returns false after a partial
write is treated as dirty until the exact baseline is explicitly restored and
verified.

## Explicit exclusions

- The `MaterialTracker` SE/AE IDs and its blind 14-byte prologue steal are not
  admitted.
- The `EngineFixes` spinlock threshold patch is unrelated and is not migrated.
- The recovered write-back parser's unknown-name fallbacks to `CameraFOV` and
  `Fixed` are not migrated. Future adapter schemas must reject unknown target
  and source names instead of selecting a valid enum implicitly.
- Recovered vtable indices are candidate evidence, not trusted live contracts.
- No naked address or `ID + fixed offset` becomes a supported descriptor.
- No protected binary, decompilation output, or recovered implementation is
  copied into this repository.
- HakaSapl Horizon Fix 0.2.3 is peer behavior evidence only. A future original
  horizon capability must preserve ENB LOD/water semantics without copying GPL
  code or adopting its hundreds-of-draw-calls donor-tile architecture.

## Remaining live work

The live ENB adapter still needs an immutable file loader and production
SHA-256 verifier feeding `AdmitRelocationArtifact`, a Windows
memory-query/read/write adapter,
complete runtime symbol records, MSVC RTTI verification, concrete owner
generation tracking, ENB lifecycle wiring, and capture/rollback tests inside
Skyrim. Optional SKSE coexistence detection must remain passive.
Depth and mid-frame phase observation need an ENB-compatible route that does
not take over `d3d11.dll`. Until those pieces validate, the corresponding
capabilities remain unavailable and mutation cannot start.
