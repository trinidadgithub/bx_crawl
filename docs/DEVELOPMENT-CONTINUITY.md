# Development Continuity

**Start here for every new OpenCode session.** This is the operational handoff
across machines, sessions, models, and interruptions. Update current status and
next action in place; preserve accepted decisions and validated milestones.

**Last reconciled:** 2026-11-21 (machine UTC record date).

**Active work:** validated local full AC-convention/shield fix on
`fix/armor-class-consistency`. Dedicated shield removal is merged through
PR #70. Full poison Phase 1 qualification still awaits Q08.

**Next action:** review the AC/save-v8 correction and playtest it with a copy of
an older character save; commit/push only when authorized. Then resume Q08;
clock questions Q02/Q06 remain unresolved.

## 1. Project Identity

| Item | Value |
|---|---|
| Project | BX Crawl / B/X Crawl |
| Repository | https://github.com/trinidadgithub/bx_crawl |
| Primary language | C++20; Python for supporting content/codegen tools |
| Build system | CMake 3.20+; doctest via `bx_crawl_tests`, registered with CTest |
| Default branch / development style | `main`; incremental, reviewable feature branches/PRs |
| Runtime state | `Game` and `Entity`, authoritative within the engine |
| Application boundary | `GameSession`, commands, `ActionResult`, snapshots, events |
| Rules/content | `RulesCatalog`, `ContentId`, compiled `include/bx_data.hpp` |
| Randomness | `DiceRoller` owns `Game::rng`; rule helpers use it |
| Campaign/persistence | `CampaignState` in-memory capture; separate binary disk save/load |
| Presentation | POSIX text frontend under `frontends/text`; tools under `tools` |

Authoritative documentation:

- [Poison Implementation Plan](POISON-IMPLEMENTATION-PLAN.md): phased scope,
  dependencies, acceptance, tests, rollback, and unresolved questions.
- [Poison Mechanics Audit](POISON-MECHANICS-AUDIT.md): original B/X evidence and
  implementation facts at `afa59d6`. Consult it rather than repeating the audit.
- [Phase 1 validation report](poison-phase-1-saves-and-death.md): local implementation,
  exact test results, and remaining qualification gate.
- [Frontend adaptability review](FRONTEND-ADAPTABILITY-REVIEW.md): concrete evidence
  and proposed small steps for non-text frontends.
- [AC consistency and migration](armor-class-consistency.md): current local fix,
  ascending/display contract, v1-v7 migration and save-v8 compatibility.
- [README](../README.md): build/run/tool entry point.
- [ROADMAP](../ROADMAP.md): historical architecture/gameplay tracks. Its older
  recommended description-audit task is not the current poison priority.
- ADRs [0001](adr/0001-engine-frontend-boundary.md),
  [0002](adr/0002-data-first-modules.md),
  [0003](adr/0003-stable-content-ids.md),
  [0004](adr/0004-rules-adventure-campaign-state.md): accepted boundaries.
- [Persistence safety](phase-1-persistence-hardening.md),
  [campaign boot/save v7](phase-30-module-campaign-boot.md),
  [DiceRoller](phase-20-dice-roller.md),
  [trap slice](phase-25-traps.md), and
  [thief playtests](../test-plans/thief-skills-playtest.md): useful historical
  validation context, not substitutes for current code or current test counts.

The repository and tests govern implementation facts. This document governs
recorded decisions and handoff context; it must not override contradictory code
evidence. Investigate and report disagreements; never silently pick whichever
version is convenient. Evidence-backed corrections can update the audit, but do
not rewrite its historical validation claims as if later work existed then.

## 2. Development Objective

Implement faithful **1981 Moldvay Basic / Cook–Marsh Expert poison mechanics**.
Original lethality is explicitly approved: **a failed save against immediately
lethal poison kills regardless of remaining HP**. Preserve original delayed and
nonfatal exceptions. Replace the generic extra-1d4 poison approximation.

This is not an invitation to rebalance poison, add antidotes, substitute BECMI/
OSE/modern mechanics, or implement a house-rule override. Those overrides are
deferred beyond the current baseline scope.

## 3. Authoritative Decisions

Approval source for the five initial decisions: the owner's directive titled
"B/X Crawl — Poison Implementation Planning and Development Continuity" in the
current conversation. **An independent approval date was not supplied.** Date
fields below distinguish that fact from the record-entry date, 2026-10-28 UTC;
the latter is not an invented historical approval timestamp.

