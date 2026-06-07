#!/usr/bin/env python3
"""Extract structured JSON data from the B/X D&D Basic markdown rulebook.

Version 3 - fixed monster and wandering monster parsers.
"""

import json
import re
from pathlib import Path

ROOT = Path("/home/skynet/pdftotext")
MD_FILE = ROOT / "dungestNdragonsBasicBox.md"
OUT_DIR = ROOT / "data"
OUT_DIR.mkdir(exist_ok=True)

text = MD_FILE.read_text()
lines = text.split("\n")


def find_line(pattern, after=0):
    for i, line in enumerate(lines):
        if i < after:
            continue
        if re.search(pattern, line):
            return i
    return None


def parse_markdown_table(lines, start):
    if start >= len(lines):
        return []
    header_line = lines[start].strip()
    if not header_line.startswith("|"):
        return []
    headers = [h.strip() for h in header_line.strip("|").split("|")]
    data_start = start + 2
    rows = []
    for i in range(data_start, len(lines)):
        line = lines[i].strip()
        if not line.startswith("|"):
            break
        cells = [c.strip() for c in line.strip("|").split("|")]
        if len(cells) != len(headers):
            break
        row = {}
        for j, h in enumerate(headers):
            row[h] = cells[j] if j < len(cells) else ""
        rows.append(row)
    return rows


# ────────────────── SIMPLE TABLES ──────────────────

def extract_prime_requisite():
    start = find_line(r"Score of Prime Requisite")
    if start is None: return []
    rows = parse_markdown_table(lines, start)
    data = []
    for r in rows:
        score = r.get("Score of Prime Requisite", "").strip()
        adj = r.get("Adjustment to Experience Earned", "").strip()
        pct = 0
        m = re.search(r"([+-]?\d+)%", adj)
        if m: pct = int(m.group(1))
        data.append({"range": score, "xpAdjustment": pct})
    return data


def extract_ability_scores():
    abilities = {}
    sections = [
        ("Strength", r"^\*\*Strength\*\*$"),
        ("Intelligence", r"^\*\*Intelligence\*\*$"),
        ("Wisdom", r"^\*\*Wisdom\*\*$"),
        ("Dexterity", r"^\*\*Dexterity\*\*$"),
        ("Constitution", r"^\*\*Constitution\*\*$"),
        ("Charisma", r"^\*\*Charisma\*\*$"),
    ]
    for name, start_pat in sections:
        s = find_line(start_pat)
        if s is None: continue
        for ti in range(s + 1, min(s + 12, len(lines))):
            if lines[ti].strip().startswith("|") and "Score" in lines[ti]:
                rows = parse_markdown_table(lines, ti)
                if rows:
                    abilities[name] = rows
                break
    return abilities


def extract_character_classes():
    classes = {}
    for name, pat in [
        ("Cleric", r"^\*\*CLERICS\*\*$"),
        ("Dwarf", r"^\*\*DWARVES\*\*$"),
        ("Elf", r"^\*\*ELVES\*\*$"),
        ("Fighter", r"^\*\*FIGHTERS\*\*$"),
        ("Halfling", r"^\*\*HALFLINGS\*\*$"),
        ("Magic-User", r"^\*\*MAGIC-USERS\*\*$"),
        ("Thief", r"^\*\*THIEVES\*\*$"),
    ]:
        s = find_line(pat)
        if s is None: continue
        for ti in range(s + 1, min(s + 10, len(lines))):
            if lines[ti].strip().startswith("|") and "Level" in lines[ti]:
                rows = parse_markdown_table(lines, ti)
                if rows:
                    classes[name] = rows
                break
    return classes


def extract_cleric_undead():
    start = find_line(r"\*\*Clerics vs. Undead\*\*")
    if start is None: return []
    for ti in range(start + 1, start + 8):
        l = lines[ti].strip()
        if l.startswith("|") and "Cleric" in l:
            return parse_markdown_table(lines, ti)
    return []


def extract_thief_abilities():
    start = find_line(r"\*\*Thieves' Abilities\*\*")
    if start is None: return []
    for ti in range(start + 1, start + 8):
        l = lines[ti].strip()
        if l.startswith("|") and "Level" in l:
            return parse_markdown_table(lines, ti)
    return []


