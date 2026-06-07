"""
Convert D&D Basic Rulebook PDF text extract into clean markdown.

Strategy:
1. Read raw page-by-page extraction
2. Remove page markers, headers, footers, page numbers
3. Join hyphenated words and reflow justified text
4. Identify structural elements (parts, sections, monsters, spells, tables)
5. Format cleanly
"""

import re

with open('/home/skynet/pdftotext/raw_extract.txt') as f:
    raw = f.read()

# Split into pages
pages = re.split(r'=====\s*PAGE\s+\d+\s*=====', raw)
pages = [p.strip() for p in pages if p.strip()]

# Known header/footer lines to strip completely
strip_lines = {
    'D&D: BASIC', 'D&D: BASIC', 'BASIC RULEBOOK', 'FANTASY ADVENTURE GAME',
    'BASIC', 'RULEBOOK', 'DUNGEONS & DRAGONS', 'DUNGEONS & DRAGONS',
    'D&D: BASIC', 'D&D BASIC', 'INTRODUCTION', 'SPELLS', 'MONSTERS',
    'TREASURE', 'PLAYER CHARACTER INFORMATION', 'THE ADVENTURE',
    'THE ENCOUNTER', 'DUNGEON MASTER INFORMATION', 'MAGIC-USERS/ELVES',
    'CHARACTERS', 'CHARACTERS DAD: BASIC', 'D&D: BASIC RULEBOOK',
}

# Section header patterns (should become ### headers)
section_headers = {
    'FOREWORD', 'ACKNOWLEDGEMENTS', 'AFTERWORD', 'CREDITS', 'GLOSSARY',
    'TABLE OF CONTENTS', 'INSPIRATIONAL SOURCE MATERIAL',
    'FICTION: YOUNG ADULT FANTASY', 'NON-FICTION: YOUNG ADULT',
    'FICTION: ADULT FANTASY', 'SHORT STORY COLLECTIONS', 'NON-FICTION',
}

### Step 1: Collect all non-blank, non-header lines per page ###

def process_page(text):
    """Clean a single page, return meaningful lines with metadata."""
    lines = text.split('\n')
    result_lines = []
    
    for line in lines:
        s = line.strip()
        
        # Skip empty lines
        if not s:
            continue
        
        # Skip standalone page numbers (B#)
        if re.match(r'^B\d+$', s):
            continue
        
        # Skip known header/footer lines
        if s in strip_lines:
            continue
        
        # Skip orphaned chars from column layout
        if s in {'1', 'R', 'B'}:
            continue
        
        # Skip stray page number prefixes
        if re.match(r'^B\d{1,2}$', s):
            continue
        
        result_lines.append(s)
    
    return result_lines


all_lines = []
for page in pages:
    all_lines.extend(process_page(page))

### Step 2: Join hyphenated words across lines ###
# The PDF uses hyphens to split words at line breaks
# Pattern: "char-\nacters" -> "characters"

def join_hyphenated(lines):
    result = []
    i = 0
    while i < len(lines):
        line = lines[i]
        # Check if line ends with a hyphen (word break)
        if line.endswith('-') and i + 1 < len(lines):
            # Join without the hyphen
            next_line = lines[i + 1]
            # Take everything before the hyphen, then next line
            combined = line[:-1] + next_line
            result.append(combined)
            i += 2
        else:
            result.append(line)
            i += 1
    return result

all_lines = join_hyphenated(all_lines)

### Step 3: Classify lines ###

def is_page_header(s):
    """Check if line is a page header like 'B3' or section header repeat."""
    if re.match(r'^B\d+$', s):
        return True
    return False

def is_part_header(s):
    """Check if line is 'PART X: TITLE'."""
    return bool(re.match(r'^PART \d+:', s))

def is_section_header(s):
    """Check if line is a known section header."""
    return s in section_headers

