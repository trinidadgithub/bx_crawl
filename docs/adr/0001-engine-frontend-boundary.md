# ADR 0001: Engine / Frontend Boundary

## Status

Accepted

## Context

BX Crawl started with text-frontend flow, terminal rendering, gameplay sequencing,
and direct `Game` state access closely coupled. The architecture review identified
this as a blocker for alternate frontends and reliable architecture tests.

Implemented follow-up phases introduced `GameSession`, `ActionResult`, structured
events, and `GameStateSnapshot` as the current boundary pieces.

## Decision

Frontends interact with gameplay through commands, action results/events, and
snapshots rather than coordinating rules directly or rendering from raw
`Game`/`Entity`/`Tile` internals.

`Game` remains the authoritative mutable runtime state inside the engine.
Transitional direct accessors may remain while startup save/load, character
generation, and legacy text flows are migrated.

## Consequences

- Text, GUI, TUI, and tests can share one rules implementation.
- New frontend behavior should prefer `GameSession::apply(...)`,
  `GameSession::events()`, and `GameSession::snapshot()`.
- Engine APIs should expose stable presentation state instead of terminal output.
- Remaining raw `Game` access should be treated as transitional and reduced
  incrementally.