def extract_saving_throws():
    start = find_line(r"\*\*SAVING THROWS\*\*")
    if start is None: return []
    for ti in range(start + 1, start + 10):
        l = lines[ti].strip()
        if l.startswith("|") and "Attack Type" in l:
            return parse_markdown_table(lines, ti)
    return []


def extract_character_attacks():
    start = find_line(r"\*\*CHARACTER ATTACKS\*\*")
    if start is None: return []
    for ti in range(start + 1, start + 10):
        l = lines[ti].strip()
        if l.startswith("|") and "Defender's" in l:
            return parse_markdown_table(lines, ti)
    return []


def extract_monster_attacks_melee():
    start = find_line(r"\*\*MONSTER ATTACKS \(Melee\)\*\*")
    if start is None: return []
    for ti in range(start + 1, start + 20):
        l = lines[ti].strip()
        if l.startswith("|") and "Monster Hit Dice" in l:
            return parse_markdown_table(lines, ti)
    return []


def extract_monster_attacks_missile():
    start = find_line(r"\*\*MONSTER ATTACKS \(Weapon Type / Missile\)\*\*")
    if start is None: return []
    for ti in range(start + 1, start + 10):
        l = lines[ti].strip()
        if l.startswith("|") and "Weapon" in l:
            return parse_markdown_table(lines, ti)
    return []


def extract_missile_ranges():
    start = find_line(r"\*\*Missile Fire Ranges\*\*")
    if start is None: return []
    for ti in range(start + 1, start + 12):
        l = lines[ti].strip()
        if l.startswith("|") and "Weapon" in l:
            return parse_markdown_table(lines, ti)
    return []


def extract_weapon_damage():
    start = find_line(r"\*\*Variable Weapon Damage\*\*")
    if start is None: return []
    for ti in range(start + 1, start + 25):
        l = lines[ti].strip()
        if l.startswith("|") and "Weapon" in l:
            return parse_markdown_table(lines, ti)
    return []


def extract_weapons():
    start = find_line(r"\*\*WEAPONS\*\*")
    if start is None: return []
    for ti in range(start + 1, start + 30):
        l = lines[ti].strip()
        if l.startswith("|") and "Item" in l:
            rows = parse_markdown_table(lines, ti)
            data = []
            for r in rows:
                cost = r.get("Cost in gp", "0").strip()
                raw_name = r.get("Item", "").strip()
                note = ""
                if "(two-handed)" in raw_name.lower():
                    note = "two-handed"
                data.append({
                    "name": raw_name,
                    "costGp": int(cost) if cost.isdigit() else 0,
                    "note": note,
                })
            return data
    return []


def extract_armor_equipment():
    start = find_line(r"\*\*ARMOR EQUIPMENT\*\*")
    if start is None: return []
    for ti in range(start + 1, start + 8):
        l = lines[ti].strip()
        if l.startswith("|") and "Item" in l:
            rows = parse_markdown_table(lines, ti)
            data = []
            for r in rows:
                ac = r.get("Armor Class", "9").strip()
                cost = r.get("Cost in gp", "0").strip()
                name = r.get("Item", "").strip()
                try:
                    ac_val = int(ac.replace("(", "").replace(")", ""))
                except ValueError:
                    ac_val = 9
                data.append({
                    "name": name,
                    "armorClass": ac_val,
                    "costGp": int(cost) if cost.isdigit() else 0,
                })
            return data
    return []


def extract_armor_classes():
    start = find_line(r"\*\*ARMOR CLASSES\*\*")
    if start is None: return []
    for ti in range(start + 1, start + 12):
        l = lines[ti].strip()
        if l.startswith("|") and "Type of Armor" in l:
            rows = parse_markdown_table(lines, ti)
            data = []
            for r in rows:
                ac = r.get("Armor Class", "9").strip()
                try:
                    ac_val = int(ac)
                except ValueError:
                    ac_val = 9
                data.append({"name": r.get("Type of Armor", "").strip(), "armorClass": ac_val})
            return data
    return []


