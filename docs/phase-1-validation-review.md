# BX-ARCH-001 Phase 1 Validation and Merge-Readiness Review

## Review Scope

Reviewed local branch `feat/phase-1-persistence-hardening` at commit
`9e0e69e` against `main` (`ea76769`). This was a read-only source review plus
clean Debug and sanitizer test validation. No source files, tests, CMake files,
README content, or architecture documents were modified during this review.

Permitted review artifact created: `docs/phase-1-validation-review.md`.

## Architecture Context Read

Read and cross-checked:

- `docs/architecture-review.md`
- `docs/architecture-review-followup.md`
- `docs/phase-1-persistence-hardening.md`

Architectural intent confirmed: Phase 1 is a safety/stabilization phase for the
existing binary persistence path, not an application-boundary or module-system
phase.

## Branch and Repository State

Commands reviewed:

```sh
git branch --show-current
git status
git log --oneline --decorate -10
git log --oneline --decorate main..feat/phase-1-persistence-hardening
git diff main...feat/phase-1-persistence-hardening --stat
```

Results:

| Item | Result |
|---|---|
| Current branch | `feat/phase-1-persistence-hardening` |
| Working tree before report | Clean |
| Phase 1 commits | `9e0e69e Harden save/load against malformed input` |
| Base | `ea76769 Merge pull request #22 from trinidadgithub/feat/architecture-review-followup` |
| Files changed by Phase 1 | `.gitignore`, `CMakeLists.txt`, `README.md`, `docs/phase-1-persistence-hardening.md`, `engine/src/game.cpp`, `tests/test_core.cpp` |

Diff stat reviewed:

```text
6 files changed, 937 insertions(+), 242 deletions(-)
```

## Scope Review

Phase 1 remains within intended scope:

- malformed/corrupt save validation,
- string length validation,
- checked binary reads/writes,
- transactional load semantics,
- enum/range validation,
- filename sanitization,
- filesystem enumeration hardening,
- save compatibility tests,
- sanitizer build option and focused documentation.

No Phase 2 or unrelated architecture was introduced. Specifically absent:

- `GameSession`, `GameCommand`, `GameStateSnapshot`, structured `GameEvent`,
- `ContentId`, `RulesCatalog`, module loading, Testing Tomb,
- scripting, SQLite, ECS, plugins, dependency injection, event bus,
- renderer, sound, combat, movement, spell, dungeon, town, or character
  generation refactors.

## Clean Debug Build and Test Results

Executed from a clean build directory:

```sh
rm -rf build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
./build/tests/bx_crawl_tests
```

Results:

| Step | Result |
|---|---|
| Configure | PASS |
| Compile | PASS |
| CTest | PASS, 1/1 test executable passed |
| Doctest cases | PASS, 122/122 |
| Doctest assertions | PASS, 3505/3505 |

Runtime emitted four pre-existing-style `Warning: failed to load descriptions.txt`
messages during direct doctest execution; these did not fail tests and are not
Phase 1 persistence regressions.

## Sanitizer Results

Executed:

```sh
rm -rf build-asan
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DBX_ENABLE_SANITIZERS=ON
cmake --build build-asan -j"$(nproc)"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=print_stacktrace=1 ctest --test-dir build-asan --output-on-failure
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=print_stacktrace=1 ./build-asan/tests/bx_crawl_tests
```

Results:

| Tool | Result | Evidence |
|---|---|---|
| ASan | PASS | Sanitized CTest and direct doctest run passed; no AddressSanitizer reports. |
| UBSan | PASS | Sanitized CTest and direct doctest run passed; no `runtime error` reports. |
| Leak check | PASS | `ASAN_OPTIONS=detect_leaks=1`; no leak reports. |

Sanitized direct doctest result: 122/122 test cases, 3505/3505 assertions.

## Compiler Warning Review

The existing project warning baseline applies `-Wall -Wextra -Wpedantic` to
`bx_engine`. A rebuild of the clean Debug tree was captured and searched for
compiler warnings/errors:

```sh
cmake --build build -j"$(nproc)" 2>&1 | tee /tmp/bx_phase1_warning_check.log >/dev/null
grep -nEi "warning:|error:" /tmp/bx_phase1_warning_check.log
```

Result: no compiler warnings or errors found in the captured build output.

## Persistence Security Review

### String Deserialization

`read_str` at `engine/src/game.cpp:3930` now:

- reads the `int32_t` length through checked `read_exact`,
- rejects negative lengths before conversion,
- rejects lengths above `kMaxSerializedStringLength = 1024` before allocation,
- allocates only after validation,
- verifies that the string payload is fully present.

