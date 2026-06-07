"""
Convert D&D Basic Rulebook PDF text extract into clean markdown.
Strategy: line-by-line cleanup with section detection, no complex parsing.
"""

import re

with open('/home/skynet/pdftotext/raw_extract.txt') as f:
    raw = f.read()

# Split into pages, discard page 0 (empty)
pages = re.split(r'=====\s*PAGE\s+\d+\s*=====', raw)
pages = [p.strip() for p in pages if p.strip()]

# Lines to strip entirely (page headers/footers)
STRIP = {
    'D&D: BASIC', 'BASIC RULEBOOK', 'FANTASY ADVENTURE GAME',
    'BASIC', 'RULEBOOK', 'DUNGEONS & DRAGONS',
    'INTRODUCTION', 'SPELLS', 'MONSTERS', 'TREASURE',
    'PLAYER CHARACTER INFORMATION', 'THE ADVENTURE',
    'THE ENCOUNTER', 'DUNGEON MASTER INFORMATION', 'MAGIC-USERS/ELVES',
    'CHARACTERS', 'CHARACTERS DAD: BASIC', 'D&D: BASIC RULEBOOK',
}

def clean_page(text):
    lines = text.split('\n')
    out = []
    for line in lines:
        s = line.strip()
        if not s:
            continue
        if re.match(r'^B\d{1,2}$', s):   # page number B#
            continue
        if s in STRIP:
            continue
        if s in {'1', 'R', 'B', 'B1', 'B2'} and len(s) < 3:
            continue
        out.append(s)
    return out

# Collect all cleaned lines
all_lines = []
for p in pages:
    all_lines.extend(clean_page(p))

# Join hyphenated word breaks: "char-\nacters" -> "characters"
i = 0
merged = []
while i < len(all_lines):
    line = all_lines[i]
    if line.endswith('-') and i + 1 < len(all_lines):
        next_line = all_lines[i + 1]
        merged.append(line[:-1] + next_line)
        i += 2
    else:
        merged.append(line)
        i += 1
all_lines = merged

# Strip trailing B-prefixed page numbers from lines (e.g. "B3", "B45")
def strip_page_num(s):
    return re.sub(r'\s+B\d{1,2}$', '', s).strip()

all_lines = [strip_page_num(l) for l in all_lines]

# Now classify each line and build output
output = []
i = 0
n = len(all_lines)

while i < n:
    line = all_lines[i]

    # Part header
    if re.match(r'^PART \d+:', line):
        output.append('')
        m = re.match(r'^(PART \d+):\s*(.*)', line)
        if m:
            output.append(f'## {m.group(1).title()}: {m.group(2).strip()}')
        output.append('')
        i += 1
        continue

    # Top-level section headers
    if line in {'FOREWORD', 'ACKNOWLEDGEMENTS', 'AFTERWORD', 'CREDITS', 'GLOSSARY',
                'TABLE OF CONTENTS', 'INSPIRATIONAL SOURCE MATERIAL'}:
        output.append('')
        output.append(f'## {line}')
        output.append('')
        i += 1
        continue

    # Sub-section headers (Fiction/Non-Fiction categories)
    if line in {'FICTION: YOUNG ADULT FANTASY', 'NON-FICTION: YOUNG ADULT',
                'FICTION: ADULT FANTASY', 'SHORT STORY COLLECTIONS', 'NON-FICTION'}:
        output.append('')
        output.append(f'### {line}')
        output.append('')
        i += 1
        continue

    # HIGHER LEVEL CLERIC AND MAGIC-USER SPELLS
    if line.startswith('HIGHER LEVEL CLERIC'):
        output.append('')
        output.append(f'### {line}')
        output.append('')
        i += 1
        continue

    # MONSTER LIST
    if line.startswith('MONSTER LIST:'):
        output.append('')
        output.append(f'### {line}')
        output.append('')
        i += 1
        continue

    # Handle single-line Copyright/ISBN etc as italic
    if line.startswith('©') or line.startswith('ISBN') or line.startswith('First printing'):
        output.append(f'*{line}*')
        i += 1
        continue

    if line.startswith('DUNGEONS & DRAGONS* and D&D'):
        output.append(f'*{line}*')
        i += 1
        continue

    if line.startswith('Distributed to'):
        output.append(f'*{line}*')
        i += 1
        continue

    if line == 'Printed in the U.S.A.':
        i += 1
        continue

    # Monster stat blocks: lines that start with a stat key
    stat_keys = {'Armor Class', 'Hit Dice', 'Move', 'Fly', 'Swimming',
                 'Attacks', 'Damage', 'No. Appearing', 'Save As',
                 'Morale', 'Treasure Type', 'Alignment'}

    if line.startswith('Armor Class') or (':' in line and line.split(':')[0] in stat_keys):
        # Collect stat lines for this monster
        block_lines = [line]
        i += 1
        while i < n:
            l = all_lines[i]
            # Check if this is a stat continuation
            parts = l.split(':', 1)
            is_stat = parts[0] in stat_keys
            # Also catch bare values (from two-column layout)
            is_value = re.match(r'^( \d+ | \d+-\d+ | \d+d\d+ | \d+\' | \d+\" | Nil | 1 claw | 2 claw | 1 bite | 1 weapon | any | chaotic | neutral | lawful)', l, re.IGNORECASE)
            
            if is_stat:
                block_lines.append(l)
                i += 1
            elif l in {'Normal', 'Giant', 'Black', 'Grizzly', 'Cave', 'Giant'}:
                block_lines.append(l)
                i += 1
            elif re.match(r'^\d+', l) and any(x in l for x in ["'", '(', '-', 'd']):
                block_lines.append(l)
                i += 1
            else:
                break
        # Check if next line starts new monster (capitalized single word like "Bandit", "Bat")
        # If so, consume it too
        after = block_lines[-1]
        # Now format the block
        output.append('')
        # The first line is either the monster name or a stat
        # Let's look backwards for the monster name
        # Find the most recent line that looks like a name
        # Actually, extract name from context
        # Just output stat lines as a block
        for bl in block_lines:
            output.append(f'  - {bl}')
        output.append('')
        continue

    # Check if this is a monster name (single capitalized word, next line has stats)
    if i + 1 < n:
        next_line = all_lines[i + 1]
        if next_line.startswith('Armor Class') or next_line.startswith('Hit Dice'):
            output.append('')
            output.append(f'**{line}**')
            output.append('')
            i += 1
            continue

    # Spell entries
    if re.match(r'^[A-Z][a-z]+(?:\s+[A-Z][a-z]+)*\s+Range:', line):
        output.append('')
        output.append(f'**{line}**')
        i += 1
        # Collect spell body lines
        while i < n:
            l = all_lines[i]
            if not l:
                break
            if re.match(r'^[A-Z][a-z]+(?:\s+[A-Z][a-z]+)*\s+Range:', l):
                break
            if l.startswith('Armor Class') or l.split(':')[0] in stat_keys:
                break
            output.append(f'  - {l}')
            i += 1
        output.append('')
        continue

    # Default: normal paragraph text
    output.append(line)
    i += 1

# Write output
result = '\n'.join(output)
result = re.sub(r'\n{3,}', '\n\n', result)

with open('/home/skynet/pdftotext/dungestNdragonsBasicBox.md', 'w') as f:
    f.write('# Dungeons & Dragons Basic Rulebook\n\n')
    f.write(result)

print(f'Written {len(result)} chars to markdown')