def extract_gear():
    count = 0
    for i, l in enumerate(lines):
        stripped = l.strip()
        if stripped == "| Item | Cost in gp |":
            count += 1
            if count == 2:
                rows = parse_markdown_table(lines, i)
                return [{"name": r.get("Item", "").strip(),
                         "costGp": int(r.get("Cost in gp", "0").strip()) if r.get("Cost in gp", "0").strip().isdigit() else 0}
                        for r in rows]
    return []


def extract_movement():
    start = find_line(r"\*\*Character Movement\*\*")
    if start is None: return []
    for ti in range(start + 1, start + 10):
        l = lines[ti].strip()
        if l.startswith("|") and "Encumbrance" in l:
            return parse_markdown_table(lines, ti)
    return []


def extract_weights():
    start = find_line(r"\*\*Weights of Items \(Encumbrance\)\*\*")
    if start is None: return []
    for ti in range(start + 1, start + 30):
        l = lines[ti].strip()
        if l.startswith("|") and "Item" in l:
            rows = parse_markdown_table(lines, ti)
            return [{"name": r.get("Item", "").strip(),
                      "weightCoins": int(r.get("Weight in Coins", "0").strip()) if r.get("Weight in Coins", "0").strip().isdigit() else 0}
                     for r in rows]
    return []


def extract_treasure_types():
    start = find_line(r"\*\*TREASURE TYPES\*\*")
    if start is None: return []
    for ti in range(start + 1, start + 25):
        l = lines[ti].strip()
        if l.startswith("|") and "Type" in l and "Copper" in l:
            raw = parse_markdown_table(lines, ti)
            data = []
            for r in raw:
                t = r.get("Type", "").strip()
                row = {"type": t}
                for col, key in [("Copper (1000s)", "copper"), ("Silver (1000s)", "silver"),
                                  ("Electrum (1000s)", "electrum"), ("Gold (1000s)", "gold"),
                                  ("Platinum (1000s)", "platinum")]:
                    val = r.get(col, "Nil").strip()
                    if val.upper() == "NIL":
                        row[key] = None
                    else:
                        m = re.match(r"(\d+)%\s*(.*)", val)
                        row[key] = {"chance": int(m.group(1)), "amount": m.group(2).strip()} if m else val
                gems = r.get("Gems & Jewelry", "Nil").strip()
                row["gemsJewelry"] = None if gems.upper() == "NIL" else gems
                magic = r.get("Magic Items", "Nil").strip()
                row["magicItems"] = None if magic.upper() == "NIL" else magic
                data.append(row)
            return data
    return []


def extract_individual_treasure():
    start = find_line(r"\*\*INDIVIDUAL TREASURE TYPES\*\*")
    if start is None: return []
    for ti in range(start + 1, start + 12):
        l = lines[ti].strip()
        if l.startswith("|") and "Type" in l:
            raw = parse_markdown_table(lines, ti)
            data = []
            for r in raw:
                row = {"type": r.get("Type", "").strip()}
                for col in ["Copper", "Silver", "Electrum", "Gold", "Platinum"]:
                    val = r.get(col, "Nil").strip()
                    row[col.lower()] = None if val.upper() == "NIL" else val
                row["gemsJewelry"] = r.get("Gems & Jewelry", "Nil").strip()
                row["magicItems"] = r.get("Magic Items", "Nil").strip()
                data.append(row)
            return data
    return []


def extract_magic_item_tables():
    tables = {}
    for key, pat in [
        ("general", r"\*\*GENERAL MAGIC ITEMS\*\*"),
        ("swords", r"\*\*SWORDS \(roll 1d8\)\*\*"),
        ("weaponsArmor", r"\*\*WEAPONS/ARMOR \(roll 1d8\)\*\*"),
        ("potions", r"\*\*POTIONS \(roll 1d8\)\*\*"),
        ("scrolls", r"\*\*SCROLLS \(roll 1d8\)\*\*"),
        ("rings", r"\*\*RINGS \(roll 1d6\)\*\*"),
        ("wandsStaffsRods", r"\*\*WANDS / STAFFS / RODS \(roll 1d6\)\*\*"),
        ("miscMagic", r"\*\*MISCELLANEOUS MAGIC ITEMS \(roll 1d10\)\*\*"),
    ]:
        start = find_line(pat)
        if start is None: continue
        for ti in range(start + 1, min(start + 20, len(lines))):
            l = lines[ti].strip()
            if l.startswith("|") and ("Die Roll" in l or "Magic Subtable" in l or "Type of" in l):
                tables[key] = parse_markdown_table(lines, ti)
                break
    return tables