All save-file string reads (`h.name`, `h.weapon_id`, `h.armor_id`, spell names)
use this helper. No bypass found.

### Binary Reads

All persistence reads are routed through `read_exact` / `read_int` / `read_str`
inside `read_game_from`. Short reads return `false` immediately and cannot keep
interpreting stale values. No unchecked `istream::read` remains in the
persistence loader.

### Binary Writes

Each primitive/string write in `save_game` now checks the result of
`write_exact` / `write_int` / `write_str`. This is a major improvement over the
previous unconditional success path. One remaining concern is documented as
Finding F-001: the final buffered flush/close status is not explicitly checked
before returning success.

### Transactional Load

`load_game` at `engine/src/game.cpp:4388` opens the file, copies the current
`Game` into `tmp`, deserializes into `tmp`, and assigns `g = std::move(tmp)`
only if `read_game_from` succeeds. This preserves current runtime-only state on
successful load and discards all partial mutations on failure. No indirect live
mutation path found: parsing operates on the scratch `Game`, uses no global
state mutation, and does not call gameplay helpers.

### Enums

Serialized enums found:

- `GameState` (`read_game_from`, state field)
- `CharacterClass` (`read_game_from`, hero class field)

Both decode through explicit range validators before `static_cast`. No unsafe
enum casts from untrusted load input remain.

### Numeric Ranges

Validated high-impact numeric values:

- `hero_level` in `[1,14]` prevents overflow around `hero_level + 1` and
  matches the existing progression cap.
- `dungeon_level` in `[1,100]` prevents overflow/pathological monster-count
  behavior while preserving practical content range.
- `nspells` in `[0,100]` prevents unbounded loops/vector growth.

Fixed-size arrays (`spell_slots`, `max_spell_slots`, `gear_counts`,
`weapon_counts`, `armor_counts`) are read by bounded range-for or
`0..Entity::MaxSpellLevel` loops; no serialized count controls those loops.

Other integers (HP, AC, ability scores, supplies, skills, inventory counts,
spell slot values) are not used as allocation sizes, container indexes, or file
paths during load. Additional semantic validation could be added later, but no
memory-safety path was found.

### Filename Sanitization

`sanitize_hero_filename` maps a hero name to a single safe path component.
Unsafe runs collapse to `_`; trailing `_`/space are trimmed; empty sanitized
names fall back to `hero`. `save_path_for_hero` always appends `.dat` under
`saves_directory()`.

Conceptual attempts reviewed:

| Input | Assessment |
|---|---|
| `../../evil`, `../evil`, `dir/..` | Cannot escape; `/` and `.` become sanitized/trimmed components. |
| `foo/bar`, `foo\bar`, `/`, `\` | Separators replaced; parent remains `saves_directory()`. |
| `.`, `..`, empty, only unsafe characters | Falls back to `hero.dat` or a non-empty safe component. |
| control characters | Replaced/collapsed to `_`; no control chars in output. |
| ordinary names (`Trinidad`, `Sir Galahad`, `Conan the Cimmerian`) | Preserved. |
| `D'Artagnan` | Safe but apostrophe becomes `_`; acceptable security tradeoff but minor usability loss. |

No path traversal bypass found. Very long names are discussed in Finding F-002.

### Filesystem Handling

Directory creation uses `create_directories(parent, ec)` and returns false on
error. Enumeration uses `directory_iterator(dir, ec)` and `it.increment(ec)`, so
missing save directories, empty save directories, and permission/iteration
failures do not throw. `find_save` uses `exists(path, ec)`. No graceful-handling
gap found in enumeration/find paths.

## Save Format Compatibility

```text
Save format changed: NO
Save version changed: NO
Current save version: 5
Backward compatibility: PASS/PARTIAL
```

The writer preserves magic `BXSV`, version 5, and field order. Version 1-4 load
paths are preserved by code inspection and covered by compatibility-style tests.
No pre-existing binary fixture was available, so empirical backward
compatibility with an actual pre-Phase-1 save fixture was not performed.

Compatibility is marked PASS/PARTIAL because of the new 1024-byte loader string
cap and stricter filename sanitization. These are reasonable safety changes,
but a theoretical pre-Phase-1 save with a hero/item/spell string over 1024 bytes
would now be rejected, and direct quick lookup by an unsafe-but-previously-valid
filename (for example an apostrophe-containing name) may now require menu
selection or sanitized naming.

