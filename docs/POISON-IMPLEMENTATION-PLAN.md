# Poison Implementation Plan

## Status and Authority

Planning document; **no poison implementation phase has begun**. Prepared against
`afa59d66866b6c7c666a5e7b74771f3c6c4d2705`, reconciled 2026-10-28.

Start each session with [Development Continuity](DEVELOPMENT-CONTINUITY.md).
Use [Poison Mechanics Audit](POISON-MECHANICS-AUDIT.md) for primary-source
references, execution traces, existing test results, and findings P01-P11.
This plan does not repeat the audit or turn proposals into implemented behavior.

The owner has **approved faithful 1981 Moldvay Basic / Cook–Marsh Expert poison**:

- Immediately lethal poison kills on a failed save regardless of remaining HP.
- Original monster-specific delays, illness, blindness, magical dancing, saving
  throw modifiers, and delivery mechanisms must be preserved.
- Generic extra-1d4 poison will be replaced, not retained as the baseline.
- House-rule override design is outside this implementation scope.
- Work remains incremental, tested, and within the existing architecture.

These decisions are recorded as DEC-POISON-001 through DEC-POISON-005 in the
continuity document. The audit's historical requests for approval of lethality
and replacement of simplified poison are **resolved by that directive**. Do not
reopen those questions or interpret all poison as immediate death.

### Architectural constraints

- Preserve authoritative `Game` state and `GameSession` orchestration; rendering
  consumes snapshots/events (ADR [0001](adr/0001-engine-frontend-boundary.md)).
- Keep rules separate from declarative adventure data and mutable campaign state
  (ADRs [0002](adr/0002-data-first-modules.md) and
  [0004](adr/0004-rules-adventure-campaign-state.md)).
- Reference monsters, spells, and saved sources using stable `ContentId` values,
  resolved through `RulesCatalog`, not array indexes or substring matching
  (ADR [0003](adr/0003-stable-content-ids.md)).
- Continue using `make_save` and `DiceRoller`; do not duplicate save math or RNG.
- Prefer narrow poison-specific state and helpers. No generic effect framework,
  ECS, scripting, new dependency, or modern-edition poison mechanics are needed.
- Do not silently add absent monsters. `Scorpion, Giant`, Black Widow, Sea Snake,
  and Giant Rattlesnake remain outside the present-monster implementation batch.
- The checked-in `bx_data.hpp` is currently curated; ordinary builds use
  `BX_REGENERATE_DATA=OFF`. Reconcile any required source/header changes narrowly.
  Do not regenerate unrelated monsters, equipment, or treasure as a side effect.

## Delivery and Dependency Map

| Poison phase | Depends on | Intended deliverable | Current status |
|---|---|---|---|
| 1. Saving throws and death | Planning review; applicable source-modifier contract | Expert saves, consistent cause-aware death, deterministic tests | Not started |
| 2. Authoritative clock | Phase 1 death seam; Q02/Q06 below | Explicit round/turn advancement independent of rendering | Not started |
| 3. Poison profiles | Phase 1 save context; Phase 2 time units; Q01/Q04/Q05 where relevant | Explicit catalog-backed profiles and distinct deliveries | Not started |
| 4. Persistence and treatment | Phases 1-3; Q01/Q02/Q03/Q06 | Validated saved poison/time/death state and original treatment | Not started |
| 5. Present monster behavior | Phases 1-4; species-specific gates | Seven current venomous species using faithful effects | Not started |
| 6. Traps and integration | Phases 1-5; Q07 | Shared poison resolution for traps and full-flow verification | Not started |

These numbers belong to the **poison track**, not the historical architecture
Phase 1-34 numbering. Use explicit names in PRs and handoffs.

Each phase may require several narrow PRs. Only mark a phase complete after all
its acceptance criteria have evidence. A merged preparatory PR is a milestone,
not automatic completion of its phase. Avoid placeholder effects in live combat:
profiles/state/treatment may be exercised through tests before activation, but
do not route nonfatal venom to an immediate-death fallback. Replace the legacy
handler per verified delivery when prerequisites are ready, and remove the
generic path once all current sources are integrated. Temporary retention during
staging is not an approved house-rule mode.

