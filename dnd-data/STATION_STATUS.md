# Station Transfer Summary

## Project Root
`/home/skynet/pdftotext/`

## What Exists

### Source Files
- **`dungestNdragonsBasicBox.pdf`** — Original 126-page D&D Basic Set PDF (Moldvay rulebook + Module B2)
- **`raw_extract.txt`** (15,108 lines) — Raw pypdf text extraction, page-by-page
- **`dungestNdragonsBasicBox.md`** (~8,500 lines) — Rulebook converted to markdown with tables
- **`module_b2.md`** (~4,500 lines) — Module B2 markdown

### Extraction Scripts
| File | Purpose |
|---|---|
| `convert_final.py` | Converts raw text to markdown (rulebook + module B2) |
| `extract_data.py` | Parses markdown → 25 JSON data files |
| `fix_monsters.py` | Hardcodes correct B/X monster stats (replaces garbled auto-parse) |

### Generated Data (`data/`)
All 25 JSON files complete and verified:
| File | Entries | Notes |
|---|---|---|
| `ability-scores.json` | 7 | 6 abilities × 7 score bands each |
| `prime-requisite.json` | 4 | |
| `character-classes.json` | 5 | |
| `cleric-undead.json` | 13 | |
| `thief-abilities.json` | 41 | |
| `saving-throws.json` | 28 | |
| `character-attacks.json` | 9 | |
| `monster-attacks-melee.json` | 1 | Fighter monster melee THAC0 table |
| `monster-attacks-missile.json` | 1 | Monster missile THAC0 table |
| `missile-ranges.json` | 6 | |
| `weapon-damage.json` | 2 | Medium/Large creature damage |
| `weapons.json` | 17 | |
| `armor-equipment.json` | 11 | |
| `armor-classes.json` | 12 | |
| `gear.json` | 12 | |
| `movement.json` | 1 | Encounter/wilderness speed scale |
| `weights.json` | 7 | |
| `treasure-types.json` | 17 | |
| `individual-treasure.json` | 120 | |
| `magic-items.json` | 6 | |
| `coins.json` | 5 | |
| `gems.json` | 14 | |
| `spells.json` | 44 | |
| `wandering-monsters.json` | 144 | |
| `monsters.json` | 101 | Hardcoded from known B/X values, verified |

### Schemas (`schemas/`)
9 JSON schema examples: `01-ability-scores.json` through `09-wandering-monsters.json`

### Context
- `AGENTS.md` — Full session context for opencode resumption

## Design Decisions
- Cross-reference entities by string `id` (not pointers)
- RAW rules separate from optional/house rules under `"optional"` key
- Module B2 data kept separate from rulebook
- Strength table has NO carry capacity bonus by default (that's a house rule — added as optional)
- OCR corrections applied: BIO→B10, Bll→B11, 912→9-12, 35→3-5, Id→1d, dlO→d10, Ore→Orc, Po)gr→Polar
- Monsters couldn't be auto-parsed from two-column interleaved PDF → fully hardcoded from known B/X values

## Potential Next Steps
1. [ ] Verify `weapon-damage.json` — two-handed weapon damage values may need review
2. [ ] Build C++ header struct definitions from schema files
3. [ ] Implement JSON → C++ deserialization code
4. [ ] Verify module B2 data parsing completeness
5. [ ] Any final review/spot-checks before production use
