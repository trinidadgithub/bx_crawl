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
  Shadow 1-5, Weretiger 1-4), and secret doors.
- `data/monster_descriptions.txt` is being reviewed against the Basic Box PDF.
  Some entries are confirmed, but this review should continue incrementally.

## Next Logical Focus

1. **Monster Mechanics Pass**

   Monster stat blocks and descriptions are now visible enough to review, so the
   next high-value game work is making key monster traits matter in play.

   Priority examples:

   - Bears: hug damage when both claw attacks hit.
   - Normal bats: confusion from groups of 10+ bats.
   - Giant bats: vampire bat variant, paralysis/unconsciousness, blood drain.
   - Goblins/orcs/kobolds: daylight penalties and leader morale effects where
     relevant.
   - Ghouls/carrion crawlers: paralysis duration and cure interactions.
   - Shadows/wights/wraiths: drain effects and appropriate weapon immunity.
   - Rust monster/oozes/slimes: equipment destruction/corrosion consistency.

2. **Encounter And Morale Pass**

   B/X monster behavior depends heavily on morale, reaction, surprise, and group
   behavior. The game should eventually model these clearly enough to make
   monsters feel different.

   Useful steps:

   - Verify current morale checks and when they happen.
   - Add leader-based morale modifiers for humanoid lairs/groups.
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

Implement the **surprise modifiers for stealthy monsters** and combine them with
existing first-contact surprise. This is well-defined, builds directly on the
surprise lifecycle, and is easy to verify.

Acceptance criteria:

- Bugbear, crab spider, shadow, and weretiger surprise the hero on better odds
  than 1-2 on 1d6.
- `tools/sim_surprise <monster>` reports the monster's actual surprise threshold.
- Add tests for each threshold and for first-contact surprise against a stealthy
  monster.