| Decision ID | Date | Decision | Rationale | Scope | Status | Supporting documentation |
|---|---|---|---|---|---|---|
| DEC-POISON-001 | Approval date not supplied; recorded 2026-10-28 | Original 1981 B/X poison mechanics are authoritative. | Owner explicitly chose original Moldvay/Cook–Marsh fidelity. | Poison rules baseline; Expert progression as requested in Phase 1. | Accepted — owner directive | Audit §2; plan Status and Authority / Phase 1; this §2. |
| DEC-POISON-002 | Approval date not supplied; recorded 2026-10-28 | Immediately lethal poison kills on a failed save regardless of HP. | Extra-d4 damage is not the original lethal outcome. | Only sources whose verified poison is immediately lethal, not every Poison save. | Accepted — owner directive | Audit §2, P01; plan Phase 1 death seam / Phase 5. |
| DEC-POISON-003 | Approval date not supplied; recorded 2026-10-28 | Preserve monster-specific effects, delays, modifiers, and delivery. | Original centipede, crab, cobra, tarantella, bee, and medusa rules differ. | Existing catalog poison and source-specific exceptions. | Accepted — owner directive | Audit §§2/4; plan Phases 3/5. |
| DEC-POISON-004 | Approval date not supplied; recorded 2026-10-28 | House-rule overrides are deferred until after baseline implementation. | Establish faithful rules before optional variants. | No reduced-lethality mode, generic extra-d4 override, or antidote design in this track. | Accepted — owner directive | Plan Status and Authority; audit's historical unresolved simplification question is now superseded. |
| DEC-POISON-005 | Approval date not supplied; recorded 2026-10-28 | Implementation remains incremental, testable, and compatible with existing architecture. | Preserve known boundaries and reduce regression risk. | RulesCatalog/ContentId, DiceRoller, Game/GameSession, validated persistence; narrow reviewed phases. | Accepted — owner directive | Plan dependency map; ADRs 0001-0004. |
| DEC-WORK-001 | Approval date not supplied; recorded 2026-11-11 UTC | Prioritize Poison Phase 1, then review support for non-text frontends. Description review is a human-assisted long-term effort, not automatic PDF/prose reconciliation. | Owner explicitly authorized Phase 1 and the subsequent engine adaptability review; described import mismatches requiring human intervention. | Current implementation/review scope; frontend implementation itself is not yet authorized. | Accepted — owner directive | Phase 1 report; frontend review; ROADMAP. |
| DEC-AC-001 | Approval date not supplied; recorded 2026-11-21 UTC | Correct the full AC inconsistency: ascending internally, conversion from original B/X armor/monster AC at the data boundary, clearly labelled display, and reviewed save compatibility. | Owner selected "Full AC fix" after the mixed ascending/descending implementation and missing shield bonus were identified. | Armor, shields, Dexterity, hit targets and display. Does not authorize unrelated attack-matrix/spell-rule changes. | Accepted — owner selection | `armor-class-consistency.md`; this session's explicit scope question/answer. |

**Preservation rule:** do not reinterpret or repeatedly request baseline-lethality
approval in later sessions. The audit's former baseline/house-rule approval
questions are historical, not current blockers. Unsettled details in section 8
remain unsettled. New decisions require actual evidence of owner approval or
existing accepted rules; proposals must be labelled Proposed, with unknown dates
left unknown. Amend decisions through a recorded superseding decision, not by
silently overwriting their history.

## 4. Current Repository State

Snapshot captured during validation of the owner-approved full AC correction,
before packaging from PR #70's baseline. The owner subsequently requested
add/commit/push for playtesting. Identify the resulting checkpoint by commit
subject `Fix ascending armor class and shield defense`, parent `d8d7f96`, using
Git history. The captured local/uncommitted state below is not permanent; verify
live Git/upstream state on resume instead of inventing a future commit SHA.

