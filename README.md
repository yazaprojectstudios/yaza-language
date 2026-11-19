# YAZA

[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](LICENSE)
[![Release](https://img.shields.io/badge/Release-v1.0.0-green.svg)](https://github.com/yazaprojectstudios/yaza-language/releases)
[![Build Matrix](https://img.shields.io/badge/Platforms-macOS%20%7C%20Linux%20%7C%20Windows-brightgreen.svg)](https://github.com/yazaprojectstudios/yaza-language/actions)
[![Zero-GC](https://img.shields.io/badge/Memory-Zero--GC%20%28Scoped%20Arenas%29-mint.svg)](#memory-model-arena_scope)
[![Benchmarks](https://img.shields.io/badge/HTTP%2F1.1-93%2C854%20req%2Fs%20%40%2078%C2%B5s-yellowgreen.svg)](#benchmarks)
[![Arena-SSR](https://img.shields.io/badge/Arena--SSR%20MVC-6.5x%20Next.js%20%40%203.17%20MB%20RSS-teal.svg)](#fullstack-web--mvc-arena-ssr-19-nov-2026)

**YAZA** is a self-hosted systems programming language designed for high-concurrency cloud backends and single-binary fullstack web applications. It compiles to native machine code via **LLVM 18**, eliminates garbage collection pauses using deterministic **Scoped Arenas (`arena_scope`)**, and ships with **YazaBase**, a built-in ACID B+Tree SQL storage engine.

---

## Key Features

- **Self-Hosted Compiler & Native LLVM Backend:** Generates optimized binaries for macOS (`arm64`, `x86_64`), Linux (`x86_64`, `aarch64`), and Windows (`x86_64 PE32+`).
- **Zero-GC Scoped Arena Architecture:** Replaces global garbage collection with bounded `arena_scope` blocks. Allocate with $O(1)$ bump pointers, rewind memory in a single step upon request exit (1,000,000 scopes reclaimed in 521 ms, 1.56 MB Peak RSS).
- **Single-Binary Fullstack MVC (`--mvc` / `--fullstack` & `--web`):** Built-in `std/web/*` stack (compiled type-safe HTML5 Arena-SSR views, Hypermedia Fragments via `X-Yaza-Fragment`, YAZA UI v1.0 design system, signed `HttpOnly` cookies, constant-time CSRF, strict Web CSP, and safe ETag static file serving) with compile-time layer boundary enforcement (`yaza arch check`).
- **Embedded ACID SQL Engine (YazaBase):** Built-in B+Tree indexed storage with ARIES WAL recovery, parameterized query binding, non-reparse VFS path confinement, and zero external database dependencies.
- **Pluggable Database Adapters:** Native wire-protocol clients for **PostgreSQL** (v3 + SCRAM-SHA-256), **MySQL** (v10 + caching_sha2_password), **MSSQL** (TDS), and **NoSQL** (Redis RESP / MongoDB BSON).
- **Hardware-Aware API Resource Profiles:** Tune memory ceilings and concurrency per deployment target (`low_memory`, `balanced`, `throughput`) with Adaptive Query Budgeting (AQB) verified at boot.
- **Enterprise-Grade Security (11/11 CI Safety Gates):** Compile-time borrow checker, AddressSanitizer reset poisoning, ThreadSanitizer concurrency isolation (0 data races across 200k scopes on 8 workers), static web XSS/CSP audit (`yaza audit --security`), and constant-time HMAC-SHA256 JWT/YWT + Argon2id authentication.

---

## Installation

### One-Line Installers

#### macOS & Linux
```sh
curl -fsSL https://yazastudios.id/install.sh | sh
```

#### Windows (PowerShell)
```powershell
irm https://yazastudios.id/install.ps1 | iex
```

### Manual Release Archives
Pre-built archives for all supported targets are available on the [Releases](https://github.com/yazaprojectstudios/yaza-language/releases) page:
- `yaza-1.0.0-darwin-arm64.tar.gz` (Apple Silicon)
- `yaza-1.0.0-darwin-x64.tar.gz` (macOS Intel)
- `yaza-1.0.0-linux-x64.tar.gz` (Linux x86-64)
- `yaza-1.0.0-linux-arm64.tar.gz` (Linux ARM64)
- `yaza-1.0.0-windows-x64.zip` (Windows x64 Native PE32+)

Extract the archive and add its `bin/` directory to your system `PATH`.

### Build from Source
The repository includes compressed LLVM bootstrap files for immediate compilation:

```sh
# Ubuntu / Debian dependencies
sudo apt-get install clang xz-utils libargon2-dev

# macOS Homebrew dependencies
brew install argon2 xz

# Compile native toolchain
./scripts/build-host.sh
export PATH="$PWD/build:$PATH"
yaza --version
```

---

## Quick Start

### 1. Hello World
Create `hello.yz`:
```yaza
start:
    say "Hello, world!"
```

Run immediately:
```sh
yaza run hello.yz
```

Compile an optimized standalone binary:
```sh
yaza build --release hello.yz -o hello
./hello
```

---

## Project Starters & CLI Tooling

YAZA provides production-ready project generators that scaffold clean architectures with full test coverage, Dockerfiles, and API specifications.

### Fullstack MVC Monolith Starter (`--mvc` / `--fullstack`)
Generates a single-binary **Arena-SSR MVC** monolith (RFC 0009) combining frontend and backend in one YAZA project: **native `view` template syntax** (zero `list_push` boilerplate, auto-escaped `{{ expr }}` and trusted `{! raw !}` composition), **YAZA UI v1.0** landing page (`GET /`) + workspace dashboard (`/dashboard`), live **Hypermedia Fragments** (`X-Yaza-Fragment`), embedded YazaBase ACID SQL CRUD, Argon2id login with signed `HttpOnly` session cookies, constant-time CSRF protection, strict Web CSP (`style-src 'self'; script-src 'self'`), and JSON API parity (`/api/v1/tasks`):

```yaza
use "std/web/view"

view render_card(title: Text, count: Number, body_html: Text):
    <section class="yz-card">
      <h2>{{ title }}</h2>
      if count > 0:
          <span class="yz-badge">{{ number_text(count) }} items</span>
      {! body_html !}
    </section>
```

```sh
yaza new --mvc my_app
cd my_app

# Provision local YazaBase credentials
mkdir -p data
yaza db set-user ./data admin 'AdminPass_2026_Secure'
export YAZABASE_USER=admin
export YAZABASE_PASSWORD='AdminPass_2026_Secure'
export YAZA_AUTH_SECRET="$(yaza secret token)"

# Enforce MVC layer boundaries, security CSP/XSS rules, and build
yaza arch check
yaza audit --security
yaza check
yaza build --release src/main.yz -o my_app
YAZA_SELF_TEST=1 ./my_app

# Start live Fullstack MVC server on http://127.0.0.1:8080
./my_app
```

For a lightweight SSR web site without a database layer, use `yaza new --web my_site`.

### REST API Starter (`--api`)
Generates a layered architecture with route controllers, YazaBase SQL CRUD, JWT/RBAC middleware, and HTTP/1.1 keep-alive multi-worker loop:

```sh
yaza new --api store_api
cd store_api

# Provision local YazaBase credentials
mkdir -p data
yaza db set-user ./data admin 'AdminPass_2026_Secure'
export YAZABASE_USER=admin
export YAZABASE_PASSWORD='AdminPass_2026_Secure'

# Verify types & run built-in self-test
yaza check
yaza build --release src/main.yz -o store_api
YAZA_SELF_TEST=1 ./store_api

# Start live HTTP server on http://127.0.0.1:8080
yaza run
```

### Cloud-Native Microservice Starter (`--microservice`)
Scaffolds a Hexagonal (Ports & Adapters) architecture with domain entities, event dispatchers, Kubernetes `/healthz`, `/readyz`, and Prometheus `/metrics` probes:

```sh
yaza new --microservice order_service
cd order_service

mkdir -p data
yaza db set-user ./data admin 'AdminPass_2026_Secure'
export YAZABASE_USER=admin
export YAZABASE_PASSWORD='AdminPass_2026_Secure'

yaza check
yaza run
```

### Specification Generators
Export interactive OpenAPI 3.1 Swagger and Postman collections directly from declared route contracts:
```sh
yaza generate --swagger -o openapi.json
yaza generate --postman -o postman.json
```

---

## Memory Model: `arena_scope`

YAZA avoids stop-the-world Garbage Collection pauses by scoping memory lifecycles to bounded regions:

```yaza
# Bounded 1 MiB arena per incoming connection
arena_scope 1048576:
    let req = http_parse_request(raw)
    let res = dispatch_http_request(req)
    tcp_write(client_fd, res, 5000)
# Scope exit: Entire slab memory rewinds in a single pointer reset
```

- **Allocation Cost:** $O(1)$ pointer increment along a contiguous cache-aligned buffer.
- **Reclaim Cost:** Instantaneous pointer reset back to the base offset; zero individual heap frees.
- **Cross-Thread Safety:** Scopes are assigned unique worker owner tokens (`scope_owner_v1`), preventing cross-task mutation or stale pointer retention.

---

## Hardware-Aware Resource Profiles (`yaza.toml`)

Configure operational memory budgets and Adaptive Query Budgeting (AQB) directly in `yaza.toml` without modifying source code:

```toml
[api.resources]
# Built-in profiles: "low_memory" | "balanced" (default) | "throughput"
profile = "balanced"
arena_mode = "adaptive"
process_arena_limit = "512MiB"
request_arena_floor = "256KiB"
request_arena_initial = "1MiB"
request_arena_ceiling = "64MiB"
max_in_flight = 32
max_query_work_units = 100000000
```

| Parameter | `low_memory` (Fixed) | `balanced` (Adaptive, Default) | `throughput` (Adaptive) | Purpose |
| :--- | :--- | :--- | :--- | :--- |
| **Process Cap** | `128 MiB` | `512 MiB` | `2 GiB` | Hard upper memory limit for the process |
| **Req Arena Initial**| `256 KiB` | `1 MiB` | `4 MiB` | Initial allocation slab for each request |
| **Req Arena Floor**  | `64 KiB`  | `256 KiB`| `1 MiB` | Minimum slab retention floor |
| **Req Arena Ceiling**| `8 MiB`   | `64 MiB` | `256 MiB` | Maximum burst allocation per request |
| **Max In-Flight**    | `8`       | `32`     | `128` | Concurrency slots before atomic HTTP 503 load-shedding |
| **Target Workload**  | Edge, IoT, Micro-VMs | Production REST APIs, K8s Pods | High-QPS Analytics, Burst Services |

At startup, YAZA proves the invariant:
$$\text{floor} \le \text{initial} \le \text{ceiling} \le \text{process\_arena\_limit}$$
Invalid configurations fail immediately during boot with clear operator diagnostics.

---

## Database Support

YAZA provides first-class standard library drivers with wire-protocol implementations:

1. **YazaBase (`std/db/yazabase`):** Embedded ACID SQL engine. Features B+Tree indexing, write-ahead logging (ARIES), parameterized SQL binding (`$1, $2, ...`), and transactional safety.
2. **PostgreSQL (`std/db/postgres`):** Pure YAZA client implementing PostgreSQL Wire Protocol v3, SCRAM-SHA-256 authentication, and typed `DbValue` mapping.
3. **MySQL (`std/db/mysql`):** Wire Protocol v10 client with `caching_sha2_password` scramble and command batching.
4. **Microsoft SQL Server (`std/db/mssql`):** Tabular Data Stream (TDS) wire protocol with prelogin handshake and batch execution.
5. **NoSQL Stores (`std/db/nosql`):** Redis RESP serialization for key-value storage and MongoDB BSON / `OP_MSG` wire framing for document stores.

### YazaBase CLI
Inspect and manipulate databases directly from the terminal:
```sh
# Interactive SQL REPL
yazabase shell ./data my_db

# Execute direct parameterized SQL
yazabase sql ./data my_db "SELECT id, sku, price_minor FROM products WHERE available = $1" --param boolean:true

# Database backup & restore
yazabase backup ./data my_db ./backups
yazabase restore ./backups my_db ./data_restored
```

---

## Benchmarks

Independent loopback benchmarks conducted on **18 November 2026** (macOS arm64, 10 logical CPUs, local TCP loopback `127.0.0.1`, 2,000 keep-alive requests across 3 randomized rounds, 0 failed requests):

### HTTP/1.1 Loopback Throughput (`GET /api/response`, 1-Item JSON, $c=8$)

| Runtime | Requests / sec | Median Latency ($p50$) |
| :--- | :--- | :--- |
| **YAZA v1.0.0 (`-O2`)** | **93,854 req/s** | **78 µs** |
| **Rust (`rustc -O`)** | 90,163 req/s | 84 µs |
| **Node.js v26** | 88,488 req/s | 89 µs |
| **Nim (`-d:release`)** | 86,253 req/s | 87 µs |
| **Go 1.27.3** | 83,480 req/s | 87 µs |
| **PHP 8.5** | 27,457 req/s | 291 µs |
| **Python 3.14** | 18,950 req/s | 422 µs |

*At higher concurrency ($c=32$): Go achieves 111.9k req/s, Rust achieves 109.7k req/s, and YAZA sustains 102.8k req/s.*

### Memory Footprint Under Load (200,000 Request Scopes)

| Runtime | Peak RSS | Binary Size | Memory vs YAZA |
| :--- | :--- | :--- | :--- |
| **Nim (`-d:release`)** | 1.44 MB | 99 KB | $0.92\times$ |
| **YAZA v1.0.0 (`-O2`)** | **1.56 MB** | **189.9 KB** | **Baseline ($1\times$)** |
| **Rust (`rustc -O`)** | 1.59 MB | 472 KB | $1.02\times$ |
| **Go 1.27.3** | 4.48 MB | 2.21 MB | $2.87\times$ |
| **Python 3.14** | 8.30 MB | N/A (Interpreter) | $5.32\times$ |
| **Node.js v26** | **53.09 MB** | N/A (V8 Engine) | **$34\times$ more RAM** |

*Deterministic reclaim verification: 1,000,000 request scopes (`10 × 256 B`) reclaimed in **521.00 ms** at **1.59 MB** RSS.*

### Fullstack Web & MVC (Arena-SSR, 19 Nov 2026)

Reproducible **8-framework HTTP/1.1 SSR & Web benchmark** (`darwin arm64`, `c = 8`, isolated OS processes, 16 XSS-escaped rows + CSRF inputs + strict Web CSP + JSON API, 0 errors) comparing compiled **YAZA v1.0.0 native `view` Arena-SSR (`-O2`)** against Go, Node.js, Fastify, Express, PHP, React 19 SSR, and Next.js 16 Production SSR:

| Framework / Runtime (`c = 8`) | Full SSR HTML (p50 · p95) | Fragment (`#task-board`) | JSON API (16 items) | Peak RSS | Cold Start | Footprint |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **YAZA v1.0.0 Arena-SSR (`-O2`, native `view`)** | **`29,024 req/s`** (`167 µs` · `326 µs`) | **`37,990 req/s`** (`123 µs` · `246 µs`) | `33,437 req/s` (`138 µs`) | **`3.17 MB`** | **`31.8 ms`** | **`208.4 KB` binary** |
| **Go `1.27.3` (`net/http` + `html/template`)** | `28,743 req/s` (`162 µs` · `464 µs`) | `27,991 req/s` (`173 µs` · `442 µs`) | `41,848 req/s` (`107 µs`) | `22.05 MB` (`7.0×`) | `614.1 ms` | `12.18 MB` binary |
| **Node.js `v26.0.0` (raw `node:http`)** | `27,430 req/s` (`182 µs` · `414 µs`) | `28,733 req/s` (`178 µs` · `359 µs`) | `38,894 req/s` (`96 µs`) | `73.86 MB` (`23.3×`) | `113.6 ms` | V8 runtime |
| **Fastify `5.12.8` (Node.js v26)** | `25,295 req/s` (`202 µs` · `450 µs`) | `25,772 req/s` (`199 µs` · `415 µs`) | `39,740 req/s` (`105 µs`) | `86.44 MB` (`27.3×`) | `163.6 ms` | Node + `node_modules` |
| **Express `5.3.0` (Node.js v26)** | `22,553 req/s` (`238 µs` · `530 µs`) | `22,492 req/s` (`239 µs` · `487 µs`) | `36,257 req/s` (`118 µs`) | `77.81 MB` (`24.5×`) | `136.3 ms` | Node + `node_modules` |
| **PHP `8.5.6` (built-in server)** | `12,001 req/s` (`435 µs` · `854 µs`) | `13,425 req/s` (`366 µs` · `785 µs`) | `14,502 req/s` (`338 µs`) | `29.47 MB` (`9.3×`) | `136.0 ms` | PHP 8.5 runtime |
| **React `19.3.1` SSR (`renderToString`)** | `6,304 req/s` (`1,006 µs` · `2,075 µs`) | `6,953 req/s` (`913 µs` · `1,830 µs`) | **`45,394 req/s`** (`75 µs`) | `140.75 MB` (`44.4×`) | `107.4 ms` | Node + React 19 |
| **Next.js `16.4.6` Production SSR (`next start`)** | `4,473 req/s` (`1,386 µs` · `2,711 µs`) | — | `10,693 req/s` (`439 µs`) | `212.30 MB` (`67.0×`) | `197.2 ms` | `365 MB` `node_modules` + `.next` |

*Static asset delivery (`20,158 B` YAZA UI v1.0 CSS): YAZA **`63,268 req/s`** (`200 OK`) & **`82,539 req/s`** (`304 ETag`).*

See [docs/benchmarks/fullstack-ssr-2026-11-19.md](docs/benchmarks/fullstack-ssr-2026-11-19.md), `scripts/bench-web-frameworks.mjs`, and [yazastudios.id/benchmarks](https://yazastudios.id/benchmarks/#fullstack-ssr) for full methodology and reproduction steps.

---

## Security & Verification

YAZA v1.0.0 enforces **11 out of 11 verified safety gates** in continuous integration:

1. **ThreadSanitizer (`TSan`):** 200,000 concurrent request scopes across 8 async worker threads completed with **0 data races** (`scripts/validate-runtime-v1-sanitizers.sh`).
2. **AddressSanitizer (`ASan`):** 100% of use-after-free and dangling pointer attempts trapped via deterministic arena-reset poisoning.
3. **Compile-Time Ownership & Borrow Checker:** Rejects use-after-move, overlapping mutable borrows, and cross-thread escapes at compile time with 0.0 ms runtime overhead (`scripts/validate-core2-features.sh`).
4. **Non-Reparse VFS Confinement (`yazabase_vfs_v1`):** Blocks directory traversal (`../`), null byte injections, and symlink escapes outside the database root under ASan, BSan, and TSan (`scripts/validate-yazabase-vfs-v1.sh`).
5. **Storage Fuzzing & Recovery:** 1,920 decoder bounds fuzz cases across 15 binary formats, and 212 simulated power-loss / `fsync` crash boundaries verified with ARIES WAL CRC32C recovery (`scripts/validate-yazabase-storage-decoder-fuzz.sh`, `scripts/validate-yazabase-sync-recovery.sh`).
6. **Constant-Time Authentication:** HMAC-SHA256 JWT/YWT token generation and Argon2id password hashing resistant to side-channel timing attacks.

---

## Editor Support

- **Visual Studio Code:** Install the official extension from the [VS Code Marketplace](https://marketplace.visualstudio.com/) or [Open VSX](https://open-vsx.org/) (`yaza-project-studios.yaza-language`), providing syntax highlighting, snippets, and diagnostic integration.
- **Tree-sitter & LSP:** Native grammar definitions located in `editors/`.

---

## Contributing, Security & License

- **Contributing Guide:** [CONTRIBUTING.md](CONTRIBUTING.md)
- **Security Policy:** [SECURITY.md](SECURITY.md)
- **License:** [Apache License 2.0](LICENSE) — free for commercial and non-commercial use.
