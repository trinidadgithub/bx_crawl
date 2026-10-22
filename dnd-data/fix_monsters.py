#!/usr/bin/env python3
"""Generate clean monster JSON by merging auto-extracted data with known B/X values.
"""
import json
import re
from pathlib import Path

ROOT = Path("/home/skynet/pdftotext")
MONSTERS_FILE = ROOT / "data" / "monsters.json"
OUT_FILE = ROOT / "data" / "monsters.json"

# Comprehensive B/X Basic monster stat blocks (Moldvay)
# Complete hardcoded data for accuracy
MONSTERS = []

def add(name, ac, hd, move, attacks, damage, no_app, save_as, morale, treasure, alignment, extra=None):
    entry = {
        "_name": name,
        "Armor Class": str(ac),
        "Hit Dice": str(hd),
        "Move": str(move),
        "Attacks": str(attacks),
        "Damage": str(damage),
        "No. Appearing": str(no_app),
        "Save As": str(save_as),
        "Morale": str(morale),
        "Treasure Type": str(treasure),
        "Alignment": str(alignment),
    }
    if extra:
        entry.update(extra)
    MONSTERS.append(entry)

# === HUMAN-TYPE NPCs ===
add("Acolyte", 2, "1", "60' (20')", "1 mace", "1-6",
    "1-8 (1-20)", "Cleric: 1", 7, "U", "Any")
add("Bandit", 6, "1", "120' (40')", "1 weapon", "1-6 or weapon",
    "1-8 (3-30)", "Thief: 1", 8, "U (A)", "Chaotic or Neutral")
add("Berserker", 7, "1+1", "120' (40')", "1 weapon", "1-8",
    "1-6 (3-18)", "Fighter: 1", 8, "Nil", "Neutral")
add("Dwarf", 4, "1", "60' (20')", "1 weapon", "1-8",
    "1-6 (3-18)", "Dwarf: 1", 8, "Nil", "Lawful")
add("Elf", 5, "1+1", "120' (40')", "1 weapon or spell", "1-8 or by spell",
    "1-4 (1-12)", "Elf: 1", 8, "Nil", "Neutral")
add("Gnome", 5, "1", "90' (30')", "1 weapon", "1-6",
    "1-8 (3-18)", "Dwarf: 1", 8, "Nil", "Lawful")
add("Halfling", 7, "1-1", "90' (30')", "1 weapon", "1-6",
    "1-8 (3-18)", "Halfling: 1", 7, "Nil", "Lawful")
add("Medium", 9, "1", "120' (40')", "1 dagger or spell", "1-4 or by spell",
    "1-4 (1-12)", "Magic-User: 1", 7, "Nil", "Any")
add("NPC Party", 0, "varies", "varies", "by class", "by weapon",
    "1-8 (1-12)", "varies", "varies", "A", "Any")
add("Noble", 2, "3", "60' (20')", "1 sword", "1-8",
    "1-8 (2-12)", "Fighter: 3", 8, "V", "Any")
add("Trader", 8, "1", "120' (40')", "1 weapon", "1-6",
    "1-8 (2-12)", "Normal Man", 7, "J", "Any")
add("Veteran", 2, "1-3", "60' (20')", "1 weapon", "1-8",
    "1-8 (2-12)", "Fighter: 1-3", 9, "Nil", "Lawful or Neutral")

# === ANIMALS ===
add("Ape, White", 6, "4", "120' (40')", "2 claws", "1-4/1-4",
    "1-6 (2-8)", "Fighter: 2", 7, "Nil", "Neutral")
add("Boar", 7, "2+2", "150' (50')", "1 tusk", "2-8",
    "1-6 (2-12)", "Fighter: 1", 9, "Nil", "Neutral")

# Bear variants
add("Bear, Black", 6, "4", "120' (40')", "2 claws/1 bite", "1-3/1-3/1-6",
    "1-4 (1-4)", "Fighter: 2", 7, "U", "Neutral")
add("Bear, Grizzly", 6, "5", "120' (40')", "2 claws/1 bite", "1-4/1-4/1-8",
    "1-4 (1-4)", "Fighter: 2", 8, "U", "Neutral")
add("Bear, Cave", 5, "7", "120' (40')", "2 claws/1 bite", "1-8/1-8/2-12",
    "1-2 (1-2)", "Fighter: 3", 9, "V", "Neutral")
