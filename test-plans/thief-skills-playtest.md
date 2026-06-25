# Thief Skills — Playtest Checklist

**Prerequisites:** Build with `cmake --build build -j`, run `build/tests/bx_crawl_tests` — all 72 tests must pass first.

**Setup:** Launch the game. Choose a **Thief** class character (option `3`).  
Accept any name/abilities (or customize to max DEX for better skill checks).  
After entering the dungeon, the game is in raw-input mode. All keys are single-keypress, no Enter needed.

---

## T1. Open Locks (`o`)

**Test ID** | **Steps** | **Expected Result**
---|---|---
`T1.1` | Walk up to a closed door. Press `o`. | If unlocked: `"You open a door."` and door becomes `/`. If locked as Thief: `"You fail to pick the lock."` or door opens. If locked as non-Thief: `"The door is locked."`
`T1.2` | Stand near a locked door as Thief with `skill_pick_locks` ≥ 15 (L1 default). Press `o` repeatedly until you succeed (expected ~15% success → ~7 tries on avg). | On success: `"You pick the lock and open the door."` — tile changes to `/`. On fail: `"You fail to pick the lock."` — door stays `+`.
`T1.3` | Stand with no adjacent door. Press `o`. | `"Nothing to open."`
`T1.4` | Walk up to a held door (Wizard Lock spell cast on it). Press `o`. | `"The door is magically held — it won't budge!"`

---

## T2. Search — Find Traps (`l`)

**Test ID** | **Steps** | **Expected Result**
---|---|---
`T2.1` | Stand adjacent to a tile that contains a hidden trap. Press `l`. | As Thief with `skill_find_traps` ≥ 10: a roll ≤ skill → `"You find a trap!"` followed by disarm success/fail message. As non-Thief: 1-in-6 → `"You find a trap!"`.
`T2.2` | After finding a trap (trap now visible as `^`), step onto it. | Trap should trigger (damage message) if not disarmed, OR nothing happens if disarmed.
`T2.3` | Stand on a tile with no adjacent traps. Press `l`. | `"You find nothing of interest."`
`T2.4` | As Thief, find a trap and succeed at the disarm roll. | `"You find a trap!"` then `"You skillfully disarm the trap."` — `^` disappears from map.
`T2.5` | As Thief, find a trap and fail the disarm roll. | `"You find a trap!"` then `"You fail to disarm the trap."` — `^` stays visible.

---

## T3. Search — Hear Noise (`l` at doors)

**Test ID** | **Steps** | **Expected Result**
---|---|---
`T3.1` | Stand adjacent to a closed door with a monster behind it. Press `l`. | Thief (≤ skill_hear_noise): `"You hear something moving behind the door!"` Non-Thief (1-in-6): same.
`T3.2` | Stand adjacent to a closed door with no monster behind it. Press `l`. | `"You hear nothing behind the door."` (only if hear-noise roll succeeds; otherwise no door message)
`T3.3` | Stand adjacent to a closed door, no monster. Success on hear-noise. | `"You hear nothing behind the door."`

---

## T4. Move Silently (passive, while hidden)

**Test ID** | **Steps** | **Expected Result**
---|---|---
`T4.1` | Press `i` to hide (Thief). Success → `"You melt into the shadows."`. Move toward a monster that is ≤3 tiles away. | If move-silent roll ≤ skill: you stay hidden (no message). If roll > skill: `"A monster notices you!"` and you emerge from stealth.
`T4.2` | While hidden, move while no monsters are within 3 tiles. | No move-silent messages — you stay hidden.
`T4.3` | After being spotted by a monster (T4.1 fail), check stealth. | Hero should no longer be hidden. Press `i` again to re-hide.

---

## T5. Climb Walls (move into wall, Thief only)

**Test ID** | **Steps** | **Expected Result**
---|---|---
`T5.1` | As a Thief, stand next to a wall tile and move into it (e.g., press `w` to go north into a wall). | On skill success: `"You scale the wall."` — hero glyph appears on top of the wall. On fail: `"You fail to climb the wall."` — hero stays in place.
`T5.2` | After climbing into a wall, move to an adjacent floor tile. | Hero moves back to floor normally.
`T5.3` | As a non-Thief (any other class), try to move into a wall. | Nothing happens (wall blocks movement, no message).
`T5.4` | As Thief with `skill_climb` = 85 (L1), attempt to climb 10 times. | Expect ~8-9 successes out of 10.

