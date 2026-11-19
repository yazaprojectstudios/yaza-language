# YAZA Fullstack Web & MVC (Arena-SSR) Benchmark Report (19 Nov 2026)

Reproducible **8-framework HTTP/1.1 SSR & Fullstack Web benchmark** comparing **YAZA v1.0.0 Arena-SSR (`-O2`, native `view` syntax + `arena_scope`)** against **Go `1.27.3` (`net/http` + `html/template`)**, **Node.js `v26.0.0` (raw `node:http`)**, **Fastify `5.12.8`**, **Express `5.3.0`**, **PHP `8.5.6`**, **React `19.3.1` SSR (`renderToString`)**, and **Next.js `16.4.6` Production SSR (`next build` + `next start`)**.

---

## Test Environment & Methodology

- **Date:** 19 November 2026
- **Host:** `darwin arm64` (Apple Silicon, 10 logical CPUs)
- **Transport:** Local TCP loopback (`127.0.0.1`), HTTP/1.1 keep-alive (`c = 8` concurrent sockets)
- **Load Profile:** `80` warm-up requests + `1,500` timed requests × `3` rounds per workload (`600 × 3` for Next.js & PHP; median of 3 rounds, `0` errors)
- **Workload Parity:** Every server runs in its own isolated OS process and dynamically constructs 16 task items per request, XSS-escapes HTML titles (`<verified & safe>`), embeds 16 hidden CSRF inputs, sets strict Web CSP security headers, and exposes `/api/v1/tasks` JSON parity.
- **Process Footprint:** Peak RSS measured across each server's full process tree via `ps -axo pid=,ppid=,rss=` during live load; cold start measured from process `spawn` until first HTTP `200 OK` on `/healthz` (`/api/healthz` on Next.js).
- **Reproduce:**
  ```sh
  YAZA_STDLIB_DIR="$PWD/std" YAZA_RUNTIME_DIR="$PWD/runtime" \
    ./build/yaza build --release examples/fullstack-ssr-bench.yz -o build/fullstack-ssr-bench
  node scripts/bench-web-frameworks.mjs
  ```

---

## 8-Framework Throughput, Latency & Resource Footprint (`c = 8`, Median of 3 Rounds)

| Rank | Framework / Runtime | Full SSR HTML (`c=8`, p50 · p95) | Fragment (`#task-board`) | JSON API (16 items) | Peak RSS (RAM) | Cold Start | Footprint |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **1** | **YAZA v1.0.0 Arena-SSR (`-O2`, native `view`)** | **`29,024 req/s`** (`167 µs` · `326 µs`) | **`37,990 req/s`** (`123 µs` · `246 µs`) | `33,437 req/s` (`138 µs`) | **`3.17 MB`** | **`31.8 ms`** | **`208.4 KB` binary** (`318 KB` Forum MVC) |
| **2** | **Go `1.27.3` (`net/http` + `html/template`)** | `28,743 req/s` (`162 µs` · `464 µs`) | `27,991 req/s` (`173 µs` · `442 µs`) | `41,848 req/s` (`107 µs`) | `22.05 MB` (`7.0×`) | `614.1 ms` | `12.18 MB` binary (`59×`) |
| **3** | **Node.js `v26.0.0` (raw `node:http`)** | `27,430 req/s` (`182 µs` · `414 µs`) | `28,733 req/s` (`178 µs` · `359 µs`) | `38,894 req/s` (`96 µs`) | `73.86 MB` (`23.3×`) | `113.6 ms` | V8 runtime |
| **4** | **Fastify `5.12.8` (Node.js v26)** | `25,295 req/s` (`202 µs` · `450 µs`) | `25,772 req/s` (`199 µs` · `415 µs`) | `39,740 req/s` (`105 µs`) | `86.44 MB` (`27.3×`) | `163.6 ms` | Node + `node_modules` |
| **5** | **Express `5.3.0` (Node.js v26)** | `22,553 req/s` (`238 µs` · `530 µs`) | `22,492 req/s` (`239 µs` · `487 µs`) | `36,257 req/s` (`118 µs`) | `77.81 MB` (`24.5×`) | `136.3 ms` | Node + `node_modules` |
| **6** | **PHP `8.5.6` (built-in server)** | `12,001 req/s` (`435 µs` · `854 µs`) | `13,425 req/s` (`366 µs` · `785 µs`) | `14,502 req/s` (`338 µs`) | `29.47 MB` (`9.3×`) | `136.0 ms` | PHP 8.5 runtime |
| **7** | **React `19.3.1` SSR (`renderToString` / Node)** | `6,304 req/s` (`1,006 µs` · `2,075 µs`) | `6,953 req/s` (`913 µs` · `1,830 µs`) | **`45,394 req/s`** (`75 µs`) | `140.75 MB` (`44.4×`) | `107.4 ms` | Node + React 19 |
| **8** | **Next.js `16.4.6` Production SSR (`next start`)** | `4,473 req/s` (`1,386 µs` · `2,711 µs`) | — *(full SSR page)* | `10,693 req/s` (`439 µs`) | `212.30 MB` (`67.0×`) | `197.2 ms` | `365 MB` `node_modules` + `.next` |

