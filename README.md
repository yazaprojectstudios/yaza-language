# YAZA

[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](LICENSE)
[![Release](https://img.shields.io/badge/Release-v1.0.2-green.svg)](https://github.com/yazaprojectstudios/yaza-language/releases)
[![Build Matrix](https://img.shields.io/badge/Platforms-macOS%20%7C%20Linux%20%7C%20Windows%20experimental%20%7C%20Android%20%26%20iOS%20ARM64-yellow.svg)](https://github.com/yazaprojectstudios/yaza-language/actions)
[![Zero-GC](https://img.shields.io/badge/Memory-Zero--GC%20%28Scoped%20Arenas%29-mint.svg)](#memory-model-arena_scope)
[![Benchmarks](https://img.shields.io/badge/Arena--SSR%20HTML-53%2C668%20req%2Fs%20%40%207.05%20MB%20RSS-yellowgreen.svg)](#post-audit-ecosystem-benchmarks-21-nov-2026--apple-m5--macos-265-arm64)
[![YBIF](https://img.shields.io/badge/YBIF%20Binary%20Format-6.22x%20Scalar%20%7C%202.13x%20SQL%20vs%20JSON-teal.svg)](#database-support)

**YAZA** is a self-hosted systems programming language designed for high-concurrency cloud backends, single-binary fullstack web applications, and native mobile engines. It compiles to native machine code via **LLVM**, eliminates garbage collection pauses using deterministic **Scoped Arenas (`arena_scope`)**, and ships with **YazaBase** (a built-in ACID B+Tree SQL storage engine), the **YBIF** binary interchange format, and the **Mobile Native Engine (RFC 0010)** for Android and iOS.

---

## Key Features & Four Core Technology Pillars (`v1.0.2`)

1. **Mobile Native Engine (RFC 0010 — Android & iOS ARM64):**
   - **LLVM AOT ARM64 Native Core Engine** (zero JIT, zero Garbage Collector) structured with modular MVI/Outbox `src/` modules (`state/`, `actions/`, `domain/`, `effects/`), `"YBIF"` binary state snapshots, task arenas, and embedded `YazaBase` (`--export-request` via `std/db/yazabase_mobile_kv`), connected to `"Welcome to YAZA Mobile"` 100% native Dumb UI (**Kotlin** on Android, **Swift / SwiftUI** on iOS) through the versioned **Thin C ABI Bridge** (`runtime/mobile/include/yaza_mobile_v1.h`, featuring `YAZA_MOBILE_EVENT_V1_ACTION = 4`, `512 KiB` max input payload, explicit buffer ownership, monotonic refcounted handle registry, `0` memory leaks).
   - **CLI Tooling:** `yaza new --mobile <name>`, `yaza init --mobile [dir]`, `yaza mobile doctor`, and `yaza mobile run --target android|ios` (automatic device/simulator detection or boot + debounced live watch & reload) produce production ARM64 artifacts: `android-arm64/libyaza_mobile.so` (`190,432 B` / ~190 KB), `ios-arm64/libyaza_mobile.a` (`237,480 B` / ~237 KB), and `ios-simulator-arm64/libyaza_mobile.a` (`233,440 B` / ~233 KB).
   - **Measured C ABI Bridge Performance:** `0.125 µs` (`create+destroy`, `6.24M ops/s`), `0.166 µs` (`1 B dispatch`, `6.05M ops/s`), `0.708 µs` (`1 KiB snapshot`, `1.37M ops/s`), `19.750 µs` (`32 KiB snapshot`, `52.2K ops/s`) at `~2.141 MiB RSS`.
2. **Arena-SSR MVC & HTML-over-the-Wire (`std/web/*`):**
   - **Zero-GC Per-Request Arena SSR** combined with partial DOM updates via **`X-Yaza-Fragment` + `yaza-live.js`** (~0.7 kB per interaction without heavy client JS bundles or SPA hydration), compiled type-safe HTML5 `view` syntax, binary `multipart/form-data` upload with magic-byte checks, HTTP 206 seekable streaming, and compile-time layer boundary checks (`yaza arch check`).
   - **6 Production CLI Project Templates:** `--minimal-api`, `--api`, `--web`, `--mvc` (alias `--fullstack`), `--microservice`, and `--mobile`.
   - **Post-Audit Loopback Benchmarks vs Node.js `v26.0.0`:** **`53,668 req/s`** Full SSR HTML (`p50 142 µs`, `7.05 MB RSS` vs Node.js v26 `36,960 req/s` & `73.53 MB RSS`), **`49,703 req/s`** Hypermedia Fragment (`p50 131 µs` vs Node.js v26 `37,612 req/s`), **`69,571 req/s`** JSON API Parity (`113 µs` p50), plus an internal multi-route Content SSR workload (**`1,337.0 req/s`** 114-section full index render / `55,624 B`, **`5,951.4 req/s`** fragment filter, **`3,488.2 req/s`** JSON API).
3. **Security-by-Construction (Default in Runtime & Standard Library):**
   - Verified under `ASan+UBSan`, `TSan`, macOS `leaks --atExit` (`0 leaks`), and a hostile TCP soak of 10,000 valid requests + 200 malformed/smuggling/oversized payloads (`0` crashes, `0` FD leaks):
   - Auto HTML/attribute escaping (`std/web/view.yz`, blocking `svg`/`math`, `srcdoc`, `javascript:`/`data:`, and `on*` event attributes) + `yaza audit --security`.
   - Session-bound CSRF token with constant-time comparison, `HttpOnly`/`Secure`/`SameSite` cookies, and automatic CSP, `X-Frame-Options: DENY`, and `X-Content-Type-Options: nosniff` headers.
   - Strict `\r\n` HTTP parser against request smuggling & keep-alive desync, capability-rooted VFS (`O_NOFOLLOW` anti-symlink/path traversal), magic-byte verification on multipart uploads, and compiler-optimization-resistant cryptographic zeroization (`volatile` wipe + memory fence).
4. **YazaBase & Binary Format YBIF (`std/ybif.yz` + C Direct Encoder):**
   - **Transactional ACID SQL Engine (`YazaBase`):** Built-in B+Tree storage with ARIES WAL CRC32C recovery, parameterized query binding, and in-process embedded sessions (`std/db/yazabase_embedded.yz`) with per-statement `arena_scope` isolation (**`9,995.2 SELECT/s`** at `p50 = 0.099 ms`, **`882.7 rows/s`** batched transactional `INSERT` — **42.7×** faster than standalone `F_FULLFSYNC`, Peak RSS **`37.3 MiB`**).
   - **YAZA Binary Interchange Format (`YBIF`):** Length-prefixed binary wire format (`std/ybif.yz` & C direct encoder `yazabase_result_encode_ybif`) achieving **6.22× faster** encode+decode than JSON on scalar records (`3.26M ops/s`, 31.9% smaller) and **2.13× faster** on 16-row SQL result batches (`2.59M ops/s`, `896 B` vs `1,188 B`, 24.6% smaller).
   - **Pluggable External Database Adapters:** Native wire-protocol clients for **PostgreSQL** (v3 + SCRAM-SHA-256 + native socket TLS & 16-byte `CancelRequest`), **MySQL** (v10 + `caching_sha2_password`), **MSSQL** (TDS), and **NoSQL** (Redis RESP / MongoDB BSON).
   - **Zero-GC In-Process Statistical, DataFrame & Visualization Suite (`std/data/*`):** Built-in single-binary statistical and analytics toolkit combining Neumaier compensated summation, Welford 1-pass moments, partial-pivoting Gaussian linear algebra, Columnar Arena DataFrame, 30+ statistical/ML methods (`ml_ols_fit`, `ml_multiple_ols_fit`, `ml_logistic_fit`, `ml_pca_first_component`, `ml_knn_classify`, `ml_kmeans_1d`, `ml_roc_auc`), **18 zero-JS Frontend SVG chart types**, and **6 Terminal CLI chart types**.

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
Pre-built archives for the released targets are available on the [Releases](https://github.com/yazaprojectstudios/yaza-language/releases/tag/v1.0.2) page and [yazastudios.id/docs/install](https://yazastudios.id/docs/install/):
- `yaza-1.0.2-darwin-arm64.tar.gz` & `yaza-1.0.2-macos-arm64.pkg` (Apple Silicon)
- `yaza-1.0.2-darwin-x64.tar.gz` (macOS Intel)
- `yaza-1.0.2-linux-x64.tar.gz` (Linux x86-64)
- `yaza-1.0.2-linux-arm64.tar.gz` (Linux ARM64)
- `yaza-1.0.2-windows-x64.zip` (Windows x64, experimental)
- `yaza-1.0.2-mobile-sdk.tar.gz` (Mobile Native Engine SDK: `android-arm64/libyaza_mobile.so`, `ios-arm64/libyaza_mobile.a`, `ios-simulator-arm64/libyaza_mobile.a`, and `include/yaza_mobile_v1.h`)
- `yaza-language-support-1.0.2.vsix` (VS Code Extension)

Extract the archive and add its `bin/` directory to your system `PATH`.
Compiling `.yz` programs on Linux needs Clang and the libcurl development
package (`libcurl4-openssl-dev` on Ubuntu/Debian). The one-line installer
downloads binaries and does not install system development packages.

### Build from Source
The repository includes compressed LLVM bootstrap files for immediate compilation:

```sh
# Ubuntu / Debian dependencies
sudo apt-get install clang xz-utils libargon2-dev libcurl4-openssl-dev

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

# Provision separate database service and web-admin credentials
mkdir -p data
yaza db set-user ./data db_service 'DatabaseServicePass_2026'
export YAZABASE_USER=db_service
export YAZABASE_PASSWORD='DatabaseServicePass_2026'
export YAZA_AUTH_SECRET="$(yaza secret token)"
yaza build --release src/admin_setup.yz -o admin-setup
YAZA_WEB_ADMIN_USER=admin YAZA_WEB_ADMIN_PASSWORD='DifferentWebPassword_2026' ./admin-setup

# Enforce MVC layer boundaries, security CSP/XSS rules, and build
yaza arch check
yaza audit --security
yaza check
yaza build --release src/main.yz -o my_app
YAZA_SELF_TEST=1 ./my_app

# Start live Fullstack MVC server on http://127.0.0.1:8080
./my_app
```

The MVC template also includes a certificate-verified HTTPS client,
Google/Brevo or local-relay SMTP, a YazaBase-backed mail outbox and general
background-job worker,
versioned migrations, latency metrics, login throttling, and single-admin
password reset/email verification. Configure `YAZA_MAIL_PROVIDER` and related
secrets as described in the generated project's `README.md`. Run the worker
and migrations as separate build targets; see that README for their commands
and the at-least-once delivery caveat. Direct outbound TLS currently needs
libcurl on macOS/Linux; Windows reports that transport unavailable.

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

# During development, rebuild and restart when .yz or yaza.toml changes
yaza run --watch
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

### Native Mobile Starter (`--mobile` · RFC 0010)
Generates (with `yaza new --mobile` or `yaza init --mobile`) a pure native Android (Kotlin + JNI) and iOS (Swift / SwiftUI) **Dumb UI** shell connected to a modular **MVI / Outbox LLVM AOT ARM64 YAZA Core Engine** (`src/state/`, `src/actions/`, `src/domain/`, `src/effects/` + `std/ybif` binary snapshots & `std/db/yazabase_mobile_kv` persistence) through the versioned C ABI (`yaza_mobile_v1.h`):

```sh
yaza new --mobile my_mobile_app     # or: mkdir existing && yaza init --mobile existing
cd my_mobile_app
yaza mobile doctor
yaza mobile run --target android     # auto-detects/boots AVD, builds, deploys & live-reloads
yaza mobile run --target ios         # auto-detects/boots Simulator, builds, deploys & live-reloads
yaza mobile run --target android --once
```

`yaza mobile run --target android` (alias: `android-emulator`) and `yaza mobile run --target ios` (alias: `ios-simulator`) support automatic single-device selection, interactive multi-device selection, auto-booting an AVD or iPhone simulator when idle, and a live file watcher (`src/**/*.yz`, `yaza.toml`, `mobile/android/**`, `mobile/ios/**`) that incrementally rebuilds and hot-reloads without exiting on compile errors. Verified with `0 leaks / 0 bytes` under `ASan+UBSan`, `TSan`, and `leaks --atExit`.

### Minimal API & SSR Web Starters (`--minimal-api` & `--web`)
For zero-boilerplate JSON microservices or lightweight SSR sites without an embedded SQL layer:
```sh
yaza new --minimal-api fast_service
yaza new --web content_site
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

The fullstack template reads `yaza.toml`. The current server applies
`max_in_flight` and `workers`; it parses and validates the arena budget fields
but does not apply them to allocation yet. `yaza config` shows parsed values.

```toml
[api.resources]
# Built-in profiles: "low_memory" | "balanced" (default) | "throughput"
profile = "balanced"
# workers = "auto"            # available CPUs, capped by max_in_flight
# max_in_flight = 32
# arena_mode = "adaptive"     # parsed, not yet applied by this server
# process_arena_limit = "512MiB"
# request_arena_floor = "256KiB"
# request_arena_initial = "1MiB"
# request_arena_ceiling = "64MiB"
# max_query_work_units = 100000000
```

| Parameter | `low_memory` (Fixed) | `balanced` (Adaptive, Default) | `throughput` (Adaptive) | Purpose |
| :--- | :--- | :--- | :--- | :--- |
| **Process Cap** | `128 MiB` | `512 MiB` | `2 GiB` | Parsed profile value; not yet enforced by fullstack |
| **Req Arena Initial**| `256 KiB` | `1 MiB` | `4 MiB` | Parsed profile value; not yet applied by fullstack |
| **Req Arena Floor**  | `64 KiB`  | `256 KiB`| `1 MiB` | Parsed profile value; not yet applied by fullstack |
| **Req Arena Ceiling**| `8 MiB`   | `64 MiB` | `256 MiB` | Parsed profile value; not yet applied by fullstack |
| **Max In-Flight**    | `8`       | `32`     | `128` | Concurrency slots before atomic HTTP 503 load-shedding |
| **Target Workload**  | Edge, IoT, Micro-VMs | Production REST APIs, K8s Pods | High-QPS Analytics, Burst Services |

The parser checks the invariant:
$$\text{floor} \le \text{initial} \le \text{ceiling} \le \text{process\_arena\_limit}$$
The fullstack starter fails startup on invalid configurations. For an actual
process arena hard cap, the current runtime reads
`YAZA_RT_V1_PROCESS_LIMIT_BYTES` from its environment.

---

## Database Support & Binary Format YBIF

YAZA provides first-class standard library drivers with wire-protocol implementations plus the **YBIF** binary interchange format:

1. **YazaBase (`std/db/yazabase` & `std/db/yazabase_embedded`):** Embedded ACID SQL engine with B+Tree indexing, ARIES write-ahead logging, parameterized SQL binding (`$1, $2, ...`), per-statement `arena_scope` memory isolation (`9,995.2 SELECT/s` at `p50 = 0.099 ms`, `882.7 rows/s` batched transactional `INSERT`, `37.3 MiB` Peak RSS), and `--export-request` mobile KV persistence (`std/db/yazabase_mobile_kv`).
2. **YBIF Binary Format (`std/ybif` & `yazabase_result_encode_ybif`):** Deterministic binary wire format with 4-byte magic (`YBIF`), versioned header, and length-prefixed typed cells — **6.22× faster** than JSON on scalar records (`3.26M ops/s`, 31.9% smaller) and **2.13× faster** on 16-row SQL result sets (`2.59M ops/s`, `896 B` vs `1,188 B`, 24.6% smaller).
3. **PostgreSQL (`std/db/postgres` & `std/pg/transport`):** Pure YAZA client implementing PostgreSQL Wire Protocol v3, SCRAM-SHA-256 authentication with channel-binding support, native socket TLS (`SSLRequest` + `libcurl` TLS upgrade + loopback `getpeername` inspection), native 16-byte `CancelRequest`, and typed `DbValue` mapping.
4. **MySQL (`std/db/mysql`):** Wire Protocol v10 client with `caching_sha2_password` scramble and command batching.
5. **Microsoft SQL Server (`std/db/mssql`):** Tabular Data Stream (TDS) wire protocol with prelogin handshake and batch execution.
6. **NoSQL Stores (`std/db/nosql`):** Redis RESP serialization for key-value storage and MongoDB BSON / `OP_MSG` wire framing for document stores.

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

### Data Science, Statistical Computing & SciPy Golden Cross-Validation (`std/data/*`, 21 Nov 2026)

Executed via `scripts/bench-data-science.sh` and `scripts/validate-data-science.sh` on `darwin arm64` (Apple M5, `-O2`, `debug`, and `ASan+UBSan`), comparing compiled **YAZA v1.0.2 (`-O2` · `std/data/*`)** against **NumPy `2.5.5` / SciPy `1.18.1` (vectorized C / Accelerate BLAS)** and **Pure Python `3.14.5` (`stdlib` / loops)**:

| Workload | YAZA v1.0.2 (`-O2` · `std/data/*`) | NumPy `2.5.5` / SciPy `1.18.1` | Pure Python `3.14.5` | Notes |
| :--- | :--- | :--- | :--- | :--- |
| **Welford 4-Moment Stats (Mean, Var, Skew, Kurt · 1M pts)** | **`15.05 ms`** (1-pass Welford) | `16.52 ms` (`scipy.stats.describe`) · `1.10 ms` (raw array ops) | `284.41 ms` | **1.10× faster** than `scipy.stats.describe`; **18.9×** vs pure Python |
| **Centered 1-Pass OLS Regression + $R^2$ (1M pts)** | **`8.62 ms`** (1-pass `ml_ols_fit`) | **`2.37 ms`** (`scipy.stats.linregress`) | `147.29 ms` | SciPy vectorized BLAS is 3.6× faster; YAZA is **17.1×** vs pure Python |
| **Vector Z-Score + Sequential EWMA + Dot (100K pts)** | **`6.15 ms`** | `13.13 ms` (vector Z-score/dot + EWMA recurrence) | `14.19 ms` | **2.13× faster** than NumPy + Python recurrence |
| **Silverman Gaussian KDE (5,000 obs × 64 grid = 320K evals)** | **`21.16 ms`** | **`3.74 ms`** (`gaussian_kde`) · **`1.92 ms`** (2D broadcast) | `21.46 ms` | NumPy/SciPy SIMD batch `exp` leads on 2D grid evaluation |
| **Columnar DataFrame ETL (5,000-row CSV Parse + FillNA + Filter + GroupBy + Inner Join)** | **`4.21 ms`** (`2.67 ms` parse + `1.55 ms` ops) | — | `5.13 ms` (`3.71 ms` `csv.DictReader` + `1.42 ms` dict ops) | **1.22× faster** total ETL (**1.39× faster** CSV parse) |
| **Process Peak RSS (Full Benchmark + DataFrame ETL)** | **`17.98 MiB`** (`18,857,984 B` · 0 GC) | `128.89 MB` (Python + NumPy/SciPy runtime) | `28.17 MB` | **7.2× lower RAM** than Python + NumPy/SciPy |

#### Automated 86-Case SciPy `1.18.1` Golden Cross-Validation (`tests/stdlib/data_science_golden.json`)

Generated automatically via `scripts/generate-data-science-golden.py` and verified in CI across `debug`, `release (-O2)`, and `ASan+UBSan` by `tests/stdlib/data_science_golden_check.yz` (`scripts/validate-data-science.sh`):

| Distribution / Statistical Function | Test Domain (Tails, Small $df$, Extreme Params) | Measured Max Error vs SciPy `1.18.1` | Enforced CI Tolerance |
| :--- | :--- | :--- | :--- |
| **`stats_normal_ppf` (`qnorm` / `norm.ppf`)** | 23 quantiles: $p \in [10^{-12}, 1 - 10^{-12}]$ (Acklam + 3 Halley steps) | **`max_rel_err = 2.10e-15`** | `rel_err <= 1.0e-13` |
| **`stats_normal_cdf` (`pnorm` / `norm.cdf`)** | 15 points: $z \in [-7.0, +7.0]$ (`erfc` lower-tail branch) | **`max_abs_err = 1.11e-16`** | `abs_err <= 1.0e-14` |
| **`stats_normal_sf` (`1 - pnorm` / `norm.sf`)** | 15 points: $z \in [-7.0, +7.0]$ (deep upper tail $1.28 \times 10^{-12}$) | **`max_rel_err = 1.30e-14`** | `rel_err <= 1.0e-12` |
| **`math_betainc_reg_float64` ($I_x(a,b)$ / `pbeta`)** | 11 cases: $(a,b) \in [(0.5, 0.5), (80.0, 0.5)]$ (Lentz CF) | **`max_abs_err = 5.00e-15`** | `abs_err <= 1.0e-12` |
| **`math_gammainc_upper_reg_float64` ($Q(a,x)$ / `pgamma`)** | 10 cases: $(a,x) \in [(0.5, 0.05), (80.0, 95.0)]$ (Series / Lentz CF) | **`max_abs_err = 9.33e-15`** | `abs_err <= 1.0e-12` |
| **`stats_student_t_p_two_tailed` (`pt` two-tailed)** | 13 cases: $df \in \{1, 2, 3.5, 5, 10, 30, 120\}$, $|t| \le 12.71$ | **`max_abs_err = 2.11e-15`** | `abs_err <= 1.0e-11` |
| **`stats_f_dist_p_upper` (`pf` upper-tail)** | 6 cases: $(d_1, d_2) \in \{(1,2), (2,5), (3,12), (5,50), (10,100)\}$ | **`max_abs_err = 2.74e-16`** | `abs_err <= 1.0e-11` |
| **`stats_chi2_p_upper` (`pchisq` upper-tail)** | 7 cases: $df \in \{1, 2, 3, 5, 10, 30\}$ | **`max_abs_err = 1.46e-16`** | `abs_err <= 1.0e-11` |
| **End-to-End Welch $t$, One-Way ANOVA $F$, $\chi^2$ GoF, R Type-7 Quantiles** | Exact match vs `ttest_ind(equal_var=False)`, `f_oneway`, `chisquare`, `np.quantile` | **`rel_err <= 2.11e-15`** | `rel_err <= 1.0e-10` |

Run the terminal CLI chart showcase or full validation suite directly:
```sh
yaza run examples/data-science-cli-charts.yz
sh scripts/validate-data-science.sh
sh scripts/bench-data-science.sh
```

### Post-Audit Ecosystem Benchmarks (21 Nov 2026 · Apple M5 · macOS 26.5 arm64)

Full reproducible audit measurements across the compiled toolchain (`YAZA LLVM 1.0.2`), Arena runtime, Arena-SSR, internal multi-route Content SSR workload, YazaBase SQL + YBIF, and the Mobile C ABI Engine ([full report](docs/benchmarks/audit-2026-11-21.md)):

| Subsystem | Measured Result (`-O2`) | Latency (`p50` / `p95`) | Memory / Artifact Footprint |
| --- | --- | --- | --- |
| **Arena 1M Scope Cycles** | **`38,064,786 scopes/sec`** (`26.27 ms`) | `26.27 ns/scope` | **`1.44 MiB`** Peak RSS · `0 B` retained |
| **Multi-Worker Soak (200K reqs, 8 workers)** | **`4,023,821 req/sec`** (`0.050 s`) | `1.00 µs` / `4.00 µs` | `1.4 -> 4.8 MiB` RSS · `0` errors |
| **Arena-SSR Full HTML / JSON API (`c=8`)** | **`53,668`** HTML · **`69,571`** JSON req/s | `142 µs` / `113 µs` (`p50`) | **`7.05 MB`** RSS · `246.4 KB` binary |
| **Internal Content SSR App (114-section / 6,236-record, `c=8`)** | `1,337` SSR · `5,951` Frag · `3,488` JSON req/s | `5.72 ms` · `1.20 ms` · `2.03 ms` | `218.2 MiB` plateau · `448.6 KB` binary |
| **Data Science (1M Welford / 1M OLS vs Pure Python)** | **`5.28 ms`** / **`4.99 ms`** (**68.8×** / **35.4×** pure Python) | `15.36 ms` KDE (320K) | **`9.75 MiB`** RSS · Neumaier/Welford default |
| **YazaBase Scoped PK `SELECT` / Batched `INSERT` / YBIF** | **`9,995`** `SELECT/s` · **`883`** `row/s` · **`2.59M`** `YBIF/s` | `0.099 ms` PK · `7.539 ms` cold CLI | **`37.3 MiB`** RSS · `1.90 MB` CLI |
| **Mobile Native Engine C ABI (`1 B` / `1 KiB` / `32 KiB`)** | **`6.05M`** / **`1.37M`** / **`52.2K ops/s`** | `0.166 µs` / `0.708 µs` / `19.75 µs` | **`190 KB`** Android `.so` · **`237 KB`** iOS `.a` |

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