## Regression and Round-Trip Behavior

Existing save/load round-trip tests still pass and cover meaningful state:

- owned weapons/armor,
- selected weapon/armor IDs,
- shield ownership,
- `total_gold`,
- per-character save path enumeration and load,
- pregenerated save fallback.

The transactional implementation preserves runtime-only fields by parsing into a
copy of the existing `Game`, so RNG/map/descriptions/monsters are not reset on a
successful load beyond the same values the caller already had.

## Malformed Save Test Audit

Meaningful coverage exists for:

- bad magic/header,
- unsupported versions,
- truncated payload at multiple offsets,
- negative string lengths,
- excessive string lengths,
- invalid `GameState` and `CharacterClass`,
- invalid `hero_level`, `dungeon_level`, and spell counts,
- failed-load state preservation,
- path traversal / unsafe hero names,
- save failure when the parent path is a regular file.

Gaps are documented as Low findings:

- no explicit empty-file test (same failure path as truncated magic/header),
- state-preservation helper does not compare map/monster/RNG fields.

The tests are somewhat byte-layout-specific, but that is appropriate here: they
act as a compatibility spec for the current binary format.

## Documentation Verification

`docs/phase-1-persistence-hardening.md` is broadly accurate. Material claims
about format version, validation, transactional behavior, sanitizer commands,
and test counts were verified against source and observed test output.

Minor overstatements/clarifications are covered by findings:

- write failure detection does not explicitly check final flush/close status
  (F-001),
- public diagnostics remain bool-only and generic in the text frontend (F-003),
- loader/writer string-size behavior is stricter on read than write (F-002).

## Findings

### F-001

Severity: MEDIUM

Area: Binary write failure detection

File: `engine/src/game.cpp`

Function/Location: `save_game`, final return at lines 4289-4385

Finding: `save_game` checks each `write` call, but it returns `out.good()`
before explicitly flushing or closing the stream. Buffered I/O errors detected
only during final flush/close (for example some full-disk or network filesystem
failures) may not be observable by the caller.

Evidence: All writes are checked through `write_exact`/`write_int`/`write_str`,
but the final statement is `return out.good();`; the destructor closes the file

Impact: A rare write/flush failure could still be reported as success. This is
not fsync/durability semantics; it is ordinary stream-flush correctness.

Recommended remediation: Explicitly `out.flush()` or `out.close()` before
returning and return false if the stream reports failure afterward. Add a
targeted test if practical.

### F-002

Severity: MEDIUM

Area: Writer/loader string-limit consistency

File: `engine/src/game.cpp`, `engine/src/chargen.cpp`

Function/Location: `write_str` lines 3920-3925, `read_str` lines 3930-3940,
`create_hero` name input lines 154-161

Finding: The loader rejects serialized strings longer than 1024 bytes, but the
writer can still serialize strings larger than 1024 bytes. Character creation
accepts an unbounded name via `std::getline`. Therefore a user/caller can
produce a save that `save_game` reports as successful but `load_game` later
rejects.

Evidence: `read_str` enforces `kMaxSerializedStringLength = 1024`; `write_str`
only rejects strings larger than `INT32_MAX`. No character-name length cap is
present in `create_hero`.

Impact: Edge-case self-incompatibility for extreme hero names or mutated
strings. Ordinary current data is unaffected, but this weakens the claim that
the writer and reader are fully consistent.

Recommended remediation: Make `write_str` enforce the same cap as `read_str`
and return false, or cap/validate hero names and other save strings before
save. If rejecting long names is intended, document the limit.

### F-003

Severity: MEDIUM

Area: Error reporting / diagnostics

File: `engine/src/game.cpp`, `frontends/text/src/main.cpp`,
`docs/phase-1-persistence-hardening.md`

Function/Location: `load_game` bool API; frontend generic failure text

Finding: Load failures are internally controlled, but caller-visible diagnostics
remain `bool` only. The text frontend still cannot distinguish file-not-found,
bad magic, unsupported version, corrupt/truncated file, invalid enum/range, or
filesystem error except through generic failure handling.

Evidence: `load_game` returns `false` for every failure class. The Phase 1 doc
acknowledges generic frontend failure text as a limitation, but the original
Phase 1 goals included improving diagnostics enough to distinguish common
failure classes.

Impact: Safety is not affected, but developer/user diagnosis remains weak.

Recommended remediation: Consider a follow-up lightweight diagnostic surface,
for example `load_game_with_error(...)` or an internal/public enum result while
preserving the existing bool wrapper.

