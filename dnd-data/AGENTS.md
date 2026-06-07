# D&D Basic Set PDF Extraction Project

## Goal
Convert 126-page D&D Basic Set PDF (Moldvay) into structured JSON for a B/X crawler database.

## Approach
- pypdf for text extraction (no OCR needed, text is selectable)
- `raw_extract.txt` → markdown conversion → JSON extraction pipeline
- Split into rulebook (`dungestNdragonsBasicBox.md`) and Module B2 (`module_b2.md`)

## Scripts in `/home/skynet/pdftotext/`
| Script | Purpose |
|---|---|
| `extract_data.py` | Main extraction: parses markdown → 26 JSON files in `data/` |
| `fix_monsters.py` | Post-processing: replaces auto-parsed monsters with hardcoded B/X stats |
| `convert_final.py` | Converts raw text to markdown tables |

## Generated Data (`data/`)
All 26 JSON files complete and verified:
- `ability-scores.json` (7 entries) — 6 ability scores x 7 score bands each
- `prime-requisite.json` (4 entries)
- `character-classes.json` (5 entries)
- `cleric-undead.json` (13 entries)
- `thief-abilities.json` (41 entries)
- `saving-throws.json` (28 entries)
- `character-attacks.json` (9 entries)
- `monster-attacks.json` (2 entries — melee + missile)
- `missile-ranges.json` (6 entries)
- `weapon-damage.json` (2 entries)
- `weapons.json` (17 entries)
- `armor-equipment.json` (11 entries)
- `armor-classes.json` (12 entries)
- `gear.json` (12 entries)
- `movement.json` (1 entry)
- `weights.json` (7 entries)
- `treasure-types.json` (17 entries)
- `individual-treasure.json` (120 entries)
- `magic-items.json` (6 entries)
- `coins.json` (5 entries)
- `gems.json` (14 entries)
- `spells.json` (44 entries)
- `wandering-monsters.json` (144 entries)
- **`monsters.json` (101 entries)** — hardcoded from known B/X values, verified

## Schemas (`schemas/`)
9 JSON schema examples in `schemas/`.

## Known Correct Values for B/X Moldvay Basic
- **Strength**: No carry capacity bonus (that's a house rule — added as `"optional"` in schema)
- **7 score bands**: 3, 4-5, 6-8, 9-12, 13-15, 16-17, 18
- **OCR corrections**: BIO→B10, Bll→B11, 912→9-12, 35→3-5, Id→1d, dlO→d10, Ore→Orc, Po)gr→Polar

## Key Design Decisions
- Cross-reference entities by string `id` (not pointers)
- RAW rules separate from optional/house rules via `"optional"` schema section
- Module B2 kept separate from rulebook
- C++ consumer loads only needed namespaced JSON files
