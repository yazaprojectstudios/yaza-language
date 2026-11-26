# YAZA

[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](LICENSE)
[![Release](https://img.shields.io/badge/Release-v1.1.0-green.svg)](https://github.com/yazaprojectstudios/yaza-language/releases)
[![Build Matrix](https://img.shields.io/badge/Platforms-macOS%20%7C%20Linux%20%7C%20Windows%20%7C%20Android%20%26%20iOS%20ARM64-blue.svg)](https://github.com/yazaprojectstudios/yaza-language/actions)
[![Zero-GC](https://img.shields.io/badge/Memory-Zero--GC%20%28Scoped%20Arenas%29-mint.svg)](#memory-model-arena_scope)
[![Benchmarks](https://img.shields.io/badge/Microservice%20%26%20SSR-116%2C072%20req%2Fs%20%40%20c%3D32-yellowgreen.svg)](#benchmarks)
[![YazaBase](https://img.shields.io/badge/YazaBase%201.1.0-3.56%20%C2%B5s%20PK%20%7C%20Tier%201--3-teal.svg)](#database-support--binary-format-ybif)

**YAZA** is an **all-in-one self-hosted systems programming language** designed to build your entire stack in one native language: high-concurrency cloud backends, single-binary fullstack web applications, native mobile core engines, statistical/data-science pipelines, and ACID SQL databases. It compiles directly to native machine code via **LLVM** (`-O2`), eliminates garbage collection pauses using deterministic **Scoped Arenas (`arena_scope`)**, and ships with **YazaBase 1.1.0** (a built-in ACID Clustered B+Tree SQL engine — embedded & clustered HA/PostgreSQL-compatible with AES-256 Page Encryption, Vector Search/HNSW, FTS BM25, PITR/CDC, FNV-1a Sharding, Migrations & JSONB `YJB1`), the **YBIF** binary interchange format, and the **Mobile Native Engine (RFC 0010)** for Android and iOS.

---

## Key Features & Four Core Technology Pillars (`v1.1.0`)

1. **Mobile Native Engine (RFC 0010 — Android & iOS ARM64):**
   - **LLVM AOT ARM64 Native Core Engine** (zero JIT, zero Garbage Collector) structured with modular MVI/Outbox `src/` modules (`state/`, `actions/`, `domain/`, `effects/`), `"YBIF"` binary state snapshots, task arenas, and embedded `YazaBase` (`--export-request` via `std/db/yazabase_mobile_kv`), connected to `"Welcome to YAZA Mobile"` 100% native Dumb UI (**Kotlin** on Android, **Swift / SwiftUI** on iOS) through the versioned **Thin C ABI Bridge** (`runtime/mobile/include/yaza_mobile_v1.h`, featuring `YAZA_MOBILE_EVENT_V1_ACTION = 4`, `512 KiB` max input payload, explicit buffer ownership, monotonic refcounted handle registry, `0` memory leaks).
   - **CLI Tooling:** `yaza new --mobile <name>`, `yaza init --mobile [dir]`, `yaza mobile doctor`, and `yaza mobile run --target android|ios` produce `v1.1.0` ARM64 artifacts: `android-arm64/libyaza_mobile.so` (`217,264 B`), `ios-arm64/libyaza_mobile.a` (`293,344 B`), and `ios-simulator-arm64/libyaza_mobile.a` (`289,240 B`).
   - **Measured C ABI Bridge Performance (`2026-11-26`):** `0.167 µs` (`create+destroy`, `4.70M ops/s`), `0.291 µs` (`1 B dispatch`, `3.46M ops/s`), `0.792 µs` (`1 KiB snapshot`, `1.24M ops/s`), `15.834 µs` (`32 KiB snapshot`, `61.8K ops/s`) at `2.188 MiB RSS`.
2. **Arena-SSR MVC, Microservices & HTML-over-the-Wire (`std/web/*`):**
   - **Zero-GC Per-Request Arena SSR** combined with partial DOM updates via **`X-Yaza-Fragment` + `yaza-live.js`**, compiled type-safe HTML5 `view` syntax, binary `multipart/form-data` upload with magic-byte checks, HTTP 206 seekable streaming, and compile-time layer boundary checks (`yaza arch check`).
   - **6 Production CLI Project Templates:** `--minimal-api`, `--api`, `--web`, `--mvc` (alias `--fullstack`), `--microservice`, and `--mobile`.
   - **Measured Loopback Throughput (`2026-11-26`, `-O2`):** **`116,072 req/s`** Quote+HMAC microservice (`c=32`, #1 vs Rust/Go/Nim/Node/Python), **`88,138 req/s`** (`c=8`) / **`102,337 req/s`** (`c=32`) JSON API, **`68,557 req/s`** Arena-SSR Hypermedia Fragment & **`79,362 req/s`** JSON API Parity at **`7.13 MB RSS`** (`246.8 KB` binary).
3. **Security-by-Construction (Default in Runtime & Standard Library):**
   - Verified under `ASan+UBSan`, `TSan` (200k concurrent scopes across 8 workers, `0` data races), macOS `leaks --atExit` (`0 leaks`), and hostile TCP soak:
   - Auto HTML/attribute escaping (`std/web/view.yz`, blocking `svg`/`math`, `srcdoc`, `javascript:`/`data:`, and `on*` event attributes) + `yaza audit --security`.
   - Session-bound CSRF token with constant-time comparison, `HttpOnly`/`Secure`/`SameSite` cookies, and automatic CSP, `X-Frame-Options: DENY`, and `X-Content-Type-Options: nosniff` headers.
   - Strict `\r\n` HTTP parser against request smuggling & keep-alive desync, capability-rooted VFS (`O_NOFOLLOW` anti-symlink/path traversal), magic-byte verification on multipart uploads, and compiler-optimization-resistant cryptographic zeroization (`volatile` wipe + memory fence).
4. **YazaBase 1.1.0 (Tier 1/2/3 Embedded + Clustered HA SQL Engine) & Binary Format YBIF:**
   - **Tier 1 / Tier 2 / Tier 3 Engine:** Built-in 16 KiB Clustered B+Tree engine with ARIES WAL CRC32C recovery, **CLOCK LRU Buffer Pool** (`page_read = 0`), **64-Entry Prepared Plan Cache**, **Group Commit** (`100 tx -> 1 fsync`), explicit multi-statement MVCC (`BEGIN`/`COMMIT`/`ROLLBACK`/`SAVEPOINT`), **Transparent Page Encryption (`YDE1`, AES-256-CTR + HMAC)**, **Concurrent Actor Engine**, **Vector Search (Flat + HNSW)**, **Full-Text Search BM25**, **PITR & CDC Stream**, **Horizontal FNV-1a Sharding (`68.8 ns/route`)**, **Online Schema Migrations**, **Binary JSONB (`YJB1`) + GIN-style Index**, **WAL Streaming Replication**, **Raft-Lite HA**, and **`pgwire v3`** (`16.0 (YazaBase 1.1.0)`).
   - **Measured YazaBase 1.1.0 Speed (`-O2`, Apple M5, `2026-11-26`):** Warm Point `SELECT` **`3.560 µs`** median (`3.500 µs` min, `page_read = 0`), 10k-row Full Scan **`111.0 µs`** `COUNT(*)` & **`130.6 µs`** `SUM(val)`, Embedded durable `INSERT` **`3.151 ms` p50** (**16×** faster than v1.0.2 at `16.50 MiB` RSS), **1.4× faster** PK lookup & **1.3× faster** Full Scan than PostgreSQL 18 (`0.022 ms` vs `0.031 ms`, `0.188 ms` vs `0.243 ms`).
   - **YAZA Binary Interchange Format (`YBIF`):** Direct C encoder `yazabase_result_encode_ybif` achieves **`2.17×` faster** throughput & **`24.6%` smaller** size than JSON (`8.255 ms`, `896 B` vs `17.947 ms`, `1,188 B` across 20k × 16-row batches); scalar `std/ybif` is **`1.86×` faster** & **`31.9%` smaller**.
   - **Pluggable External Database Adapters & JDBC 4.2:** Native wire-protocol clients for **PostgreSQL** (`std/pg/transport` + SCRAM-SHA-256 + TLS + `CancelRequest`), **MySQL**, **MSSQL**, **Redis/MongoDB**, plus official **YazaBase JDBC Driver (`yazabase-jdbc-1.1.0.jar`)**.
   - **Zero-GC Statistical, DataFrame & Visualization Suite (`std/data/*`):** Welford 1M **`9.13 ms`** (**37.5×** Python), OLS 1M **`6.97 ms`** (**23.2×**), DataFrame 5K ETL **`3.84 ms`**, **86/86 SciPy golden PASS** (`max_rel_err = 2.10e-15`), 18 zero-JS SVG chart types, and 6 terminal CLI charts.

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
Pre-built archives for the released targets are available on the [Releases](https://github.com/yazaprojectstudios/yaza-language/releases/tag/v1.1.0) page and [yazastudios.id/docs/install](https://yazastudios.id/docs/install/):
- `yaza-1.1.0-darwin-arm64.tar.gz` & `yaza-1.1.0-macos-arm64.pkg` (Apple Silicon)
- `yaza-1.1.0-darwin-x64.tar.gz` (macOS Intel)
- `yaza-1.1.0-linux-x64.tar.gz` (Linux x86-64)
- `yaza-1.1.0-linux-arm64.tar.gz` (Linux ARM64)
- `yaza-1.1.0-windows-x64.zip` (Windows x64)
- `yaza-1.1.0-mobile-sdk.tar.gz` (Mobile Native Engine SDK: `android-arm64/libyaza_mobile.so`, `ios-arm64/libyaza_mobile.a`, `ios-simulator-arm64/libyaza_mobile.a`, and `include/yaza_mobile_v1.h`)
- `yazabase-1.1.0-*.tar.gz` / `.zip` & `yazabase-jdbc-1.1.0.jar` (Standalone YazaBase + JDBC 4.2 Driver)
- `yaza-language-support-1.1.0.vsix` (VS Code Extension)

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
yaza mobile run android             # auto-detects/boots AVD, builds debug, & live hot-swaps
yaza mobile run ios                 # auto-detects/boots Simulator, builds debug, & live hot-swaps
yaza mobile run --target android --once
yaza mobile build --target android-arm64 --debug    # emits libyaza_mobile.so + libyaza_mobile_hotswap.so
yaza mobile build --target android-arm64 --release  # production -O2 static/shared build (no hot-swap)
```

`yaza mobile run [android|ios]` (also `--target android|ios` and aliases `android-emulator` / `ios-simulator`) defaults to **debug mode** (`YAZA_MOBILE_BUILD_MODE=debug`, `-O0`) with **Debug Dynamic Core Hot-Swap (`dlopen`)**: edits inside `src/**/*.yz` or `yaza.toml` recompile the core and swap `yaza_mobile_v1_*` pointers in-process without closing or restarting the app (`⚡ <file> changed -> hot-swapped in ...ms, no restart`), while native UI edits under `mobile/android/**` or `mobile/ios/**` trigger a full rebuild and relaunch (`Ctrl+C` stops cleanly, `--once` disables watching). Verified with `0 leaks / 0 bytes` under `ASan+UBSan`, `TSan`, and `leaks --atExit`.

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

1. **YazaBase 1.1.0 (`std/db/yazabase` & `std/db/yazabase_embedded`):** Tier 1/2/3 Embedded + Clustered ACID SQL engine with 16 KiB Clustered B+Tree indexing, ARIES write-ahead logging, CLOCK LRU Buffer Pool (`3.560 µs` median warm point SELECT, `page_read = 0`), 64-entry Prepared Plan Cache, 10k-row direct streaming aggregates (`111.0 µs` `COUNT(*)`, `130.6 µs` `SUM(val)`), Group Commit (`100 tx → 1 fsync`), explicit MVCC (`BEGIN`/`COMMIT`/`ROLLBACK`/`SAVEPOINT`), Transparent Page Encryption (`YDE1`, AES-256-CTR + HMAC), Vector Search (Flat + HNSW), FTS BM25, PITR & CDC, Horizontal FNV-1a Sharding (`68.8 ns/route`), Online Schema Migrations, Native Binary JSONB (`YJB1`), WAL Streaming Replication, Raft-Lite HA, `pgwire v3` (`16.0 (YazaBase 1.1.0)`), official **YazaBase JDBC 4.2 Driver (`yazabase-jdbc-1.1.0.jar`)**, and `--export-request` mobile KV persistence (`std/db/yazabase_mobile_kv`).
2. **YBIF Binary Format (`std/ybif` & `yazabase_result_encode_ybif`):** Deterministic binary wire format with 4-byte magic (`YBIF`), versioned header, and length-prefixed typed cells — direct C encoder runs **2.17× faster** & **24.6% smaller** than JSON across `20,000` × 16-row SQL batches (`8.255 ms`, `896 B` vs `17.947 ms`, `1,188 B`); scalar `std/ybif` runs **1.86× faster** & **31.9% smaller**.
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

Full reproducible benchmarks conducted on **26 November 2026** (Apple M5, macOS 26.5 arm64, 10 logical CPUs, `YAZA LLVM 1.1.0 -O2`, 0 failed checks — [complete v1.1.0 report](docs/benchmarks/v1.1.0-2026-11-26.md)):

### Cloud-Native Microservice Comparison (6 Runtimes, Persistent DB + HMAC-SHA256)

Endpoints: `GET /healthz`, `GET /api/v1/quote` (math + HMAC-SHA256), `POST /api/v1/orders` (JSON parse + HMAC-SHA256), and `GET /api/v1/catalog?id=500` (10,000-row persistent embedded DB PK lookup: YazaBase vs SQLite 3):

| Runtime | Quote + HMAC ($c=8$) | Quote + HMAC ($c=32$) | Order POST ($c=8$) | Embedded DB Lookup ($c=8$) | Peak RSS | Binary Size |
| :--- | ---: | ---: | ---: | ---: | ---: | ---: |
| **YAZA v1.1.0 (`-O2`)** | `88,816 req/s` (`83 µs`) | **`116,072 req/s`** (`256 µs`) | `84,320 req/s` (`88 µs`) | `65,118 req/s` (`111 µs`) | `12.44 MiB` | `1,896.5 KB` |
| **Rust 1.99 (`-O2`)** | `90,186 req/s` (`78 µs`) | `110,442 req/s` (`268 µs`) | `95,012 req/s` (`78 µs`) | `67,340 req/s` (`107 µs`) | `3.31 MiB` | `549.3 KB` |
| **Go 1.27 (`net/http`)** | `84,732 req/s` (`86 µs`) | `113,342 req/s` (`239 µs`) | `85,553 req/s` (`85 µs`) | `67,784 req/s` (`104 µs`) | `20.97 MiB` | `9,192.0 KB` |
| **Nim 2.2.12 (`-d:release`)** | `87,760 req/s` (`84 µs`) | `91,687 req/s` (`331 µs`) | `79,282 req/s` (`91 µs`) | `78,775 req/s` (`92 µs`) | `6.64 MiB` | `359.1 KB` |
| **Node.js v26 (`node:http`)** | `82,882 req/s` (`92 µs`) | `85,969 req/s` (`321 µs`) | `70,056 req/s` (`106 µs`) | `71,155 req/s` (`89 µs`) | `79.45 MiB` | VM |
| **Python 3 (`http.server`)** | `19,368 req/s` (`378 µs`) | `18,939 req/s` (`1,571 µs`) | `18,098 req/s` (`389 µs`) | `11,527 req/s` (`641 µs`) | `21.34 MiB` | VM |

### HTTP/1.1 Loopback API (`GET /api/response`, 1-Item JSON, 26 Nov 2026)

| Runtime | $c=1$ req/s ($p50$) | $c=8$ req/s ($p50$) | $c=32$ req/s ($p50$ / $p99$) |
| :--- | ---: | ---: | ---: |
| **Rust (`rustc 1.99.1 -O`)** | `36,252` (`26 µs`) | `91,720` (`80 µs`) | `107,513` (`270 µs` / `796 µs`) |
| **YAZA v1.1.0 (`-O2`)** | `26,406` (`36 µs`) | **`88,138`** (`85 µs`) | **`102,337`** (`273 µs` / `834 µs`) |
| **Go (`go1.27.3`)** | `24,345` (`38 µs`) | `74,462` (`98 µs`) | `97,422` (`277 µs` / `1,086 µs`) |
| **Node.js `v26.0.0`** | `26,499` (`36 µs`) | `74,560` (`98 µs`) | `77,820` (`346 µs` / `1,044 µs`) |
| **Nim (`2.2.12 -d:release`)** | `27,360` (`35 µs`) | `75,984` (`94 µs`) | `76,637` (`384 µs` / `800 µs`) |
| **PHP `8.5.6`** | `8,785` (`109 µs`) | `23,506` (`303 µs`) | `24,189` (`1,252 µs` / `2,085 µs`) |
| **Python `http.server`** | `12,425` (`78 µs`) | `17,779` (`403 µs`) | `17,430` (`1,715 µs` / `4,115 µs`) |

*On `POST /api/echo` ($c=8$): YAZA sustains **`87,414 req/s`** (`83 µs` $p50$), beating Go (`67,397`), Nim (`72,407`), and Node.js (`80,066`).*

### Fullstack Arena-SSR ($c=8$, 26 Nov 2026)

| Runtime / Footprint | Full SSR HTML | Hypermedia Fragment | JSON API (16 items) | Static CSS (200 / 304) | Peak RSS |
| :--- | ---: | ---: | ---: | ---: | ---: |
| **YAZA v1.1.0 (`246.8 KB` bin)** | `32,356 req/s` (`171 µs`) | **`68,557 req/s`** (`108 µs`) | **`79,362 req/s`** (`96 µs`) | `73,872` / `85,973 req/s` | **`7.13 MB`** |
| **Go `1.27.3` (`12.18 MB` bin)** | `42,706 req/s` (`155 µs`) | `40,400 req/s` (`163 µs`) | `76,236 req/s` (`87 µs`) | `68,315` / `86,952 req/s` | `22.55 MB` (`3.2×`) |
| **Node.js `v26.0.0` (V8)** | `38,571 req/s` (`177 µs`) | `41,212 req/s` (`173 µs`) | `77,751 req/s` (`75 µs`) | `71,633` / `83,706 req/s` | `73.47 MB` (`10.3×`) |

### Data Science, Statistical Computing & SciPy Golden Cross-Validation (`std/data/*`, 26 Nov 2026)

Executed via `scripts/bench-data-science.sh` and `scripts/validate-data-science.sh` on `darwin arm64` (Apple M5, `YAZA LLVM 1.1.0 -O2`, `debug`, and `ASan+UBSan`) comparing **YAZA v1.1.0 (`-O2` · `std/data/*`)** against **Python 3 (`stdlib`)** + 86-case SciPy golden fixtures (`tests/stdlib/data_science_golden.json`):

| Workload | YAZA v1.1.0 (`-O2` · `std/data/*`) | Pure Python 3 (`stdlib`) | Speedup / Notes |
| :--- | :--- | :--- | :--- |
| **Welford 4-Moment Stats (Mean, Var, Skew, Kurt · 1M pts)** | **`9.13 ms`** (1-pass Welford) | `342.65 ms` | **37.5× faster**; ill-conditioned var exact (`1`) vs `0.0` |
| **Centered 1-Pass OLS Regression + $R^2$ (1M pts)** | **`6.97 ms`** (1-pass `ml_ols_fit`) | `161.74 ms` | **23.2× faster**; shifted OLS exact (`2`) vs `nan` |
| **Vector Z-Score + Sequential EWMA + Dot (100K pts)** | **`4.36 ms`** | `16.03 ms` | **3.68× faster** |
| **Silverman Gaussian KDE (320K evals)** | **`17.99 ms`** | `23.71 ms` | **1.32× faster** |
| **Columnar DataFrame ETL (5,000-row CSV Parse + FillNA + Filter + GroupBy + Inner Join)** | **`3.84 ms`** (`2.44 ms` parse + `1.40 ms` ops) | `8.02 ms` (`4.98 ms` parse + `3.04 ms` ops) | **2.09× faster** total ETL |
| **Process Peak RSS (Full Suite + DataFrame ETL)** | **`18.02 MiB`** (`18,890,752 B` · 0 GC) | `31.88 MB` | **1.77× lower RAM** · 86/86 SciPy golden PASS (`qnorm max_rel_err=2.10e-15`) |

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

### Full Ecosystem Summary (`26 Nov 2026` · `YAZA LLVM 1.1.0` · Apple M5 arm64)

Full reproducible measurements across the `YAZA LLVM 1.1.0` toolchain, Arena runtime, Microservice, Arena-SSR, Core, Search, YazaBase Tier 1/2/3 + YBIF, Data Science, and Mobile C ABI ([full report](docs/benchmarks/v1.1.0-2026-11-26.md)):

| Subsystem | Measured Result (`-O2`) | Latency (`p50` / `p95`) | Memory / Artifact Footprint |
| --- | --- | --- | --- |
| **Core (7 Workloads) & 5M Binary Search** | **`6.136 ms`** Core mean · **`97.224 ms`** Search (`ListView`) | Core beats Rust `6.234 ms` | Search beats C/Rust noinline, Nim, Go |
| **Arena 1M Scope Cycles & 200K Soak** | **`29.57M scopes/s`** (`33.82 ns`) · **`4.52M req/s`** soak | `1.00 µs` / `3.00 µs` soak | **`1.422 MiB`** RSS · `0 B` retained · 11/11 gates |
| **Cloud-Native Microservice (`c=8` / `c=32`)** | **`88,816`** / **`116,072 req/s`** Quote+HMAC · **`65,118`** DB | `83 µs` (`c=8`) · `256 µs` (`c=32`) | **`12.44 MiB`** RSS (vs Node `79.45 MiB`) |
| **Fullstack Arena-SSR (`c=8`)** | **`68,557`** Frag · **`79,362`** JSON · **`32,356`** SSR req/s | `108 µs` / `96 µs` / `171 µs` | **`7.13 MB`** RSS · `246.8 KB` binary |
| **YazaBase Warm Point `SELECT` & 10k Scans** | **`3.560 µs`** PK · **`111.0 µs`** `COUNT(*)` · **`130.6 µs`** `SUM` | `0` disk reads · `16.50 MiB` RSS | Embedded INSERT `3.151 ms` p50 (**16×** vs v1.0.2) |
| **YazaBase vs SQLite 3 / PG 18 & Tier 1/2/3** | **`0.022 ms`** PK · **`0.188 ms`** SUM · **`68.8 ns`** Shard | **1.4×** PK & **1.3×** Scan vs PG 18 | Crypto `YDE1` `399.5 µs`/pg · FTS `8.5 µs`/q · JSONB |
| **YBIF Binary Format vs JSON (`20k×`)** | Direct C **`8.255 ms`** (`2.17×`) · Scalar **`6.432 ms`** (`1.86×`) | vs JSON `17.947 ms` / `11.978 ms` | **24.6% smaller** SQL (`896 B`) · **31.9%** scalar (`49 B`) |
| **Mobile Native Engine C ABI (`1 B` / `1 KiB` / `32 KiB`)** | **`4.70M`** init · **`3.46M`** / **`1.24M`** / **`61.8K ops/s`** | `0.167 µs` / `0.291 µs` / `0.792 µs` / `15.83 µs` | **`2.188 MiB`** RSS · Android `217 KB` · iOS `293 KB` |

---

## Security & Verification

YAZA `v1.1.0` enforces **11 out of 11 verified safety gates** across CI and host sanitizer suites (`11/11 PASS` on `2026-11-26`):

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
