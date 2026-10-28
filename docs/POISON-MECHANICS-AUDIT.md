# Poison Mechanics Rules-Fidelity Audit

Repository: `trinidadgithub/bx_crawl`

Audited revision: `afa59d6` (PR #66)

Scope: read-only gameplay investigation; this report is the only repository change.

## 1. Executive Summary

**Poison is not currently faithful to original B/X.** The engine implements an
immediate, HP-based approximation: a qualifying monster hits, ordinary damage is
applied, and a failed poison save adds 1d4 damage. Poisonous dart traps use the
same extra-damage idea. This does not implement the general save-or-die rule or
the distinct delayed and nonfatal effects in Moldvay's monster descriptions.

Important qualifications:

- The existing behavior is a real, established simplification, not evidence of a
  newly introduced regression. No explicit approval of it as a house rule was
  found. Its original intent cannot be established from code alone.
- The current monster-name predicate marks exactly **five** of the 105 compiled
  monster rows poisonous: Giant Centipede, Cobra, Pit Viper, Crab Spider, and
  Tarantella. It misses **Killer Bee and Medusa**, whose attack text explicitly
  includes poison. Several requested monsters, including Giant Scorpion and
  Black Widow, are **absent from the compiled catalog**.
- No pending victim-side poison, poison death deadline, poison immunity system,
  or Neutralize Poison implementation was found.
- The poison saving-throw category and roll direction are correct. Class-based
  poison targets match the Expert low-level table, but level progression and
  source-specific bonuses are missing. Basic and Expert actually differ in their
  low-level dwarf/halfling tables; the engine's 8 is not simply a transcription
  error for Basic's 10.
- Poison-induced HP death sets `hp = 0` and `alive = false`, and the ordinary
  session/frontend flow stops play. It omits the normal combat PlayerDeath sound.
- Existing tests pass; they do not establish poison fidelity.

Do not just replace `1d4` with unconditional death: that would make Giant
Centipede and Tarantella wrong in a different way, miss crab-spider delay/save
bonus, and leave the untreated persistence and treatment problems intact.

## 2. Verified B/X Rules

### Sources and provenance

Primary source A is the repository's 1981 Moldvay Basic PDF:
`dnd-data/dungestNdragonsBasicBox.pdf`. The searchable extraction is
`dnd-data/dungestNdragonsBasicBox.md`. Relevant printed pages were cross-checked
against the PDF text rather than assuming `monster_sources.txt` is authoritative.

Primary source B is the 1981 Expert rulebook edited by David Cook with Steve
Marsh. The inspected scan identifies those editors, copyright 1981, and first
printing January 1981:

- [Internet Archive item](https://archive.org/details/dn-d-expert-rules-cook)
- [OCR text](https://archive.org/download/dn-d-expert-rules-cook/DnD_Expert_Rules_Cook_djvu.txt)
- [Searchable scan](https://archive.org/download/dn-d-expert-rules-cook/DnD_Expert_Rules_Cook_text.pdf)

Expert text was downloaded only into `/tmp/opencode/` for inspection. OCR
occasionally drops hyphens and table headings. Printed-page references are used
where established; otherwise the named section is cited without guessing a page.

**Edition boundaries:** Basic rules below are Moldvay's, not Mentzer/BECMI.
Expert findings are from the Cook/Marsh scan, not a later Expert reprint.
Old-School Essentials and later poison/antidote systems were not used as primary
evidence. No BECMI poison strength, modern poisoned-condition disadvantage,
damage-over-time assumption, or automatic mundane antidote is recommended here.
The engine's additional 1d4 is project behavior, not a B/X or OSE rule inferred
from its name.

### General delivery, saving throws, and death

- **Basic B26, Saving Throws:** when a poisonous attack hits, save vs. Poison or
  die unless neutralized. Ordinary damage plus poison are separate: a successful
  save avoids the poison, not the bite damage. Extraction lines 2643-2655.
- **Basic B29, Poison:** a failed save will *usually* kill. Specific monster
  descriptions supply exceptions; the word does not license replacing every
  poison with small damage. Extraction lines 3009-3012.
- **Basic B26:** roll a d20 equal to or above the saving-throw target. Death Ray
  or Poison is its own category, distinct from Paralysis/Turn to Stone.
- An attack's delivery must be resolved: a bite, sting, venom spit, inhaled gas,
  and poisoned drink are not interchangeable melee-hit effects. Basic B49's
  poison potion requires a save if any amount is swallowed, even a sip.
- Where no delay is specified, the operational default is death on resolving
  the failed save. Do not silently insert a universal grace period before death;
  Expert's revival treatment is a separate rule.

### Time terminology and explicit exceptions

**Basic B23, Time in Encounters:** a round is ten seconds; a turn is ten
minutes; sixty rounds make a turn. For dungeon accounting an encounter of sixty
rounds or less is counted as a full turn, including recovery. Extraction
2247-2262. A deadline of `1d4 turns` is not `1d4 commands` or `1d4 rounds`.

| Original creature/source | Verified poison rule |
|---|---|
| Giant Centipede, Basic B33 | Bite does **no damage**. Failed Poison save causes ten days of violent illness: half movement and no other physical action (3630-3634). |
| Killer Bee, Basic B37 | Successful sting kills the ordinary bee; victim saves vs. Poison or dies. Embedded stinger inflicts 1 HP per round until a round is spent removing it. Queen can sting repeatedly without dying (4365-4377). The stinger damage is distinct from the saving throw. |
| Medusa, Basic B39 | Snake bite deals 1d6 and requires Poison save or **death in one turn**; snake group attacks once per round (4745-4756). Gaze uses a different save. |
| Spitting Cobra, Basic B42 | Spit reaches six feet; a hit calls for Poison save or blindness. Bite deals 1d3 plus Poison save or **death in 1d10 turns**. May spit or bite in a round, not both; usually spits (5407-5416). |
| Pit Viper, Basic B42 | Bite requires Poison save or death; no delay stated. Always wins initiative (5417-5421). |
| Sea Snake, Basic B42 | Bite goes unnoticed 50% of the time. Failed Poison save has slow-acting full effects after **1d4+2 turns**. At that point 25% chance even Neutralize Poison cannot save the victim (5422-5432). Do not replace that wording with an invented treatment algorithm. |
| Giant Rattlesnake, Basic B42 | Failed Poison save kills in **1d6 turns**; second attack at end of each round (5433-5439). |
| Crab Spider, Basic B43 | Exact operative wording: "save vs. Poison or die in 1d4 turns"; weak poison grants **+2 to the saving throw roll** (5492-5499). This is lethal delayed poison, not paralysis. |
| Black Widow Spider, Basic B43 | Failed Poison save kills in **one turn** (5500-5507). |
| Tarantella, Basic B43 | **Nonfatal** bite poison: failed Poison save causes magical dancing for 2d6 turns; -4 to hit; attackers +4; exhaustion after five turns makes dancers helpless. Onlookers save vs. Spells or dance as long as the original victim. Magical poison treatment and Dispel Magic can stop the dance (5508-5520). |
| Giant Scorpion, Expert X39 | Two claws and sting. If a claw hits, sting attacks at +2; **sting**, not claws, requires Poison save or death. No delay stated. |

### Poison-adjacent effects and immunity

- **Basic B33, Cave Locust:** most poisons do not harm it. Its foul spit causes
  inability to act for one turn on a failed Poison save (3600-3617). This is not
  lethal venom just because the save is called Poison.
- **Basic B41, Robber Fly:** immune to killer-bee poison (5155-5156). Robber Fly
  is not in the current runtime catalog.
- **Basic B44, Troglodyte:** stench calls for Poison save; failure causes -2
  melee hit rolls, not death (5647-5654).
- **Basic B44, Undead:** unaffected by effects on living creatures such as
  poison (5655-5659). No universal racial or Constitution-based poison immunity
  for player characters was established. Class saving throws are not immunity.
- **Basic B34-B35, Dragons:** green-dragon chlorine gas and gold-dragon gas
  option use dragon breath rules: damage based on current dragon HP with a
  breath save for half, not a bite-triggered Poison save (dragon table and
  3820-3844, 3890-3895). Treat these separately from lethal venom.

### Treatment, repeated exposures, and modifiers

- **Expert, fourth-level clerical spells, Neutralize Poison:** touch/range zero,
  permanent result; cancels poison effects and can revive a poisoned character
  if cast within **ten rounds**. Can render poison or a poisoned item harmless.
  Acts on poison present when cast, not on future exposures. The inspected OCR
  text is at lines 1625-1632; this named section is the citation, not an assumed
  modern interpretation of "permanent" as future immunity.
- **Basic B12, Purify Food and Water:** makes poisoned/spoiled supplies usable;
  does not say it neutralizes venom already in a victim (1411-1415).
- **Basic B12, Cure Light Wounds:** HP healing or paralysis removal, not poison
  neutralization. Poison-specific treatment should not be conflated with HP
  recovery. Tarantella's Dispel Magic exception is explicitly monster-specific.
- **Basic B26:** each successful poisonous attack requires a save. Existing
  poisoning does not grant immunity to the next bite. The inspected text does
  not define a universal stacked-dose/deadline-reset policy; see section 11.
- **Basic B6, Wisdom:** magic-based save adjustments depend on whether the
  attack is magical; natural venom is not automatically a Wisdom-adjusted save
  (606). **Basic B12, Protection from Evil** gives +1 to saves against qualifying
  different-alignment attackers (1400-1410).
- **Expert X24, Saving Throws:** class/level bands improve saves. Dwarf/halfling
  level 1-3 Poison target is **8** in Expert, versus **10** in Basic B26. Other
  player classes' low-level Poison targets agree between these sources.

## 3. Current Implementation

### Runtime source of truth

1. `include/bx_data.hpp:351-473` is the compiled Monster struct/table: 105 rows,
   including duplicate and malformed legacy rows. This, not runtime JSON loading,
   supplies monster stat strings.
2. `dnd-data/data/monsters.json` has 101 rows. Relevant poison rows include
   Centipede (279), Killer Bee (638), Medusa (840), Cobra (1112),
   Pit Viper (1125), Crab Spider (1138), and Tarantella (1151). The compiled table
   is curated: `CMakeLists.txt:22-49` makes regeneration optional and OFF by
   default. Both inspected build caches use OFF. **No generator was run during
   this audit.** Do not assume regenerating the header is a safe fidelity fix.
3. `scripts/gen_data.py:294-326` generates monster rows from JSON; the example
   `dnd-data/schemas/05-monsters.json:1-73` contains only four sample monsters
   and no executable poison specification. `dnd-data/fix_monsters.py` is another
   source of hardcoded imported values, not a runtime rules interpreter.
4. `RulesCatalog::find_monster` wraps the compiled table
   (`engine/src/rules_catalog.cpp:105-107`). Module startup resolves a catalog
   name to a slot and uses the same spawn factory
   (`engine/src/game_session.cpp:199-215`). Procedural/test/wandering spawns
   also use `spawn_bx_monster_by_index` (`game.cpp:1023-1028,1284-1339`).
5. `Entity::has_poison` defaults false (`include/entity.hpp:132-145`). Spawn sets
   it from **name only**, via `game.cpp:847-854,1305`. The predicate searches
   lowercase `poison` or title-case `Spider`, `Cobra`, `Viper`, `Centipede`, or
   `Scorpion`. It never checks `m.attacks`, `m.damage`, or runtime descriptions.
6. `data/monster_descriptions.txt` and `monster_sources.txt` supply display text,
   not behavior. Tool poison flags *do* scan attack/damage strings
   (`tools/show_monster.cpp:153-165`, `tools/list_monsters.cpp:60-77`), so a
   displayed Poison flag is not proof the engine applies poison.

### Complete attack execution path

`GameSession::apply` -> initiative -> `monsters_turn` -> adjacent monster
`attack` -> `attack_internal` -> save -> state mutation -> session death check.

| Step | Actual behavior and evidence |
|---|---|
| Eligibility | `monsters_turn` skips dead/charmed/fleeing monsters and hidden heroes; orthogonal adjacency reaches `attack` (`game.cpp:1980-2061`). Spawns set one attack per turn, regardless of multi-part stat text (`1316`). |
| Attack roll | `combat.cpp:185-230`: d20 plus monster attack bonus and modifiers vs. hero AC including selected spell defenses. A miss does not call poison. |
| Ordinary damage | `249-258`: roll `attacker.damage_die` plus bonus, minimum 1, then subtract HP. `parse_damage_die` reduces multi-part strings to a single die; `Nil` becomes d6 (`game.cpp:672-696`). |
| Lethal ordinary damage | `267-300`: HP zero, dead, PlayerDeath sound, slain log. Poison is **not** processed: its branch is in the `else` for survivors. Suppressing a save after ordinary death is not itself a B/X fidelity defect; no live target remains. |
| Poison trigger | Surviving hero hit by non-hero with `has_poison` (`301-304`). No source immunity, bite/sting/spit distinction, save bonus, or ongoing-poison check. |
| Save | `make_save(g, g.hero.char_class, 0)` (`304`); category zero selects Death Ray or Poison. |
| Success | Logs `You resist the poison!`; bite damage remains (`305`). |
| Failure | Immediately rolls d4 and subtracts HP (`307-313`). No poison record is created. |
| Poison HP death | HP clamped to zero, `alive=false`, `You succumb to the poison!` log (`314-318`); no PlayerDeath sound here. |
| Termination/presentation | `monsters_turn` returns when hero is dead (`game.cpp:2059-2060`); session sets `hero_died`/`keep_playing` and frontend renders death (`game_session.cpp:87-110,127-132`; `frontends/text/src/main.cpp:527-538`). |

`roll_d` uses the existing `DiceRoller`/game RNG boundary (`game.cpp:27-29`,
`engine/src/dice.cpp:3-10`); future poison dice should continue to use it.

### Special-attack dispatch

`combat.cpp:303-365` is mutually exclusive: poison, else disease, else paralysis,
else vampire bat, else level drain, else strength drain, else rust, else slime.

**Confirmed behavior:** if an entity has poison and paralysis flags, even a
successful poison save suppresses the paralysis branch. **Not a confirmed
catalog bug:** the five currently poison-flagged rows do not also receive those
other special flags from the spawn predicates. Do not claim that this currently
suppresses Medusa's gaze: Medusa has neither the poison flag nor an implemented
gaze branch. Multiple-delivery monsters will need explicit attack resolution;
changing every `else if` to independent `if` statements would be unsafe.

### House-rule evidence versus intent

- `git log -S poison_dmg` locates introduction in `cd7ec17` and extraction into
  `combat.cpp` in `04b7f73`. The introduction commit claims B/X integration but
  does not justify reduced-damage poison as an approved house rule.
- Trap extra damage dates to `1975ed7`; the trap slice documents it explicitly
  (`docs/phase-25-traps.md:43-48`; `test-plans/thief-skills-playtest.md:79`).
- Thus call both **established simplified behavior inconsistent with the requested
  original-rules baseline**, not an accidental regression or proven deliberate
  balance policy. Approval to replace that behavior is still needed.

## 4. Monster Compliance Matrix

In the tables, **P** means Death Ray or Poison; **S** means Rods/Staves/Spells.
"+d4" means ordinary hit damage followed by immediate extra d4 on a failed P
save, not an ongoing condition. Counts refer to compiled monster rows, not prose
keys or enum names.

Current effects below are established by source/data execution tracing; the
existing suite was run, but each monster was not separately playtested. The
five-match predicate inventory was also evaluated against all 105 compiled
names using a read-only `git show`/Python pipeline.

### Existing runtime monsters with original venom

| Monster | B/X Poison Effect | Current Effect | Save | Timing | Assessment |
|---|---|---|---|---|---|
| Centipede, Giant (`bx_data.hpp:393`) | No bite HP damage; ten-day illness, half move/no other physical actions (B33) | `Nil` parsed as d6 bite, plus d4; no illness | P, no monster modifier | Immediate HP damage | Wrong ordinary damage **and** poison consequence. |
| Snake, Cobra (`457`) | Spit blindness; bite d3 and death in d10 turns (B42) | d6 bite plus d4; no spit/blindness | P for current bite; no spit save | Immediate | Poison recognized but delivery, damage, consequence, delay wrong. |
| Snake, Pit Viper (`458`) | Bite plus death on failed P save; no delay (B42) | d6 bite plus d4 | P | Immediate HP damage | Correct category/gating, wrong poison effect; special initiative absent. |
| Spider, Crab (`459`) | Bite d8; death in d4 turns, +2 on save (B43) | d8 bite plus d4 | P, **no +2** | Immediate | Wrong effect/timing; weak-venom modifier missing. |
| Spider, Tarantella (`460`) | Nonfatal dancing 2d6 turns; onlookers S; exhaustion five turns (B43) | d8 bite plus d4; can kill by HP loss | P; no onlooker S | Immediate | Not a capability false positive: it really has poison. Generic effect is wrong and may kill instead of causing dancing. |
| Killer Bee (`420`) | Failed P save kills; ordinary bee dies after sting; removable stinger damage (B37) | d4 sting **only**, `has_poison=false`; bee continues living | None for sting | Hit damage only | Confirmed name-based false negative; stinger/bee lifecycle missing. |
| Medusa (`436`) | d6 snake bite; death in one turn on failed P save; gaze separate (B39) | d6 ordinary attack **only**, `has_poison=false` | No poison/gaze save | Hit damage only | Confirmed false negative despite attack text `+ poison`; no delay. |

There are **no verified nonpoisonous-name false positives among the five current
predicate matches**. Their effects are misclassified, not their possession of
venom. A future name containing `Spider` would acquire the generic effect even
if its rules differed; that is a fragile design, not a demonstrated additional
current creature.

### Existing poison-adjacent creatures and effects

| Monster | B/X Poison Effect | Current Effect | Save | Timing | Assessment |
|---|---|---|---|---|---|
| Cave Locust (`392`) | Nauseating spit: one turn unable to act; immunity to most poisons (B33) | Single ordinary bite-die representation; no spit condition/immunity | No spit P | Immediate damage | Missing separate special attack; not an omitted lethal-venom flag. |
| Troglodyte (`466`) | Stench: P or -2 melee to hit (B44) | No stench branch; one generic attack | No stench P | No condition | Missing nonfatal save-based effect. |
| Dragon, Green (`398`), Dragon, Gold (`397`) | Chlorine gas breath, breath save for half damage (B34-B35) | First damage expression parsed as ordinary attack; no breath delivery branch | No breath save | Single hit damage | Breath gap, **not** a reason to add lethal `has_poison`. |
| Skeleton, Zombie, Ghoul (two rows), Wight (plus malformed row), Wraith, Mummy; also Shadow marked undead by engine | Genuine undead immune to poison (B44); Shadow is explicitly not undead (B42) | `undead` flag is not checked by poison branch; only hero defenders receive generic poison | Hero P only | No generalized immunity | Ordinary play has no monster-on-monster poisoning, so immunity failure is latent if target support expands. Shadow's undead classification is a separate confirmed metadata error, not evidence of poison immunity. |
| Sprite (`461`) | Basic B43 prose does not establish sleep-poison weapons | Runtime description claims sleep poison; compiled attacks have no poison and engine has no such delivery | None | None | Do not classify as a verified poisonous B/X monster merely from project prose. |

### Requested/source-known monsters absent from the runtime catalog

| Monster | B/X Poison Effect | Current Effect | Save | Timing | Assessment |
|---|---|---|---|---|---|
| Spider, Black Widow | Fatal bite (B43) | Absent in compiled table and source monster JSON | P | One turn | Catalog gap, not a running poison-branch instance. |
| Scorpion, Giant | Fatal sting; claws improve sting +2 (Expert X39) | Absent in table/JSON, though name predicate anticipates `Scorpion` | P on sting | No delay stated | Adding the name alone would incorrectly poison claw hits unless attack deliveries are separated. |
| Sea Snake | Slow-acting poison; bite detection and treatment exception (B42) | Absent table/JSON | P | d4+2 turns | Future exception; do not silently substitute default death. |
| Giant Rattlesnake | Fatal bite (B42) | Absent table/JSON; name also would not match current predicate | P | d6 turns | Future false negative if imported unchanged. |
| Rock Python | Constriction, not venom (B42) | Absent | No poison save | Repeated constriction | Explicit nonpoisonous snake comparison. |
| Spider, Giant / Spider, Phase | Generic/other-edition prose keys exist, but no compiled rows; not the three Moldvay spider entries | Not a current spawned poison monster | Not assigned here | Not assigned | Do not promote project prose to verified B/X rules. Legacy `EntityType::GiantSpider` alone does not set `has_poison`. |

Other Expert monsters with poison (for example Wyvern) are not silently treated
as implemented: no Wyvern row exists here. This inventory covers the entire
current 105-row runtime table and the requested missing examples, not every
monster in the Expert book.

## 5. Poison Trap Assessment

`TrapType::PoisonDart` exists in `include/map.hpp:28-33`. Procedural traps are
placed in `engine/src/map.cpp:250-280`; module `poison_dart` definitions map to
the same enum (`game_session.cpp:22-28,225-241` and
`module_content.cpp:488`). No separate poison-item/gas/potion runtime system was
found.

Actual `exploration.cpp:189-237` behavior:

1. Eligible hero steps onto trap; no dart hit roll is made.
2. `Trap` sound, base d4 damage roll, P save using `make_save` category zero.
3. Failure adds another d4; success leaves base d4. Damage is combined and then
   applied, even if the base component would already have been fatal.
4. Death sets zero HP/dead and emits PlayerDeath sound plus death log.
5. Trap is removed after activation. No pending poison survives.

`hero_move` triggers traps before completing movement/stair handling
(`exploration.cpp:399-427`); pole probing reveals/prevents stepping, and hidden
heroes bypass this activation check. These are existing trap-delivery policies,
not evidence for the original poison consequence.

**Rules comparison:** Basic B52's Sample Traps explicitly says poison gas and
poison needle: save vs. Poison or die (extraction 6573,6582). A specific
`PoisonDart` with base d4, automatic delivery, and an extra d4 is a project trap
definition, not an exact reproduced B52 stat block. Damage/attack-roll delivery
for that custom dart needs an explicit decision. If it is intended to carry
ordinary lethal B/X poison, its failed-save consequence is not faithful. Keep
that distinction rather than claiming B52 specifies a d4 dart and hit roll.

## 6. Saving Throw Assessment

`game.cpp:1056-1084` is the single authoritative `make_save`. Internal wrappers
in `combat.cpp:33-35`, `exploration.cpp:21-23`, and
`game_internal.hpp` delegate to it. Do not create another save implementation.

| Question | Finding |
|---|---|
| Category | Correct for natural venom and current poisonous traps: category 0 -> `death_ray_poison`. |
| Random/result | Uniform d20 via `roll_d`; roll >= target succeeds. |
| Class | Correct enum mapping for seven PC classes. `None`/unknown uses Fighter, not Basic's Normal Man row. |
| Level | `g.hero_level` is never read. One `SaveRow` per class (`bx_data.hpp:538-564`); schema level strings are not runtime level bands. |
| Low-level Poison targets | Cleric 11, Fighter 12, Magic-User 13, Thief 13, Dwarf 8, Elf 12, Halfling 8. These agree with Expert X24 level-1 bands; Basic B26 uses 10 for Dwarf/Halfling. |
| Higher-level targets | Wrong for Expert: e.g. level-10 Cleric still saves at 11 instead of 6. Other class bands also absent. |
| Source modifier | No bonus parameter or source context; crab spider's +2 cannot be expressed at the call site. |
| Temporary condition | Bat confusion subtracts 2 from all saves, correctly matching Basic's bat rule. |
| Wisdom | Added only for category 4. Correctly **not** applied to ordinary venom; insufficient context for magical-poison cases. Other magical categories' Wisdom behavior is an adjacent issue, not the natural-poison bug. |
| Protection | `protection_turns` gives combat AC elsewhere, but no qualifying +1 save adjustment is made here. Required alignment/source relationship is not passed. |
| Constitution/equipment | No general Constitution poison-save bonus is expected from the inspected B/X tables. No implemented poison-resistance item modifier found; do not import later-edition assumptions. |

Data sources differ: `dnd-data/data/saving-throws.json:1-11` retains Basic's
Dwarf/Halfling 10, while `dnd-data/schemas/07-saving-throws.json:41-67` uses 8,
which generates the compiled value (`gen_data.py:398-426`). Choose and document
Basic-only versus combined B/X/Expert tables; do not label either historical
value fabricated. The current generic table test intentionally pins Dwarf 8.

## 7. Timing and Persistence Assessment

### Current clocks and reusable pieces

| System | Advances when | Consequence for poison design |
|---|---|---|
| `GameSession::apply`, initiative/monster turns | Most dungeon commands; Inventory/Save/Quit skip the combat cycle (`game_session.cpp:63-135`) | Useful action boundary, not a complete world clock. |
| `tick_clock` / exploration turn | `prepare_dungeon_frame` calls `compute_visibility` **before reading the command** (`main.cpp:510-521`; `game_session.cpp:59-61`) | Even inventory/save/help/quit display loops may advance this counter despite not consuming a combat turn. Direct `apply` calls do not advance it automatically. |
| Exploration conversion | `ticks_per_exploration_turn = exploration_move_feet/10`, currently 12/9/6/3 ticks by encumbrance (`exploration.cpp:40-54,87-137`) | These are movement-based approximations, not sixty combat rounds. `game.hpp:70`'s "10 actions" comment is stale. Changing encumbrance changes the modulo threshold. |
| Paralysis | Applied as 2d4; decremented per blocked player command (`combat.cpp:338`; `game.cpp:2465-2490`) | Duration called turns but runs on actions. Do not reuse that decrement unchanged for crab poison. |
| Unconsciousness | d10, decremented per blocked command; vampire-bat blood drain in monster turn (`game.cpp:2041-2056,2475-2488`) | Round-like session flow only; no authoritative elapsed-round total. |
| Green slime | Persistent flag; d8 at start of eligible player commands until Scrape (`game.cpp:2493-2511,2551-2558`) | Reusable flag/display plumbing, not a delayed-event scheduler. Early incapacity returns bypass slime damage. |
| Oil blisters | 144 exploration-turn boundaries (`exploration.cpp:95-98`) | Closest duration example; inherits frame/encumbrance clock limitations. |
| Buffs | Decrement in `monsters_turn`, not uniformly across exploration (`game.cpp:1983-1987`) | Another incompatible timing unit. |

No victim-side poison field or deadline exists in the complete `Entity`, `Game`,
snapshot, campaign, spell, or disk-save paths examined. Searches for poison,
venom, neutralization, and antidote found only the immediate combat/trap paths,
display data, and documentation. There is no hidden delayed poison system.

**Crab-spider scenario:** current engine loses HP immediately and remembers
nothing once the hit is resolved. A character who leaves combat has no pending
death event. To support B43, record failed exposure and a d4-turn deadline in
authoritative hero/campaign state; advance it during combat **and** exploration,
allow intervening treatment, and keep it across encounter/level/location changes.
An empty monster list must not cancel the timer.

Minimum timing requirement is an elapsed-time function owned by the engine or
GameSession, not renderer calls: distinguish rounds, exploration turns, explicit
rest/travel, and non-time commands. Honor B23's encounter accounting without
charging both sixty rounds and the same encounter turn twice. A small
round-based time accumulator (60 per turn) plus explicit turn advances can serve
both units; exact encounter recovery/deadline boundary ordering needs approval.
Use deadlines or remaining duration plus a persisted partial-turn remainder,
not an unlabelled `poison_turns--` in the command switch.

### Disk persistence versus in-memory campaign capture

- Current disk format: `BXSV`, **version 7** (`save.cpp:46,122-126,498-501`).
  Versions 1-7 load with conditional defaults; v6 introduced module state and v7
  hero position. No poison state, poison cause of death, round clock, or
  `tick_clock` is written (`save.cpp:503-604`).
- Some ongoing effects **are** saved: shield/protection/courage/resist-cold and
  light counters (`529-536`). Paralysis, unconsciousness, slime, and blisters
  are **not** serialized. Therefore the answer is not "all effects are saved"
  or "none are saved."
- `load_game` starts from **a copy of current Game**, not a fresh zeroed one
  (`save.cpp:616-621`). Omitted counters can survive from the *receiving*
  session unless explicitly reset; a fresh process defaults them to zero. Do
  not mistake such carry-over for correct persistence from the saved character.
  Loader explicitly restores `alive=true` (`366-379`).
- `capture_campaign_state`/restore copy the full hero, RNG, tick clock, and
  monsters in memory (`campaign_state.cpp:3-59`). That can carry transient
  conditions in memory; it does not make them survive disk reload.
- Validation uses checked binary reads, bounded strings/counts, enum decoding,
  level bounds, and commit-on-success loading. Unsupported versions reject;
  trailing bytes after a valid v7 payload are tolerated (`save.cpp:381-384`).
  Writer flushes/closes/checks errors but writes directly to its destination;
  do not describe it as an atomic rename transaction.

For pending poison, prefer an explicit **v8 tail**: bounded source/effect IDs,
remaining duration or deadline/time base, and any neutralization/revival state.
Validate effect enum, counts, nonnegative timers and legal combinations. Older
saves must default explicitly to no poison and no revival deadline, even when
loading into an already-poisoned Game. Preserve current failure-no-mutation
guarantees. A v7 loader rejects v8 rather than migrating it backward; ignoring
an unknown tail cannot faithfully preserve a lethal condition. No format change
is made in this audit.

## 8. Death Handling Assessment

| Concern | Ordinary combat HP death | Monster poison HP death | PoisonDart death |
|---|---|---|---|
| `hp=0`, `alive=false` | Yes (`combat.cpp:267-269`) | Yes (`314-316`) | Yes (`exploration.cpp:228-230`) |
| Structured log event | Slain message | Succumb message | Death message |
| PlayerDeath sound event | Yes | **Missing** | Yes |
| Stop later adjacent attacks | Yes | Yes, `monsters_turn` checks alive after `attack` | Session catches dead hero after player action |
| `ActionResult.hero_died`, terminate normal loop | Yes | Yes in normal GameSession path | Yes in normal dungeon path |
| Dedicated typed death event | No such event type for any cause (`game_event.hpp:19-22`: LogMessage and Sound only) | Same absence | Same absence |
| Persist death cause/revival window | No | No | No |
| Automatic save/delete/permadeath enforcement | Not in inspected death frontend | Not in inspected death frontend | Same |

`log_event` delivers both legacy log text and structured LogMessage events
(`game.cpp:45-55`). Thus monster-poison death is **not entirely unreported**;
the missing event is specifically PlayerDeath **sound**, not a nonexistent
dedicated death domain event that ordinary combat already emits.

`GameState` has no Dead/GameOver enumerator (`game.hpp:12-21`). Normal game-over
handling relies on hero state plus ActionResult. Direct lower-level API callers
must check `alive`; they do not receive ActionResult automatically. Existing
saves remain on disk and the frontend does not auto-save/delete on death.
`save_game` does not serialize `alive` or reject all dead-state calls, and loader
forces `alive=true`; this is a general persistence limitation, not unique to
poison. A valid pre-death save remains reloadable.

Expert revival requires **cause-aware poison death** and an elapsed ten-round
treatment opportunity. Current frontend exits the session immediately. A solo
character with no available healer should not automatically revive, but an
approved party/NPC treatment model cannot work merely by adding a spell branch
after immediate session termination. Decide the support boundary first.

## 9. Existing Test Results and Coverage

Exact executed commands, from the repository root:

```sh
cmake --build build -j 2
./build/tests/bx_crawl_tests
ctest --test-dir build --output-on-failure
cmake --build build-sanitize -j 2
ctest --test-dir build-sanitize --output-on-failure
```

Results:

- Both builds succeeded with no compiler warnings shown. Sanitizer cache has
  `BX_ENABLE_SANITIZERS=ON`; both have `BX_REGENERATE_DATA=OFF`.
- Direct Debug doctest: **228/228 cases, 15265/15265 assertions**, no failures.
  It prints existing `Warning: failed to load descriptions.txt` messages when
  run from root; this is not a clean-no-runtime-warning claim.
- Debug CTest: **1/1 passed**, 2.84 seconds reported total.
- Sanitizer CTest: **1/1 passed**, 15.93 seconds; no ASan/UBSan errors reported.
- No gameplay code, data, test source, or save format was changed. No new test
  harness or generated data was introduced for the audit.

Relevant existing tests in `tests/test_core.cpp`:

| Coverage | Evidence / limitation |
|---|---|
| Save table data | `4589-4600`: seven rows, Fighter Poison 12, Dwarf Poison 8. Does not exercise live poison saves or level progression. |
| Poisonous monster attacks | **No direct `has_poison`/poison-outcome test found.** Search of the full test file for poison/Poison/has_poison returns table checks and module-trap references only. |
| Poison traps | `8092-8129` plants a PoisonDart and checks its type, then actually steps on a **pit**, not the PoisonDart. `7444-7456` checks module parsing. Neither proves failed/successful poison saves. |
| Ordinary hit/death | `3065-3123` checks generic hit/miss and monster death. |
| Session hero death | `6174-6215` checks monster-first lethal attack before action, ordinary death flow rather than poison-specific sound. |
| Save-category/condition paths | Ghoul/paralysis `1069-1212`; bat confusion `1214-1333`; vampire bat `1335+`; exercise nearby mechanics, not venom profiles. |
| Timers | `687+` blisters; `3932+` visibility/torch turn clock; `6217+` frame timing; slime/cling tests and unconsciousness test action-based conditions. No delayed poison cross-encounter test. |
| Treatment | CLW `3402+`, blisters `621+`; no Neutralize Poison because it is absent. |
| Persistence validation | `4772+`, `5143-5441`, `8723+`, `8791+`: round trips, v1-v7 defaults, corrupt/truncated versions, module state and non-turn save. No pending poison round trip or explicit reset test. |

### Proposed tests (not implemented)

| Proposed scenario | Rules basis / decision gate |
|---|---|
| Missed venomous attack produces no exposure/save; successful save preserves only ordinary bite damage | Basic B26. |
| Pit Viper failed save kills even at high HP; poison sound/log/session termination match ordinary death | B26/B42 plus existing engine death-event contract. |
| Crab Spider +2 shifts the save boundary; failed save rolls d4 **turns**, survives combat then dies only when deadline expires | B43/B23. Test 1- and 4-turn endpoints and crossing encounters. |
| Black Widow / Medusa one-turn deadline; Cobra bite d10-turn deadline and spit blindness | B39/B42/B43; Black Widow requires separately approved catalog addition. |
| Centipede hit causes no bite HP damage; failure causes ten-day illness/half move/no other physical action | B33. No generic save-or-die expectation. |
| Tarantella dancing, onlooker S, -4/+4, five-turn exhaustion, 2d6 duration | B43. Exact short-duration-versus-exhaustion boundary should follow approved timing order. |
| Killer Bee ordinary sting death, removable 1-HP/round stinger; queen survives repeated stings | B37. Queen representation requires design approval; do not invent a current queen row. |
| Giant Scorpion claws alone do not poison; successful claw improves sting hit roll +2 | Expert X39; gated on future attack/catalog support. |
| Neutralize Poison removes present exposure; later bite still saves; revival inside/outside ten-round window | Expert Neutralize Poison section; window anchor and solo treatment policy must first be decided. |
| CLW heals wounds but leaves a poison deadline; Purify Food affects supplies rather than victim venom; Dispel Magic stops tarantella dance only as specified | Basic B12/B43, Expert treatment. |
| v8 round trip pending effect + partial clock; old saves default clean; invalid timers/IDs/counts/truncation do not mutate game | B43's persistent delayed consequence plus established loader guarantees; proposed v8 is design, not RAW. |
| Two successful venomous hits require two saves; success on one does not cancel failure on the other | B26. Deadline combination/neutralization scope assertions require decisions before test encoding. |
| PoisonDart success/failure actually triggered; sound/death/disable trap verified | B52 lethal-poison baseline **if approved**; dart damage and delivery are project-specific decisions. |
| Undead immunity and locust/robber-fly exceptions when those targets become poison-eligible | B33/B41/B44; monster-on-monster targeting is not current functionality. |
| Class-level boundaries and qualifying protection/confusion/source modifiers use existing `make_save` | Basic B6/B12/B26/B30, Expert X24; choose Basic versus Expert low-level demihuman baseline explicitly. |
| Inventory/help/save/quit/redraw do not advance poison; rest/travel do; no double-counted encounter recovery | B23 plus approved application time contract. |

## 10. Confirmed Defects Relative to the Requested Baseline

Severity here is gameplay-fidelity severity, not security severity. Established
house-rule/simplification status is identified rather than silently overridden.

| ID / severity | Evidence and current observation | Original expectation / gameplay impact | Smallest recommended correction |
|---|---|---|---|
| P01 **High**: one HP-damage poison for distinct effects | `combat.cpp:303-318`; immediate extra d4 for all five flagged rows | B26 and monster exceptions: high-HP heroes survive lethal venom, while centipede/tarantella can lose HP instead of suffering their defined effects | Approve retiring or explicitly configuring simplified poison; use small explicit profiles, not universal death. |
| P02 **High**: poisonous species missed | Bee/Medusa attack strings vs. `game.cpp:847-854,1305` | B37/B39 effects never occur; misleading tool output | Exact species/content mapping that includes these deliveries and source-specific outcomes. |
| P03 **High**: delayed effects absent | `Entity`/Game/save inspected; no record after d4 loss | B39/B42/B43 delay cannot continue after combat; rescue opportunity wrong | Persist pending effects and advance a correctly dimensioned engine clock. |
| P04 **High**: centipede ordinary damage fabricated | `bx_data.hpp:393`, `game.cpp:676-678`, `combat.cpp:249-250` | B33 explicitly no bite damage; current d6 plus possible d4 | Allow zero physical damage for poison-only deliveries without globally clamping them to 1. |
| P05 **Medium**: crab save bonus absent | `combat.cpp:304`, `make_save` has no source modifier | B43 +2 weak poison omitted; saves harder than specified | Extend existing save boundary with explicit source modifier/context. |
| P06 **Medium**: Expert saves do not improve | `game.cpp:1056-1084`; one row/class | Expert X24 improved level bands absent | Add/resolve class-level data through existing `make_save`; keep Basic/Expert low-level choice explicit. |
| P07 **Medium**: qualifying Protection from Evil save bonus absent | `make_save` ignores protection; no attacker alignment passed | Basic B12 qualifying saves +1; hero loses protection on venom save | Supply source context and apply only qualifying bonus, not unconditional +1. |
| P08 **Medium**: poison HP death omits PlayerDeath sound | `combat.cpp:314-318` versus `271-275` | Existing death notification contract inconsistent; frontend still terminates correctly | Small shared cause-aware death finalizer used by ordinary, poison, and trap paths. |
| P09 **High**, if lethal RAW trap intended: extra-d4 trap poison | `exploration.cpp:210-216`; documented approximation | B52 lethal poison becomes survivable extra damage | First approve dart delivery/damage; then share correct poison resolver, not a second save/death implementation. |
| P10 **High for expanded B/X scope**: treatment absent | 39-spell table and `spells.cpp`; no Neutralize Poison | Expert treatment/revival unavailable; simply enabling fatal poison removes a core countermeasure | Add verified clerical spell/availability and cause-aware treatment lifecycle after clock/save design. |
| P11 **Medium**, adjacent spell fidelity | `spells.cpp:288-299` Purify Food heals d4 HP | B12 purifies supplies, not victim wounds; appears to mitigate simplified poison only by replacing HP | Separate supplies purification from victim neutralization; no unsupported antidote. |

Catalog omissions (Black Widow, Scorpion, Sea Snake, Rattlesnake), missing bee
stinger lifecycle, cobra spit, and tarantella onlookers are documented coverage
gaps; their absence is not evidence that a nonexistent runtime row currently
uses the poison handler. Generic monster multiattack parsing is a related
precondition for faithful sting-only scorpion poison, not a reason for a global
combat rewrite in the first poison patch.

## 11. Rules Ambiguities and Decisions Required

1. **Rules mode:** original Basic-only saves or combined B/X using Expert X24
   progression (including its stronger low-level demihuman saves)? Current
   play already uses Expert's low-level poison targets.
2. **Simplification policy:** is extra-d4 poison intentionally retained as an
   optional house rule, or replaced? No explicit original approval was found.
3. **Repeat delayed exposures:** independent deadlines, earliest fatal deadline,
   or some other policy? RAW requires a save per attack but the inspected text
   does not prescribe a general dose-stacking/reset algorithm. Do not reset a
   near-expiry lethal timer to a later date without a decision.
4. **Timing boundaries:** when during a turn does the deadline resolve; what
   happens if treatment and expiry share a boundary; how to charge B23's
   encounter recovery without double counting? UI redraws must not decide it.
5. **Neutralization/revival:** Expert says "revive a poisoned character if cast
   within ten rounds" but the quoted sentence does not explicitly anchor that
   window to bite versus poison death. Define and record the chosen reading;
   do not import a later rulebook's wording as a verbatim Cook/Marsh citation.
   Also decide how a solo session can receive outside treatment after death.
6. **Sea-snake exception:** the delayed "full effects" and 25% treatment failure
   need source-aware scheduling/notification if this monster is later added.
   That exception must not become generic poison failure probability.
7. **Trap delivery:** the existing custom dart has automatic hit/base d4. B52
   supplies lethal examples, not an exact dart specification. Approve its final
   mechanical contract separately from shared poison consequences.
8. **Magical poison saves:** natural poison does not automatically use Wisdom.
   Determine source classification for Tarantella and other magical cases under
   Basic B6; avoid blanket modifiers without source context.
9. **Persistent illness and town time:** centipede's ten days require rest/travel/
   town elapsed time, which current dungeon-frame counters do not represent.
10. **Catalog scope:** support present monsters first; imported/non-Basic prose
    does not authorize adding new creatures, antidotes, or their assumed rules.

## 12. Proposed Remediation

### Essential, ordered, separately reviewable changes

1. **Approve the rules contract.** Resolve section 11 decisions, confirm primary
   citations, and specify expected profile for every present species. Keep the
   existing behavior as an explicit option only if requested; do not quietly
   increase lethality in a refactor.
2. **Unify death notification and characterize saves.** Reuse `make_save`,
   DiceRoller, and GameSession. Add source modifier/context and Expert class-level
   lookup if that mode is chosen. Establish tests for poison and ordinary death
   parity before enabling fatal effects. Do not duplicate save math in profiles.
3. **Fix the time boundary before delayed poison.** Advance elapsed rounds/turns
   from engine actions, not frame visibility. Specify encounter recovery,
   non-turn commands, travel/rest, and timer ordering. This is a small explicit
   clock contract, not a generic effect framework.
4. **Introduce minimal source profiles and victim state.** Map current species
   to explicit poison definitions, retaining RulesCatalog content IDs. Allow
   zero-damage centipede delivery, distinct cobra spit/bite and later scorpion
   claws/sting. A poison boolean may remain a convenience but is not sufficient
   to select an effect.
5. **Add persistence and treatment with the new state.** v8 bounded records,
   explicit old-version defaults, failure-no-mutation tests; add Neutralize
   Poison and approved revival opportunity together with fatal/delayed effects.
   A fresh-load cure exploit is unacceptable for a pending death condition.
6. **Implement per-species outcomes and shared trap poison.** Present creatures
   first: crab, medusa, cobra, pit viper, centipede, tarantella, bee. Reuse one
   exposure/save/treatment/death path. Preserve their delays/nonfatal effects
   rather than mapping all to instant death. Then add approved absent creatures
   and advanced delivery exceptions in separate slices.
7. **Update user-facing descriptions and tests against actual behavior.** Tool
   flags should eventually agree with the source profile; unimplemented RAW
   descriptions must not be presented as proof of working mechanics.

### Smallest viable data model to evaluate

A narrow `PoisonProfileId` lookup keyed by existing monster content ID is enough
initially. Each profile can encode the fixed B/X choices: delivery, P save plus
modifier, effect kind (fatal/illness/blindness/dancing), and explicit delay or
duration in labelled units. Immediate fatal delay is zero; damage-only poison
is not necessary unless an approved source actually calls for it.

Victim state needs source/effect ID, remaining time/deadline with unit, and
cause-aware fatal/revival information. Multiple records versus an earliest-
deadline summary depends on the approved exposure policy. Neutralization should
remove applicable present poison without conferring future immunity. Bee
stingers and dancing-onlooker links are small specific companion states, not
generic flags forced into the fatal-poison timer.

**Optional architecture work:** moving profiles fully into generated JSON,
catalog/codegen drift reconciliation, tool flag unification, and a broader
status-effect abstraction. None is required to justify a generic ECS, scripting
system, or all-purpose effect engine. Preserve `RulesCatalog`, `DiceRoller`,
authoritative `Game`/hero state, and `GameSession` boundaries.

## Final Assessment

1. **Already correct:** successful attack gating for existing flagged melee
   venom, P category, d20 >= target, retention of ordinary damage on save,
   class selection, Expert low-level Poison values, bat-confusion modifier,
   alive/HP mutation and normal session termination after current poison death.
2. **Demonstrably inconsistent:** extra-d4 rather than source-specific effects;
   centipede bite damage; missing delays, crab +2, bee/medusa activation,
   Expert level progression, qualifying protection bonus, treatment, and poison
   death sound. PoisonDart consequence is a documented simplification needing
   an explicit RAW/house-rule decision.
3. **Incorrect current venom monsters:** all seven present species in the first
   matrix, for different reasons. Do not assign identical outcomes to them.
4. **Delayed poison supported?** No.
5. **Reuse timed conditions?** Reuse state/snapshot/log/RNG plumbing, not their
   inconsistent decrement locations or unlabeled timing assumptions.
6. **Poison deaths correct?** HP/alive/normal loop stop are correct for the
   existing damage model; sound parity and poison-aware revival/persistence are
   not complete.
7. **Save/load requirements:** validated new timed-poison and clock/death-cause
   state, explicit legacy defaults and a versioned format extension; in-memory
   campaign copying is not disk persistence.
8. **Safest order:** rule decisions -> shared death/save tests -> clock contract
   -> profiles/state -> persistence/treatment -> source-specific effects/traps.
9. **Before development:** resolve baseline, house-rule status, repeated doses,
   timing boundaries, treatment/revival model, custom trap delivery, magical
   modifiers, and catalog scope.

**Stop point:** recommendations are not implemented. This audit changes only
this documentation file and requires separate review/approval before gameplay
development.
