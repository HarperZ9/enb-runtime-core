# Skyrim Engine Bridge Contract

## Intent

This slice is the neutral safety boundary for an ENB-compatible SkyrimBridge
adapter. ENB remains the rendering host. The core can admit an exact Skyrim
runtime identity, validate adapter-supplied relocation and engine-symbol
contracts, publish fail-closed observation capabilities, and journal narrowly
scoped reversible property changes. It does not install an SKSE plugin, resolve
live relocation IDs, or own a renderer.

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
3. The adapter supplies the exact SKSE runtime-provider DLL identity; relocation
   provider kind, artifact identity, selected runtime, and semantic version;
   and its statically linked engine-adapter build version. The admitted SKSE
   2.2.6 provider is
   `skse64_1_6_1170.dll`; its filename, size, and SHA-256 must all match.
   Loader executables are deliberately excluded because recovered official and
   custom loaders differed while providing the same runtime DLL. The initial
   relocation allowlist contains the two captured Address Library databases.
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

The recovered build metadata pins CommonLibSSE-NG 3.7.0 and records its two
vcpkg baselines. CommonLib is a statically linked implementation detail of the
first adapter, not an end-user runtime dependency. Its build version is
provenance evidence, not blanket symbol admission: each runtime symbol still
needs its own validated descriptor.

The neutral API does not make Address Library permanent. It reserves an
`EmbeddedManifest` relocation-provider kind for a Truth-owned, versioned
relocation/signature manifest shipped with the product. That provider remains
unavailable until an exact manifest artifact and its per-symbol contracts are
added to the allowlist; provider-kind selection can never bypass validation.

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

## Reversible property journal

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

The memory adapter's `Write` contract is all-or-nothing: `false` means no byte
was changed. A Windows implementation must provide that guarantee around page
protection changes and instruction-cache handling before executable patches
can ever be considered. This first slice rejects executable regions for
property patches entirely.

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

## Remaining live work

The first live adapter still needs a deferred SKSE bootstrap, a statically
linked CommonLib implementation, a Windows memory-query/read adapter,
relocation ID resolution, complete runtime symbol records, MSVC RTTI
verification, separate pull-provider and push-callback lifecycle adapters, and
live capture tests inside Skyrim. The dependency-free Truth path additionally
needs its owned embedded relocation/signature manifest and a native bootstrap
adapter so Address Library can disappear from product setup and CommonLib can
later be replaced inside the owned build path.
Depth and mid-frame phase observation need an ENB-compatible route that does
not take over `d3d11.dll`. Until those pieces validate, the corresponding
capabilities remain unavailable and mutation cannot start.
