"""
Convert D&D Basic Rulebook PDF extract into clean markdown.

Simple approach:
  1. Strip page markers, headers, footers, page numbers
  2. Join hyphenated word breaks
  3. Strip trailing B-page numbers
  4. Remove excess whitespace per line
  5. Mark part headers, section headers
  6. Output as clean text (no reflow — too risky with two-column layout)
"""

import re

with open('/home/skynet/pdftotext/raw_extract.txt') as f:
    raw = f.read()

pages = re.split(r'=====\s*PAGE\s+\d+\s*=====', raw)
pages = [p.strip() for p in pages if p.strip()]

HEADER_STRIP = {
    'D&D: BASIC', 'BASIC RULEBOOK', 'FANTASY ADVENTURE GAME',
    'BASIC', 'RULEBOOK', 'DUNGEONS & DRAGONS', 'DUNGEONS & DRAGONS',
    'INTRODUCTION', 'SPELLS', 'MONSTERS', 'TREASURE',
    'PLAYER CHARACTER INFORMATION', 'THE ADVENTURE',
    'THE ENCOUNTER', 'DUNGEON MASTER INFORMATION', 'MAGIC-USERS/ELVES',
    'CHARACTERS', 'CHARACTERS DAD: BASIC', 'D&D: BASIC RULEBOOK',
}

all_lines = []
for p in pages:
    for line in p.split('\n'):
        s = line.strip()
        if not s:
            continue
        if re.match(r'^B\d{1,2}$', s):
            continue
        if s in HEADER_STRIP:
            continue
        if s in {'1', 'R', 'B', 'B1'} and len(s) < 3:
            continue
        all_lines.append(s)

# Join hyphenated word breaks
joined = []
i = 0
while i < len(all_lines):
    line = all_lines[i]
    if line.endswith('-') and i + 1 < len(all_lines) and all_lines[i+1][0].islower():
        joined.append(line[:-1] + all_lines[i+1])
        i += 2
    else:
        joined.append(line)
        i += 1
all_lines = joined

# Strip trailing B-page numbers
def strip_page_num(s):
    return re.sub(r'\s+B\d{1,2}$', '', s).strip()

all_lines = [strip_page_num(l) for l in all_lines]

# Also clean up "BIO" -> "B10", "Bll" -> "B11" (OCR artifacts)
all_lines = [l.replace('BIO', 'B10').replace('Bll', 'B11') for l in all_lines]

# Build output
output = []
i = 0
n = len(all_lines)

while i < n:
    line = all_lines[i]

    # Part headers
    m = re.match(r'^(PART \d+):\s*(.*)', line)
    if m:
        output.append('')
        output.append(f'## {m.group(1).title()}: {m.group(2).strip()}')
        output.append('')
        i += 1
        continue

    if line in {'FOREWORD', 'ACKNOWLEDGEMENTS', 'AFTERWORD', 'CREDITS', 'GLOSSARY',
                'TABLE OF CONTENTS', 'INSPIRATIONAL SOURCE MATERIAL'}:
        output.append('')
        output.append(f'## {line}')
        output.append('')
        i += 1
        continue

    if line in {'FICTION: YOUNG ADULT FANTASY', 'NON-FICTION: YOUNG ADULT',
                'FICTION: ADULT FANTASY', 'SHORT STORY COLLECTIONS', 'NON-FICTION'}:
        output.append(f'### {line}')
        i += 1
        continue

    if line.startswith('HIGHER LEVEL CLERIC'):
        output.append(f'### {line}')
        i += 1
        continue

    if line.startswith('MONSTER LIST:'):
        output.append(f'### {line}')
        i += 1
        continue

    # Decorative lines
    if line.startswith('©') or line.startswith('DUNGEONS & DRAGONS* and D&D') or \
       line.startswith('First printing') or line.startswith('Distributed to') or \
       line.startswith('ISBN'):
        output.append(f'*{line}*')
        i += 1
        continue

    # Default
    output.append(line)
    i += 1

result = '\n'.join(output)
result = re.sub(r'\n{3,}', '\n\n', result)

with open('/home/skynet/pdftotext/dungestNdragonsBasicBox.md', 'w') as f:
    f.write('# Dungeons & Dragons Basic Rulebook\n\n')
    f.write(result)

print(f'Written {len(result)} chars')