### F-004

Severity: LOW

Area: Malformed-save test coverage

File: `tests/test_core.cpp`

Function/Location: `Malformed saves: truncated files are rejected without side effects`

Finding: There is no explicit empty-file test. The truncated-file test starts at
4 bytes, so the exact zero-byte path is untested.

Evidence: Truncation cuts are `{4, 8, 12, 32, ...}`.

Impact: Low. Empty files exercise the same initial `read_exact` failure path as
short-header files, and code inspection confirms they fail cleanly.

Recommended remediation: Add a zero-byte payload case to the truncation test.

### F-005

Severity: LOW

Area: State-preservation test depth

File: `tests/test_core.cpp`

Function/Location: `assert_load_rejected_cleanly`

Finding: The state-preservation helper compares a meaningful subset of live
state but does not compare map, monsters, rooms, event log, or RNG state.

Evidence: The helper compares hero name/hp/class/equipment/spells, game state,
gold, XP, hero level, and dungeon level.

Impact: Low. The implementation is clearly transactional by inspection, but a
broader representative comparison would make the regression test stronger.

Recommended remediation: Add a few representative runtime-only fields (for
example map size/type, monster count/name, event log size, or deterministic RNG
next-roll state) to the preservation assertion.

## Acceptance-Criteria Matrix

| Acceptance Criterion | Result | Evidence | Notes |
|---|---|---|---|
| Existing regression tests pass | PASS | Clean Debug: 122/122 tests, 3505/3505 assertions | Includes pre-existing and new tests. |
| Malformed saves fail safely | PASS | Malformed tests plus code audit of `read_game_from` | Empty file lacks explicit test but same path is covered by short header. |
| Negative string lengths rejected | PASS | `read_str` rejects `len < 0`; tests use `-1`, `-100` | Before allocation. |
| Excessive lengths rejected pre-allocation | PASS | `read_str` checks `n > 1024` before `assign`; tests use `0x10000000`, `0x7FFFFFFF` | Writer consistency concern in F-002. |
| Truncated saves fail cleanly | PASS | Multiple truncation offsets; checked reads throughout | No explicit zero-byte file test (F-004). |
| Invalid enums rejected | PASS | `decode_game_state`, `decode_character_class`; tests patch invalid values | Valid enum range matches declarations. |
| Dangerous numeric values validated | PASS | `hero_level`, `dungeon_level`, `nspells` validation and tests | Other integers are not allocation/index drivers during load. |
| Failed loads preserve live Game | PASS | Transactional `Game tmp = g`; tests assert representative state unchanged | Broader test could include map/RNG (F-005). |
| Path traversal prevented | PASS | Sanitized one-component filenames; path tests for traversal/separators | Long-name limit not addressed (F-002 related). |
| Filesystem failures handled | PASS | `error_code` enumeration and `exists`; create dir checked | Permission iteration returns empty rather than throwing. |
| Failed writes detected | PARTIAL | Per-write checks are present; directory failure test passes | Flush/close status not explicit (F-001). |
| Save compatibility preserved | PASS/PARTIAL | Format/version unchanged; versions 1-4 tested | New string cap can reject extreme old strings; no old binary fixture. |
| ASan clean | PASS | Sanitizer run passed 122/122 tests | No ASan reports. |
| UBSan clean | PASS | Sanitizer run passed 122/122 tests | No UBSan runtime errors. |
| No unrelated architecture introduced | PASS | Diff scoped to persistence/tests/docs/build option | No Phase 2 constructs. |
| Documentation accurate | PASS/PARTIAL | Material claims verified | Minor caveats F-001/F-002/F-003. |

## Security Classification Summary

| Severity | Count | Findings |
|---|---:|---|
| BLOCKER | 0 | None |
| HIGH | 0 | None |
| MEDIUM | 3 | F-001, F-002, F-003 |
| LOW | 2 | F-004, F-005 |

## Final Merge-Readiness Decision

# READY WITH MINOR CHANGES

No Blocker or High findings were found. It is reasonable to push this branch
and open a PR now. The Medium findings should be addressed or explicitly
accepted before merge, especially the final flush/close check and writer/loader
string-limit consistency.

## Phase 2 Gate

```text
BX-ARCH-002 GameSession readiness: READY
```

Phase 1 has established a substantially safer persistence baseline and did not
introduce architectural scope creep. The remaining findings are localized to
persistence edge cases/diagnostics and do not block beginning PR review or
planning the GameSession boundary after Phase 1 is accepted.
