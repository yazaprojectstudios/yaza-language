# YAZA Fullstack Arena-SSR Benchmark Report (19 Nov 2026)

Reproducible multi-runtime MVC/Web HTTP/1.1 benchmark comparing **YAZA Fullstack Arena-SSR (`std/web/*` + `arena_scope`, `-O2`)** against **Go `1.27.3` (`net/http` + `html/template`)** and **Node.js `v26.0.0` (`node:http`)** producing **identical HTML5, Hypermedia Fragment, CSS, ETag, Web CSP, and JSON payloads**.

---

## Test Environment & Methodology

- **Date:** 19 November 2026
- **Host:** `darwin arm64` (Apple Silicon, 10 logical CPUs)
- **Transport:** Local TCP loopback (`127.0.0.1`), HTTP/1.1 keep-alive
- **Load Profile:** `c = 8` concurrent sockets, `80` warmup requests + `2,000` timed requests × `3` rounds per workload (`30,000` measured requests per runtime, `0` errors)
- **Peer Runtimes:** `go version go1.27.3 darwin/arm64` (`net/http` + `html/template`), `Node.js v26.0.0` (`node:http`), each isolated in its own OS process for clean Peak RSS (`ps -o rss=`) measurement
- **Reproduce:**
  ```sh
  YAZA_STDLIB_DIR="$PWD/std" YAZA_RUNTIME_DIR="$PWD/runtime" \
    ./build/yaza build --release examples/fullstack-ssr-bench.yz -o build/fullstack-ssr-bench
  node scripts/bench-fullstack-ssr.mjs
  ```

---

## Footprint, Binary Size & Peak RSS

| Metric | YAZA Arena-SSR (`-O2`) | Go `1.27.3` (`html/template`) | Node.js `v26.0.0` |
| :--- | :--- | :--- | :--- |
| **Standalone Binary** | **`208.4 KB`** (`302 KB` full MVC starter) | `12.47 MB` (`60×` larger) | V8 Runtime |
| **Peak RSS (30k live reqs)** | **`2.83 MB`** (8 concurrent workers) | `21.38 MB` (`7.6×` more RAM) | `68.08 MB` (`24.1×` more RAM) |

---

## Throughput & Latency Results (`c = 8`, Median of 3 Rounds)

| Workload | Payload | YAZA `-O2` | Go `1.27.3` (`html/template`) | Node.js `v26.0.0` |
| :--- | :--- | :--- | :--- | :--- |
| **Full SSR HTML (16 escaped rows + CSP)** | `3,980 B` | **`51,327 req/s`** (`138 µs` p50 · `257 µs` p95) | `40,626 req/s` (`163 µs` · `407 µs`) | `37,454 req/s` (`178 µs` · `397 µs`) |
| **Hypermedia Fragment (`#task-board`)** | `3,706 B` | **`50,930 req/s`** (`139 µs` p50 · `271 µs` p95) | `37,385 req/s` (`176 µs` · `444 µs`) | `40,377 req/s` (`162 µs` · `345 µs`) |
| **Static CSS Asset (`200 OK`)** | `8,878 B` | **`65,051 req/s`** (`98 µs` p50 · `209 µs` p95) | `58,531 req/s` (`134 µs` · `238 µs`) | `61,730 req/s` (`100 µs` · `234 µs`) |
| **Static CSS Conditional (`304 ETag`)** | `0 B` | **`72,720 req/s`** (`109 µs` p50 · `174 µs` p95) | `72,224 req/s` (`110 µs` · `191 µs`) | `68,563 req/s` (`87 µs` · `220 µs`) |
| **JSON API Parity (16 items)** | `1,813 B` | `39,837 req/s` (`178 µs` p50 · `360 µs` p95) | `56,962 req/s` (`125 µs` · `276 µs`) | **`65,509 req/s`** (`106 µs` · `224 µs`) |

### Key Engineering Takeaways
1. **Compiled Arena-SSR Beats Both Go `html/template` and Node.js:**
   - Full HTML5 SSR: **`51,327 req/s`** (**`+26.3%`** vs Go, **`+37.0%`** vs Node.js) with **`257 µs` p95** vs Go's `407 µs` and Node's `397 µs`.
   - Hypermedia Fragment (`#task-board`): **`50,930 req/s`** (**`+36.2%`** vs Go, **`+26.1%`** vs Node.js).
   - Because YAZA views compile straight to LLVM machine code and assemble inside per-request `arena_scope` via `TextList` + `text_join`, there is zero runtime template reflection and zero GC tail-latency spikes.
2. **Container Footprint Advantage:** **`2.83 MB` Peak RSS** (`7.6×` less RAM than Go, `24.1×` less RAM than Node.js) and **`208.4 KB` binary** (`60×` smaller than Go's `12.47 MB`).
3. **Static Assets (`200 OK` & `304 ETag`):** YAZA leads on both full CSS delivery (`65,051 req/s`) and conditional `304` ETag checks (`72,720 req/s`).
4. **Honest Tradeoff on 16-Item JSON:** Node's native C++ `JSON.stringify` (`65.5k`) and Go's `encoding/json` (`56.9k`) remain faster than YAZA's pure-YAZA character-scanning `json_escape_text` loop (`39.8k req/s`).

*Scope note: This benchmark isolates MVC HTTP/1.1 framing, `arena_scope` lifecycle, compiled XSS-escaped SSR views, Hypermedia Fragment responses, ETag static assets, and strict Web CSP over loopback without external TLS or disk SQL I/O. End-to-end YazaBase SQL CRUD (`INSERT`/`SELECT`/`UPDATE`/`DELETE`), Argon2id login, CSRF protection, and browser rendering are verified separately via `scripts/validate-fullstack-mvc.sh`.*
