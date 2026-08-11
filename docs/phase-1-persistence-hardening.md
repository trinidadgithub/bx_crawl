# Phase 1 — Persistence Safety & Baseline Stabilization

## Summary

Hardened the binary save/load path in `engine/src/game.cpp` against malformed
input while preserving the existing on-disk save format (version 5) and all
gameplay behavior. Save files are now treated as untrusted input.

Previously the loader:

- allocated strings from a raw, unvalidated `int32_t` length (negative length
  threw an uncaught `std::length_error`; huge lengths triggered a pathological
  allocation before any bound check),
- progressively mutated the caller's live `Game` while parsing, so a corrupt
  file left a half-loaded game behind,
- cast serialized integers directly into `GameState` / `CharacterClass`
  values without validation,
- reported save success without checking that every write actually succeeded,
  and
- built save paths from raw hero names, letting names like `../../evil` escape
  the saves directory.

All of those vectors are now closed. The public persistence API
(`save_game`, `load_game`, `saves_directory`, `save_path_for_hero`,
`enumerate_saves`, `enumerate_all_saves`, `find_save`) is unchanged.

## Files Modified

| File | Purpose |
|---|---|
| `engine/src/game.cpp` | Replaced raw `read_str`/`write_str`/`read_int`/`write_int` helpers with byte-exact checked helpers; added `read_game_from` (single deserialization routine); rewrote `save_game`/`load_game` to be checked and transactional; added `sanitize_hero_filename`; made directory enumeration and `find_save` error-code safe. |
| `tests/test_core.cpp` | Added a persistence-hardening section that handcrafts version-5 payloads (a compatibility spec) and exercises every rejection path. |
| `CMakeLists.txt` | Added optional `BX_ENABLE_SANITIZERS` (Address + UndefinedBehavior, GCC/Clang) for development/testing builds. |
| `README.md` | Corrected the documented save location to `~/.bx_crawl/saves/<hero>.dat` and noted the robustness/sanitization behavior. |

## Tests Added

Baseline before these changes: **113 test cases / 2901 assertions**. After:
**123 test cases / 4259 assertions**, all passing in the normal build and under
ASan+UBSan.

New categories:

- **Format contract / compatibility spec** — a version-5 payload is handcrafted
  byte-for-byte and must load the expected hero; old versions 1–4 must still
  load with version-gated fields defaulted.
- **Bad magic / unsupported version** — wrong magic bytes and versions outside
  1..5 are rejected.
- **Truncated files** — cuts at length, mid-string, and mid-payload boundaries
  all fail cleanly.
- **Invalid string lengths** — negative (`-1`, `-100`) and excessive
  (`0x10000000`, `0x7FFFFFFF`) lengths are rejected before allocation.
- **Invalid enums** — out-of-range `GameState` and `CharacterClass` values are
  rejected.
- **Impossible counts/levels** — `hero_level` outside 1..14, `dungeon_level`
  outside 1..100, and `nspells` outside 0..100 are rejected (prevents
  `hero_level + 1` overflow and unbounded loop/allocation).
- **State preservation** — every malformed payload is loaded into a known live
  `Game`; the test asserts the load fails and the live game is unchanged.
- **Write failure** — `save_game` returns false (not success) when it cannot
  create the target directories.