## Phase 1 — Saving Throws and Death Handling

### Scope

1. Make the existing save lookup select **Cook–Marsh Expert X24 class/level
   bands**, rather than one row for every level. Keep the existing correct
   low-level Poison targets, including Expert's Dwarf/Halfling 8; do not substitute
   Basic's 10 while claiming Expert progression. Verify table transcription for
   the supported level ranges before editing; decide invalid class/level handling
   explicitly instead of silently choosing a plausible target.
2. Extend the existing `make_save` boundary with minimal source context/modifiers
   so crab-spider +2 and qualifying Protection from Evil saves can be expressed.
   Preserve bat confusion. Natural venom must not acquire a universal Wisdom or
   Constitution bonus. Defer unresolved magical-poison classification (Q04).
3. Establish a small cause-aware death finalization path for ordinary damage,
   monster poison, and poison traps: consistent HP/alive state, log/sound event,
   and `ActionResult` termination. Keep causes distinct enough for later poison
   revival; ordinary bite death must not become poison-revivable.
4. Do **not** activate generic save-or-die combat here. Establish the fatal-poison
   death operation and tests without misapplying it to centipedes/tarantellas.

### Dependencies and likely files

- Planning review and agreed save-source interface; source/alignment context may
  require a small rules-data adapter. No new poison save implementation.
- `engine/src/game.cpp` (`make_save`), `combat.cpp`, `exploration.cpp`,
  `game_internal.hpp`, `game_session.cpp`; `include/entity.hpp`, `game.hpp`,
  `game_event.hpp`/snapshot only if necessary; save-table source and compiled
  table; `tests/test_core.cpp`.

### Rules and acceptance

- Basic B6/B12/B26: relevant modifiers, protection, correct saving throw category,
  d20 >= target, ordinary damage retained on save. Basic B43: crab +2.
- Expert X24: correct supported class/level boundaries. Tests must cover each
  band's endpoints and transitions, not merely table size.
- All death causes in scope emit consistent notifications, stop further monster
  attacks, and report hero death through `GameSession`. No duplicate sound/event.
- A fatal-poison test with abundant remaining HP still produces death through
  the death seam; this does not authorize blanket activation for all profiles.
- No save-format bump or unrelated special-attack dispatch rewrite.

### Automated tests, risks, and rollback

- Seeded success/failure and modifier boundary tests through the existing save
  boundary; preserve random draw ordering unless a reviewed rules change requires
  it. Include crab +2, confusion, qualifying/nonqualifying protection, and natural
  venom unaffected by Wisdom. Magical-source assertions await Q04.
- Death tests for ordinary damage, poison cause, trap, already-dead idempotence,
  monster-first/hero-first session ordering, and sound/log parity.
- Run Debug/full suite and ASan+UBSan; inspect existing paralysis/vampire-bat saves.
- Risks: changing shared saves affects all specials, and central death handling
  can double-award XP or alter event/RNG order. Avoid broad combat refactoring.
- Rollback: revert the narrow PR(s), preserving tests that characterize the
  accepted contract when practical. No disk migration is expected in this phase.

## Phase 2 — Authoritative Game Clock

### Scope

- Separate elapsed gameplay time from `compute_visibility` and frontend frame
  preparation. Visibility/snapshots/redraws must not advance gameplay timers.
- Define explicit engine-owned advances for ten-second combat rounds and
  ten-minute dungeon turns: **60 rounds = 1 turn**. A labelled accumulator/deadline
  representation is a candidate, not an approved concrete type yet.
- Implement Basic B23 encounter accounting: an encounter of up to sixty rounds
  consumes a full dungeon turn including recovery. Resolve Q02 before deciding
  how elapsed rounds plus recovery affect poison at a boundary; do not charge
  the same time twice.
- Inventory, help, save, quit, failed UI selection, and render-only operations
  consume no elapsed time. Classify other rejected/blocked gameplay commands
  explicitly; a command enum alone is not proof that time passed.
- Track elapsed time beyond combat and across empty encounters, level changes,
  procedural exits, module transitions, and explicit rest/travel. Q06 governs
  town/rest durations; do not invent them in timer code.
