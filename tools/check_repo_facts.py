"""Bind every number on the artwork card to the source that declares it.

The art gate settles whether the card fits its columns and matches its spec.
Whether the card is true of this library is a different question, and this file
is where it gets answered: each row is measured again from the header or the
translation unit that declares it, and a row that no longer agrees is a failure
rather than a stale drawing nobody noticed.

Standard library only, so it runs anywhere the repository is checked out and
does not need the MSVC toolchain or the external SDK cache.
"""
import io
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = ROOT / "docs" / "art" / "enb-runtime-core.art.json"

WORDS = {
    1: "one", 2: "two", 3: "three", 4: "four", 5: "five", 6: "six",
    7: "seven", 8: "eight", 9: "nine", 10: "ten", 11: "eleven",
    12: "twelve", 13: "thirteen", 14: "fourteen", 15: "fifteen",
    16: "sixteen",
}

SOURCE_DIRS = ("include", "src")
SOURCE_SUFFIXES = (".hpp", ".cpp")


def _read(relative: str) -> str:
    return io.open(ROOT / relative, encoding="utf-8", newline="").read()


def _word(count: int) -> str:
    """Spelled out, because the card draws words and not digits."""
    if count not in WORDS:
        raise AssertionError(f"no word for {count}; widen WORDS or use digits")
    return WORDS[count]


def _block(text: str, opener: str) -> str:
    """The body between an opening line and the `};` that closes it."""
    start = text.index(opener) + len(opener)
    return text[start:text.index("\n};", start)]


def _enumerators(text: str, name: str) -> int:
    opener = re.search(rf"enum class {name} : [\w:]+ \{{", text)
    if opener is None:
        raise AssertionError(f"enum class {name} is gone from the source")
    body = _block(text, opener.group(0))
    return len(re.findall(r"^\s{4}\w+ = [^,]+,$", body, re.MULTILINE))


def _constant(text: str, name: str) -> int:
    found = re.search(rf"{name} = (\d+);", text)
    if found is None:
        raise AssertionError(f"{name} is gone from the source")
    return int(found.group(1))


def _source_files() -> list[Path]:
    found = []
    for directory in SOURCE_DIRS:
        for path in sorted((ROOT / directory).rglob("*")):
            if path.suffix in SOURCE_SUFFIXES and path.is_file():
                found.append(path)
    return found


def measure() -> dict[str, str]:
    """Every card value, rebuilt from the source rather than from the card."""
    contract = _read("include/enbcore/enb/SdkContract.hpp")
    gate = _read("include/enbcore/runtime/LifecycleGate.hpp")
    bridge = _read("include/enbcore/skyrim/EngineBridge.hpp")
    schema = _read("include/enbcore/skyrim/EngineProperties.hpp")
    rows = _read("src/skyrim/EngineProperties.cpp")
    wrapper = _read("src/enb/WrapperFingerprint.cpp")
    providers = _read("src/skyrim/EngineBridge.cpp")
    descriptors = _read("src/skyrim/CanonicalDescriptors.cpp")

    exports = len(re.findall(r"\{nullptr\};", _block(contract, "struct SdkExports final {")))
    capabilities = _constant(bridge, "CapabilityCount")
    properties = _constant(schema, "EnginePropertyCount")
    writable = rows.count("EnginePropertyAccess::ObserveAndReversibleObjectWrite")
    builds = int(re.search(r"std::array<WrapperBuildRecord, (\d+)> kSupportedBuilds",
                           wrapper).group(1))
    artifacts = _block(providers, "constexpr std::array kSupportedRelocationProviders{").count(
        "RelocationProviderRecord{")
    shipped = len(re.findall(r"^SymbolDescriptor \w+\(\) noexcept$", descriptors, re.MULTILINE))
    files = _source_files()
    lines = sum(len(io.open(p, encoding="utf-8", newline="").read().splitlines())
                for p in files)

    return {
        "sdk exports": f"{_word(exports)} names",
        "lifecycle states": f"{_word(_enumerators(gate, 'State'))} of them",
        "lifecycle events": f"{_word(_enumerators(gate, 'Event'))} of them",
        "validation codes": f"{_word(_enumerators(contract, 'ValidationCode'))} codes",
        "engine properties": f"{_word(properties)} named",
        "writable fields": f"{_word(writable)} of the {_word(properties)}",
        "capabilities": f"{_word(capabilities)} of them",
        "wrapper allowlist": f"{_word(builds)} build",
        "address library": f"{_word(artifacts)} artifacts",
        "source files": f"{len(files)} files",
        "source lines": f"{lines:,} lines",
        "shipped descriptors": f"{_word(shipped)} of {_word(capabilities)}",
    }