def extract_coins():
    start = find_line(r"\*\*COINS\*\*")
    if start is None: return []
    for ti in range(start + 1, start + 10):
        l = lines[ti].strip()
        if l.startswith("|") and "Coin" in l:
            return parse_markdown_table(lines, ti)
    return []


def extract_gems():
    start = find_line(r"\*\*GEMS\*\*")
    if start is None: return []
    for ti in range(start + 1, start + 10):
        l = lines[ti].strip()
        if l.startswith("|") and "Die Roll" in l and "Value" in l:
            rows = parse_markdown_table(lines, ti)
            return [{"roll": r.get("Die Roll", "").strip(),
                      "valueGp": int(m.group(1)) if (m := re.search(r"(\d+)", r.get("Value of Gem", "0").strip())) else 0}
                     for r in rows]
    return []


# ────────────────── SPELLS ──────────────────

def extract_spells():
    spells = {}
    # Find Part 3 to skip TOC
    part3 = find_line(r"^## Part 3: SPELLS", after=1000)
    if part3 is None:
        return spells
    
    for key, start_pat, end_pat, cls, lvl in [
        ("cleric-level1", r"^First Level Clerical Spells$", r"FIRST LEVEL CLERICAL SPELLS", "Cleric", 1),
        ("magicuser-level1", r"^First Level Magic-user and Elf Spells$", r"SECOND LEVEL", "Magic-User", 1),
        ("magicuser-level2", r"^Second Level Magic-user and Elf Spells$", r"FIRST LEVEL MAGIC-USER", "Magic-User", 2),
        ("cleric-level2", r"^Second Level Cleric Spells$", r"EXPLANATION|THIRD LEVEL MAGIC-USER", "Cleric", 2),
        ("magicuser-level3", r"^Third Level Magic-user Spells$", r"EXPLANATION", "Magic-User", 3),
    ]:
        s = find_line(start_pat, after=part3)
        if s is None: continue
        spells[key] = []
        i = s + 1
        while i < len(lines):
            stripped = lines[i].strip()
            if re.search(end_pat, stripped, re.IGNORECASE): break
            m = re.match(r"^\d+\.\s+(.*)", stripped)
            if m:
                name = re.sub(r"\*$", "", m.group(1)).strip()
                name = name.rstrip("'")
                if name:
                    spells[key].append({"name": name, "level": lvl, "class": cls})
            i += 1
    return spells


# ────────────────── MONSTER STAT BLOCKS ──────────────────

MONSTER_KEY_SET = {
    "Armor Class:", "Hit Dice:", "Move:", "Fly:", "Attacks:", "Damage:",
    "No. Appearing:", "Save As:", "Morale:", "Treasure Type:", "Treasure:", "Alignment:"
}

def is_monster_key(line):
    return any(line.startswith(k) for k in MONSTER_KEY_SET)

def get_key_name(line):
    for k in MONSTER_KEY_SET:
        if line.startswith(k):
            return k.rstrip(":")
    return None

def parse_monster_stat_lines(stat_lines):
    """Parse stat lines for a single monster entry.
    
    Handles two formats:
    1. Inline: 'Armor Class: 6' -> direct assignment
    2. Two-column interleaved: keys as 'Armor Class:', then values as '6' on later lines
    """
    monster = {}
    pending_keys = []
    pending_values = []
    
    for sl in stat_lines:
        if not sl:
            continue
        
        if is_monster_key(sl):
            colon_pos = sl.find(":")
            after_colon = sl[colon_pos + 1:].strip()
            key = get_key_name(sl)
            
            if after_colon:
                # Inline key:value - flush any pending pairs first
                # (These would be leftover from the two-column block)
                if pending_keys and pending_values:
                    for idx, pk in enumerate(pending_keys):
                        if idx < len(pending_values):
                            monster[pk] = pending_values[idx]
                    pending_keys = []
                    pending_values = []
                monster[key] = after_colon
            else:
                # Key without inline value - add to pending
                pending_keys.append(key)
        else:
            # Value line
            if pending_keys:
                pending_values.append(sl)
            else:
                # Value without pending key - might be orphaned, skip
                pass
    
    # Flush remaining pending pairs
    if pending_keys and pending_values:
        for idx, pk in enumerate(pending_keys):
            if idx < len(pending_values):
                monster[pk] = pending_values[idx]
    
    return monster