| Item | Verified state |
|---|---|
| Current branch | `fix/armor-class-consistency` — local; no upstream/push yet |
| HEAD / branch base | `d8d7f961b028978ba2f8937ce96275c4ee3d9227` — merge PR #70, same as local `main` at branch creation |
| Earlier documentation merges | PR #67 `4198886` merged the audit/plan; PR #68 `29128b1` merged the baseline handoff. The old Phase 1 preparation branch was deleted after PR #68. |
| Confirmed documentation revision | `a62a5799146f92decfeeaed273252b0e2309d186` — `Document poison audit and implementation continuity` |
| Committed gameplay | Phase 1 foundation `f33eb89` via PR #69; explicit shield removal `3df0f35` via PR #70. The earlier poison audit baseline remains `afa59d6`, not the current revision. |
| Tracking | Started from clean local `main` tracking its locally known `origin/main`; no fetch/remote synchronization was needed or claimed during this implementation task. |
| PR status | No implementation PR created. Prior empty open-PR query belongs to the earlier planning session and was not refreshed here. |
| Poison progress | Phase 1 foundation merged/tested; full qualification integration awaits Q08. Phases 2-6 not started. Generic extra-d4 poison remains pending profile/timing/treatment integration. |
| Audit dependency | The previously untracked audit is now committed with the plan, continuity file, and README in `a62a579`; it can travel with the branch across machines. |
| Local files | AC rules/header, chargen/inventory/town/spawn/combat/display, save semantic-version migration, pregen/show tools, regression tests and docs. Source monster/armor data and build configuration remain unchanged. |
| Commit/push/PR | No commit/push/PR/merge for the AC branch yet. |
| Latest engine validation | AC working tree: Debug 244/244 cases / 16251 assertions, Debug CTest 1/1 (0.72 s), sanitizer build/CTest 1/1 (5.18 s), isolated legacy Hrothgar text smoke AC 15 -> 14 passed. |
| Save format | Local writer/reader v8 marks corrected base-AC semantics; v1-v7 migrate on load. Committed main is still v7. No poison deadline/cause/clock payload added; future poison extension candidate is v9. |

The AC smoke used an isolated HOME and the shipped legacy Hrothgar template,
sent `b25ttx`, asserted both displayed AC values and shield-removal message, and
sent no save command. No user's campaign file was rewritten by the smoke.

Remaining limitations: source-specific poison outcomes, Killer Bee/Medusa poison
activation, delayed state, automatic protection qualification, and Neutralize
Poison. Expert level saves, crab +2 and poison-death sound are fixed in merged code.
Clock advancement is still coupled to frame preparation. Some conditions are
not serialized and can inherit receiving-Game residue on load. See audit rather
than assuming historical README descriptions prove complete fidelity.

## 5. Completed Development

This is a poison-track handoff, not a reconstruction of every historical commit.
Older validated work is referenced by its phase documents. Milestones below are
in chronological development order; dates are not inferred from a machine clock
or fabricated from this record's creation date.

| Milestone | Files / commits | Validation evidence | Acceptance satisfied | Remaining limitations |
|---|---|---|---|---|
| Pre-poison baseline: Oil Beetle spray merged as PR #66 | `d52508f`, merge `afa59d6`; combat/time/spell/status code, beetle data, tests; see `docs/oil-beetle-blistering-spray.md` and `git show --stat afa59d6` for full file set | Subsequent audit rebuilt and ran the committed revision; §9 records actual results | Provides the audited baseline and an example timed blister condition | Not poison implementation; blisters are session-only and use the flawed exploration timing boundary |
| Read-only Poison Mechanics Audit | `docs/POISON-MECHANICS-AUDIT.md`, committed in `a62a579` | Original-source checks, full runtime inventory/traces, Debug and sanitizer tests executed in audit session (§9) | Evidence baseline, findings P01-P11, exception matrix and proposed tests documented | Recommendations not implemented; original remaining questions partially superseded by owner decisions above |
| Implementation planning and continuity bootstrap | Plan, this file, README entry point; committed in `a62a579` | Documentation/Git checks only in this session (§9); no new engine validation claimed | Six-phase plan, accepted decisions, unresolved gates, handoff/update protocol and resume entry point recorded | Planning package awaits owner review; no implementation phase is complete |
| Phase 1 save/death foundation (not full phase completion) | `f33eb89`: save tables/interfaces, combat/trap/session/snapshot/load changes and tests | 236 cases / 15919 assertions, Debug and sanitizer CTest; boot smokes (historical §9) | Expert X24 lookup, explicit modifiers, crab +2, cause-aware/idempotent death, session/snapshot reporting | Actual protection qualification lacks hero alignment/source input; full phase sign-off remains pending Q08. Death cause is not serialized in v7. |
| Non-text frontend adaptability review | `docs/FRONTEND-ADAPTABILITY-REVIEW.md`, committed in `f33eb89` | API/ownership/platform source inspection, no new GUI/platform implementation or tests | Concrete gaps and incremental frontend-neutral acceptance recommendations documented | Runtime clock, startup/load, choices, snapshots, platform backends still need scoped implementation. |
| Phase 1/review merge | `f33eb89`, owner-merged PR #69 `5e52301`; old feature branch deleted locally/remotely | Implementation-session test evidence above; merge/cleanup verified separately | Foundation and review available on main/across machines | Q08 remains open; merge does not mean full Phase 1 sign-off |
| Dedicated shield removal playtest fix | `3df0f35`: `UnequipShield` command/helper, menus/input and regression test | Debug build and CTest 1/1 passed (historical §9) | Explicit `5` removal in armor/shield menu; retains owned shield, allows unarmed removal and later re-equip | Subsequent user playtest exposed the separate AC-convention bug |
| Shield action merge | `3df0f35`, owner-merged PR #70 `d8d7f96` | Prior slice's Debug validation; merge/pull verified | Dedicated shield removal available on main | User playtest exposed missing/inconsistent AC defense; next row addresses that separately |
| Full AC-convention/shield correction | Local `include/armor_class.hpp`, data-boundary conversions, effective AC, equipment/display paths, v8 migration and tests; no commit yet | 244 cases / 16251 assertions, Debug/sanitizer CTest, isolated Hrothgar UI smoke (current §9) | Shared ascending AC and derived physical shield; legacy migration and stable v8 reload verified | Old binaries reject v8 saves; unmodelled ad hoc legacy AC adjustments cannot be inferred. Poison Q08/clock/treatment remain open. |

