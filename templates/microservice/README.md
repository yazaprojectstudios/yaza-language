# {{project_name}} — Cloud-Native YAZA Microservice

High-performance, memory-bounded microservice built with the **YAZA LLVM toolchain**, **Core 2 Native Async/Await**, and the **YazaBase embedded engine**.

## Modern Architecture Pattern: Hexagonal (Ports & Adapters) + Async Concurrency

- **Domain Layer (`src/domain/`)**: Pure business entities (`Order`), domain invariants (`order_is_valid`), and asynchronous domain events (`OrderCreatedEvent`).
- **Ports Layer (`src/ports/`)**: Asynchronous interfaces for database persistence (`order_repository_find_async`, `order_repository_save_async`) returning `Task[T]`.
- **Adapters Layer (`src/adapters/`)**: Concrete inbound HTTP adapter (`handle_microservice_request`) with asynchronous endpoint dispatching (`spawn` / `await`), Prometheus telemetry, and Kubernetes probes.
- **Concurrency & Memory Model**: Native Core 2 `spawn` and `.await` running over multi-core worker threads with **Zero-GC Isolated Task Arenas** (`yaza_rt_v1_task_arena_handle`), preventing cross-task interference.
- **Resource Profile (`yaza.toml`)**: Configured with `throughput` profile featuring Adaptive Query Budgeting (AQB) capped at 512 MiB with 64 concurrency slots.

## Endpoints

| Method | Path | Concurrency | Description |
|---|---|---|---|
| `GET` | `/healthz` | Async | Kubernetes liveness probe |
| `GET` | `/readyz` | Async | Kubernetes readiness probe |
| `GET` | `/metrics` | Async | Prometheus metrics scrape target |
| `GET` | `/api/v1/orders` | Async | Non-blocking query order collection |
| `POST` | `/api/v1/orders` | Async | Create order record & dispatch event |

## Development & Run

```bash
# Check syntax and typing
yaza check

# Run locally in development mode
yaza run

# Build release binary
yaza build --release src/main.yz -o build/service

# Build and run Docker container
docker compose up --build
```