add("Bear, Polar", 6, "6", "120' (40')", "2 claws/1 bite", "1-6/1-6/1-10",
    "1-2 (1-2)", "Fighter: 3", 8, "U", "Neutral")

# Cat variants
add("Cat, Great", 6, "4+2", "150' (50')", "2 claws/1 bite", "1-4/1-4/1-8",
    "1-4 (1-4)", "Fighter: 2", 9, "U", "Neutral")
add("Cat, Wild", 5, "1+1", "180' (60')", "2 claws/1 bite", "1-2/1-2/1-4",
    "1-8 (1-8)", "Fighter: 1", 6, "Nil", "Neutral")
add("Cat, Mountain Lion", 6, "3+2", "150' (50')", "2 claws/1 bite", "1-3/1-3/1-6",
    "1-4 (1-4)", "Fighter: 2", 8, "Nil", "Neutral")

add("Ferret, Giant", 5, "1+1", "120' (40')", "1 bite", "1-8",
    "1-8 (1-8)", "Fighter: 1", 7, "Nil", "Neutral")

# Bat variants
add("Bat, Normal", 6, "1 point", "9' (3')", "Confusion", "Nil",
    "1-100 (1-100)", "Normal Man", 6, "Nil", "Neutral",
    {"Fly": "120' (40')"})
add("Bat, Giant", 6, "2", "30' (10')", "1 bite", "1-4",
    "1-10 (1-10)", "Fighter: 1", 8, "Nil", "Neutral",
    {"Fly": "180' (60')"})

add("Mule", 7, "2", "120' (40')", "1 kick or bite", "1-4",
    "0 (2-12)", "Fighter: 1", 8, "Nil", "Neutral")
add("Rat, Giant", 8, "1 point (0)", "30' (10')", "1 bite (disease)", "1-3",
    "1-10 (3-30)", "Normal Man", 8, "Nil", "Neutral")
add("Rock Baboon", 6, "2", "120' (40')", "1 club", "1-6",
    "1-6 (2-12)", "Fighter: 1", 8, "Nil", "Neutral")
add("Shrew, Giant", 4, "1", "180' (60')", "1 bite", "1-6",
    "1-8 (3-12)", "Fighter: 1", 10, "Nil", "Neutral")
add("Wolf", 7, "2+2", "150' (50')", "1 bite", "1-6",
    "1-6 (3-18)", "Fighter: 1", 8, "Nil", "Neutral")

# === BEASTIES ===
add("Beetle, Fire", 4, "1+2", "120' (40')", "1 bite", "2-8",
    "1-8 (2-8)", "Fighter: 1", 7, "Nil", "Neutral")
add("Beetle, Giant", 3, "2+2", "90' (30')", "1 bite", "2-12",
    "1-6 (1-6)", "Fighter: 1", 8, "Nil", "Neutral")
add("Beetle, Oil", 4, "1+2", "120' (40')", "1 bite + special", "1-6 + special",
    "1-8 (2-8)", "Fighter: 1", 7, "Nil", "Neutral")
add("Beetle, Tiger", 3, "3+1", "150' (50')", "1 bite", "2-12",
    "1-6 (1-6)", "Fighter: 2", 9, "U", "Neutral")
add("Cave Locust", 4, "1", "30' (10')", "1 bite", "1-2",
    "2-20 (1-10)", "Fighter: 1", 6, "Nil", "Neutral",
    {"Fly": "180' (60')"})
add("Centipede, Giant", 9, "1/2 (1-4 hp)", "60' (20')", "1 bite (poison)", "Nil",
    "1-6 (1-8)", "Fighter: 1", 7, "Nil", "Neutral")
add("Driver Ant", 3, "4", "180' (60')", "1 bite", "2-12",
    "1-6 (1-6)", "Fighter: 2", 8, "Nil", "Neutral")

# === HUMANOIDS ===
add("Bugbear", 5, "3+1", "120' (40')", "1 weapon", "2-8",
    "1-6 (2-8)", "Fighter: 3", 9, "B", "Chaotic")
add("Doppleganger", 5, "4", "90' (30')", "1", "1-12",
    "1-6 (1-6)", "Fighter: 4", 8, "Nil", "Chaotic")
add("Ghoul", 6, "2", "90' (30')", "2 claws/1 bite (paralysis)", "1-3/1-3/1-3",
    "1-6 (2-12)", "Fighter: 2", 9, "B", "Chaotic")
