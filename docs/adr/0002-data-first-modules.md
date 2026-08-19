# ADR 0002: Data-First Modules

## Status

Accepted

## Context

The architecture review recommends adventure modules that can prove gameplay
concepts without embedding one-off C++ logic. Testing Tomb is the first module
proof of concept.

Executable plugins or scripts would introduce ABI, sandboxing, versioning, and
security problems before the module contract is understood.

## Decision

Adventure modules are declarative data packages and do not initially contain
executable code.

The engine owns reusable concepts such as manifests, locations, maps, doors,
encounters, traps, treasures, and state transitions. Modules provide data using
those concepts.

## Consequences

- Testing Tomb must not require module-specific C++ logic.
- When a module needs behavior, add a generally useful engine/module contract
  concept instead of special-casing that module.
- Module loading should remain strict and validate IDs, namespaces, paths, and
  references before content enters runtime state.
- Scripting or plugin support remains deferred until there is a demonstrated
  need that cannot be met by data plus existing engine operations.
