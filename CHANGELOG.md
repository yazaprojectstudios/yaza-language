# Changelog

All notable changes to the **YAZA Programming Language**, **YazaBase SQL Engine**, **Fullstack Arena-SSR MVC**, and **Mobile Native Engine** are documented in this file.

---

## [v1.1.1] — 2026-11-27

### Performance & Runtime Optimizations
1. **LLVM Guarded `for_range` Vectorization/SCEV & Read-View `list_count` (`compiler/core_llvm.yz`):**
   - Emitted `add nsw i64` on `for_range` induction steps and added a runtime-guarded (`first >= 0 && end <= 3000000000 && acc == 0`) provably-safe `add nsw i64` SSA accumulator fast path while keeping `@llvm.sadd.with.overflow.i64` trap semantics outside the guard (`4.5B` overflow trapped with exit `1`).
   - Added a `load atomic i64 acquire` active read-view fast path for `list_count`, falling back cleanly on concurrent mutations.
   - **Measured impact:** Core **Sum 10M integers** dropped **2.19×** (`4.992 ms` → **`2.279 ms`**), Core 7-workload mean dropped to **`5.459 ms`** (**beating Rust 1.99.1 `6.005 ms` & Nim `6.630 ms`**), and 5M Binary Search improved (`ListView` **`95.259 ms`** vs C `-O2` `102.696 ms`; `List[Number]` `340.4 ms` → **`325.171 ms`**, ahead of Go `1.27.3` `353.291 ms`).
2. **Arena Runtime Chunk Recycling, Fused Scope & TLS Context Elision (`runtime/src/yaza_runtime_v1.c`):**
   - Added a 64-shard `YazaRtV1ChunkCacheShard` cache to recycle `64 KiB` arena chunks across `arena_scope` cycles (with exit cleanup, ASan memory poisoning, and `YAZA_RT_V1_TESTING` fault-injection bypass).
   - Fused inline lexical scope entry/exit (`yaza_rt_v1_scope_enter_inplace` + `yaza_rt_v1_scope_leave`) into single tree-lock acquisitions and added a pure `_Thread_local` `yaza_rt_v1_is_current_context_arena` / `yaza_rt_v1_find_current_context_for_arena` fast path for `yaza_arena_v1_generation` and short-lived paired `yaza_list_v1_begin` / `yaza_list_v1_begin_typed` / `yaza_list_v1_end` (keeping explicit leases on long-lived iterators and mutable views).
   - **Measured impact:** 200k-scope soak improved **1.52×** (`57.74 ms` → **`38.11 ms`**, **`5.25M scopes/s`**, beating Node.js `v26` `45.45 ms` & Python `68.64 ms`), 1M C runtime cycles improved **+25.6%** (`29.57M/s` → **`37.15M scopes/s`**, `26.92 ns/scope`), and Cloud-Native Microservice Peak RSS dropped **-29.8%** (`12.44 MiB` → **`8.73 MiB`**).
3. **YazaBase Convex & Boundary Leaf Predicate Elision (`yazabase/btree_scan_v1.yz`):**
   - Added `yaza_db_btree_where_is_convex_integer_key_predicate_v1` + per-leaf `leaf_all_pk_match` validation and enabled `direct_mode == 1` (`COUNT(*)`) on boundary leaves (`start_after_key` / `end_key`) with strict per-slot fence checks, removing tuple materialization on filtered and narrow-range scans (`!=` and `OR` fall back to exact row evaluation).
   - **Measured impact:** In-process filtered range `COUNT(*)` (`5,001 / 10k` rows) improved **4.58×** (`0.398 ms` → **`0.0869 ms`**); 3-way `filtered_count` (`>=` / `<=` / `<`) dropped **`0.269/0.245/0.229 → 0.089/0.091/0.088 ms`** (**2.8× faster than PostgreSQL 18**) and `narrow_range` (`50` rows) dropped **`0.057/0.056 → 0.031/0.030 ms`**, allowing YazaBase to beat PostgreSQL 18 across **all 7/7 warm queries**.