def is_subsection_header(s):
    """Check if line is a subsection header like 'How To...', 'Definitions...'."""
    patterns = [
        r'^How To', r'^How to', r'^What ', r'^Definitions',
        r'^Use of the Word', r'^How to Use', r'^MONSTER LIST:',
        r'^Character Abilities', r'^Ability Score', r'^Hit Points',
        r'^Bonuses and', r'^Thieves\' Abilities', r'^Character Classes',
        r'^Character Alignment', r'^Cost of', r'^ARMOR CLASSES',
        r'^Languages', r'^Inheritance', r'^"Hopeless Characters"',
        r'^EXAMPLE OF CREATING', r'^Clerical Spells',
        r'^First Level', r'^Second Level', r'^HIGHER LEVEL',
        r'^Beginning The', r'^Party Size', r'^Organizing a',
        r'^Time$', r'^Movement$', r'^Encumbrance', r'^Character Movement',
        r'^Weights and', r'^Light$', r'^Doors$', r'^Retainers$',
        r'^Retainer Reactions', r'^Traps$', r'^Wandering Monsters',
        r'^GIVING EXPERIENCE', r'^Order of Events', r'^Monster Reactions',
        r'^COMBAT$', r'^Combat Sequence', r'^Defensive Movement',
        r'^How to Attack', r'^DAMAGE$', r'^Missile Fire',
        r'^Saving Throws', r'^Saving Throws for', r'^HAND-TO-HAND',
        r'^Character Attacks', r'^Monster Attacks',
        r'^Missile Fire Ranges', r'^Variable Weapon', r'^Morale',
        r'^EXAMPLE OF COMBAT', r'^Treasure Types', r'^General Magic',
        r'^MAGIC ITEMS', r'^Swords$', r'^Weapon/Armor', r'^Potion$',
        r'^Scroll$', r'^Ring$', r'^Wand/Staff/Rod',
        r'^Miscellaneous Magic Items', r'^COINS$', r'^GEMS$',
        r'^JEWELRY$', r'^EXPLANATION OF MAGIC',
        r'^Swords, Weapons', r'^Potions$', r'^Scrolls$', r'^Rings$',
        r'^Wands, Staves', r'^A\. CHOOSE', r'^B\. DECIDE.*SETTING',
        r'^C\. DECIDE.*MONSTERS', r'^D\. DRAW THE MAP',
        r'^E\. STOCK THE', r'^F\. FILLING IN',
        r'^CREATING AN NPC', r'^WANDERING MONSTERS',
        r'^AN EXAMPLE OF DUNGEON DESIGN',
        r'^DUNGEON KEY$', r'^DUNGEON MAPS$',
        r'^Sample Dungeon Expedition',
        r'^Dungeon Mastering as',
        r'^Summary:$', r'^Character Class Tables',
        r'^Read Languages', r'^Read Magic', r'^Shield$', r'^Sleep$',
        r'^Ventriloquism', r'^Continual Light', r'^Detect Evil',
        r'^ESP$', r'^Invisibility', r'^Knock$', r'^Levitate$',
        r'^Locate Object', r'^Mirror Image', r'^Phantasmal Force',
        r'^Web$', r'^Wizard Lock',
        r'^Clerics vs\. Undead',
    ]
    for p in patterns:
        if re.match(p, s):
            return True
    return False

def is_monster_stat_line(s):
    """Check if line starts a monster stat like 'Armor Class:'."""
    return s.startswith('Armor Class:')

def is_ability_table_line(s):
    """Check if this is part of the ability score tables."""
    if re.match(r'^(Strength|Intelligence|Wisdom|Dexterity|Constitution|Charisma):?\s', s):
        return True
    if re.match(r'^Score\s', s) or s in {'3', '4-5', '6-8', '9-12', '13-15', '16-17', '18'}:
        return True
    return False

def is_spell_line(s):
    """Check if line is a spell entry (name, Range, Duration)."""
    if re.match(r'^[A-Z][a-z]+(?:\s+[A-Z][a-z]+)*\s+Range:', s):
        # Not a section header that happens to have "Range:"
        return True
    return False