### Static Asset Delivery (`public/yaza-ui.css`, `20,158 B`, `c = 8`)
Measured separately in `scripts/bench-fullstack-ssr.mjs`:
- **Static CSS (`200 OK`, `20,158 B`):** YAZA **`63,268 req/s`** (`99 µs` p50) vs Node.js `61,456 req/s` (`103 µs`).
- **Static CSS Conditional (`304 ETag`):** YAZA **`82,539 req/s`** (`69 µs` p50 · `153 µs` p95) vs Node.js `81,136 req/s` (`78 µs` · `160 µs`).

---

## Key Engineering Takeaways

1. **#1 Overall on Full SSR & Hypermedia Fragments:** Compiled YAZA `view` functions assemble HTML directly inside per-request `arena_scope` via `TextList` + single-pass `text_join` with zero template reflection, zero Virtual DOM reconciliation, and zero GC pauses.
2. **Vs Next.js `16.4.6` & React `19.3.1` SSR:**
   - **`6.5×` faster Full SSR** than Next.js `16.4.6` (`29,024` vs `4,473 req/s`) and **`4.6×` faster** than React `19.3.1` `renderToString` (`6,304 req/s`).
   - **`67×` less RAM** than Next.js (`3.17 MB` vs `212.30 MB`) and **`44.4×` less RAM** than React SSR (`140.75 MB`).
   - **`208.4 KB` single binary** vs `365 MB` `node_modules` + `.next` build artifacts.
3. **Vs Express `5.3.0`, Fastify `5.12.8`, Node.js `v26`, and Go `1.27.3`:**
   - Outperforms Express 5 (`+28.7%` Full SSR, `+68.9%` Fragment), Fastify 5 (`+14.7%` Full SSR, `+47.4%` Fragment), raw Node.js (`+5.8%` Full SSR, `+32.2%` Fragment), and Go `html/template` (`+35.7%` Fragment, `30%` tighter Full SSR p95: `326 µs` vs `464 µs`), while using `7×`–`27×` less RAM.
4. **Honest Tradeoff on 16-Item JSON:** V8's C++ `JSON.stringify` (Node/Fastify/Express/React API: `36k–45k req/s`) and Go's `encoding/json` (`41.8k req/s`) remain faster than YAZA's current pure-YAZA `json_escape_text` loop (`33.4k req/s`), though YAZA still beats Next.js API routes (`10.7k req/s`) by `3.1×` and PHP (`14.5k req/s`) by `2.3×`.

*Scope note: This loopback benchmark isolates HTTP/1.1 keep-alive framing, `arena_scope` lifecycle, compiled XSS-escaped SSR views, Hypermedia Fragments, strict Web CSP, and JSON serialization without external TLS or disk SQL I/O. Full end-to-end YazaBase ACID SQL CRUD, Argon2id sessions, CSRF, and browser rendering are verified via `scripts/validate-fullstack-mvc.sh` and `yaza-forum`.*