add("Gnoll", 5, "2", "90' (30')", "1 weapon", "2-8",
    "1-6 (3-12)", "Fighter: 2", 8, "D", "Chaotic")
add("Goblin", 6, "1-1", "60' (20')", "1 weapon", "1-6",
    "2-8 (6-40)", "Fighter: 1", 7, "K", "Chaotic")
add("Gray Ooze", 8, "3+3", "10' (3')", "1 pseudopod (corrosion)", "2-16",
    "1 (1)", "Fighter: 3", 12, "Nil", "Neutral")
add("Hobgoblin", 6, "1+1", "90' (30')", "1 weapon", "1-8",
    "1-6 (4-24)", "Fighter: 1", 8, "K", "Chaotic")
add("Kobold", 7, "1/2 (1-4 hp)", "60' (20')", "1 weapon", "1-4",
    "4-16 (6-40)", "Fighter: 1", 6, "J", "Chaotic")
add("Lizard, Gecko", 7, "1", "120' (40')", "1 bite", "1-2",
    "1-6 (1-6)", "Fighter: 1", 7, "Nil", "Neutral")
add("Lizard, Horned", 4, "2+2", "120' (40')", "1 bite", "1-8",
    "1-4 (1-4)", "Fighter: 1", 7, "Nil", "Neutral")
add("Minotaur", 6, "6", "120' (40')", "1 weapon or gore", "1-8 or 1-6",
    "1-6 (1-8)", "Fighter: 6", 9, "C", "Chaotic")
add("Ogre", 5, "4+1", "90' (30')", "1 club", "1-10",
    "1-6 (2-8)", "Fighter: 4", 10, "C", "Chaotic")
add("Orc", 6, "1", "120' (40')", "1 weapon", "1-6",
    "1-6 (3-24)", "Fighter: 1", 7, "D", "Chaotic")
add("Troglodyte", 5, "2", "120' (40')", "2 claws/1 bite", "1-4/1-4/1-6",
    "1-8 (5-40)", "Fighter: 2", 8, "A", "Chaotic")

# === GIANT ANIMALS / VERMIN ===
add("Tick, Giant", 3, "3", "30' (10')", "1 bite (disease)", "1-4",
    "1-4 (1-4)", "Fighter: 2", 8, "Nil", "Neutral")

# === DRAGONS ===
add("Dragon, White", 3, "6", "90' (30')", "1 breath/1 bite", "1-8 or 2-16",
    "1-4 (1-4)", "Fighter: 6", 8, "H", "Chaotic",
    {"Fly": "240' (80')"})
add("Dragon, Black", 2, "7", "90' (30')", "1 breath/1 bite", "1-8 or 3-18",
    "1-4 (1-4)", "Fighter: 7", 8, "H", "Chaotic",
    {"Fly": "240' (80')"})
add("Dragon, Green", 1, "8", "90' (30')", "1 breath/1 bite", "1-8 or 3-24",
    "1-4 (1-4)", "Fighter: 8", 8, "H", "Chaotic",
    {"Fly": "240' (80')"})
add("Dragon, Blue", 0, "9", "90' (30')", "1 breath/1 bite", "1-8 or 4-24",
    "1-4 (1-4)", "Fighter: 9", 8, "H", "Chaotic",
    {"Fly": "240' (80')"})
add("Dragon, Red", -1, "10", "90' (30')", "1 breath/1 bite", "1-8 or 5-30",
    "1-4 (1-4)", "Fighter: 10", 8, "H", "Chaotic",
    {"Fly": "240' (80')"})
add("Dragon, Gold", -2, "11", "90' (30')", "1 breath/1 bite", "1-10 or 6-36",
    "1-4 (1-4)", "Fighter: 11", 9, "H", "Lawful",
    {"Fly": "240' (80')"})

# === UNDEAD ===
add("Skeleton", 7, "1", "60' (20')", "1 weapon", "1-6",
    "1-8 (3-24)", "Fighter: 1", 12, "Nil", "Chaotic")
add("Zombie", 8, "2", "120' (40')", "1 weapon", "1-8",
    "1-6 (2-12)", "Fighter: 1", 12, "Nil", "Chaotic")
add("Ghoul", 6, "2", "90' (30')", "2 claws/1 bite", "1-3/1-3/1-3",
    "1-6 (2-12)", "Fighter: 2", 9, "B", "Chaotic")