### Helper functions for formatting ###

def format_monster_block(block):
    """Format monster stat block."""
    if not block:
        return ''
    
    name = block[0]
    desc_lines = []
    stat_lines = []
    
    for line in block[1:]:
        if any(line.startswith(k) for k in ['Armor Class:', 'Hit Dice:', 'Move:', 'Fly:',
            'Swimming:', 'Attacks:', 'Damage:', 'No. Appearing:', 'Save As:',
            'Morale:', 'Treasure Type:', 'Alignment:']):
            stat_lines.append(line)
        elif any(line.startswith(k) for k in ['120\'', '60\'', '30\'', '90\'', '180\'', '1 claw', '2 claw', '1 bite',
            '1 mace', '1-', '2-', '1d', '2d', '1 weapon', 'Nil']):
            stat_lines.append(line)
        else:
            desc_lines.append(line)
    
    md = f'**{name}**  \n'
    for sl in stat_lines:
        md += f'  - {sl}  \n'
    
    if desc_lines:
        desc = ' '.join(desc_lines)
        md += f'\n{desc}'
    
    return md


def format_spell_block(block):
    """Format spell entry."""
    if not block:
        return ''
    
    name_line = block[0]
    m = re.match(r'^([A-Za-z\s/]+?)\s+Range:', name_line)
    if m:
        name = m.group(1).strip()
        rest = name_line[len(name):].strip()
    else:
        name = name_line.split('Range:')[0].strip() if 'Range:' in name_line else name_line
        rest = ''
    
    md = f'**{name}**  \n'
    if rest:
        md += f'  - {rest}  \n'
    for line in block[1:]:
        md += f'  - {line}  \n'
    
    return md

### Step 4: Build structured output ###

output = []
i = 0
n = len(all_lines)

