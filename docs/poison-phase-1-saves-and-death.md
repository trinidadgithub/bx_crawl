# Poison Phase 1 — Expert Saves and Death Handling

Status: **validated foundation; full Phase 1 sign-off remains pending alignment
qualification integration**. This is the poison track's Phase 1, not the earlier
persistence architecture phase.

Base: `29128b1` (merged PR #68). Prepared on
`feat/poison-expert-saves-and-death`; implementation checkpoint commit subject:
`Implement Expert saves and shared hero death handling` (resolve its SHA through
Git history). Commit/push was authorized after validation; no merge is claimed.
Validation recorded 2026-11-11 UTC, using the machine's UTC date.

## Implemented

- `include/saving_throws.hpp` contains the original Cook/Marsh Expert X24 bands
  for all seven classes and all five save categories. The table was checked
  against the scan image, including the OCR-obscured Thief 5-8 paralysis target
  of 11. Source provenance is in the [audit](POISON-MECHANICS-AUDIT.md).
- `bx::expert_save_row` returns an optional row. Classless level-1 characters use
  Expert Normal Man saves; invalid classes, levels outside a printed class
  table, and invalid save categories are explicitly rejected by the live helper
  with a log and no RNG draw. Table ranges are not new character advancement
  caps. Existing high-level demihuman saves outside those ranges are not silently
  assigned invented higher-level targets.
- The existing `make_save` remains the sole live save resolver. It now uses the
  hero level and a narrow `SaveContext` with source modifier, explicitly magical
  source, and explicit protection applicability. It preserves bat confusion,
  applies Wisdom to wands/spells or explicitly magical saves, excludes dragon
  breath from Wisdom, and does not apply Wisdom to ordinary venom.
- Crab Spider currently passes its verified +2 save modifier at its existing
  poison call site. The exact-name bridge is temporary until Phase 3 profiles;
  it is not a new substring-based profile system.
- `game_internal::kill_hero` centralizes HP/alive state, a session-only
  `DeathCause`, one PlayerDeath sound, and the supplied log. Repeated calls on a
  dead hero emit nothing and do not overwrite the original cause. Physical
  attacks (including bears), poison damage, traps, disease, blood drain, slime,
  and starvation use the common helper at their existing death sites.
- Trap physical damage remains distinct from the poison contribution: a dart
  that already kills physically is not labelled poison-revivable. Monster bite
  death likewise remains physical even if the attacker also has poison.
- Hero snapshots expose death cause. `GameSession` stops an already-dead session
  before processing another command. Existing initiative-order death reporting
  remains covered.
- v7 saves are unchanged on disk. Loading explicitly clears the receiving
  session's unsaved death cause; no claim is made that v7 preserves death causes,
  pending poison, or a revival opportunity.

The legacy `bx::saving_throws` table remains compatibility/low-level metadata;
the live helper now uses the explicit Expert bands. No generator, monster data,
save schema, platform configuration, or extra dependency changed in this slice.

## Rules Scope and Remaining Gate

Basic B6/B12/B26/B43 and Expert X24 govern the save changes. Original lethal
poison remains the approved target, but **this phase does not activate universal
save-or-die**. The existing extra-d4 combat/trap poison is still present pending
source profiles, time, persistence, and treatment. The high-HP fatal-poison test
exercises the shared death seam after a failed save, not a blanket species effect.

`SaveContext::protection_applies` must be supplied by a rules caller that knows
the attack qualifies for Protection from Evil. Current characters have no
alignment field or alignment-selection/persistence contract. No alignment is
guessed and existing monster/trap calls do not unconditionally grant +1.
Known qualifying/nonqualifying contexts are tested, but automatic qualification
in current gameplay is **not wired**. This prevents unconditional Phase 1
completion claims. Resolve how the engine obtains that information before full
sign-off; do not ask each frontend to invent the rule. See the
[frontend review](FRONTEND-ADAPTABILITY-REVIEW.md) and continuity Q08.

Classification of magical venom such as Tarantella remains Q04. A generic
explicit magical-source context works, but this slice does not decide that
source classification or implement its dance. Resurrection, treatment, poison
deadlines, and saved death causes remain later phases.

## Regression Coverage

- Every printed class band and level within it, across five categories;
  unsupported ranges/classes and Normal Man selection.
- Live save boundary equality and one below, all categories at Fighter level 7,
  Cleric level 10 improvement, source +2, protection on/off/absent, natural versus
  explicit magical Wisdom, dragon-breath exclusion, and bat confusion.
- Invalid save requests consume no RNG; seeded rule tests use DiceRoller.
- Crab +2 changes a formerly failed save into a success on an actual attack.
- High-HP fatal-poison death seam and idempotent state/log/sound behavior.
- Ordinary and poison-damage deaths stop later monsters; both initiative orders
  reach frontend-neutral session death results and snapshots.
- PoisonDart actually triggers, testing physical versus poison death attribution,
  sound parity, and trap consumption.
- Loading a living v7 character clears a receiving Game's stale poison death
  cause. Existing corruption/legacy-save coverage still passes.

## Validation Executed

```sh
cmake --build build -j 2
./build/tests/bx_crawl_tests
ctest --test-dir build --output-on-failure
cmake --build build-sanitize -j 2
ctest --test-dir build-sanitize --output-on-failure
```

- Debug build: pass, no compiler warnings shown.
- Direct doctest: **236/236 cases, 15919/15919 assertions**, all passed.
- Debug CTest: **1/1 passed**, 2.83 seconds.
- ASan+UBSan build/CTest: **1/1 passed**, 15.77 seconds; no sanitizer errors.
- Existing root-launched test description-loading warnings still occur.
- Both following manual boot/quit smokes exited 0, without saving:

```sh
printf 'x' | timeout 30 env BX_LOAD_SAVE=Hrothgar ./build/frontends/text/bx_crawl_text >/dev/null
printf 'x' | timeout 30 env BX_LOAD_SAVE='Tomb Runner' ./build/frontends/text/bx_crawl_text >/dev/null
```

The Tomb Runner smoke emitted the existing description-loading warning. These
are boot smokes, not a new poison playtest or graphical frontend proof.

## Review and Rollback

Review the narrow rules changes and the outstanding alignment gate before
merging. A revert needs no disk migration because the format remains v7. Do not
turn the temporary generic damage behavior into an approved house-rule option.
Future Phase 4 must serialize death cause and relevant time/state before claiming
poison revival across reloads works.
