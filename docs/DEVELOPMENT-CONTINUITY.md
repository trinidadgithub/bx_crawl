# Development Continuity

**Start here for every new OpenCode session.** This is the operational handoff
across machines, sessions, models, and interruptions. Update current status and
next action in place; preserve accepted decisions and validated milestones.

**Last reconciled:** 2026-10-28 (UTC).

**Active work:** review of the committed poison audit/planning package; no poison
implementation phase started.

**Next approved action:** review this documentation and obtain approval for the
bounded Poison Phase 1 implementation scope. Do not implement yet.

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

**Preservation rule:** do not reinterpret or repeatedly request baseline-lethality
approval in later sessions. The audit's former baseline/house-rule approval
questions are historical, not current blockers. Unsettled details in section 8
remain unsettled. New decisions require actual evidence of owner approval or
existing accepted rules; proposals must be labelled Proposed, with unknown dates
left unknown. Amend decisions through a recorded superseding decision, not by
silently overwriting their history.

## 4. Current Repository State

Snapshot verified after the authorized documentation commit and push; re-check
it at every resume. This handoff-only follow-up records the confirmed package
revision below rather than inventing its own future commit SHA.

| Item | Verified state |
|---|---|
| Current branch | `feat/poison-audit-and-development-continuity` |
| Confirmed documentation revision | `a62a5799146f92decfeeaed273252b0e2309d186` — `Document poison audit and implementation continuity` |
| Gameplay baseline / parent | `afa59d66866b6c7c666a5e7b74771f3c6c4d2705` — merge PR #66; application source remains unchanged |
| Tracking | Documentation branch pushed with upstream `origin/feat/poison-audit-and-development-continuity`. No changes merged into `main` by this session. |
| Open PR query | `gh pr list --state open --json number,title,headRefName,baseRefName,url` returned `[]` during this session. Not a permanent statement about GitHub. |
| Committed poison implementation | Still generic extra-d4 mechanics from the audited revision; none of the six poison implementation phases started. |
| Audit dependency | The previously untracked audit is now committed with the plan, continuity file, and README in `a62a579`; it can travel with the branch across machines. |
| Working tree at verification | Clean immediately after the package commit. This subsequent continuity/plan handoff update is documentation-only; inspect Git for its final commit and any newer local work. |
| Commit/push/PR | Owner authorized branch creation, commit, and push; `a62a579` and the remote branch were confirmed. No PR was created and no merge was performed. This follow-up updates the handoff; its final revision is discoverable with `git log`. |
| Latest engine validation | Audit-session validation against `afa59d6`: Debug build, direct doctest 228/228 cases and 15265/15265 assertions, Debug CTest 1/1, sanitizer build and CTest 1/1 passed. Details in §9; not rerun for this documentation-only task. |
| Save format | v7; no poison deadline/cause/clock extension implemented. v8 in the plan is a candidate, not current fact. |

Limitations inherited from the gameplay baseline: missing source-specific poison outcomes,
Killer Bee/Medusa poison activation, delayed state, Expert level-based saves,
crab +2, qualifying protection modifiers, Neutralize Poison, and poison-death
sound. Clock advancement is coupled to frame preparation. Some conditions are
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

Do not promote any poison phase to Completed until its plan acceptance criteria,
tests, relevant manual checks, and remaining caveats are reconciled. Record a
partially complete slice separately, even after its PR merges.

## 6. Active Development

**Active stage:** documentation planning/handoff, not Poison Phase 1 coding.

- **Objective:** make the verified audit and approved decisions durable, then
  prepare a bounded saving-throw/death-handling slice for review.
- **Committed and pushed:** audit, plan, continuity record, README entry point
  in `a62a579`. This handoff follow-up records the confirmed packaging result.
- **Remaining now:** owner reviews the branch/documentation, optionally through
  a PR, and approves a bounded implementation slice. Do not merge or begin coding
  as part of the packaging request.
- **Implementation blockers:** scope approval for Phase 1; context/interface
  choices required for qualifying modifiers. Later phase blockers are Q01-Q07,
  not a reason to re-audit all poison or change DEC-POISON-001..005.
- **Relevant source entry points for later work:** `game.cpp` `make_save`, spawn
  and `monsters_turn`; `combat.cpp` `attack_internal`; `exploration.cpp` trap/time;
  `game_session.cpp` action/death/frame boundaries; `save.cpp`; `spells.cpp`;
  `rules_catalog.cpp`, `ContentId`, `DiceRoller`; `tests/test_core.cpp`.
- **Tests still required for implementation:** all new Phase 1 save-boundary,
  source-modifier and death-parity tests. Existing passing suites do not cover
  direct poison behavior; no proposed poison regression test has been added.

Phase 1 should not replace generic d4 with universal death. Its first deliverable
is the save/death foundation, preserving the path for later source-specific
activation once clock/persistence/treatment prerequisites exist.

