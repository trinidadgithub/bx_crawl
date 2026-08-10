# Roadmap

This file tracks the next practical work for BX Crawl. The goal is to keep
development focused on playable B/X dungeon-crawl mechanics rather than adding
systems at random.

## Current State

- Character generation, town, camp, dungeon entry/exit, save/load, shops, and
  equipment are playable.
- Dungeon exploration includes light, hunger, rations, traps, searching, door
  interaction, thief skills, cleric turning, spells, scrolls, throwing weapons,
  and procedural sound effects.
- Monster playtest tooling exists:
  - `tools/list_monsters` lists monsters by exact challenge level.
  - `tools/show_monster` displays B/X stat blocks and descriptions.
  - `tools/edit_monster` edits monster descriptions.
- Combat mechanics added: bear hug (2d8 when both claws hit), first-contact
  surprise with stealthy-monster modifiers (Bugbear 1-3, Crab Spider 1-4,
  Shadow 1-5, Weretiger 1-4), paralysis (ghoul/carrion crawler/gelatinous cube/
  thoul; 2-8 turns, Cure Light Wounds removes it, elves immune to ghouls),
  normal bat swarm confusion (10+ bats: -2 to hit/saves, no spells, bats never
  bite), giant vampire bats (5% of encounters; save vs. Paralysis or unconscious
  1-10 rounds, 1-4 HP blood drain per round), humanoid daylight penalties and
  lair leader morale (goblins/orcs -1 to hit in light; leaders boost nearby
  morale and slaying one breaks the group), and secret doors.
- `data/monster_descriptions.txt` is being reviewed against the Basic Box PDF.
  Some entries are confirmed, but this review should continue incrementally.

## Next Logical Focus

1. **Monster Mechanics Pass**

   Monster stat blocks and descriptions are now visible enough to review, so the
   next high-value game work is making key monster traits matter in play.

   Priority examples:

   - Bears: hug damage when both claw attacks hit. Done.
   - Normal bats: confusion from groups of 10+ bats. Done: a swarm of 10+
     normal bats within 8 tiles confuses the hero (-2 to hit, -2 on saves,
     no spellcasting), bats never bite, and they check morale each round.
-   Giant bats: vampire bat variant. Done: 5% of giant bat encounters are
      vampire bats; a bite forces a save vs. Paralysis or unconsciousness for
      1-10 rounds, and an adjacent vampire bat drains 1-4 HP/round while the
      hero is unconscious.
   - Goblins/orcs/kobolds: daylight penalties and leader morale effects where
     relevant. Done: goblins and orcs fight at -1 to hit in the hero's light
     (the goblin king is used to light); one lair humanoid per level is a
     leader (goblin king 15 HP/3 HD/+1 dmg, orc leader 8 HP/+1 dmg, kobold
     chieftain 9 HP/2 HD, hobgoblin king 22 HP/5 HD/+2 dmg); while a leader
     lives nearby morale is boosted (goblin 9, orc 8, kobold 8, hobgoblin 10);
     slaying a leader is announced and drops the group back to its base morale.
   - Ghouls/carrion crawlers: paralysis duration and cure interactions. Done:
     save vs. Paralysis or 2-8 turns, Cure Light Wounds removes it, elves are
     immune to ghoul paralysis.
   - Shadows/wights/wraiths: drain effects and appropriate weapon immunity.
   - Rust monster/oozes/slimes: equipment destruction/corrosion consistency.

2. **Encounter And Morale Pass**

   B/X monster behavior depends heavily on morale, reaction, surprise, and group
   behavior. The game should eventually model these clearly enough to make
   monsters feel different.

   Useful steps:

   - Verify current morale checks and when they happen.
   - Leader-based morale modifiers for humanoid lairs/groups. Done: one lair
     humanoid per level is promoted to leader; living leaders raise nearby
     group morale (goblin 9, orc 8, kobold 8, hobgoblin 10), and their death
     sends the group back to base morale.
   - ~~Add surprise modifiers for stealthy monsters such as bugbears, crab spiders,
     shadows, and weretigers.~~ Done: `monster_surprise_threshold` gives
     Bugbear 1-3, Crab Spider 1-4, Shadow 1-5, and Weretiger 1-4; wired into
     first-contact surprise and `tools/sim_surprise`.
   - Decide how much lair/group behavior matters in a small roguelike dungeon.

3. **Data Accuracy Pass**

   Continue reviewing monster descriptions and stat data against
   `dnd-data/dungestNdragonsBasicBox.pdf`.

   Rules:

   - Prefer Basic Box text when present.
   - If a monster exists in `bx_data.hpp` but not in the Basic PDF extraction,
     mark it as B/X-style rather than verified.
   - Keep descriptions useful for play: important rules should not be hidden.

4. **Documentation Cleanup**

   `Monsters.md` is stale and still describes the old small monster set. Replace
   it with either:

   - A short pointer to `tools/show_monster` and `data/monster_descriptions.txt`,
     or
   - A generated monster reference.

## Recommended Next Task

Now that daylight penalties and leader morale are in place, the next
high-value Monster Mechanics item from section 1 is **drain effects and weapon
immunities for shadows, wights, and wraiths** — when hit they drain a level or
ability score, and Normal Weapons cannot harm them (which also needs the
inventory weapon-classification support to make it playable).

Alternatives from the Encounter And Morale Pass (section 2):

- Add leader-based morale modifiers for humanoid lairs/groups. (Leaders + group
  morale are now implemented; a possible follow-up is how much lair/group
  behavior should matter in a small roguelike dungeon.)
- Verify current morale checks and exactly when they happen.

Acceptance criteria (for the next task):

- Shadows/wights/wraiths drain on a successful hit and are immune to Normal
  Weapons unless silvered/magical.
- Logs and tests cover the drain and immunity cases.