- **Filename safety** — hero names containing `/`, `\`, `..`, control
  characters, or empty, never escape the saves directory; sanitized names
  round-trip through save/load/enumerate and `find_save`.

## Validation Remediation

### F-001

`save_game` now explicitly flushes and closes the output stream before reporting
success, so ordinary buffered stream failures observed during final flush/close
are not silently reported as successful saves. A direct final-flush failure test
was not added because a portable deterministic trigger would require filesystem
fault injection, disk exhaustion, or platform-specific devices such as
`/dev/full`; the existing write-failure test remains focused on deterministic
directory failure, and the flush/close behavior is verified by implementation
inspection.

### F-002

`write_str` now enforces the same `kMaxSerializedStringLength = 1024` limit as
`read_str`. The invariant is now that any string `save_game` successfully writes
is within the size range accepted by `load_game`. Tests cover a maximum-size
hero name that saves and reloads, and a maximum-plus-one name that causes
`save_game` to return false.

### F-003

F-003 remains intentionally deferred. Richer caller-visible load diagnostics
will be reconsidered when the application/GameSession boundary is designed, to
avoid designing the public persistence error API twice.

### F-004

The truncated-save regression now includes an explicit zero-byte payload. The
test verifies clean load failure and unchanged live `Game` state.

### F-005

The failed-load state-preservation test now includes representative runtime-only
state: map dimensions/tile data/items, monster count and fields, room data,
event log contents, and deterministic RNG state via copied `std::mt19937`
comparison.

## Save Format Compatibility

The on-disk format is **unchanged**: magic `BXSV`, version 5, same field order
and byte sizes. No version bump was needed — all hardening is on the read/write
side. Version-gated loading for saves 1–5 is preserved and now covered by
compatibility tests. Trailing bytes after a valid payload remain tolerated, so
future fields appended at the end of the format can be ignored by this loader.

## Validation Added

- **Strings** (`read_str`): negative lengths rejected; lengths over a named cap
  (`kMaxSerializedStringLength = 1024` — far above any real name/id/spell-name)
  rejected before allocation; the payload must be fully present; every caller
  checks the result.
- **Enums**: `GameState` and `CharacterClass` are decoded through range checkers
  (`decode_game_state`, `decode_character_class`) instead of blind casts.
- **Numeric ranges**: `hero_level` in `[1,14]` (matches the code's hard level
  cap, prevents `hero_level + 1` overflow in XP math), `dungeon_level` in
  `[1,100]` (prevents `dungeon_level + 1` overflow and pathological monster
  spawn counts), spell count in `[0,100]` (prevents unbounded loop/allocation).
  These are the only values whose corruption could cause overflow, unbounded
  loops, or large allocations; the fixed-size array reads (`spell_slots`,
  `gear_counts`, `weapon_counts`, `armor_counts`) are bounds-safe by
  construction.
- **Stream state**: every logical read and write goes through byte-exact
  helpers that verify success; a short read or failed write aborts the
  operation with a controlled `false` return instead of being ignored.
- **Filesystem paths**: `sanitize_hero_filename` restricts names to a safe
  ASCII set with unsafe runs collapsed to `_`, so a hero name always produces a
  single path component inside the saves directory; directory enumeration and
  `exists` checks use `std::error_code` overloads so missing/permission-errored
  directories are handled gracefully instead of throwing.

## Failure Behavior

On malformed, truncated, or invalid saves `load_game` returns `false` and the
caller's live `Game` is left completely untouched. The loader deserializes into
a temporary copy of the current runtime state (`load_game` → `read_game_from`
on a scratch `Game`) and only commits via move-assignment after the whole file
has parsed and validated. `save_game` reports `false` if directory creation or
any write fails.

## Sanitizer Results

Ran the full suite under AddressSanitizer + UndefinedBehaviorSanitizer
(`-DBX_ENABLE_SANITIZERS=ON`, Debug build in `build-asan/`):

```
123 test cases | 123 passed | 0 failed
4259 assertions | 4259 passed | 0 failed
```

No ASan findings, no UBSan `runtime error` reports, no leaks
(`ASAN_OPTIONS=detect_leaks=1`). Re-run with:

```sh
cmake -S . -B build-asan -DBX_ENABLE_SANITIZERS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-asan -j
ASAN_OPTIONS=detect_leaks=1 ctest --test-dir build-asan --output-on-failure
```

## Known Limitations

Intentionally deferred (documented here rather than solved in this phase):

- Save format still stores only hero/meta state, not full dungeon/monster/
  world state — out of scope for Phase 1 (architectural, not a robustness gap).
- The loader tolerates trailing bytes after a complete payload; a future,
  stricter loader will reject unexpected trailing data if a `payload_length`
  field is added.
- The text frontend (`frontends/text/src/main.cpp`) still reports a failure as
  the generic string "Failed to load save." A future `SaveService`/`GameSession`
  boundary is the right place for structured diagnostics. Internally, failure is
  now distinguished at each validation point (magic, version, enum, range,
  stream) even though the public API remains `bool`.
- Environment-driven autoload (`BX_LOAD_SAVE`) and the legacy `~/.bx_crawl/
  save.dat` fallback are preserved as-is.
- Save files have no integrity checksum; corruption that happens to preserve
  every validated field still loads. This is acceptable for a prototype and can
  be revisited when the campaign save format is redesigned.

## Phase 2 Readiness

Yes. Persistence is now safe, predictable, and testable, and the existing test
suite documents the current format as a compatibility contract. The codebase is
ready to begin `BX-ARCH-002 — GameSession Application Boundary` as the next
incremental step. Phase 2 work (`GameSession`, `GameCommand`,
`GameStateSnapshot`, modules, content IDs, structured events, renderer/sound
extraction) was **not** introduced in this phase.
