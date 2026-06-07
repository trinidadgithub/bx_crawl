import re

with open('/home/skynet/pdftotext/raw_extract.txt') as f:
    raw = f.read()

# Split into pages
pages = re.split(r'=====\s*PAGE\s+\d+\s*=====', raw)
# Remove page 0 (empty leading)
pages = [p.strip() for p in pages if p.strip()]

# Known page header markers to strip (appear at top of many pages)
header_markers = ['D&D: BASIC', 'D&D: BASIC', 'BASIC RULEBOOK', 'CHARACTERS DAD: BASIC',
                  'FANTASY ADVENTURE GAME', 'BASIC', 'RULEBOOK', 'DUNGEONS & DRAGONS']

# Collect known headers that appear on pages as section markers we want to preserve
section_headers = ['INTRODUCTION', 'SPELLS', 'MONSTERS', 'TREASURE', 'PLAYER CHARACTER INFORMATION',
                   'THE ADVENTURE', 'THE ENCOUNTER', 'DUNGEON MASTER INFORMATION',
                   'MAGIC-USERS/ELVES', 'HIGHER LEVEL CLERIC AND MAGIC-USER SPELLS',
                   'GLOSSARY', 'AFTERWORD', 'CREDITS', 'INSPIRATIONAL SOURCE MATERIAL',
                   'FICTION: YOUNG ADULT FANTASY', 'NON-FICTION: YOUNG ADULT',
                   'FICTION: ADULT FANTASY', 'SHORT STORY COLLECTIONS', 'NON-FICTION']

def clean_page_text(text):
    """Remove page headers, footers, and artifacts from a single page."""
    lines = text.split('\n')
    cleaned = []
    
    # Page number patterns: standalone B# or just numbers at end
    for line in lines:
        stripped = line.strip()
        
        # Skip empty lines but keep for paragraph detection
        if not stripped:
            continue
            
        # Skip standalone page numbers (B# or just B# with spaces)
        if re.match(r'^B\d+$', stripped):
            continue
        if re.match(r'^B\d+\s*$', stripped):
            continue
            
        # Skip section header repeats that appear as page headers
        # These are lines that are exactly known section names
        if stripped in section_headers:
            # Check if this is just a header or actual content
            # We'll add it back carefully later
            continue
            
        # Remove orphaned markers like "1" or single chars from column layout
        if stripped == '1' or stripped == 'R':
            continue
            
        cleaned.append(line)
    
    return '\n'.join(cleaned)

def join_hyphenated(text):
    """Join words broken by hyphenation across lines."""
    # Join hyphenated line breaks: word-\nword -> wordword
    text = re.sub(r'(\w)-\n(\w)', r'\1\2', text)
    return text

def join_broken_lines(text):
    """Join lines that are broken mid-sentence (justified text)."""
    lines = text.split('\n')
    result = []
    for line in lines:
        stripped = line.strip()
        if not stripped:
            if result:
                result.append('')
            continue
        
        # If previous line ends with a complete sentence end, start new paragraph
        if result and result[-1]:
            prev = result[-1]
            # Check if previous line ends with sentence-ending punctuation
            if re.search(r'[.!?:]\s*$', prev):
                result.append(stripped)
            # Check if this line starts a known section header
            elif re.match(r'^(PART \d|How To|What |Definitions|Use of|Character|Hit |Bonuses|'
                         r'Thieves|Cost of|Armor |Languages|Inheritance|EXAMPLE|'
                         r'First Level|Second Level|HIGHER LEVEL|Order of|Monster |'
                         r'Combat |Defensive |Damage |Missile |Saving |HAND-TO-HAND|'
                         r'Variable |Morale|MONSTER LIST|Treasure |General |MAGIC |'
                         r'Swords|Weapon|Potion|Scroll|Ring|Wand|Coins|Gems|Jewelry|'
                         r'EXPLANATION|A\. |B\. |C\. |D\. |E\. |F\. |G\. |H\. |I\. |'
                         r'CREATING|WANDERING|AN EXAMPLE|DUNGEON |Sample |'
                         r'Dungeon |CREDITS|AFTERWORD|INSPIRATIONAL|FICTION|NON-FICTION|'
                         r'GLOSSARY|ACKNOWLEDGEMENTS|FOREWORD)', stripped):
                result.append('')
                result.append(stripped)
            else:
                # Continue the paragraph - join with space
                # But check if previous was a header/section marker
                result[-1] = result[-1] + ' ' + stripped
        else:
            result.append(stripped)
    
    return '\n'.join(result)