def extract_monsters():
    """Parse monster stat blocks."""
    part6 = find_line(r"^## Part 6: MONSTERS$", after=1000)
    if part6 is None:
        return []
    start_idx = find_line(r"### MONSTER LIST:", after=part6)
    if start_idx is None:
        return []

    # Collect raw lines from monster list to next major section
    raw = []
    i = start_idx + 1
    while i < len(lines):
        s = lines[i].strip()
        if s.startswith("## Part") or s.startswith("### FICTION") or s.startswith("### NON-FICTION"):
            break
        raw.append(s)
        i += 1

    # Identify monster entry boundaries
    entries = []
    j = 0
    while j < len(raw):
        line = raw[j]
        
        # Skip empties, tables, xrefs
        if not line or line.startswith("|") or line.startswith("*") or line.startswith("Animals"):
            j += 1
            continue
        
        # Potential monster name: not a key, not obviously a value
        if not is_monster_key(line) and not re.match(r"^[\d\(]", line):
            # Check ahead for key lines
            has_key_ahead = False
            for k in range(j + 1, min(j + 15, len(raw))):
                if not raw[k]: continue
                if is_monster_key(raw[k]):
                    has_key_ahead = True
                    break
            if not has_key_ahead:
                j += 1
                continue
            
            # Collect stat lines until next monster name
            name = line
            stat_lines = []
            k = j + 1
            while k < len(raw):
                sl = raw[k]
                if not sl:
                    k += 1
                    continue
                # Check if this is a new monster name
                if not is_monster_key(sl) and not re.match(r"^[\d\(]", sl):
                    n = k + 1
                    next_has_key = False
                    for kk in range(n, min(n + 12, len(raw))):
                        if not raw[kk]: continue
                        if is_monster_key(raw[kk]):
                            next_has_key = True
                            break
                    if next_has_key:
                        break
                stat_lines.append(sl)
                k += 1
            
            parsed = parse_monster_stat_lines(stat_lines)
            if parsed:
                parsed["_name"] = name
                entries.append(parsed)
            j = k
        else:
            j += 1
    
    # Filter: at least 3 stat fields
    return [e for e in entries if sum(1 for k in e if k != "_name") >= 3]


# ────────────────── WANDERING MONSTERS ──────────────────