Do not promote any poison phase to Completed until its plan acceptance criteria,
tests, relevant manual checks, and remaining caveats are reconciled. Record a
partially complete slice separately, even after its PR merges.

## 6. Active Development

**Active stage:** full AC-convention/shield correction under review. Phase 1
foundation, frontend review and explicit shield action are merged.

- **Completed locally:** ascending armor/monster conversion, derived physical
  shield bonus, shared combat/snapshot/display defense, retained DEX on equipment
  changes, old-save migration, v8 writer semantics and regression/UI smoke tests.
- **Remaining now:** owner reviews the AC change and v8 compatibility, playtests
  the rebuilt game and authorizes packaging if desired. Shield removal remains a
  dedicated action (`b` -> `2` -> `5`), not an equip toggle.
- **Poison follow-up:** resolve Q08: how the engine gets/preserves character/source
  alignment for protection qualification. No default alignment or unconditional
  +1 was invented. Later clock work still needs Q02/Q06 decisions.
- **Blockers:** Q08 prevents claiming full Phase 1 qualification integration.
  Later Q01-Q07 decisions remain open. This is not a reason to repeat the audit
  or reopen original lethality decisions.
- **Relevant source entry points for later work:** `game.cpp` `make_save`, spawn
  and `monsters_turn`; `combat.cpp` `attack_internal`; `exploration.cpp` trap/time;
  `game_session.cpp` action/death/frame boundaries; `save.cpp`; `spells.cpp`;
  `rules_catalog.cpp`, `ContentId`, `DiceRoller`; `tests/test_core.cpp`.
- **Tests still required:** integration with actual character/source qualification
  after Q08; later phase timer/profile/treatment/persistence tests. Known explicit
  qualifying/nonqualifying save contexts already pass regression tests.

Phase 1 should not replace generic d4 with universal death. Its first deliverable
is the save/death foundation, preserving the path for later source-specific
activation once clock/persistence/treatment prerequisites exist.

## 7. Planned Development

See the [implementation plan](POISON-IMPLEMENTATION-PLAN.md) for full criteria.

1. **Saves/death:** Expert class-level tables, source modifiers, common death seam
   and tests implemented locally; automatic qualification integration awaits Q08.
2. **Clock:** explicit rounds/turns, no render-time advances, encounter accounting;
   depends on Phase 1 death boundary and Q02/Q06.
3. **Profiles:** explicit ContentId/catalog poison/delivery definitions; depends
   on Phase 1 save context and Phase 2 units.
4. **Persistence/treatment:** saved pending state, validated version extension,
   Neutralize Poison/revival; depends on 1-3 and repeat-exposure/treatment choices.
5. **Present monsters:** faithful per-species activation; depends on 1-4 and the
   applicable source/delivery questions. No silent catalog expansion.
6. **Traps/integration:** shared poison consequences and full-loop tests; depends
   on 1-5 and approved custom dart delivery.

**Approved:** original rules direction, Phase 1 implementation, and subsequent
frontend-adaptability review (DEC-WORK-001).

**Proposed, not yet approved to execute:** frontend changes and later poison
phases' concrete clock/state/wire-format designs.

**Exact next action:** review/playtest the AC correction and compatibility, then
package when authorized. Return to Q08 before full Phase 1 sign-off and scope
action-owned time for Phase 2/non-text frontend support afterward.

## 8. Known Issues and Technical Debt

