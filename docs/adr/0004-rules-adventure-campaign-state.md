# ADR 0004: Rules vs Adventure vs Campaign State

## Status

Accepted

## Context

The architecture review separates reusable B/X rules, immutable adventure module
definitions, and mutable campaign progress. Mixing these concerns would make
modules harder to reuse and saves larger and more brittle.

Phase work introduced `RulesCatalog`, `CampaignState`, manifest/module content
DTOs, and a module session-entry seam as the first implementation boundaries.

## Decision

Immutable rules content, immutable adventure definitions, and mutable campaign
state are architecturally distinct.

The engine executes rules against runtime state. Modules define immutable
adventure content. Campaign saves record what changed during play rather than
duplicating static module definitions.

## Consequences

- Rule data such as monsters, equipment, spells, and treasure tables belongs in
  rules content/catalogs.
- Module data such as locations, maps, descriptions, doors, encounters, traps,
  and treasures belongs in module packages.
- Campaign state should persist mutable facts such as current location, door
  state, trap state, treasure state, monster instance state, flags, party state,
  and RNG/replay state.
- Module save/reload should reconstruct from immutable rules and module content,
  then apply campaign deltas.