- Classify/migrate affected existing counters with documented units. Fix the
  clock without silently rewriting every unrelated monster mechanic.

### Dependencies and likely files

- Phase 1's death/event seam; Q02 boundary/recovery and Q06 town-time decisions.
- `include/game.hpp`, `game_session.hpp`, `entity.hpp`, `campaign_state.hpp`;
  `engine/src/game_session.cpp`, `game.cpp`, `exploration.cpp`, `town.cpp`,
  `campaign_state.cpp`, relevant spell timers; frontend frame preparation and
  snapshot renderer; `tests/test_core.cpp`. Disk persistence waits for Phase 4.

### Rules and acceptance

- Basic B23 time terminology and encounter recovery; B43 delayed poison as the
  target use case. B/X combat rounds are ten seconds, not six seconds.
- Sixty explicit rounds equal one turn; one or more explicit dungeon turns
  advance the same clock consistently. Changing encumbrance cannot reset a poison
  deadline or redefine the length of a minute.
- A test-only pending deadline survives combat-to-exploration transitions and
  expiry is independent of monster presence/render count. Fatal source behavior
  is not activated before persistence/treatment prerequisites.
- One authoritative ordering for time advance, effect expiry, death, action
  eligibility, and recovery is documented and matches Q02's eventual decision.
- Save command is non-time-consuming through the *actual frontend/session path*,
  not merely through a direct `apply_player_command` test.

### Automated tests, risks, and rollback

- Test 0/1/59/60/61 round boundaries, multiple turns, fractional-turn remainder,
  short/long encounter recovery, and no double counting. Exact recovery assertions
  must follow the approved policy rather than a guessed rule.
- Cross-encounter/location tests, no-monsters tests, time-neutral command/redraw
  tests, encumbrance changes, light/hunger/wandering/buff regressions, incapacity.
- Rest/travel tests require approved durations; record blocked cases rather than
  marking the whole clock phase complete while ten-day illness cannot advance.
- Risks: frame/action decoupling changes RNG opportunities and hunger/light rates;
  paralysis currently decrements per command despite its "turns" name.
- Rollback: isolate clock changes and migrations in reviewable commits. Before
  live poison is activated, reverting must not strand an enabled poison timer.
  Coordinate with Phase 4 once the clock is serialized.

## Phase 3 — Explicit Poison Profiles

### Scope

- Resolve poison behavior via explicit stable monster/profile IDs through the
  existing catalog boundary. Remove substring inference as the authority for each
  migrated poison delivery; an exact source table is a reasonable first step.
- Profiles need only express verified choices: delivery (bite/sting/spit), save
  category/modifier, effect kind, delay/duration with units, and treatment facts.
  Do not build a universal status language or assume every poison deals HP damage.
- Handle multiple attack deliveries distinctly. Keep physical damage separate;
  zero physical damage must be representable without the current `Nil -> d6` and
  minimum-one behavior.
- Define profiles for the seven existing venomous species in Phase 5. Do not
  make all spider attacks fatal or turn every Poison saving throw into venom.
- Poison immunity checks should be explicit and source-aware where eligible
  targets exist. Current hero-only venom handling is not a completed generalized
  monster-immunity implementation.

### Dependencies and likely files

- Phase 1 save/death contract and Phase 2 units. Q01 multiple exposures, Q04
  magical venom, Q05 unsupported targets/variants may gate specific profile work.
- `include/rules_catalog.hpp`, `content_id.hpp`, `entity.hpp`, narrow new poison
  types if justified; `engine/src/rules_catalog.cpp`, `game.cpp` spawn and damage
  parsing, `combat.cpp`, `game_internal.hpp`; tools if their poison flags change;
  relevant data sources/header only as needed; `tests/test_core.cpp`.

### Rules and acceptance

- Basic B26/B33/B37/B39/B42/B43, and audit section 4's presence-verified matrix.
  Expert Scorpion is an illustration of delivery separation, not authorization
  to add that absent monster.
- Profile resolution is independent of display-name changes/substrings and table
  reordering; IDs use ADR 0003 and failed lookup is explicit.
- Killer Bee and Medusa have real profile mappings; generic/absent spider prose
  does not create catalog entries. Cave Locust spit, Troglodyte stench, and
  dragon breath are not classified as ordinary lethal venom.
