# Phase 8 — Testing Tomb Entry Module

## Summary

Added the first checked-in data-only adventure module fixture:
`modules/testing-tomb`. This phase proves that immutable module content can be
loaded from disk and that the module entry location can be resolved without any
module-specific C++ gameplay logic.

This remains a narrow architectural slice. It does not start a real module game,
instantiate monsters, run combat, trigger traps, award treasure, or alter saves.

## Added Module Content

```text
modules/
  testing-tomb/
    module.json
    locations.json
    maps/
      tomb.json
    encounters.json
    treasures.json
    traps.json
    text/
      descriptions.txt
```

The placeholder encounter, treasure, and trap files are present as empty arrays
because the Phase 7 manifest already declares them. They are not loaded yet.

## Loader Boundary

Introduced `ModuleContent` as a data-only DTO around the existing manifest:

- `ModuleManifest manifest`
- `std::map<std::string, ModuleLocation> locations`
- `std::map<std::string, ModuleMapDefinition> maps`
- `std::map<std::string, std::string> descriptions`

`load_module_content(...)` currently loads:

- The module manifest via `load_module_manifest(...)`.
- `locations.json`.
- One map definition from `maps/tomb.json`.
- Sectioned room descriptions from `text/descriptions.txt`.

## Validation

The loader validates:

- Required manifest file entries for `locations`, `map`, and `descriptions`.
- Location IDs are valid and inside `module.<module_id>.location.*`.
- Description IDs are valid and inside `module.<module_id>.text.*`.
- Map IDs are valid and inside `module.<module_id>.map.*`.
- The manifest entry location exists in loaded locations.
- Location map references resolve to loaded maps.
- Location description references resolve to loaded descriptions.
- Map dimensions are positive.
- Map entry coordinates are inside bounds.

The parser intentionally supports only the tiny schema used by this module
fixture. No external JSON dependency was introduced.

## Tests Added

Two tests were added:

- `Testing Tomb module loads and exposes its entry location`
- `Module content loader rejects invalid location references`

The positive test is the first architecture proof that checked-in module data can
be loaded and the entry room can be reached as data.

## Compatibility

- No gameplay behavior changed.
- No save format/version changed.
- No `Game` or `GameSession` module runtime was added.
- No module-specific C++ logic was added.
- No encounter, treasure, trap, or door behavior is active yet.

## Deferred Work

Still intentionally deferred:

- Starting a campaign from `ModuleContent`.
- Converting `ModuleMapDefinition` into runtime `Map` tiles.
- Loading encounters, treasures, traps, doors, and module flags.
- Monster instantiation from `bx.monster.goblin`.
- Combat, treasure, and trap integration.
- Save/reload restoration for module state.

## Validation

Performed from clean Debug and sanitizer build trees:

- **Debug build**: configure PASS, compile PASS.
- **CTest**: PASS, 1/1 test executable.
- **Doctest**: 149 / 149 test cases, 4835 / 4835 assertions.
- **Sanitizers**: ASan PASS, UBSan PASS, LeakSanitizer PASS.
- **Compiler warnings**: none under `-Wall -Wextra -Wpedantic`.

## Next Readiness

READY for the next Testing Tomb slice: map tile/topology loading or the first
`GameSession` entry-point seam for starting at a loaded module location.
