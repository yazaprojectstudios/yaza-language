# Changelog

All notable changes to the **YAZA Programming Language**, **YazaBase SQL Engine**, **Fullstack Arena-SSR MVC**, and **Mobile Native Engine** are documented in this file.

---

## [v1.0.2] — 2026-11-21

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
     - **Production App `yaza-quran` (`448.6 KB` binary, 114 Surahs / 6,236 Ayahs):** **`1,337.0 req/s`** full 114-surah SSR (`55,624 B`), **`5,951.4 req/s`** fragment filter, **`3,488.2 req/s`** JSON API.

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
