# Phase 28 - Testing Tomb: Tavern Rumor Tables and Delivery State

Rumors are the first module content the hero hears rather than finds. The
module declares a rumor table, the tavern serves each rumor exactly once, and
the heard/not-heard boundary follows the same "content is data, runtime state
is session" split every earlier Testing Tomb slice used.

## Module Data

`rumors.json` declares the rumor table as an ordered list of content-id pairs:

```json
[
  {
    "rumor_id": "module.testing_tomb.rumor.hidden_passage",
    "description_id": "module.testing_tomb.text.rumor_hidden_passage"
  },
  {
    "rumor_id": "module.testing_tomb.rumor.buried_gold",
    "description_id": "module.testing_tomb.text.rumor_buried_gold"
  }
]
```

- each `rumor_id` must use the module rumor namespace
  (`module.<module_id>.rumor.`), be unique, and have a `description_id` in the
  module text namespace that actually resolves to a declared description block
  (mirroring the location-to-description cross-check);
- the module declaration order is the tavern's serving order;
- `module.json:files` gains `"rumors": "rumors.json"`, and the two companion
  text blocks were added to `text/descriptions.txt`.

## Loading

`load_rumors` (`engine/src/module_content.cpp`) follows the encounters/
treasures/traps loader shape: optional file gate, per-object field extraction,
namespace validation, duplicate rejection, and a cross-reference into the
already-loaded `module.descriptions` map. It runs last in the
`load_module_content` chain because it only consumes prior stages. Errors stay
in the established `<noun> <problem>` style (`invalid rumor_id`,
`duplicate rumor_id`, `invalid rumor description_id`,
`rumor references missing description`, `could not open rumors`).

## Delivery and State

`PlayerCommandType::Listen` replaces the old no-op `Help` stub that tavern key
2 produced. In `engine/src/town.cpp` the Tavern case now serves it:

- `GameSession::start_module` binds the module's rumor table to `Game` as
  `module_rumors` (rumor id + resolved description text). The text resolves at
  module start through `find_description`, so the tavern never re-parses
  content; the loader already guarantees every reference exists.
- `Game` carries `heard_rumors` (the rumor ids served this session).
- `Listen` finds the first un-heard rumor in declaration order, records its
  id, and logs its text; once all rumors have been served the barkeep has
  nothing new to share.
- The text frontend tavern key `2` now maps to `Listen`.

Rumor delivery state is deliberately session-only: persisting it is the next
campaign save/reload slice.

## Tests

- `Testing Tomb module loads its scripted rumors` — the real module exposes
  the two rumors with correct ids/description ids, `find_rumor` resolves, and
  the description text loads.
- `Module content loader rejects invalid rumors` — scratch module with all
  six rejection paths (all valid load, missing rumor_id, missing
  description_id, invalid rumor_id namespace, duplicate rumor_id, description
  outside the text namespace, undeclared description, and missing declared
  file).
- `GameSession binds the module rumor table to the tavern` — start_module
  resolves the two rumor texts onto `Game` and `heard_rumors` starts empty.
- `GameSession tavern serves each module rumor exactly once` — via the real
  Town -> Tavern navigation, `Listen` delivers rumor one, then rumor two, then
  reports no new rumors while `heard_rumors` stays at two.
- `GameSession tavern serves no rumors before the module is loaded` — a fresh
  town session without module context gets the "no rumors" line.

## Validation

- Debug build: warning-free.
- Doctest: 193/193 cases, 14374/14374 assertions (Debug + sanitizer).
- ctest: PASS (Debug + sanitizer).
- Frontend smoke: boots to town from the pregenerated `Hrothgar` save with
  `printf 'x' | timeout 30 env BX_LOAD_SAVE=Hrothgar
  ./build/frontends/text/bx_crawl_text` and quits cleanly.

READY for the next Testing Tomb slice: module flags and campaign save/reload —
the rumor slice proves the mutable session state (heard ids, current module,
open door/location state) that the campaign save model needs to capture.