- Per-profile definitions/tests express delays and nonfatal effects correctly.
  No profile is active through an incomplete fatal fallback. Generic poison
  inference is removed from live resolution once all affected deliveries migrate.

### Automated tests, risks, and rollback

- Catalog-ID mapping and negative lookup tests; full seven-species table tests;
  delivery-specific save triggering and ordinary-damage separation; renaming and
  reordered lookup without index-based coupling; zero-damage centipede delivery.
- Keep monster-on-monster immunity or queen representation tests pending Q05 if
  not supported; do not count them as complete merely from a boolean flag.
- Risks: name-derived stable IDs and malformed duplicate catalog rows require
  care; do not silently rename IDs or regenerate the entire curated header.
- Rollback: separate passive profile definitions from activation; revert one
  integration without changing already serialized IDs. Do not reuse retired IDs
  for incompatible effects.

## Phase 4 — Persistence and Treatment

### Scope

- Add authoritative victim-side records for pending poison/effect source,
  deadlines/durations, relevant symptom/treatment state, and poison death cause/
  revival eligibility. Use only the state required by approved profiles.
- Design a versioned save extension (candidate **v8**, if still unused at that
  time), with explicit migration defaults for older saves and bounded, validated
  fields. Preserve partial-time state so reload does not postpone death.
- Implement original Neutralize Poison as a fourth-level clerical spell with
  appropriate catalog/availability/slot handling; remove currently present poison,
  not confer future immunity. Resolve Q03's ten-round revival anchor and solo
  treatment model first.
- Ordinary healing must not clear venom; Purify Food and Water concerns supplies,
  not a poisoned victim. Tarantella's Dispel Magic interaction is specific. No
  unsupported antidotes. Decide the minimal supplies-purification correction
  separately if it expands beyond the current poison work.
- Keep `GameSession`, `CampaignState`, and frontend death/treatment presentation
  consistent. A dead victim is not automatically restored to life by load.

### Dependencies and likely files

- Phases 1-3; Q01/Q02/Q03/Q06 and a reviewed wire-format/treatment contract.
- `include/entity.hpp`, `game.hpp`, `campaign_state.hpp`, snapshots;
  `engine/src/save.cpp`, `campaign_state.cpp`, `spells.cpp`, `game_session.cpp`,
  `game.cpp`, `chargen.cpp` if spell availability needs it; spell source/table;
  frontend status/death flow; tests and save-format documentation.

### Rules and acceptance

- Basic B23/B43 delayed consequences; Expert fourth-level clerical Neutralize
  Poison section (audit source links). Re-read only the unresolved window passage
  if needed, not the entire audit.
- Save/load across fresh and already-poisoned receiving Games preserves the
  *saved* victim, clock remainder, deadlines, and cause, not receiver residue.
- v1-v7 remain readable with explicitly clean poison defaults; unsupported new
  versions reject safely. A v7 executable is not expected to read new saves.
- Invalid counts, enums/IDs, negative or overflowing times, inconsistent alive/
  death state, and truncated tails fail without modifying the live Game.
- Neutralize Poison before expiry removes applicable current effects; later
  exposures still require saves. Revival inside/outside the approved window and
  non-poison-death rejection are deterministic. No silent revival on reload.
- Treatment availability in solo play is explicit. Do not claim post-death
  treatment works while the frontend has already terminated all opportunities.

### Automated tests, risks, and rollback

- Handcrafted legacy/new payloads, round trips, multiple pending effects under
  Q01, dirty-receiver defaults, corrupted/truncated state, write failure, module
  and procedural resume; preserve existing validation/security guarantees.
- Spell slot/known-spell/class checks, neutralization without HP healing assumptions,
  repeat exposure after treatment, poison-versus-ordinary death, window boundaries.
- Risks: old binaries reject new versions; writer is not currently an atomic
  rename transaction; in-memory campaign capture is not a disk format.
- Rollback: use isolated test saves/backups; never test by overwriting the user's
  only campaign save. A code revert must not downgrade or truncate new-format
  saves. Document binary/save compatibility and preserve original files; decide
  migration before rolling back an activated phase.