| Item | Evidence / next handling |
|---|---|
| Current poison gives extra d4, including incorrect centipede physical damage | Audit P01/P04; approved direction is replacement with source-specific behavior, not a house-rule toggle |
| Name matching misses Bee/Medusa and cannot represent delays/delivery | Audit P02/P03/P05; poison phases 3-5 |
| Expert saves and protection | P06 fixed locally with Expert X24 bands; crab +2 and explicit context tested. P07 automatic qualification remains Q08 because current heroes lack alignment. |
| Poison death notification | P08 fixed locally by common cause-aware/idempotent death handling; no death-cause disk persistence until Phase 4 |
| Render-driven clock; incompatible existing duration units | Audit §7; phase 2 before delayed-poison activation |
| No poison persistence, treatment, cause-aware revival | Audit §§7-8, P10; phase 4; old condition carry-over is not real saved-state support |
| PoisonDart has documented extra-damage approximation | Audit §5/P09; consequence must become original lethal poison; exact dart delivery remains Q07 |
| Purify Food and Water currently heals HP | Audit P11; supplies purification is not victim venom treatment; review the smallest correction scope |
| Source/codegen drift and duplicate/malformed catalog rows | Audit §3; curated compiled table is runtime authority; leave regeneration OFF and avoid unrelated data loss |
| Test gaps | New tests now cover actual poison damage/death and actual PoisonDart activation/attribution. Delayed/profile/treatment/save-state tests remain future work; audit results describe the earlier baseline. |
| Historical docs can be stale | ROADMAP's description-audit priority, old phase save versions/counts, thief checklist assumptions; consult actual code, not old text as current proof |
| Compatibility | Local AC fix uses v8; v1-v7 load by equipment/DEX reconstruction, unknown legacy armor rejects without mutation, older executables reject v8. Future poison persistence must use the next available version (candidate v9). Never silently downgrade saves. |

Unresolved plan questions, with no invented answers:

- **Q01:** multiple exposures, deadline combination, neutralization scope.
- **Q02:** expiry/treatment ordering, encounter recovery and action time policy.
- **Q03:** Expert revival-window anchor and solo/NPC treatment lifecycle.
- **Q04:** magical-poison/source-aware save modifiers.
- **Q05:** grid range, distinct deliveries, queen/onlooker representation, scope
  of gaze/immunity integration without adding absent monsters.
- **Q06:** town/rest/travel durations for multi-day illness and deadlines.
- **Q07:** custom PoisonDart activation/hit/base damage and compatibility.
- **Q08:** engine-owned character/source alignment or equivalent qualification
  input for Protection from Evil; selection/preservation across frontends and
  saves. Source modifiers work, but no alignment information is guessed.

These are elaborated and dependency-gated in the plan. Basic-only versus faithful
B/X poison lethality is **not** still an open choice. Use Expert progression as
requested, preserving source distinctions documented in the audit.

Deferred: house-rule overrides, new monsters, generic status framework, broad
catalog/codegen cleanup, new dependencies, and unrelated gameplay rebalancing.

## 9. Validation and Test History

### Historical engine validation — audit session, revision `afa59d6`

The following commands **were executed in the preceding audit investigation**,
from the repository root, and are recorded in audit §9:

```sh
cmake --build build -j 2
./build/tests/bx_crawl_tests
ctest --test-dir build --output-on-failure
cmake --build build-sanitize -j 2
ctest --test-dir build-sanitize --output-on-failure
```

- Debug build passed; direct doctest: **228 cases / 15265 assertions, all passed**.
- Debug CTest: **1/1 passed**, reported total 2.84 seconds.
- ASan+UBSan build passed; sanitizer CTest: **1/1 passed**, 15.93 seconds; no
  sanitizer errors reported. Do not invent a separate direct sanitizer-doctest
  count/run that was not performed in this audit.
- No compiler warnings shown. Direct root-launched tests emitted existing
  `Warning: failed to load descriptions.txt` messages. Do not call that run
  runtime-warning-free or interpret it as a poison failure.
- Both caches had `BX_REGENERATE_DATA=OFF`; sanitizer cache had
  `BX_ENABLE_SANITIZERS=ON`. No data generation was run.
- No poison-specific interactive playtest was performed during the audit or this
  documentation task. Source tracing is not a claim that every monster was
  manually exercised.

### Historical planning/documentation session — 2026-10-28 record

Executed Git status/HEAD/history inspection, the open-PR query, UTC record-date
check, `git diff --check`, and separate `git diff --no-index --check /dev/null`
checks for the new documents. During authorized packaging, the audit's two
Markdown hard-break lines were normalized to blank lines, and
`git diff --cached --check` passed for all four staged documentation files.
The package commit/push succeeded, followed by this handoff reconciliation.
Documentation review only; no engine/test/data edits or new engine test run.
These checks do not validate any planned implementation phase.

### Historical Phase 1 validation — 2026-11-11 UTC

Executed from the repository root:

```sh
cmake --build build -j 2
./build/tests/bx_crawl_tests
ctest --test-dir build --output-on-failure
cmake --build build-sanitize -j 2
ctest --test-dir build-sanitize --output-on-failure
```

