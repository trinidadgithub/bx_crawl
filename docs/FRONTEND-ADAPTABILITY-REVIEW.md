# Engine Adaptability for Non-Text Frontends

Review recorded 2026-11-11 UTC after the Poison Phase 1 foundation validation.
Base `29128b1`, with the local save/death changes described in
[the phase report](poison-phase-1-saves-and-death.md). This is an architectural
review, not a GUI implementation or a claim of tested Windows/WebAssembly support.

## Assessment

**The core is reusable, but not yet a complete frontend-independent application
API.** A Linux GUI could already link the engine, submit commands, and render
snapshots. A browser/server adapter is also plausible. Neither should duplicate
combat rules or reproduce terminal menus to discover allowed actions.

Keep ADRs 0001-0004: authoritative `Game`, orchestration in `GameSession`, stable
content IDs, data-only modules, and immutable rules versus mutable campaign state.
No rewrite, ECS, plugin layer, or GUI framework choice is needed to close the
concrete gaps below.

## Existing Strengths

- `bx_engine` is a static library, separate from the text executable
  (`engine/CMakeLists.txt:3-33`). It has no graphical dependency.
- `GameSession::apply`, `ActionResult`, `snapshot()`, and `events()` already form
  a usable application boundary (`include/game_session.hpp:11-77`). Existing
  tests run it without terminal input.
- Character-generation rules are non-interactive (`include/chargen.hpp`);
  prompts live in the text frontend. A GUI need not emulate stdin.
- RulesCatalog/ContentId, module loaders, and DiceRoller support deterministic,
  data-driven gameplay across renderers.
- Snapshot reads do not themselves advance the RNG/clock. Recent Phase 1 work
  exposes death cause through `HeroView`, reducing the need to parse death text.
- Rules queue sound requests; the text frontend consumes them. Actual audio
  playback is not required to resolve combat or determine death.

## Confirmed Gaps

| Priority | Gap and evidence | Impact / smallest next step |
|---|---|---|
| High | **Time depends on frame preparation.** `GameSession::prepare_dungeon_frame` calls `compute_visibility`, which advances the clock; `main.cpp:510-521` calls it before input. | A 60-FPS redraw loop would advance gameplay without commands. Poison Phase 2 should make rendering/visibility time-neutral and have explicit action-owned elapsed time. |
| High | **Save/load and startup policy remain in the text frontend.** `main.cpp:371-496` accesses mutable Game to resolve assets, load saves, decide module/procedural resume, and start heroes. | Another frontend would duplicate campaign rules. Add a small application-level start/load/resume service taking explicit asset and storage inputs and returning structured results. Keep serialization rules in the engine. |
| High | **Action/menu choices are implicit terminal conventions.** `PlayerCommand` has dx/dy and a reused `spell_index` (`game.hpp:200-205`); `town.cpp` interprets it for buy/equip/sell selections. | GUI widgets need stable action/item/spell IDs, enabled choices and targeting metadata, not magic submenu numbers or reconstructed filtered indexes. Evolve the command/choice boundary incrementally with a text adapter. |
| High | **Character/source alignment is unavailable for rules qualification.** `Entity` has no alignment field; chargen has no alignment selection or persistence. | Protection from Evil cannot infer qualifying sources faithfully. Phase 1 can accept explicit qualification, but engine-owned alignment/source context, persistence/default policy and frontend choices must be resolved before automatic qualification is complete (Q08). |
| Medium | **Snapshots are incomplete application views.** `HeroView` exposes spells/count arrays but no spell-slot availability or paralysis counter; `GameStateSnapshot` exposes raw `town_submenu`, not action choices (`game_snapshot.hpp`). | Alternate frontends cannot fully show status or available actions without raw Game access/catalog assumptions. Add typed view fields only as needed; preserve snapshots as values. |
| Medium | **Presentation-specific selection logic has already drifted.** Text casting prompts for directions only for Magic Missile/Charm Person (`main.cpp:140-165`), while Lightning Bolt requires a direction (`spells.cpp:511-515`). | Confirmed UI/engine contract mismatch. Introduce engine-owned spell targeting metadata and use it in all frontends; avoid another per-frontend string list. |
| Medium | **Transitions/errors are mostly prose.** ActionResult provides keep-playing/death booleans; most rule outcomes and invalid commands are LogMessage/Sound events (`game_event.hpp`). | Network/UI clients should not parse English logs. Add small structured action outcome/reason and status-change records when needed; retain log text for human presentation. |
| Medium | **Event delivery uses a growing vector/cursor.** `Game::events`, `GameSession::events`, and `game.cpp` log/sound append; campaign restore clears events. | Long sessions and reconnects need bounded delivery/drain or sequence IDs with reset semantics. Do not claim the current vector is a reliable network replay protocol. |
| Medium | **Environment/filesystem configuration is implicit.** Runtime reads BX_NO_MONSTERS/BX_TEST_MONSTER; save paths use HOME; descriptions use a data directory and startup emits stderr warnings. | Tests, embedded apps and hosted sessions need explicit configuration/storage inputs. Frontend launchers can translate environment flags into that configuration. |
| Medium | **Legacy rendering is still linked into engine source.** `game.cpp` retains ANSI/std::cout render functions, while the active frontend uses `render_snapshot.cpp`. | A headless app can bypass them, so they are not blocking input calls. Remove/deprecate them after verifying no callers; no need to rewrite combat. |
| Medium | **Platform-specific audio is compiled in bx_engine.** `engine/CMakeLists.txt:18-19` includes sound/debug backends; `sound.cpp:11-13,100-142` uses POSIX headers/fork/exec. | Static linking does not by itself make compilation portable. Move the playback backend to the text/platform target or provide an optional backend; retain sound events as the engine contract. |