add("Wight", 5, "3", "90' (30')", "1 touch (energy drain)", "1-4",
    "1-6 (2-8)", "Fighter: 3", 11, "B", "Chaotic")
add("Wraith", 3, "4", "120' (40')", "1 touch (energy drain)", "1-6",
    "1-4 (1-6)", "Fighter: 4", 11, "E", "Chaotic",
    {"Fly": "240' (80')"})
add("Shadow", 7, "2+2", "90' (30')", "1 touch (STR drain)", "1-4",
    "1-8 (2-12)", "Fighter: 2", 11, "F", "Chaotic")

# === MAGIC ITEMS / CONSTRUCTS ===
add("Living Statue, Crystal", 4, "3", "90' (30')", "1 fist", "1-6",
    "1-4 (1-4)", "Fighter: 3", 11, "Nil", "Lawful")
add("Living Statue, Jade", 4, "4", "60' (20')", "1 fist", "1-8",
    "1-2 (1-2)", "Fighter: 4", 10, "Nil", "Lawful")
add("Living Statue, Steel", 0, "5", "60' (20')", "1 fist", "2-12",
    "1-2 (1-2)", "Fighter: 5", 10, "Nil", "Lawful")

# === OOZES / SLIMES / JELLIES ===
add("Gelatinous Cube", 8, "4", "60' (20')", "1 touch (paralysis)", "2-8",
    "1 (1)", "Fighter: 2", 12, "Nil", "Neutral")
add("Ochre Jelly", 8, "5", "30' (10')", "1 touch (corrosion)", "2-12",
    "1 (1)", "Fighter: 3", 12, "Nil", "Neutral")
add("Green Slime", 0, "2", "3' (1')", "1 touch (corrosion)", "special",
    "1 (1)", "Fighter: 1", 12, "Nil", "Neutral")

# === MONSTROUS ===
add("Carrion Crawler", 7, "3+1", "120' (40')", "8 tentacles (paralysis)", "paralysis",
    "1-4 (1-4)", "Fighter: 2", 9, "B", "Neutral")
add("Gargoyle", 5, "4", "90' (30')", "2 claws/1 bite/1 horn", "1-3/1-3/1-6/1-4",
    "1-6 (2-12)", "Fighter: 4", 11, "C", "Chaotic",
    {"Fly": "150' (50')"})
add("Harpy", 7, "3", "60' (20')", "1 weapon + song", "1-6",
    "1-6 (2-8)", "Fighter: 3", 7, "C", "Chaotic",
    {"Fly": "150' (50')"})
add("Medusa", 8, "4", "90' (30')", "1 snake bite + poison", "1-6",
    "1-6 (1-6)", "Fighter: 4", 8, "F", "Chaotic")
add("Mummy", 3, "5+1", "60' (20')", "1 touch (disease)", "1-12",
    "1-6 (2-8)", "Fighter: 5", 12, "D", "Lawful")
add("Owl Bear", 5, "5", "120' (40')", "2 claws/1 bite", "1-8/1-8/1-8",
    "1-4 (1-4)", "Fighter: 3", 9, "U", "Neutral")
add("Pixie", 3, "1", "90' (30')", "1 dagger or spell", "1-4",
    "1-6 (2-8)", "Elf: 1", 7, "C", "Neutral",
    {"Fly": "180' (60')"})
add("Rust Monster", 7, "5", "120' (40')", "1 touch (rust)", "special",
    "1-4 (1-4)", "Fighter: 3", 7, "Nil", "Neutral")
add("Shrieker", 7, "3", "9' (3')", "nil", "nil",
    "1-8 (1-8)", "Fighter: 1", 12, "Nil", "Neutral")
add("Snake, Cobra", 7, "2", "90' (30')", "1 bite (poison)", "1-6",
    "1-8 (3-12)", "Fighter: 1", 7, "Nil", "Neutral")
add("Snake, Pit Viper", 6, "2", "90' (30')", "1 bite (poison)", "1-6",
    "1-8 (3-12)", "Fighter: 1", 7, "Nil", "Neutral")
add("Spider, Crab", 7, "2", "120' (40')", "1 bite", "1-8",
    "1-6 (3-12)", "Fighter: 1", 7, "Nil", "Neutral")