- Debug build passed, no compiler warnings shown; direct doctest **236/236 cases,
  15919/15919 assertions** passed. Existing description-load warnings remain.
- Debug CTest **1/1 passed**, 2.83 seconds; ASan+UBSan build/CTest **1/1 passed**,
  15.77 seconds, no sanitizer errors reported.
- `printf 'x' | timeout 30 env BX_LOAD_SAVE=Hrothgar ./build/frontends/text/bx_crawl_text >/dev/null`
  exited 0.
- `printf 'x' | timeout 30 env BX_LOAD_SAVE='Tomb Runner' ./build/frontends/text/bx_crawl_text >/dev/null`
  exited 0, with the existing description-load warning. No save command was used.
- Initial new-test compilation used an unsupported DiceRoller equality operator;
  corrected the test to compare the deterministic stream without modifying the
  RNG implementation. The final commands above passed.
- No interactive poison rescue, alignment-selection, GUI, Windows, WebAssembly,
  or new frontend test was performed. Review findings are source inspection,
  not proof those deployments work. Phase 1's Q08 gate is not cleared by tests.
- Expert table source image X24 was rechecked for implementation transcription;
  the prior audit was not repeated. No data generator or dependency change.

### Historical shield action validation — 2026-11-19 UTC

Executed `cmake --build build -j 2`,
`ctest --test-dir build --output-on-failure`, and `git diff --check`.
Build passed without compiler warnings shown; Debug CTest passed **1/1**,
2.68 seconds. The new regression tests removal through camp/character-sheet
commands, retained equipment, unarmed removal, repeat removal, and re-equipping.
No new direct-doctest count or sanitizer run is claimed for this small fix.
The frontend key binding/menu was inspected; interactive user verification of
`b` -> `2` -> `5` after restarting remains pending. No source changes followed
the successful build/test run; only this handoff was updated.

### Current AC-consistency validation — 2026-11-21 UTC

Executed `cmake --build build -j 2`, `./build/tests/bx_crawl_tests`,
`ctest --test-dir build --output-on-failure`,
`cmake --build build-sanitize -j 2`, and
`ctest --test-dir build-sanitize --output-on-failure`.

- Debug **244/244 cases / 16251 assertions** passed; CTest **1/1**, 0.72 seconds.
- Sanitizer build initially hit the 120-second tool timeout during test
  compilation; resumed with 300 seconds and passed. Sanitizer CTest **1/1**,
  5.18 seconds; no sanitizer errors. Cache checked ON, regeneration OFF.
- Existing tests that asserted descending runtime AC were updated to the
  explicitly corrected convention. Neutral test heroes now use STR/DEX 10.
- A Python subprocess smoke ran the text binary with isolated HOME and
  BX_LOAD_SAVE=Hrothgar, input `b25ttx`, and asserted ascending character AC
  **15 -> 14**, shield removal log, and exit 0. No save command or template
  regeneration was run. Existing root-test description warnings remain.
- Manual user playtesting of longer combat is still recommended; this does not
  certify attack-matrix/spell-rule fidelity beyond the AC-convention scope.

### Portable validation recipe — suggested, not newly executed results

