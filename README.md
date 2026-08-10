# BX Crawl

A small Moldvay Basic–inspired dungeon crawler prototype written in C++.

The game now includes character generation, a town layer, procedural dungeons,
class-restricted equipment, hunger/rations, spells, thief skills, sound effects,
save/load, and B/X-flavored combat.

## Building

Requires:

- CMake 3.20+
- A C++20-capable compiler (g++, clang++, MSVC)
- A POSIX-like environment for the text frontend

```bash
git clone <your-repo-url> bx-crawl
cd bx-crawl
cmake -S . -B build
cmake --build build
./build/frontends/text/bx_crawl_text
```

Run tests:

```bash
./build/tests/bx_crawl_tests
```

## Developer Flags

Launch with no monsters, including wandering monsters:

```bash
BX_NO_MONSTERS=1 ./build/frontends/text/bx_crawl_text
```

This is useful for testing town, shop, armorer, hunger, exploration, and save/load
without combat interruptions.

Launch with exactly one chosen monster in each generated dungeon:

```bash
BX_TEST_MONSTER=Goblin ./build/frontends/text/bx_crawl_text
BX_TEST_MONSTER="Carrion Crawler" ./build/frontends/text/bx_crawl_text
BX_TEST_MONSTER=1 ./build/frontends/text/bx_crawl_text  # 1-based bx_data monster index
```

`BX_TEST_MONSTER` accepts exact names, partial case-insensitive names, or a
1-based index into the generated B/X monster table. It also suppresses wandering
monsters, which is useful for focused monster behavior playtests.

List monsters by approximate challenge level before choosing one:

```bash
./build/tools/list_monsters --level 1
./build/tools/list_monsters --level 1 --safe
./build/tools/list_monsters --all
```

The listed `Idx` values can be passed directly to `BX_TEST_MONSTER`.

Simulate B/X surprise odds against a named monster without launching the game.
Stealthy monsters surprise the hero on better odds than 1-2 (e.g. Bugbear 1-3,
Spider, Crab 1-4, Shadow 1-5, Weretiger 1-4):

```bash
./build/tools/sim_surprise Goblin --trials 1000 --seed 12345
./build/tools/sim_surprise "Bugbear" --trials 1000 --seed 12345
./build/tools/sim_surprise "Shadow" --trials 1000 --seed 12345
```

Inspect generated dungeon features, including secret doors:

```bash
./build/tools/show_map_features --seed 2 --draw
```

## Monster Mechanics

Special monster traits implemented per B/X:

- **Bear hug** — a bear (or werebear) whose two claws both hit the same target
  in one round deals an additional 2d8 hug damage.
- **Surprise** — stealthy monsters surprise the hero on better odds than 1-2
  on 1d6: Bugbear 1-3, Spider, Crab 1-4, Shadow 1-5, Weretiger 1-4.
- **Paralysis** — a hit from a ghoul, carrion crawler, gelatinous cube, or
  thoul forces a save vs. Paralysis; failure paralyzes the hero for 2-8 turns.
  Elves are immune to ghoul paralysis (but not other paralysis). A paralyzed
  hero cannot act; only Cure Light Wounds (cast while paralyzed) restores
  movement immediately.
- **Normal bats** — normal bats never bite; they swarm around the hero's head.
  A swarm of 10+ normal bats within 8 tiles confuses the hero (-2 to hit, -2 on
  saving throws, no spellcasting). The HUD shows `CONFUSED!` while active. Each
  round a swarming flock checks morale, and a panicked flock flees.
- **Giant bats / vampire bats** — 5% of giant bat encounters are giant vampire
  bats. A vampire bat's bite deals normal damage but forces a save vs.
  Paralysis; failure knocks the hero unconscious for 1-10 rounds. While
  unconscious the hero cannot act, and an adjacent vampire bat drains 1-4 HP of
  blood each round. The HUD shows `UNCONSCIOUS!` while knocked out.

## Controls
- `w`, `a`, `s`, `d` — cardinal movement
- `q`, `e`, `z`, `c` — diagonal movement
- Bump into a monster to attack
- `o` — open door / pick lock if Thief
- `p` — close door
- `g` — get items
- `u` — use/read item on the ground
- `r` — rest
- `k` + direction — throw knife
- `f` + direction — throw dart
- `m` — cast spell
- `t` + direction — turn undead as Cleric
- `i` — hide in shadows as Thief
- `l` — search for traps/secret doors / hear noise
- `h` — help
- `x` — quit

## Town

The game starts in town after character generation.

- Tavern — paid rest and rumors
- Temple — healing and gold-for-XP donations
- The Saucy Satchel — all B/X adventuring gear from `bx_data.hpp`
- Armorer — buy/sell weapons, armor, and shields using B/X prices
- Camp — full rest, save game, enter dungeon

Camp is the dungeon staging point. Enter the dungeon from camp, and use the up
stairs on dungeon level 1 to return to camp.

## Supplies and Hunger

Rations are consumed over dungeon time. If the hero runs out of food, hunger
penalties begin and eventually cause starvation damage. The HUD shows current
rations and hunger status.

## Save Data

Camp saves to:

```text
~/.bx_crawl/save.dat
```

## Sound and Debug Log

The text frontend enables procedural sound effects when available through
`aplay` or `paplay`. Debug sound logs are written to:

```text
/tmp/bx_crawl_debug.log
```

See [ROADMAP.md](ROADMAP.md) for current development priorities,
[Monsters.md](Monsters.md) for legacy creature notes, and [Scrolls.md](Scrolls.md)
for scroll spells.