add("Spider, Tarantella", 6, "3", "120' (40')", "1 bite (poison)", "1-8",
    "1-4 (1-4)", "Fighter: 2", 8, "U", "Neutral")
add("Sprite", 5, "1+1", "90' (30')", "1 dagger or charm", "1-4",
    "1-6 (3-18)", "Elf: 1", 7, "C", "Neutral",
    {"Fly": "180' (60')"})
add("Stirge", 7, "1+1", "30' (10')", "1 bite (blood drain)", "1-3",
    "1-8 (3-24)", "Fighter: 1", 9, "Nil", "Neutral",
    {"Fly": "180' (60')"})
add("Thoul", 6, "3", "120' (40')", "1 weapon or 2 claws", "1-6 or 1-3/1-3",
    "1-6 (2-8)", "Fighter: 3", 10, "C", "Chaotic")
add("Lycanthrope, Werewolf", 5, "4", "180' (60')", "1 bite", "2-8",
    "1-6 (2-12)", "Fighter: 4", 8, "C", "Chaotic")
add("Lycanthrope, Werebear", 5, "6", "120' (40')", "2 claws/1 bite", "1-6/1-6/2-12",
    "1-4 (1-4)", "Fighter: 6", 8, "C", "Neutral")
add("Lycanthrope, Wereboar", 6, "5", "150' (50')", "1 bite", "2-12",
    "1-4 (1-6)", "Fighter: 5", 9, "C", "Chaotic")
add("Lycanthrope, Weretiger", 5, "5", "150' (50')", "2 claws/1 bite", "1-6/1-6/2-12",
    "1-4 (1-4)", "Fighter: 5", 9, "C", "Neutral")
add("Lycanthrope, Wererat", 6, "3", "120' (40')", "1 weapon", "1-6",
    "1-8 (2-16)", "Fighter: 3", 8, "C", "Chaotic")

# === EXTRA NPC TYPES (from wandering monster tables) ===
add("Killer Bee", 6, "1+2", "90' (30')", "1 sting (poison)", "1-4",
    "1-6 (1-6)", "Fighter: 1", 9, "Nil", "Neutral",
    {"Fly": "150' (50')"})
add("Lizard, Giant Gecko", 7, "1", "120' (40')", "1 bite", "1-2",
    "1-6 (1-6)", "Fighter: 1", 7, "Nil", "Neutral")
add("Lizard, Giant Horned", 4, "2+2", "120' (40')", "1 bite", "1-8",
    "1-4 (1-4)", "Fighter: 1", 7, "Nil", "Neutral")
add("Giant Shrew", 4, "1", "180' (60')", "1 bite", "1-6",
    "1-8 (3-12)", "Fighter: 1", 10, "Nil", "Neutral")
add("Giant Ferret", 5, "1+1", "120' (40')", "1 bite", "1-8",
    "1-8 (1-8)", "Fighter: 1", 7, "Nil", "Neutral")
add("Normal Human", 9, "1-1", "120' (40')", "1 weapon", "1-6",
    "varies", "Normal Man", 6, "Nil", "Any")
add("Giant Tick", 3, "3", "30' (10')", "1 bite (disease)", "1-4",
    "1-4 (1-4)", "Fighter: 2", 8, "Nil", "Neutral")

# Now try to keep any auto-extracted entries that are valid and not already added
existing = json.loads(MONSTERS_FILE.read_text())
existing_names = {m["_name"] for m in MONSTERS}
added = 0
for m in existing:
    name = m["_name"]
    if name in existing_names:
        continue
    # Only keep if it has a valid AC and name looks like a real monster
    try:
        ac = int(m.get("Armor Class", ""))
        if ac < 3 or ac > 9:
            continue
    except:
        continue
    if len(name) < 3 or len(name) > 30:
        continue
    if name.startswith(("weapon", "Flying", "Attack", "In water", "Swimming", "See below", "Spores", "Nil", "Energy", "sight.", "Dice:", "items.)", "White Black", "class and", "only be", "+ special", "lbite")):
        continue
    MONSTERS.append(m)
    added += 1
    existing_names.add(name)

print(f"Total monsters: {len(MONSTERS)} (kept {added} from auto-extract)")

# Sort alphabetically
MONSTERS.sort(key=lambda m: m["_name"].lower())

OUT_FILE.write_text(json.dumps(MONSTERS, indent=2))
print(f"Written to {OUT_FILE}")