## Phase 5 — Monster-Specific Behavior

### Scope and per-monster acceptance

Activate verified profiles for **existing catalog monsters only**. Correct
physical delivery data where necessary through narrow source/table changes.

| Current monster | Required behavior / original source | Automated acceptance focus |
|---|---|---|
| Centipede, Giant | No bite HP damage; P save or ten days' illness, half movement and no other physical actions (Basic B33) | Zero damage on either save result; action/movement restrictions; approved days/rest/travel clock; persisted recovery. |
| Snake, Cobra | Spit up to six feet: hit + P save or blindness; bite d3 + P save or death in d10 turns; spit or bite, not both (B42) | Distinct delivery selection/range; no lethal spit fallback; delayed bite save/load/treatment. Grid-range mapping requires Q05. |
| Snake, Pit Viper | Bite + P save or immediate death; always wins initiative (B42) | Failed save fatal at high HP; success retains bite damage; initiative rule without global monster-first behavior. |
| Spider, Crab | d8 bite + P save with +2 or death in d4 turns (B43) | Exact modifier and deadline boundaries, cross-encounter persistence and treatment. |
| Spider, Tarantella | P save or magical dancing 2d6 turns; onlookers S; -4/+4; exhaustion after five turns; specified treatment/Dispel interaction (B43) | Nonfatal effect, linked onlooker duration if eligible targets supported, exhaustion ordering, persisted status, correct cure. Q04/Q05 gate unresolved cases. |
| Killer Bee | P save or immediate death; ordinary bee dies on sting; stinger 1 HP/round until a round spent removing it (B37) | Separate sting/venom/stinger state; save success does not suppress ordinary damage/stinger; removal takes time; no new queen row without approval. |
| Medusa | Snake group once/round, d6 + P save or death in one turn (B39) | Separate venom and gaze deliveries; correct delay, treatment, no gaze-as-poison classification. Scope of missing gaze integration must be reviewed explicitly (Q05). |

Dependencies: Phases 1-4, applicable Q01-Q06 decisions. A delivery marked
incomplete cannot be reported as a fully faithful species implementation.

Likely files: poison/profile definitions and catalog adapter, `combat.cpp`,
`game.cpp` spawn/AI, `spells.cpp`, `game_session.cpp`, snapshots/frontend actions
for stinger removal or incapacity, source monster rows/header/descriptions,
`tests/test_core.cpp`, phase documentation. Do not rewrite unrelated bear/slime
mechanics or add Black Widow/Scorpion to make a test easier.

### Regression risks and rollback

- Lethality is an intentional approved rules change, not a behavior-preserving
  refactor. Verify successful save, ordinary damage, death cause, RNG ordering,
  notifications, rescue opportunities, and save/load for each species.
- Existing mutually exclusive special dispatch must not suppress independent
  deliveries, nor should changing every `else if` to `if` fire unrelated effects.
- Use one reviewed species/delivery slice per PR where practical. Once active,
  rollback must honor Phase 4 save compatibility. Do not mask a regression by
  reverting to an undocumented low-damage house rule; record the rollback and
  affected saves/features explicitly.

## Phase 6 — Traps and Integration

### Scope

- Resolve the custom PoisonDart's activation, hit-roll/base-damage specification
  (Q07). Lethal venom consequence is already authoritative; only the unspecified
  dart delivery is undecided. Do not reintroduce extra-d4 poison as a workaround.
- Route qualifying trap exposures through the same profile/save/treatment/death
  mechanics as monsters, keeping trap delivery separate from poison effect.
- Confirm procedural and module traps share resolution and disable/report their
  state correctly. Do not silently add poison gas, potions, or new trap types.
- Validate the full combat -> exploration -> treatment -> save/reload lifecycle,
  including a failed crab-spider save followed by surviving the encounter.

### Dependencies and likely files

- Phases 1-5 and reviewed Q07 contract.
- `exploration.cpp`, shared poison/save/death helpers, `game_session.cpp`,
  `module_content.cpp`/`map.cpp` only if the delivery contract requires them;
  relevant trap definitions if explicitly reviewed, frontend status/events,
  `tests/test_core.cpp`, `test-plans/thief-skills-playtest.md` and trap docs.

