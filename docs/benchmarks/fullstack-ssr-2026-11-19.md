# YAZA Fullstack Arena-SSR Benchmark Report (19 Nov 2026)

Reproducible loopback HTTP/1.1 benchmark comparing **YAZA Fullstack Arena-SSR (`std/web/*` + `arena_scope`, `-O2`)** against an equivalent **Node.js `v26.0.0` `node:http` SSR** server producing **identical byte-for-byte HTML, CSS, ETag, Web CSP, and JSON payloads**.

---

## Test Environment & Methodology

- **Date:** 19 November 2026
- **Host:** `darwin arm64` (Apple Silicon, 10 logical CPUs)
- **Transport:** Local TCP loopback (`127.0.0.1`), HTTP/1.1 keep-alive
- **Load Profile:** `c = 8` concurrent sockets, `80` warmup requests + `2,000` timed requests × `3` rounds per workload (`30,000` measured requests per runtime, `0` errors)
- **Peer Runtime:** Node.js `v26.0.0` (isolated child process for clean Peak RSS measurement via `ps -o rss=`)
- **Reproduce:**
  ```sh
  YAZA_STDLIB_DIR="$PWD/std" YAZA_RUNTIME_DIR="$PWD/runtime" \
    ./build/yaza build --release examples/fullstack-ssr-bench.yz -o build/fullstack-ssr-bench
  node scripts/bench-fullstack-ssr.mjs
  ```

---

## Footprint & Memory Summary

| Metric | YAZA Arena-SSR (`-O2`) | Node.js `v26.0.0` | Comparison |
| :--- | :--- | :--- | :--- |
| **Standalone Binary Size** | **`208.4 KB`** (`283 KB` full MVC starter) | N/A (V8 runtime) | Single native binary, 0 `node_modules` |
| **Peak RSS (30k live reqs)** | **`5.45 MB`** (8 concurrent workers) | `73.72 MB` | **`13.5×` less RAM** |

---

## Throughput & Latency Results (`c = 8`, Median of 3 Rounds)

| Workload | Payload | YAZA `-O2` (req/s) | YAZA p50 / p95 | Node.js v26 (req/s) | Node p50 / p95 |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Hypermedia Fragment (`#task-board`)** | `3,706 B` | **`46,552 req/s`** | **`152 µs` / `315 µs`** | `39,202 req/s` | `176 µs` / `358 µs` |
| **Static CSS Asset (`200 OK`)** | `6,929 B` | **`71,213 req/s`** | **`86 µs` / `185 µs`** | `69,762 req/s` | `89 µs` / `193 µs` |
| **Static CSS Conditional (`304 ETag`)** | `0 B` | **`81,861 req/s`** | **`72 µs` / `155 µs`** | `80,255 req/s` | `79 µs` / `166 µs` |
| **Full SSR HTML (16 escaped rows + CSP)** | `3,980 B` | `36,616 req/s` | **`174 µs` / `360 µs`** | **`37,118 req/s`** | `175 µs` / `363 µs` |
| **JSON API Parity (16 items)** | `1,813 B` | `44,903 req/s` | `149 µs` / `363 µs` | **`76,056 req/s`** | `87 µs` / `167 µs` |

### Key Takeaways
1. **Hypermedia Fragment SSR (`+18.7%` ahead of Node.js):** YAZA's compiled `std/web/view` + `std/html` escaping inside per-request `arena_scope` hits **`46,552 req/s`** (`152 µs` p50) with `0` GC pauses.
2. **Static Serving & ETag Validation:** YAZA leads both `200 OK` CSS (`71,213 req/s`) and `304 Not Modified` (`81,861 req/s`, `72 µs` p50).
3. **Full SSR Parity at `13.5×` Lower RAM:** Full HTML5 document SSR matches Node.js throughput (`36.6k` vs `37.1k req/s`, `174 µs` vs `175 µs` p50) while holding Peak RSS to **`5.45 MB`** vs Node's **`73.72 MB`**.
4. **Honest Optimization Opportunity (JSON Loop):** Node's native C++ `JSON.stringify` leads on the 16-item JSON serialization workload (`76k` vs `44.9k req/s`) because the pure-YAZA benchmark loop uses repeated `+` concatenation rather than a buffered `TextBuilder`/`TextList` join.

*Scope note: This benchmark isolates the HTTP/1.1 framing, `arena_scope` lifecycle, HTML escaping, view rendering, ETag static serving, and strict Web CSP header assembly over loopback without external TLS termination or disk SQL I/O. End-to-end YazaBase SQL CRUD, Argon2id session login, CSRF verification, and browser rendering are verified separately via `scripts/validate-fullstack-mvc.sh`.*