def extract_wandering_monsters():
    """Parse the three wandering monster tables.
    
    Format (columnar, non-table):
      Title: WANDERING MONSTERS: LEVEL X
      Die Roll column (2-20), Wandering Monster Name column, then stat columns (No., AC, HD, etc.)
      Each column is printed vertically (all die rolls, then all names, then all stats).
    """
    levels = {}

    for level in [1, 2, 3]:
        # Flexible pattern: WANDERING MONSTERS: LEVEL 1 (with variable spaces)
        pat = re.compile(rf"WANDERING MONSTERS:\s*LEVEL\s+{level}")
        start = None
        for i, line in enumerate(lines):
            if i < 5000:
                continue  # skip past TOC and Part 6
            if pat.search(line):
                start = i
                break
        if start is None:
            continue

        # Collect raw lines until the next WANDERING MONSTERS or major section
        raw = []
        i = start + 1
        while i < len(lines):
            s = lines[i].strip()
            if pat.search(s) and s != lines[i].strip() and "LEVEL" in s:
                # This shouldn't trigger for the same level, but check
                pass
            # Check for next section using flexible pattern
            if re.search(rf"WANDERING MONSTERS:\s*LEVEL\s+{level + 1}", s):
                break
            if level == 3:
                if s.startswith("## Part") or s.startswith("### FICTION") or s.startswith("### NON-FICTION"):
                    break
            elif re.search(r"WANDERING MONSTERS:\s*LEVEL", s) and f"LEVEL {level}" not in s:
                # Different level header found
                m = re.search(r"LEVEL\s+(\d+)", s)
                if m and int(m.group(1)) != level:
                    break
            if s.startswith("## Part") or s.startswith("### FICTION") or s.startswith("### NON-FICTION"):
                break
            raw.append(s)
            i += 1

        # Parse: first find die rolls (sequence of numbers 2-20), then monster names
        die_rolls = []
        monster_names = []
        
        mode = "die_rolls"
        for line in raw:
            if not line or line in ("Die", "Roll", "Wandering", "Monster", "Monster:", "No.", "AC", "HD", "Damage", "Move", "Save", "Morale"):
                continue
            
            if mode == "die_rolls":
                # Try to parse as a number
                clean = line.replace('"', '').strip()
                if clean.isdigit():
                    num = int(clean)
                    if 2 <= num <= 20:
                        die_rolls.append(num)
                        continue
                    elif num > 20:
                        # Reached stat column data starting with "318" etc.
                        mode = "names"
                        continue
                else:
                    # Not a number, switch to name mode
                    mode = "names"
            
            if mode == "names":
                # Skip stat value lines (numbers, fractions, etc.)
                if re.match(r"^[\d\*\/\'\"]", line) or re.match(r"^l[\-\d]", line) or line.startswith("V"):
                    break
                if line.startswith("DM INSTRUCTIONS") or line.startswith("WANDERING MONSTERS"):
                    break
                if line.startswith("Normal") or line.startswith("Giant"):
                    break
                
                # It's a monster name (with alignment)
                align = ""
                clean = line
                am = re.search(r"\(([ALNCalnc\-]{1,3})\)$", clean)
                if am:
                    align = am.group(1).upper()
                    clean = re.sub(r"\s*\([ALNCalnc\-]{1,3}\)$", "", clean).strip()
                clean = clean.lstrip("'")
                # Clean up OCR artifacts
                clean = clean.replace("Ore", "Orc")
                monster_names.append({"name": clean, "alignment": align})

        # Pair die rolls with monster names
        if die_rolls and monster_names:
            entries = []
            for idx, roll in enumerate(die_rolls):
                if idx < len(monster_names):
                    entries.append({
                        "roll": roll,
                        "monster": monster_names[idx]["name"],
                        "alignment": monster_names[idx]["alignment"],
                    })
            if entries:
                levels[level] = entries

    return levels


# ══════════════════════════════════════════
# MAIN
# ══════════════════════════════════════════

data = {}

extractors = [
    ("prime-requisite", extract_prime_requisite),
    ("ability-scores", extract_ability_scores),
    ("character-classes", extract_character_classes),
    ("cleric-undead", extract_cleric_undead),
    ("thief-abilities", extract_thief_abilities),
    ("saving-throws", extract_saving_throws),
    ("character-attacks", extract_character_attacks),
    ("monster-attacks-melee", extract_monster_attacks_melee),
    ("monster-attacks-missile", extract_monster_attacks_missile),
    ("missile-ranges", extract_missile_ranges),
    ("weapon-damage", extract_weapon_damage),
    ("weapons", extract_weapons),
    ("armor-equipment", extract_armor_equipment),
    ("armor-classes", extract_armor_classes),
    ("gear", extract_gear),
    ("movement", extract_movement),
    ("weights", extract_weights),
    ("treasure-types", extract_treasure_types),
    ("individual-treasure", extract_individual_treasure),
    ("magic-items", extract_magic_item_tables),
    ("coins", extract_coins),
    ("gems", extract_gems),
    ("spells", extract_spells),
    ("monsters", extract_monsters),
    ("wandering-monsters", extract_wandering_monsters),
]

for key, func in extractors:
    print(f"Extracting {key}...")
    data[key] = func()

for key, value in data.items():
    safe_name = key.replace("_", "-")
    path = OUT_DIR / f"{safe_name}.json"
    path.write_text(json.dumps(value, indent=2))
    count = len(value) if isinstance(value, (list, dict)) else "?"
    print(f"  -> {path.name} ({count} items)")

print(f"\nDone. Output in {OUT_DIR}")