### Rules, acceptance, tests, risks, and rollback

- Basic B26/B52 lethal poison and source-specific modifiers; the audit establishes
  that B52 does not specify the project's exact custom d4 dart contract.
- Tests must **trigger the PoisonDart**, not just assert it was placed and then
  step on a pit. Cover save success/failure, delivery avoidance under the approved
  policy, ordinary damage, consumption, death events, treatment, and persistence.
- Integration tests span monster/hero initiative order, multiple exposures,
  poison deadlines with no monsters alive, non-time commands, module/procedural
  transitions, rest and reload, failed-load state preservation, and cure boundaries.
- Run full Debug and sanitizer suites plus appropriate manual frontend checks;
  verify HUD/events/death/treatment flow, not just unit-level HP.
- Risks: duplicated trap poison, old playtest expectations still describing extra
  damage, accidental clock advancement during save, and treatment after session
  termination. Review source/prose/tool agreement at completion.
- Rollback: isolate trap integration from previously validated monster resolution;
  preserve saved effect identifiers and document module/save compatibility. Keep
  an explicit phase status if any integration criterion remains unsatisfied.

## Unresolved Questions Register

These are proposed design questions, **not approved house rules**. Record their
eventual resolutions with decision ID, actual approval evidence/date when known,
supporting rule text, and affected phases in continuity. Do not guess an answer
to unblock implementation.

| ID | Decision needed | Gate |
|---|---|---|
| Q01 | Repeat delayed exposures: how independent deadlines/effects coexist, neutralization scope, and nonfatal repeat-effect interaction. Every successful poisonous attack still requires its own save. | Profile state / Phase 4 persistence / species integration |
| Q02 | Exact expiry-versus-treatment ordering and Basic B23 encounter recovery accounting without double charging; semantics of unsuccessful time-consuming actions. | Phase 2 clock contract |
| Q03 | Expert's ten-round revival window anchor and solo/NPC treatment opportunity after poison death; restored HP/alive representation must follow verified source or explicit reviewed interpretation. | Phase 4 treatment/death persistence |
| Q04 | Source-aware Wisdom/modifier treatment for magical poison, especially Tarantella, under Basic B6; no blanket natural-poison Wisdom bonus. | Relevant Phase 1/3 save context and Phase 5 tests |
| Q05 | Minimal representations for six-foot spit on the grid, multiple attack deliveries, bee queen (no new catalog monster authorized), onlooker targets, and scope/dependencies of Medusa gaze and broader monster immunity. | Relevant Phase 3/5 slices |
| Q06 | Rest/town/travel elapsed-time contract needed for ten-day illness and cross-location deadlines; no invented day advancement. | Phase 2/4 and Centipede completion |
| Q07 | PoisonDart hit/activation/base damage contract and any module compatibility implications. The original lethal poison consequence is already settled. | Phase 6 |

Table transcription checks and validation of unsupported character levels are
implementation prerequisites under Expert progression, **not** a reopening of
the approved B/X baseline. No optional house-rule configuration is planned.

## Validation, Review, and Resume Gates

- Use continuity section 9 for exact current validation history and portable
  commands. The audit's passing 228-case suite is a baseline, not evidence that
  any new poison-phase criteria have passed.
- Add deterministic regression tests for changed rules; full suites must pass
  alongside source-specific acceptance and relevant manual checks. Avoid tests
  that merely freeze guesses about unresolved questions.
- Proposed workflow: a narrow branch/PR per slice with detailed scope, source
  references, tests actually executed, outstanding gates, compatibility, and
  rollback notes. The owner handles merges. Commit/push/PR creation requires
  separate authorization; the plan itself authorizes none of those operations.
- At each handoff update [continuity](DEVELOPMENT-CONTINUITY.md), identifying
  both committed work and local changes. Do not close a phase just because its
  branch merged or tests passed.

**Exact next action:** review this plan and continuity with the owner. Request
approval for a bounded **Poison Phase 1** slice: Expert saving-throw selection
and common cause-aware death handling with deterministic tests. Resolve only
the source-context questions needed for that slice. Until that review/approval,
do not edit application code, data, tests, save formats, or build configuration.
