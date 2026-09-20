# Phase 33 - Green Slime: Contact Dissolution and Clinging

B/X's green slime is the corrosion family's living-danger member: what touches
it gets its non-magical armor and weapons eaten away, and the slime itself
clings to bare flesh for 1d8 per round until scraped off. This slice implements
both halves, reusing the equipment-subtraction machinery Phase 32 established
for the rust monster.

Scope note: the ROADMAP originally asked for "oozes damage even in armor", but
B/X has no armor-based damage reduction — AC only affects to-hit — so there is
no ignores-armor mechanic to build. Per direction, the phase covers green slime
faithfully; gray ooze (2-16) and ochre jelly (2-12) were verified to already
match their B/X damage strings and need no change.

## Data at spawn

- `green_slime` on `Entity` is set from the monster name (`== "Green Slime"`)
  in `spawn_bx_monster_by_index`, like every other special. The combat special
  chain gained `else if (attacker.green_slime) apply_green_slime(...)`.
- New hero status field `green_slime_turns` marks "slime stuck on you" (0 =
  clear). It is session state like paralysis/unconsciousness, so save v7 is
  unchanged; it mirrors into `HeroView` for the snapshot renderers.

## Contact: dissolve the worn non-magical gear

`apply_green_slime` removes, all at once, the gear the slime touched: the worn
armor (metal or leather), the shield, and the wielded weapon — any non-magical
item, wood included (the slime dissolves wood and metal, unlike the rust
monster which only eats metal). Each piece decrements its count, armor resets
`ac` to 9, and a cleared weapon resets `damage_die` to 2 (unarmed 1d2), exactly
following the rule-of-thumb the sell and rust paths already use. The silver
dagger survives: as in Phases 31-32, the reference silver item is the game's
one protected material.

A single message names what was eaten ("The green slime eats through your
Leather Armor, Shield, and Club!"). If nothing is worn (or only silver is),
the touch still clings — the dissolve is skipped and "The green slime clings
to you!" is logged.

## Clinging: 1d8 per round until a Scrape action

While `green_slime_turns > 0`, the hero takes 1d8 at the start of each round
spent doing anything except scraping (Quit and Save don't advance the round).
B/X's "scrape it off, which takes one round" is now an explicit player action:

- new `PlayerCommandType::Scrape`, bound to dungeon key `s` in the text
  frontend (added to the in-game help);
- the Scrape action consumes a normal turn (monsters still act that round),
  clears the counter, and logs "You frantically scrape the green slime off!";
- scraping with nothing stuck is a harmless no-op message.

If the clinging damage drops the hero to zero, the slime devours the hero.

## Tests

- `Green Slime gets the contact-dissolves trait at spawn` — `BX_TEST_MONSTER
  =green slime` spawns with the flag; a goblin does not.
- `Green Slime contact eats gear and clings`:
  - a touch dissolves worn leather armor, the shield, and a wielded wooden
    club all at once (counts, ids, `ac`, and unarmed `1d2` all verified) and
    logs "eats through";
  - a silver dagger survives the touch, and the slime still clings;
  - after contact, each round the hero acts (not scraping) costs 1-8 hp ("eats
    through you!"), a Scrape action clears the counter, and the following
    round causes no further damage.

## Validation

- Debug and sanitizer builds: warning-free.
- Doctest: 213/213 cases, 15163/15163 assertions (Debug + sanitizer).
- ctest: PASS (Debug + sanitizer).
- Smoke (Debug): Hrothgar and Tomb Runner saves still boot/enter the dungeon
  with exit 0, and `BX_TEST_MONSTER=green` boots, enters the dungeon, and quits
  cleanly.

Known, intentionally untouched: the rest of the slime family's B/X combat
details — only fire (and some spells) can destroy green slime, and ochre
jellies/black puddings split when struck by fire or lightning. Those are the
next slice (ROADMAP), and can build on the spawn-flag + special-chain pattern
this phase extends.