## 7. Planned Development

See the [implementation plan](POISON-IMPLEMENTATION-PLAN.md) for full criteria.

1. **Saves/death:** Expert class-level tables, source modifiers, common death seam
   and tests. Depends on scope review and applicable source-context choices.
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

**Approved:** original rules direction and documentation preparation.

**Proposed, not yet approved to execute:** the implementation slices and concrete
clock/state/wire-format designs.

**Exact next action:** review this package with the owner and obtain approval for
a narrowly scoped Poison Phase 1 branch/PR. Stop before code edits until that
approval. No further investigation of settled audit facts is needed unless code
or sources changed or a specific discrepancy is found.

## 8. Known Issues and Technical Debt

| Item | Evidence / next handling |
|---|---|
| Current poison gives extra d4, including incorrect centipede physical damage | Audit P01/P04; approved direction is replacement with source-specific behavior, not a house-rule toggle |
| Name matching misses Bee/Medusa and cannot represent delays/delivery | Audit P02/P03/P05; poison phases 3-5 |
| Fixed class saves, missing qualifying protection/save context | Audit P06/P07; phase 1; Expert low-level demihuman 8 is intentional source selection, not a typo for 10 |
| Poison HP death omits PlayerDeath sound | Audit P08; phase 1 death parity; normal session termination does work |
| Render-driven clock; incompatible existing duration units | Audit §7; phase 2 before delayed-poison activation |
| No poison persistence, treatment, cause-aware revival | Audit §§7-8, P10; phase 4; old condition carry-over is not real saved-state support |
| PoisonDart has documented extra-damage approximation | Audit §5/P09; consequence must become original lethal poison; exact dart delivery remains Q07 |
| Purify Food and Water currently heals HP | Audit P11; supplies purification is not victim venom treatment; review the smallest correction scope |
| Source/codegen drift and duplicate/malformed catalog rows | Audit §3; curated compiled table is runtime authority; leave regeneration OFF and avoid unrelated data loss |
| Test gaps | Audit §9: no direct monster-poison assertions; module PoisonDart test places dart but walks onto pit; new phase tests required |
| Historical docs can be stale | ROADMAP's description-audit priority, old phase save versions/counts, thief checklist assumptions; consult actual code, not old text as current proof |
| Compatibility | Candidate v8 must preserve v1-v7 readability and failed-load non-mutation; old executables may reject new saves; never silently downgrade them |

Unresolved plan questions, with no invented answers:

- **Q01:** multiple exposures, deadline combination, neutralization scope.
- **Q02:** expiry/treatment ordering, encounter recovery and action time policy.
- **Q03:** Expert revival-window anchor and solo/NPC treatment lifecycle.
- **Q04:** magical-poison/source-aware save modifiers.
- **Q05:** grid range, distinct deliveries, queen/onlooker representation, scope
  of gaze/immunity integration without adding absent monsters.
- **Q06:** town/rest/travel durations for multi-day illness and deadlines.
- **Q07:** custom PoisonDart activation/hit/base damage and compatibility.

These are elaborated and dependency-gated in the plan. Basic-only versus faithful
B/X poison lethality is **not** still an open choice. Use Expert progression as
requested, preserving source distinctions documented in the audit.

Deferred: house-rule overrides, new monsters, generic status framework, broad
catalog/codegen cleanup, new dependencies, and unrelated gameplay rebalancing.

## 9. Validation and Test History

### Last executed engine validation — audit session, revision `afa59d6`

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

### This planning/documentation session

Executed Git status/HEAD/history inspection, the open-PR query, UTC record-date
check, `git diff --check`, and separate `git diff --no-index --check /dev/null`
checks for the new documents. During authorized packaging, the audit's two
Markdown hard-break lines were normalized to blank lines, and
`git diff --cached --check` passed for all four staged documentation files.
The package commit/push succeeded, followed by this handoff reconciliation.
Documentation review only; no engine/test/data edits or new engine test run.
These checks do not validate any planned implementation phase.

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
   package is committed/pushed on the feature branch, not merged into `main` by
   this session. Fetch/check out the branch or its later merged successor on a
   new machine. Preserve any newer uncommitted work.
6. Identify the active phase and its unmet acceptance criteria. Currently this
   is documentation review, **not implemented Phase 1**.
7. Identify the next **approved** action. Currently seek scope approval for a
   bounded Phase 1 save/death slice; the plan is not permission to execute all six.
8. Avoid repeating completed work. Re-run relevant tests when source/toolchain
   changes justify validation, not to imply the original audit never happened.
9. Preserve DEC-POISON-001..005. Report discrepancies and resolve open details;
   do not weaken original lethality or invent answers to Q01-Q07.
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

Work stops at the documentation handoff. The separately authorized packaging
committed and pushed the documentation branch; no gameplay implementation,
source-data edits, save-format changes, build configuration edits, PR creation,
or merge was performed.