`std::getline` in module/description loaders reads files, not interactive input.
The remaining engine console output is legacy rendering/startup diagnostics;
this review found no reason to re-extract already separated chargen prompts.

## Recommended Order

1. Review the Phase 1 save/death slice and resolve the alignment/source-context
   gap. Do not weaken the poison rules to avoid collecting necessary information.
2. Make Poison Phase 2's authoritative clock the first cross-frontend hardening
   step. One command history must yield the same time/state with zero, one, or
   many render requests, in a headless loop or a GUI loop.
3. Extract start/load/resume policy behind explicit configuration and storage
   parameters. Test module and procedural campaigns through that boundary.
4. Expose action choices and targeting metadata, then adapt text menus to it.
   Include spell targets, inventory/shop ContentIds, validation reasons and the
   missing character/state fields. No new UI framework is required for this step.
5. Separate platform playback/diagnostics and define event delivery semantics.
   Validate one headless application harness before choosing a deployed UI.
6. Implement one thin additional frontend as a proof: command submission,
   snapshot rendering, event consumption, startup and save/load through the same
   application boundary. Choose desktop GUI versus browser/server based on the
   owner's deployment requirements, not assumptions in the engine.

## Acceptance for a Frontend-Neutral Engine Slice

- No gameplay time or RNG advancement from redraws/snapshot reads.
- Text and headless adapters submit equivalent intents and reach equivalent
  state/event outcomes with the same seed, including non-time commands and death.
- Character creation, module/procedural start, load/resume, and save can be driven
  without terminal input or duplicating campaign policy.
- All user choices have stable identities and sufficient targeting/availability
  data; an adapter does not need to mutate raw Game to perform ordinary actions.
- Rules state and outcomes remain authoritative in the engine. Host networking
  input is validated; sessions are independently owned, with an explicit
  concurrency policy rather than shared mutable global state.
- Audio/filesystem/logging are host/platform concerns. Build portability must be
  tested on the intended target; a passing Linux test is not a Windows/browser
  certification.
- Existing text behavior stays covered by adapter/engine tests while rule changes
  such as original poison lethality remain explicit, separately validated work.

## Verification and Scope

The Phase 1 report records actual Debug/sanitizer tests and text boot smokes.
This review inspected session APIs, snapshots, commands, chargen, startup/load,
render paths, events, CMake, audio, and persistence/configuration ownership. No
new graphical/web frontend, platform build, menu API, storage service, or clock
refactor was implemented. Those are review recommendations requiring scoped work.

Description review remains a **human-assisted backlog**: extracted PDF data and
runtime descriptions are not reliably correlated. Do not replace that human
review with automated assertions that imported prose is verified.
