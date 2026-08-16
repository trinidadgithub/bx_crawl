# Phase 7 — Module Manifest Loader

## Summary

Introduced a minimal data-only module manifest loader. This phase validates a
`module.json` file and returns a `ModuleManifest` DTO, but it does **not** load
module content, instantiate maps, run Testing Tomb, or change gameplay.

The goal is to establish the manifest boundary needed by the upcoming Testing
Tomb phase while keeping module runtime work deferred.

## Manifest Boundary

```text
module directory
    |
    v
module.json
    |
    v
load_module_manifest(...)
    |
    v
ModuleManifest
```

## Manifest Fields

The loader currently accepts this minimal shape:

```json
{
  "module_id": "testing_tomb",
  "title": "Testing Tomb",
  "version": "0.1.0",
  "author": "BX Crawl",
  "ruleset": "bx.moldvay_basic",
  "engine_min_version": "0.1.0",
  "content_schema_version": 1,
  "entry_location_id": "module.testing_tomb.location.entrance",
  "files": {
    "locations": "locations.json",
    "map": "maps/tomb.json",
    "encounters": "encounters.json",
    "treasures": "treasures.json",
    "traps": "traps.json",
    "descriptions": "text/descriptions.txt"
  }
}
```

## Validation

- `module_id` must be lowercase ASCII letters, digits, or `_`.
- Required string fields must be present.
- `ruleset` and `entry_location_id` must be valid content IDs.
- `entry_location_id` must be inside the module location namespace:
  `module.<module_id>.location.*`.
- `content_schema_version` must be positive.
- `files` must be present and non-empty.
- File keys must be lowercase ASCII letters, digits, or `_`.
- File paths must be relative and must not contain `.` or `..` components.
- `module.json` is capped at 64 KiB.

The loader reports a small diagnostic string in `ModuleManifestResult::error`.
This is specific to module-manifest validation and does not change the existing
`load_game` bool API or Phase 1 F-003 deferment.

## Compatibility

- No gameplay behavior changed.
- No save format/version changed.
- No module content is loaded yet.
- No Testing Tomb files were added.
- No JSON dependency was introduced; the loader intentionally parses only the
  small manifest subset needed now.

## Tests Added

Five test cases were added:

- `Module manifest loader accepts a minimal valid manifest`
- `Module manifest loader rejects missing or invalid required fields`
- `Module manifest loader validates entry location namespace`
- `Module manifest loader rejects unsafe file references`
- `Module manifest loader handles missing directories and files`

The full suite now contains 147 test cases and 4818 assertions.

## Deferred Work

Still intentionally deferred:

- Loading `locations.json`, maps, encounters, treasures, traps, or descriptions.
- Testing Tomb content files.
- Starting a game from a loaded module.
- Module object IDs for doors, traps, treasure, or monster instances.
- Campaign save integration.
- Declarative triggers.
- Rich module diagnostics beyond the current manifest-level error string.

## Validation

Performed from clean Debug and sanitizer build trees:

- **Debug build**: configure PASS, compile PASS.
- **CTest**: PASS, 1/1 test executable.
- **Doctest**: 147 / 147 test cases, 4818 / 4818 assertions.
- **Sanitizers**: ASan PASS, UBSan PASS, LeakSanitizer PASS.
- **Compiler warnings**: none under `-Wall -Wextra -Wpedantic`.

## Next Readiness

READY for the Testing Tomb proof-of-concept module phase. The manifest loader
now provides the first safe module package boundary.