---

## T6. Backstab (hidden → attack)

**Test ID** | **Steps** | **Expected Result**
---|---|---
`T6.1` | Hide (`i`) successfully. Move adjacent to a monster. Move into it (attack). | Attack roll gets +4 bonus. On hit: `"Backstab hits for double damage!"` Damage should be 2× normal. Then stealth breaks: `"You emerge from the shadows."`
`T6.2` | Verify backstab damage. | If normal damage = d6 (knife/dagger), backstab should do 2d6. Compare health reduction with/without hiding.
`T6.3` | Attack a monster without hiding. | No backstab message, normal damage.

---

## T7. Traps (triggered on step)

**Test ID** | **Steps** | **Expected Result**
---|---|---
`T7.1` | Walk onto a hidden trap tile (not visible, not found). | Trap triggers: pit → `"You fall into a pit! (N damage)"`, dart → `"A dart shoots out at you! (N damage)"`, poison dart → `"A poisoned dart hits you! (N damage)"` with poison save for extra.
`T7.2` | After triggering, check the tile. | Trap is consumed (`has_trap = false`) — no second trigger.
`T7.3` | Step onto a disarmed trap (found + disarmed via T2). | Nothing happens.
`T7.4` | Fall to 0 HP from trap damage. | `"You have died."` game over (if death screen implemented).
`T7.5` | While hidden (`i`), step onto a hidden trap. | Trap should NOT trigger (`!g.hero.hidden` condition). But this is tricky — depends on order of checks. Confirm behavior.

---

## T8. Stealth Interaction Matrix

**Test ID** | **Action** | **Stealth preserved?**
---|---|---
`T8.1` | Move (no monster nearby) | Preserved
`T8.2` | Move (monster within 3 tiles, move-silent success) | Preserved
`T8.3` | Move (monster within 3 tiles, move-silent fail) | Broken (`"A monster notices you!"`)
`T8.4` | Attack (backstab) | Broken after attack
`T8.5` | Open door (`o`) | Broken
`T8.6` | Close door (`p`) | Broken
`T8.7` | Search (`l`) | **Preserved** (new feature)
`T8.8` | Get item (`g`) | Broken
`T8.9` | Rest (`r`) | Preserved
`T8.10` | Cast spell (`m`) | Broken
`T8.11` | Turn undead (`t`) | Broken
`T8.12` | Throw (`k`/`f`) | Broken
`T8.13` | Hide again (`i`) | Preserved
`T8.14` | Wait (space) | Preserved
`T8.15` | Help (`h`) | Preserved

---

## T9. Edge Cases

**Test ID** | **Steps** | **Expected Result**
---|---|---
`T9.1` | Try to Search while not adjacent to anything. | `"You find nothing of interest."`
`T9.2` | Try to pick a lock as non-Thief. | `"The door is locked."` — no roll, no lockpick attempt.
`T9.3` | Thief with 0% skill (if artificially set) tries any skill. | Always fails (0 ≤ roll never true).
`T9.4` | Thief with 100% skill (if artificially set) tries any skill. | Always succeeds.
`T9.5` | Monster steps on a trap tile (monster movement). | Traps are only hero-triggered in current implementation. Confirm no crash.
`T9.6` | Generate a new level via stairs, verify traps/locks exist. | Descend/ascend. New map should have traps in rooms and locked doors.

---

## Tracking

If a test fails, note the **Test ID** and the **observed vs expected** behavior.  
Example: `T2.1 — "You find a trap!" message missing when standing next to known trap tile.`

File issues/bugs with prefix matching the feature:
- `[LOCK]` – Open Locks (T1.x)
- `[TRAP]` – Traps (T2.x, T7.x)
- `[NOISE]` – Hear Noise (T3.x)
- `[SILENT]` – Move Silently (T4.x)
- `[CLIMB]` – Climb Walls (T5.x)
- `[BACKSTAB]` – Backstab (T6.x)
- `[STEALTH]` – Stealth interaction (T8.x)
- `[EDGE]` – Edge cases (T9.x)