On a new machine, first inspect actual checkout and build settings. Then, for an
authorized implementation slice, configure explicitly as needed:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBX_REGENERATE_DATA=OFF
cmake --build build -j 2
./build/tests/bx_crawl_tests
ctest --test-dir build --output-on-failure
cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DBX_ENABLE_SANITIZERS=ON -DBX_REGENERATE_DATA=OFF
cmake --build build-sanitize -j 2
ctest --test-dir build-sanitize --output-on-failure
git diff --check
```

Build directories/binaries are local artifacts, not cross-machine evidence.
ASan+UBSan support and frontend POSIX requirements follow the README/CMake
configuration. Seeded RNG tests should be deterministic within the tested
toolchain; do not assume library-dependent distribution sequences are portable
golden files. Record failures, missing tools/assets, and test commands skipped
with reasons; never silently convert them into passes.

Historical manual smoke examples are in the phase-30 document. They rely on
available pregenerated saves and correct input flow; do not rerun them against a
user's only campaign file or claim they test poison treatment. Future manual
poison scenarios need isolated saves and the acceptance criteria for that phase.

## 10. Development Session History

Keep entries concise; dates are actual record dates or explicitly unknown, not
inferred from commit timestamps or a previous machine's clock.

| Date | Objective / changes | Commit / validation | Decisions / outstanding / next |
|---|---|---|---|
| Earlier audit session; separate date not recorded in report | Read-only poison rules/engine audit; local audit document created | Base `afa59d6`; audit itself uncommitted. Engine validation actually executed as detailed in §9 | Identified P01-P11 and clock/persistence/treatment gaps. Follow-up was owner rules decision and implementation planning. |
| 2026-10-28 UTC — continuity record date | Review audit/README/ROADMAP/ADRs/test docs; create six-phase plan, continuity system, README entry point | Base still `afa59d6`; all new docs local/uncommitted. Git/doc checks only; engine tests not repeated | Record DEC-POISON-001..005 from owner directive; Q01-Q07 remain open. Next: owner review and bounded Phase 1 authorization. |
| 2026-10-28 UTC — authorized packaging | Create `feat/poison-audit-and-development-continuity`; commit/push audit, plan, continuity and README; reconcile handoff in a follow-up | Package `a62a579` committed and pushed; staged whitespace check passed; no new engine test run | No new gameplay decisions, no PR/merge. Next: owner review of documentation and bounded Phase 1 approval. |
| 2026-10-28 UTC — post-merge handoff | Verify owner-merged PR #67, fast-forward local `main`, delete merged branch locally/remotely, update this handoff locally | Merge `4198886`; Git status and whitespace checks; no engine tests rerun for documentation-only sync | No implementation phase started. Next: authorize bounded Phase 1 scope; this handoff edit remains uncommitted. |
| 2026-10-28 UTC — Phase 1 branch preparation | Create `feat/poison-phase-1-saves-and-death` from `4198886`; preserve and update local continuity handoff with confirmed merge and baseline | `gh pr view 67`, Git status/HEAD/history, baseline file-list diff and whitespace check; no new engine tests | Branch/handoff only authorized. No code, tests, data, saves, commit, push, or PR changes. Next: approve bounded Phase 1 implementation scope. |
| 2026-10-28 UTC — Phase 1 handoff packaging | Owner requests commit/push of the continuity update on the new branch | Documentation-only; commit subject `Record merged baseline for poison Phase 1`, parent `4198886`; confirm resulting SHA/upstream through Git | No implementation authorization or new rules decisions. Next: approve bounded Phase 1 implementation scope. |
| 2026-11-11 UTC — Phase 1 implementation and frontend review | Owner authorizes Phase 1 then non-text engine review; local Expert saves/context/crab +2, shared death/cause/snapshot/load reset and tests; frontend review; roadmap priorities updated | Branch `feat/poison-expert-saves-and-death`, base `29128b1`; 236 cases / 15919 assertions; Debug/sanitizer CTest and two boot smokes passed; no commit/push | DEC-WORK-001 records ordering and human-assisted description backlog. Q08 alignment/source qualification discovered as remaining integration gate. Next: review slice, resolve Q08, then scope clock work. |
| 2026-11-11 UTC — authorized Phase 1 packaging | Owner requests commit/push of the tested foundation and frontend review | Commit subject `Implement Expert saves and shared hero death handling`, parent `29128b1`; verify resulting SHA/upstream with Git. Whitespace checks; no application changes after the recorded tests | No new rules decisions, PR creation, or merge authorized. Q08 remains open; full Phase 1 sign-off is not claimed. |
| 2026-11-19 UTC — shield playtest bug | Owner reports inability to unequip shield and asks for explicit removal; add dedicated command/helper, key `5` in armor/shield menu, regression test and handoff | Branch `fix/shield-unequip`, base `5e52301`; Debug build and CTest 1/1 passed; no commit/push | Distinct unequip action, not toggle. Next: restart rebuilt game, check `b` -> `2` -> `5`, package when authorized; Q08 remains paused. |
| 2026-11-19 UTC — authorized shield packaging | Owner requests commit/push of the tested shield fix | Commit subject `Add explicit shield unequip action`, parent `5e52301`; confirm resulting revision/upstream with Git. No source changes after the passing tests | No PR/merge requested. Interactive user confirmation remains pending; Q08 remains open. |
| 2026-11-21 UTC — AC playtest correction | Owner reports no AC change after shield removal, then selects full ascending-AC consistency fix; update data-boundary conversion, shared defenses, equipment paths, v8 migration, tests and docs | Branch `fix/armor-class-consistency`, base `d8d7f96`; 244 cases / 16251 assertions; Debug/sanitizer CTest and isolated Hrothgar AC smoke passed; no commit/push | DEC-AC-001 records scope. Next: owner review/playtest and packaging authorization; future poison save version must not reuse v8. |
| AC packaging — after validation | Owner requests staging, commit and push to origin for playtesting | Commit subject `Fix ascending armor class and shield defense`, parent `d8d7f96`; resolve final SHA/upstream with Git. No behavior changes after the passing tests | No PR/merge requested. Next: owner playtest with v8-compatible build; Q08 remains open. |

For later sessions append a short row with actual date, objective, changed files
or linked milestone, exact commits if any, executed tests/results, decisions,
outstanding problems, and exact next action. Update sections 4/6/7/9 in place so
the reader need not reconstruct current status from this history table.

## 11. Resume Instructions

### Instructions for the Next OpenCode Session

1. Read **this continuity document first**.
2. Read the current [implementation plan](POISON-IMPLEMENTATION-PLAN.md).
3. Consult the [audit](POISON-MECHANICS-AUDIT.md) for supporting facts and sources;
   do not re-run completed investigations without a specific changed fact or gap.
4. Inspect `git status --short --branch`, `git rev-parse HEAD`, recent commits,
   local diffs/untracked files, and relevant PR state. On another machine fetch
   and reconcile remote history appropriately before choosing a base. Do not
   overwrite another session's uncommitted work or force-push divergent history.
5. Reconcile this snapshot with actual repository state. The documentation
   package/handoff and Phase 1 foundation are merged into `main` through PRs
   #67/#68/#69 and shield action #70. AC-consistency work was local at validation
   capture and subsequently authorized for packaging on `fix/armor-class-consistency`.
   Check remote availability and fetch its checkpoint on another machine;
   preserve newer local edits and reconcile Git status.
6. Identify the active phase and its unmet acceptance criteria. Currently this
   is the full AC-convention/shield playtest fix. Phase 1 qualification is still
   gated by Q08; the six-phase poison implementation is not complete.
7. Identify the next approved scope: review/playtest the AC fix; commit/push
   requires authorization. Resume Q08 afterward, not automatic Phase 2 coding.
8. Avoid repeating completed work. Re-run relevant tests when source/toolchain
   changes justify validation, not to imply the original audit never happened.
9. Preserve DEC-POISON-001..005. Report discrepancies and resolve open details;
   do not weaken original lethality or invent answers to Q01-Q08.
10. Resume only approved scope. The owner merges PRs. Commit/push/PR operations
    require authorization; do not assume a successful test run authorizes them.

Do not rely on this session's `/tmp/opencode` files, absolute workspace paths,
build artifacts, chat memory, or a particular AI model. Source references and
needed decisions must live in versioned documentation. If code and documents
disagree, investigate and report before updating the handoff; neither silently
discard evidence nor use continuity to override source-code facts.

## Continuity Update Policy

Update this document after completing a phase, making an architectural decision,
resolving a significant defect, changing the plan, adding a dependency, changing
a persistence format, discovering an important technical limitation, and ending
a substantial session. Also reconcile the current snapshot after an authorized
commit, push, merge, or branch transition.

Make targeted edits: keep current state/active work/next action prominent;
append accepted decisions and validated milestones without rewriting their
history. Retain replaced decisions with a superseding reference. Use actual
dates when known; explicitly label unknown approval dates. Link substantial
details to phase docs. Keep historical test results attached to their revisions,
not promoted to a claim about later untested changes.

To make continuity portable, include the updated documents and their audit
dependencies in authorized commits. If a session stops before committing, name
every local file and state plainly that another machine will not have it yet.
After commit, record the confirmed code/docs revision or a stable commit subject
and parent reference; do not fabricate a self-referential SHA for the commit
that is still being created.

## End-of-Session Handoff Protocol

At the end of each future implementation session:

1. Run relevant tests and required checks. For documentation-only work, state
   explicitly why engine tests were not rerun and what doc checks were performed.
2. Review the full repository diff, staged state, and untracked files.
3. Identify changed files and distinguish pre-existing work from this session.
4. Record completed work against acceptance criteria; partial code is not a
   completed phase, and passing tests alone do not prove all criteria.
5. Record outstanding work, blockers, skipped validation, and failures.
6. Record new decisions with approval evidence/status; label proposals clearly.
7. Update this continuity document's current state, active phase, history, tests,
   limitations, and next action; update the plan only if its scope changed.
8. Distinguish committed/pushed/merged state from local edits using Git evidence.
   An approved commit may include the handoff; refresh its snapshot after Git
   actions instead of claiming a future commit already exists.
9. Give a concise session summary with files, validation, commit/PR if applicable,
   limitations, and exact remaining work.
10. Identify the **exact next approved development action** or the specific
    approval needed before further changes.

Current handoff packages the validated full AC-convention fix, including save-v8
semantics and legacy migration, for the owner-requested commit/push. No PR/merge,
source monster-data change, template regeneration or build-configuration change
is part of this slice. Preserve the outstanding poison qualification gate afterward.