while i < n:
    line = all_lines[i]
    
    # Skip page header remnants
    if is_page_header(line):
        i += 1
        continue
    
    # Part headers
    if is_part_header(line):
        output.append('')
        # Extract part number and title
        m = re.match(r'^(PART \d+):\s*(.*)', line)
        if m:
            output.append(f'## {m.group(1).title()}: {m.group(2).strip()}')
        else:
            output.append(f'## {line}')
        output.append('')
        i += 1
        continue
    
    # Known section headers (Foreword, Glossary, etc.)
    if is_section_header(line):
        output.append('')
        # Handle special cases for formatting
        if line == 'TABLE OF CONTENTS':
            output.append('## Table of Contents')
        elif line in {'FICTION: YOUNG ADULT FANTASY', 'NON-FICTION: YOUNG ADULT',
                      'FICTION: ADULT FANTASY', 'SHORT STORY COLLECTIONS', 'NON-FICTION'}:
            output.append(f'### {line}')
        else:
            output.append(f'## {line}')
        output.append('')
        i += 1
        continue
    
    # Subsection headers
    if is_subsection_header(line):
        output.append('')
        # Clean up common artifacts
        clean = line.replace('BIO', 'B10').replace('Bll', 'B11')
        # Some lines like "How To Create A Player Character" are essentially headers
        if line.startswith('EXAMPLE OF CREATING') or line == 'Summary:':
            output.append(f'### {clean}')
        elif line.startswith('MONSTER LIST:'):
            output.append(f'### {clean}')
        elif line.startswith('Clerics vs. Undead'):
            output.append(f'**{clean}**')
        elif line.startswith('HIGHER LEVEL'):
            output.append(f'### {clean}')
        elif re.match(r'^[A-Z]\.', line):
            output.append(f'**{clean}**')
        elif line.startswith('GIVING EXPERIENCE'):
            output.append(f'**{clean}**')
        elif line.startswith('EXAMPLE OF COMBAT') or line.startswith('EXAMPLE OF CREATING'):
            output.append(f'**{clean}**')
        elif line.startswith('HAND-TO-HAND'):
            output.append(f'### {clean}')
        elif line.startswith('COMBAT$') or line == 'COMBAT':
            output.append(f'**{clean}**')
        elif line == 'DAMAGE':
            output.append(f'**{clean}**')
        elif line in {'MAGIC ITEMS', 'EXPLANATION OF MAGIC ITEMS'}:
            output.append(f'### {clean}')
        elif line.startswith('MAGIC ITEMS') and line != 'MAGIC ITEMS':
            output.append(f'**{clean}**')
        elif line in {'COINS', 'GEMS', 'JEWELRY'}:
            output.append(f'**{clean}**')
        else:
            output.append(f'### {clean}')
        output.append('')
        i += 1
        continue
    
    # Handle Copyright line
    if line.startswith('©') or line.startswith('DUNGEONS & DRAGONS* and D&D'):
        output.append(f'*{line}*')
        i += 1
        continue
    
    # Handle "Printed in the U.S.A." and similar
    if line.startswith('Printed in'):
        i += 1
        continue
    if line.startswith('First printing'):
        output.append(f'*{line}*')
        i += 1
        continue
    if line.startswith('ISBN'):
        output.append(f'*{line}*')
        i += 1
        continue
    if line.startswith('Distributed to'):
        output.append(f'*{line}*')
        i += 1
        continue
    
    # Handle Credits page items
    if line in {'Jeff Dee', 'David S. LaForce', 'Erol Otus', 'James Roslof', 'Bill Willingham',
                'Cover by Erol Otus', 'Illustrations by', 'By Gary Gygax and Dave Arneson',
                'Edited by Tom Moldvay', 'Previous edition edited by J. Eric Holmes'}:
        if line in {'Illustrations by'}:
            output.append(f'*{line}*')
        else:
            output.append(line)
        i += 1
        continue
    
    # Handle Acknowledgement page
    if line.startswith('Special thanks') or line.startswith('The authors and editors'):
        output.append(line)
        i += 1
        continue
    
    # Handle Foreword text with sig
    if line.startswith('Tom Moldvay') and 'December' in line:
        output.append(f'— {line}')
        i += 1
        continue
    
    # Monster stat blocks
    if is_monster_stat_line(line):
        # Collect the full monster block
        block = []
        j = i
        while j < n:
            l = all_lines[j]
            # Stop at empty lines or known boundaries
            if not l.strip():
                break
            # Check for next monster (next line starts with capital letter and is not a stat continuation)
            if j > i and re.match(r'^[A-Z][a-z]+(?:\s+[A-Z][a-z]+)*(?:,|\s|$)', l):
                # This might be a new monster name - check if it's not a stat value
                if not any(l.startswith(k) for k in ['Armor', 'Hit', 'Move', 'Fly', 'Swimming',
                    'Attacks', 'Damage', 'No.', 'Save', 'Morale', 'Treasure', 'Alignment',
                    'Normal', 'Giant', 'Black', 'Grizzly', 'Cave', 'Po', '120\'']):
                    # Also check it's not part of a compound stat line
                    if ':' not in l or l.count(':') > 1:
                        break
            block.append(l)
            j += 1
        
        output.append(format_monster_block(block))
        i = j
        continue
    
    # Spell entries
    if is_spell_line(line):
        block = []
        j = i
        while j < n:
            l = all_lines[j]
            if not l.strip():
                break
            block.append(l)
            j += 1
        output.append(format_spell_block(block))
        i = j
        continue
    
    # Default: paragraph text
    output.append(line)
    i += 1


# Write output
result = '\n'.join(output)

# Clean up excessive blank lines
result = re.sub(r'\n{3,}', '\n\n', result)

with open('/home/skynet/pdftotext/dungestNdragonsBasicBox.md', 'w') as f:
    f.write('# Dungeons & Dragons Basic Rulebook\n\n')
    f.write(result)

print(f'Written {len(result)} chars')
