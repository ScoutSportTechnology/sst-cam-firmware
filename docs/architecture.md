# Architecture

This project follows a **hexagonal (ports and adapters) architecture**, organized into **modules**. Each module is built to keep business logic independent from frameworks and I/O details (GStreamer, filesystem, CLI, etc.).

## Goals

- Keep core logic testable without external dependencies.
- Make integrations (camera backends, encoders, storage, UI) swappable.
- Prevent domain logic from “leaking” infrastructure concerns.
- Keep dependencies flowing inward (toward domain).

## Repository layout

- The repository has a **single `src/` tree**.
- Headers (`.hpp`) and sources (`.cpp`) live together (no separate `include/` folder).
- Code is divided into **modules**.

Example shape:

``` md
src/
├── [module]/
│   ├── adapters/
│   ├── app/
│   ├── domain/
│   ├── ports/
│   └── utils/            (optional)
└── ...
```

## Modules

A *module* is a cohesive unit of functionality (e.g., `config`, `capture`, `pipeline`, `decision`, etc.).
Modules own their own ports, adapters, and domain types.

Cross-module dependencies should be deliberate and minimal. Prefer depending on other modules through ports or small domain contracts rather than reaching into adapter implementations.

## Hexagonal layers (within a module)

Each module uses these subfolders:

### `domain/`

The core of the module.

Contains:

- Domain entities and value objects
- Pure logic (no I/O)
- Domain errors / invariants
- Domain types (strong types preferred)

Rules:

- Must not depend on `adapters/`
- Avoid depending on heavy external libraries
- No logging required inside domain (prefer returning errors/status)

### `ports/`

Interfaces that define how the domain/app communicates with the outside world.

Contains:

- Abstract interfaces (ports)
- DTOs/contracts that cross the boundary
- Minimal types needed to call into/out of the module

Rules:

- Ports are stable contracts.
- Ports should not expose framework-specific types.

### `app/`

Application use-cases / orchestration.

Contains:

- Use-case services (e.g., “start capture”, “process frame”, “apply config”)
- Coordination across domain objects and ports
- Validation and mapping between domain and adapter-friendly formats

Rules:

- `app/` depends on `domain/` and `ports/`
- `app/` must not depend directly on external runtime details unless isolated behind ports

### `adapters/`

Implementation details and integrations.

Contains:

- GStreamer/OpenCV/etc. integration code
- Filesystem, networking, OS interaction
- Concrete implementations of ports

Rules:

- Adapters depend inward (`ports/`, `app/`, sometimes `domain/`)
- Domain must never depend on adapters

### `utils/` (optional)

Small shared helpers that don’t belong to domain rules.

Contains:

- Small utilities (time helpers, small conversions)
- Should stay lightweight and boring

Rules:

- Do not let `utils/` become a dumping ground.
- If it grows teeth, it probably belongs in `domain/` or `app/`.

## Dependency direction

Allowed dependency direction (typical):

- `adapters/` → `ports/`, `app/`, `domain/`
- `app/` → `ports/`, `domain/`
- `ports/` → (ideally) `domain/` types only, or standalone contracts
- `domain/` → (nothing inward) only standard library / minimal low-level libs

Forbidden:

- `domain/` → `adapters/`
- `ports/` → `adapters/`
- “Helper” utilities that import GStreamer/OpenCV and get used by domain/app

## Naming and style

- C++ follows **Google C++ Style Guide**.
- Prefer explicit names and strong types over cleverness.
- Keep headers minimal; avoid leaking heavy includes across modules.
- Where appropriate, use forward declarations to reduce compile dependencies.

## Testing expectations (high level)

- Domain and app logic should be testable with fake/mock ports.
- Adapters are tested with integration/smoke tests where needed (e.g., GStreamer pipeline smoke).

## Practical boundary examples

- GStreamer pipeline creation lives in `capture/adapters/`.
- Frame data structures and invariants live in `capture/domain/` (or a shared `frames/domain/` if truly cross-cutting).
- “Capture one frame and publish to buffer” orchestration lives in `capture/app/`.
- The buffer interface (“latest frame”, “subscribe”, etc.) is a port; implementations can be adapter-specific.