def format_document(text):
    """Apply final formatting to produce markdown."""
    lines = text.split('\n')
    result = []
    i = 0
    
    while i < len(lines):
        line = lines[i]
        stripped = line.strip()
        
        if not stripped:
            result.append('')
            i += 1
            continue
        
        # Part headers
        m = re.match(r'^(PART \d+):\s*(.*)', stripped, re.IGNORECASE)
        if m:
            result.append(f'## Part {m.group(1)[-1]}: {m.group(2).title()}')
            result.append('')
            i += 1
            continue
        
        # Known section headers
        if stripped == 'FOREWORD':
            result.append('## Foreword')
            result.append('')
            i += 1
            continue
        if stripped == 'ACKNOWLEDGEMENTS':
            result.append('## Acknowledgements')
            result.append('')
            i += 1
            continue
        if stripped == 'AFTERWORD':
            result.append('## Afterword')
            result.append('')
            i += 1
            continue
        if stripped == 'CREDITS':
            result.append('## Credits')
            result.append('')
            i += 1
            continue
        if stripped == 'GLOSSARY':
            result.append('## Glossary')
            result.append('')
            i += 1
            continue
        if stripped == 'INSPIRATIONAL SOURCE MATERIAL':
            result.append('## Inspirational Source Material')
            result.append('')
            i += 1
            continue
        if stripped in ['FICTION: YOUNG ADULT FANTASY', 'NON-FICTION: YOUNG ADULT',
                        'FICTION: ADULT FANTASY', 'SHORT STORY COLLECTIONS', 'NON-FICTION']:
            result.append(f'### {stripped}')
            result.append('')
            i += 1
            continue
        if stripped in ['MAGIC-USERS/ELVES']:
            result.append(f'### Magic-Users and Elves')
            result.append('')
            i += 1
            continue
        if stripped == 'HIGHER LEVEL CLERIC AND MAGIC-USER SPELLS':
            result.append(f'### {stripped.title()}')
            result.append('')
            i += 1
            continue
        
        # Sub-section headers (How To..., Definitions, etc.)
        if re.match(r'^(How To|What |Definitions of|Use of|How to |How To )', stripped):
            # Make sure it's not part of running text
            if len(stripped) < 80:
                result.append(f'### {stripped}')
                result.append('')
                i += 1
                continue
        
        # Monster names (Armor Class: pattern) - handle as formatted block
        if stripped == 'Armor Class:' or stripped.startswith('Armor Class:'):
            # Collect monster stat block
            block_lines = []
            while i < len(lines) and lines[i].strip():
                block_lines.append(lines[i].strip())
                i += 1
            result.append(format_monster_block(block_lines))
            result.append('')
            continue
        
        # Monster name line (e.g., "Acolyte" then next lines have stats)
        # Check if next line is "Armor Class:"
        if i + 1 < len(lines):
            next_s = lines[i+1].strip()
            if next_s == 'Armor Class:' or next_s.startswith('Armor Class:'):
                # This is a monster name
                block_lines = [stripped]
                i += 1
                while i < len(lines) and lines[i].strip():
                    block_lines.append(lines[i].strip())
                    i += 1
                result.append(format_monster_block(block_lines))
                result.append('')
                continue
        
        # Spell entries: "Spell Name Range: X Duration: Y"
        if re.match(r'^[A-Z][a-zA-Z]+(\s+[A-Z][a-zA-Z]+)*\s+Range:', stripped):
            # Check it looks like a spell (has Range and Duration)
            result.append(format_spell_entry(lines, i))
            # Skip the spell lines
            while i < len(lines) and lines[i].strip():
                i += 1
            result.append('')
            continue
        
        # Table of Contents page
        if stripped == 'TABLE OF CONTENTS':
            result.append('## Table of Contents')
            result.append('')
            i += 1
            continue
        
        # TOC entries (PART X: ...)
        if re.match(r'^PART \d+:', stripped):
            result.append(f'  - **{stripped}**')
            i += 1
            continue
        
        # "How to Create a Player Character" with numbered steps
        if stripped == 'How To Create A Player Character' or stripped == 'How to Create a Player Character':
            result.append(f'### How to Create a Player Character')
            result.append('')
            i += 1
            continue
        
        # Glossary entries: "word - definition" or "word:"
        if stripped and stripped[0].isupper() and ':' in stripped and not stripped.startswith('http'):
            # Could be glossary or random
            pass
        
        # Copyright line
        if stripped.startswith('©') or stripped.startswith('DUNGEONS & DRAGONS* and D&D'):
            result.append(f'*{stripped}*')
            i += 1
            continue
        
        # Default: add as paragraph
        result.append(stripped)
        i += 1
    
    return '\n'.join(result)