def check_card_rows_match_the_source() -> list[str]:
    card = json.load(io.open(SPEC, encoding="utf-8"))["cards"][0]
    drawn = {field["key"]: field["value"] for field in card["fields"]}
    measured = measure()
    bad = []
    for key, value in sorted(measured.items()):
        if key not in drawn:
            bad.append(f"the card no longer has a {key!r} row")
        elif drawn[key] != value:
            bad.append(f"{key}: the card says {drawn[key]!r}, the source says {value!r}")
    for key in sorted(set(drawn) - set(measured)):
        bad.append(f"{key!r} is drawn but nothing measures it")
    return bad


def check_the_enum_counts_are_read_not_guessed() -> list[str]:
    """A regex that matches nothing would report zero and pass every row.

    So the parser is checked against a shape it must reject and one it must
    accept, and the accepted count is compared against a hand read of the
    header. If somebody reformats the enums, this fails before the card does.
    """
    contract = _read("include/enbcore/enb/SdkContract.hpp")
    bad = []
    if _enumerators(contract, "CallbackId") != 8:
        bad.append("CallbackId no longer reads as eight callbacks")
    if _enumerators(contract, "StateId") != 16:
        bad.append("StateId no longer reads as sixteen host state slots")
    try:
        _enumerators(contract, "NoSuchEnumExists")
    except AssertionError:
        pass
    else:
        bad.append("a missing enum was counted instead of refused")
    return bad


def check_the_sdk_window_is_the_one_the_flow_draws() -> list[str]:
    """The admission flow draws 1002 to 1999 and one game identifier."""
    contract = _read("include/enbcore/enb/SdkContract.hpp")
    bad = []
    pins = {"kSdkVersion": 1002, "kSdkFamilyBegin": 1000, "kSdkFamilyEnd": 2000}
    for name, expected in pins.items():
        found = re.search(rf"{name} = (\d+);", contract)
        if found is None or int(found.group(1)) != expected:
            bad.append(f"{name} is no longer {expected}")
    if "kGameIdentifier = 0x10000006;" not in contract:
        bad.append("the admitted game identifier is no longer 0x10000006")
    return bad


def check_the_marked_row_is_still_an_honest_null() -> list[str]:
    """The one toned row says four capabilities have no descriptor.

    If somebody ships the missing descriptors, this fails, and the right repair
    is to redraw the card rather than to loosen the check.
    """
    descriptors = _read("src/skyrim/CanonicalDescriptors.cpp")
    bridge = _read("include/enbcore/skyrim/EngineBridge.hpp")
    covered = {name for name in re.findall(r"Capability::(\w+),", descriptors)}
    declared = set(re.findall(r"^\s{4}(\w+) = \d+,$",
                              _block(bridge, "enum class Capability : std::uint8_t {"),
                              re.MULTILINE))
    bad = []
    if not covered <= declared:
        bad.append(f"descriptors name capabilities that do not exist: {covered - declared}")
    if len(declared) - len(covered) != 4:
        bad.append(f"{len(declared) - len(covered)} capabilities lack a descriptor, "
                   "and the card says four")
    return bad


CHECKS = (
    check_card_rows_match_the_source,
    check_the_enum_counts_are_read_not_guessed,
    check_the_sdk_window_is_the_one_the_flow_draws,
    check_the_marked_row_is_still_an_honest_null,
)


def main() -> int:
    worst = 0
    for check in CHECKS:
        failures = check()
        name = check.__name__.removeprefix("check_")
        print(("ok   " if not failures else "FAIL ") + f"facts.{name}")
        for failure in failures:
            print(f"       {failure}")
        worst = max(worst, 1 if failures else 0)
    return worst


if __name__ == "__main__":
    raise SystemExit(main())