4. **Fullstack Arena-SSR Wire & HTML Escaping (`std/web/response.yz`, `std/html.yz`):**
   - Switched `web_format_response` header assembly to single-pass `TextList` pushes and length-gated sanitization, and added a zero-allocation `text_byte` ASCII fast scan in `html_escape` returning `ok(value)` without intermediate buffers when no `& < > " '` exists.
   - **Measured impact:** Full SSR HTML (`c=8`) achieved lowest `p50` latency (**`138 µs`** vs Go `170 µs` & Node `179 µs`, `34,651 req/s`), Hypermedia Fragment **`66,634 req/s`** (**1.82× Go & 1.71× Node**), and Static CSS **`68,673 req/s` (#1)** at **`6.70 MiB` Peak RSS** (`247.5 KB` binary).
5. **Toolchain & Driver Synchronization (`v1.1.1`):**
   - Bumped `yaza` / `yaza-llvm` (`YAZA LLVM 1.1.1`), `yazabase-cli` (`yazabase 1.1.1`), `libyazabase` C API, `pg_wire_v1` (`16.0 (YazaBase 1.1.1)`), `yazabase-jdbc-1.1.1.jar`, VS Code extension (`1.1.1`), macOS installers (`yaza-1.1.1-darwin-arm64.tar.gz`, `yaza-1.1.1-macos-arm64.pkg`, `yazabase-1.1.1-darwin-arm64.tar.gz`), and all 10 cross-platform bootstrap IR archives (`bootstrap/*.ll.xz`).

---

## [v1.1.0] — 2026-11-26

### Highlights & New Capabilities
1. **YazaBase Tier 1 / Tier 2 / Tier 3 Enterprise Capabilities:**
   - **Tier 1 — Transparent Page Encryption (`page_crypto_v1.yz`, `YDE1`):** 16 KiB page-level AES-256-CTR + HMAC encryption (`399.5 µs/page` write / `391.4 µs/page` read) with key derivation/rotation and ASan/UBSan verification.
   - **Tier 2 — Concurrent Engine, Vector Search, FTS BM25, PITR & CDC (`concurrent_engine_v1.yz`, `vector_index_v1.yz`, `fts_bm25_v1.yz`, `pitr_cdc_v1.yz`):** Coalescing Group-Commit actor pipeline (`100 commits / 50 batches / 50 fsyncs` in `299.6 ms`), CLOCK LRU striped buffer pool, flat & HNSW cosine/L2 vector search (`3.67 ms` flat / `11.58 ms` HNSW), BM25 full-text search (`8.5 µs/query`), point-in-time recovery (PITR), and change-data-capture (CDC) streaming.
   - **Tier 3 — Horizontal Sharding, Online Migrations & Native Binary JSONB (`shard_router_v1.yz`, `schema_migration_v1.yz`, `jsonb_v1.yz`):** FNV-1a consistent hash-shard routing (`68.8 ns/route`, `14.5M routes/s`), declarative versioned schema migrations, and native binary `YJB1` JSONB (`3.06 ms` 500× encode / `1.03 ms` path extract / `1.55 ms` containment / GIN-style index lookups).
   - **JDBC 4.2 Driver `v1.1.0` (`yazabase-jdbc-1.1.0.jar`):** Synchronized with `yazabase 1.1.0` & PostgreSQL Wire Protocol v3 (`pg_wire_v1` reporting `16.0 (YazaBase 1.1.0)`).

2. **Compiler Memory Reduction (`compiler/core_llvm.yz`):**
   - Eliminated redundant duplicate `verify_core_yir(program)` passes inside `lower_core_yir_to_llvm`, `lower_core_yir_to_llvm_module`, and `lower_core_yir_to_llvm_request_module` (already verified once on load in `load_core_yir`).
   - Reduced `emit-llvm compiler/yazabase_cli.yz` peak memory footprint from `32.34 GB` (`OOM`) down to **`17.74 GB`** (`6.14 GB` RSS) in `64.69s`, and refreshed all 10 cross-platform bootstrap IR archives (`bootstrap/*.ll.xz`).

3. **Mobile Native Engine SDK `v1.1.0` & Packaging Parity:**
   - Rebuilt `yaza-1.1.0-mobile-sdk.tar.gz` (`android-arm64/libyaza_mobile.so` **`217,264 B`**, `ios-arm64/libyaza_mobile.a` **`293,344 B`**, `ios-simulator-arm64/libyaza_mobile.a` **`289,240 B`**) + `yaza_mobile_v1.h`.
   - Added standalone `yazabase` binary parity to Windows x64 archives (`scripts/package-windows-release.sh`), dynamic versioning in macOS `.pkg` postinstall (`scripts/build-macos-pkg.sh`), and bumped VS Code extension to `1.1.0`.

4. **Fresh Ecosystem Benchmarks (`2026-11-26` · Apple M5 · `-O2` · [Full Report](docs/benchmarks/v1.1.0-2026-11-26.md)):**
   - **Cloud-Native Microservice (6 runtimes, persistent 10k-row embedded DB + HMAC-SHA256):** **`116,072 req/s`** Quote+HMAC (`c=32`, #1 overall ahead of Go `113,342` & Rust `110,442`), **`88,816 req/s`** (`c=8`), **`65,118 req/s`** persistent YazaBase lookup (`c=8`, within 3.3% of Rust+SQLite3), **`12.44 MiB` RSS** (vs Node `79.45 MiB`).
   - **HTTP/1.1 Loopback API (7 runtimes):** **`88,138 req/s`** (`c=8`) / **`102,337 req/s`** (`c=32`) 1-item JSON, **`87,414 req/s`** `POST /api/echo` (`c=8`).
   - **Fullstack Arena-SSR (`c=8`):** **`68,557 req/s`** Hypermedia Fragment, **`79,362 req/s`** JSON API, **`32,356 req/s`** Full SSR at **`7.13 MB` RSS** (`246.8 KB` binary).
   - **Core & Binary Search:** **`6.136 ms`** Core mean of 7 medians (beats Rust `6.234 ms` & Nim `6.762 ms`); **`97.224 ms`** 5M binary search (`ListView`, beats C/Rust noinline, Nim, Go).
   - **YazaBase & YBIF:** **`3.560 µs`** Warm Point SELECT (`page_read=0`), **`111.0 µs`** 10k `COUNT(*)`, **`130.6 µs`** `SUM(val)`, Embedded durable INSERT `3.151 ms` p50 (**16×** faster than v1.0.2 at `16.50 MiB` RSS), **1.4×** faster PK & **1.3×** faster Full Scan than PostgreSQL 18; YBIF **`2.17×` faster** / `24.6%` smaller (C 16-row batch) & **`1.86×` faster** / `31.9%` smaller (YAZA scalar).
   - **Data Science & Mobile C ABI:** Welford 1M **`9.13 ms`** (**37.5×** Python), OLS 1M **`6.97 ms`** (**23.2×**), DataFrame 5K ETL **`3.84 ms`**, **86/86 SciPy golden PASS** (`max_rel_err = 2.10e-15`); Mobile C ABI `0.167 µs` (`create+destroy`), `0.291 µs` (`1 B`), `0.792 µs` (`1 KiB`), `15.834 µs` (`32 KiB`) at `2.188 MiB` RSS.

---

## [v1.0.2] — 2026-11-21

### YazaBase Phase A/B/C & Performance Update — 2026-11-25
- **Clustered Server, Replication & HA (`buffer_pool_v1.yz`, `plan_cache_v1.yz`, `explicit_tx_v1.yz`, `group_commit_v1.yz`, `replication_v1.yz`, `cluster_ha_v1.yz`, `router_v1.yz`, `autoscale_v1.yz`, `pg_wire_v1.yz`):**
  - Added CLOCK LRU buffer pool (`page_read = 0` on warm reads), 64-entry prepared `SELECT` plan cache, single-shot staged dirty-page overlay + deferred checkpointing, Group Commit (`100 tx → 1 WAL fsync`), explicit multi-statement MVCC transactions (`BEGIN`/`COMMIT`/`ROLLBACK`/`SAVEPOINT`), Physical WAL Streaming Replication, Raft-Lite leader election/failover, Lag-Aware R/W & FNV-1a Hash-Shard routing, autoscale policies, and PostgreSQL Wire Protocol v3 (`pg_wire_v1`).
- **Write & Scan Engine Optimizations (`sql_insert_write_v1.yz`, `page_v1.yz`, `btree_scan_v1.yz`, `page_wal_v1.yz`):**
  - Eliminated redundant `2×N` preflight page CRC32C decodes via fence- & cursor-aware leaf caching, replaced `can_append` per-slot allocations with zero-alloc bulk leaf slot writing, and added direct page streaming aggregate scans (`COUNT(*)`, scalar `SUM`/`MIN`/`MAX` over fixed numeric/float64 columns).
  - Measured (`-O2`, Apple M5): Warm Point `SELECT` **`3.57 µs`** median (`3.49 µs` min), 10k Full Scan `COUNT(*)` **`111 µs`** (**27.4×** faster) & `SUM(val)` **`130 µs`** (**21.4×** faster), `INSERT` Batch **`0.85 ms` p50 / 50 rows** (`26.94 ms` across `10×50`), `UPDATE` Batch **`~40.7k rows/s`**, `DELETE` Batch **`~47.3k rows/s`**.

### Highlights
`v1.0.2` is the post-audit production hardening, performance optimization, and full ecosystem synchronization release across the **4 Core Technology Pillars**:

1. **Mobile Native Engine (RFC 0010 — Android & iOS ARM64):**
   - **LLVM AOT ARM64 Native Core Engine** (no JIT, no GC) executing domain rules, state reducers, task arenas, and embedded `YazaBase` (`--export-request` compatible `std/db/yazabase_mobile_kv` re-exported via `std/db/yazabase_embedded`), bridged to 100% native **Kotlin** (Android) and **Swift / SwiftUI** (iOS) UI shells via the versioned **Thin C ABI Bridge** (`runtime/mobile/include/yaza_mobile_v1.h`).
   - Added request-safe mobile embedded KV persistence (`yaza_db_embedded_kv_load_v1`, `yaza_db_embedded_kv_store_v1`) wired into `templates/mobile/src/mobile_engine.yz` (`YAZA_MOBILE_STORAGE_DIR`).
   - Hardened JNI adapter (`runtime/mobile/android/yaza_mobile_jni.c`) against OOM and partial array copy failures with strict `ExceptionCheck` guards and deterministic `yaza_mobile_v1_buffer_release`.
   - Production ARM64 artifact footprints:
     - `android-arm64/libyaza_mobile.so`: **`190,432 B`** (~190 KB)
     - `ios-arm64/libyaza_mobile.a`: **`237,480 B`** (~237 KB)
     - `ios-simulator-arm64/libyaza_mobile.a`: **`233,440 B`** (~233 KB)
   - Measured C ABI bridge latency (`-O2`, Apple M5):
     - Cold `create + destroy`: **`0.125 µs`** p50 (`6,238,634 ops/s`)
     - `1 B` event dispatch + release: **`0.166 µs`** p50 (`6,048,753 ops/s`)
     - `1 KiB` snapshot + release: **`0.708 µs`** p50 (`1,373,551 ops/s`)
     - `32 KiB` snapshot + release: **`19.750 µs`** p50 (`52,219 ops/s`)
     - Peak RSS: **`~2.141 MiB`**, `0 leaks / 0 bytes` verified under `ASan+UBSan`, `TSan`, and `leaks --atExit`.

2. **Arena-SSR MVC & HTML-over-the-Wire (`std/web/*`, `std/json.yz`, `std/html.yz`):**
   - Added zero-allocation ASCII fast-path and contiguous safe-segment slicing in `json_escape_text` (`std/json.yz`) and `html_escape_bytes` (`std/html.yz`), accelerating JSON API Parity by **+135% (2.35×)** and Full SSR HTML throughput.
   - All 6 CLI project starters verified end-to-end: `--minimal-api`, `--api`, `--web`, `--mvc` (alias `--fullstack`), `--microservice`, and `--mobile`.
   - Post-audit loopback benchmarks (`c = 8`, vs Node.js `v26.0.0`):
     - **Full SSR HTML (16 rows + CSP):** **`53,668 req/s`** (`p50 = 142 µs`, `p95 = 236 µs`, **`7.05 MB` Peak RSS** vs Node.js v26 `36,960 req/s` & `73.53 MB` RSS)
     - **Hypermedia Fragment (`X-Yaza-Fragment` + `yaza-live.js`):** **`49,703 req/s`** (`p50 = 131 µs` vs Node.js v26 `37,612 req/s`)
     - **JSON API Parity (16 items):** **`69,571 req/s`** (`p50 = 113 µs`, `p95 = 195 µs`)
     - **Static CSS (`200 OK` / `304 ETag`):** **`59,683 req/s`** / **`75,454 req/s`**
     - **Internal Multi-Route Content SSR App (`448.6 KB` binary, 114 sections / 6,236 records):** **`1,337.0 req/s`** full 114-section SSR (`55,624 B`), **`5,951.4 req/s`** fragment filter, **`3,488.2 req/s`** JSON API.

3. **Security-by-Construction (Runtime & Standard Library Audit Hardening):**
   - **Auto HTML/Attribute Escaping (`std/web/view.yz`):** Context-aware escaping blocking `<svg>`, `<math>`, `srcdoc`, `javascript:`, `data:`, and `on*` event attributes by construction.
   - **Session-Bound CSRF & Cookie Hardening (`std/web/csrf.yz`, `std/http.yz`):** Session-bound CSRF tokens verified in constant time; `HttpOnly`, `Secure`, `SameSite` cookies; automatic `Content-Security-Policy`, `X-Frame-Options: DENY`, and `X-Content-Type-Options: nosniff` headers.
   - **Strict HTTP/1.1 Wire Framing (`std/http.yz`):** Rejects bare-LF headers, spaces before header colons, malformed `Content-Length`, and `Transfer-Encoding` + `Content-Length` smuggling combinations; keeps connections synchronized across keep-alive cycles.
   - **Capability-Rooted VFS & Multipart Magic-Byte Checks (`runtime/src/yaza_runtime_v1.c`, `std/web/upload.yz`):** Enforces `O_NOFOLLOW` and descriptor-relative path confinement against symlink and `..` traversal; verifies magic bytes on binary uploads.
   - **Cryptographic Zeroization (`std/crypto.yz`, `std/security.yz`, `runtime/src/yaza_runtime_v1.c`):** Compiler-barrier `volatile` memory wipe + `atomic_signal_fence` across key material, password buffers, and SCRAM proofs.
   - **Native PostgreSQL Socket TLS & Query Cancel (`runtime/src/yaza_runtime_v1.c`, `std/pg/transport.yz`):** Added `yaza_net_v1_tls_client_with_ca` (PostgreSQL `SSLRequest` negotiation + `libcurl` TLS upgrade), `yaza_net_v1_peer_is_loopback` (`getpeername` inspection), and `yaza_net_v1_pg_cancel` (native 16-byte `CancelRequest` packet).

4. **YazaBase & Binary Format YBIF (`std/ybif.yz` & `runtime/src/yazabase_c_api.c`):**
   - **Per-Statement `arena_scope` Session Isolation (`yazabase/session_v1.yz`, `std/db/yazabase_embedded.yz`):** Clones surviving catalog/session handles across transient statement arenas, reducing embedded loop Peak RSS **14.8×** (from `554.4 MiB` to **`37.3 MiB`**) while achieving **`9,995.2 SELECT/s`** (`p50 = 0.099 ms`) and **`882.7 rows/s`** batched transactional `INSERT` (**42.7×** faster than standalone `F_FULLFSYNC`).
   - **Direct C Binary YBIF Encoder (`yazabase_result_encode_ybif` in `runtime/src/yazabase_c_api.c`):** Encodes YazaBase query results directly into a single linear binary buffer (`YBIF` magic + typed cells), achieving **2.13× faster** throughput than JSON on 16-row SQL batches (`2.59M ops/s`, `7.723 ms` vs `16.419 ms`, **24.6% smaller** payload at `896 B` vs `1,188 B`) and **6.22× faster** throughput on scalar records (`3.26M ops/s`, **31.9% smaller**).

---

## [v1.0.1] — 2026-11-19

- Shipped Fullstack MVC Monolith starter (`yaza new --mvc`), native `view` SSR syntax, YAZA UI v1.0 design system, and `yaza-live.js` Hypermedia Fragment updates.
- Added Zero-GC Data Science, Statistical Computing, ML & Visualization suite (`std/data/*`) with 18 zero-JS SVG chart types and 6 terminal CLI chart types.
- Initial Mobile Native Engine (RFC 0010) C ABI bridge (`yaza_mobile_v1.h`) and Android/iOS project scaffolding (`yaza new --mobile`).

---

## [v1.0.0] — 2026-11-18

- Initial official release of the self-hosted YAZA LLVM compiler (`yaza`), deterministic scoped arenas (`arena_scope`), `YazaBase` ACID B+Tree SQL storage engine (`yazabase`), and multi-database wire-protocol adapters (PostgreSQL, MySQL, MSSQL, Redis, MongoDB).
