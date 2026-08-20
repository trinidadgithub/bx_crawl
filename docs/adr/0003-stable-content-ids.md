# ADR 0003: Stable Content IDs

## Status

Accepted

## Context

Rules content and module content need to reference each other across source-code,
data, and save-file changes. Array positions, enum ordinals, and implementation
indexes are fragile because they can change when data is reordered.

The architecture follow-up introduced `ContentId` and rules catalog adapters for
stable `bx.*` references. Module work now uses `module.<module_id>.*` namespaces.

## Decision

Rules and module content reference each other through stable textual IDs, not
array positions or implementation-specific indexes.

Current namespace examples:

- `bx.monster.goblin`
- `bx.moldvay_basic`
- `module.testing_tomb.location.entrance`
- `module.testing_tomb.door.entrance_to_room_a`

## Consequences

- Saves and modules can survive code/data reordering.
- Loaders must validate content ID syntax and expected namespaces.
- Runtime/campaign state should store source IDs for immutable definitions rather
  than implicit indexes.
- Compatibility decisions should be made around stable IDs and versioned content,
  not container order.