def format_monster_block(lines):
    """Format monster stat block as markdown table."""
    # lines[0] is monster name
    # Then pairs of stat_name: stat_value
    name = lines[0]
    
    stats = {}
    for line in lines[1:]:
        # Handle lines like "Armor Class: 6" or "Armor Class: 6 No. Appearing: 2-12"
        # Some lines have two stats
        parts = re.split(r'\s{2,}', line)
        for part in parts:
            part = part.strip()
            if ':' in part:
                key, val = part.split(':', 1)
                stats[key.strip()] = val.strip()
            elif part:
                # Orphaned value - associate with previous key
                pass
    
    # Format as a table
    table_rows = []
    # Organize into two columns
    left_keys = ['Armor Class', 'Hit Dice', 'Move', 'Fly', 'Swimming', 'Attacks', 'Damage']
    right_keys = ['No. Appearing', 'Save As', 'Morale', 'Treasure Type', 'Alignment']
    
    # Build table
    row_data = []
    for k in left_keys:
        if k in stats:
            row_data.append((k, stats[k]))
    
    max_left = max(len(k+v) for k,v in row_data) if row_data else 20
    
    right_data = []
    for k in right_keys:
        if k in stats:
            right_data.append((k, stats[k]))
    
    # Build markdown table
    md = f'**{name}**\n\n'
    md += '| Stat | Value | Stat | Value |\n'
    md += '|------|-------|------|-------|\n'
    
    max_rows = max(len(row_data), len(right_data))
    for j in range(max_rows):
        left = row_data[j] if j < len(row_data) else ('', '')
        right = right_data[j] if j < len(right_data) else ('', '')
        md += f'| {left[0]} | {left[1]} | {right[0]} | {right[1]} |\n'
    
    return md

def format_spell_entry(lines, start_idx):
    """Format a spell entry."""
    spell_lines = []
    i = start_idx
    while i < len(lines) and lines[i].strip():
        spell_lines.append(lines[i].strip())
        i += 1
    
    if not spell_lines:
        return ''
    
    name_line = spell_lines[0]
    # Extract spell name
    m = re.match(r'^([A-Za-z\s/]+?)\s+Range:', name_line)
    if m:
        name = m.group(1).strip()
    else:
        name = name_line.split('Range:')[0].strip() if 'Range:' in name_line else name_line
    
    # Build spell block
    md = f'**{name}**  \n'
    for s_line in spell_lines:
        s_line = s_line.strip()
        if s_line.startswith(name):
            # Skip the name part, extract Range/Duration
            rest = s_line[len(name):].strip()
            if rest:
                md += f'  - {rest}  \n'
        else:
            md += f'  {s_line}  \n'
    
    return md.strip()

# Process all pages
processed_pages = []
for page_text in pages:
    cleaned = clean_page_text(page_text)
    # Remove extra blank lines
    cleaned = re.sub(r'\n{3,}', '\n\n', cleaned)
    # Join hyphenated words
    cleaned = join_hyphenated(cleaned)
    processed_pages.append(cleaned)

# Join all pages
full_text = '\n\n'.join(processed_pages)

# Join broken lines
full_text = join_broken_lines(full_text)

# Final formatting
markdown = format_document(full_text)

# Clean up: remove excessive blank lines
markdown = re.sub(r'\n{3,}', '\n\n', markdown)

with open('/home/skynet/pdftotext/dungestNdragonsBasicBox.md', 'w') as f:
    f.write('# Dungeons & Dragons Basic Rulebook\n\n')
    f.write(markdown)

print(f'Written {len(markdown)} chars to markdown')
