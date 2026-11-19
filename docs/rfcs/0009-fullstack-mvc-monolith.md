# RFC 0009: YAZA Fullstack Monolith Architecture (Arena-SSR MVC + Hypermedia Fragments)

- **Status:** Implemented on `feat/fullstack-mvc-monolith` (Phases 1–4 Verified)
- **Target CLI:** `yaza new --fullstack <name>` / `yaza new --mvc <name>`
- **Validation & Benchmarks:** `scripts/validate-std-web.sh`, `scripts/validate-fullstack-mvc.sh`, `docs/benchmarks/fullstack-ssr-2026-11-19.md`
- **Depends On:** `std/http.yz`, `std/html.yz`, `std/fsroot.yz`, `std/url.yz`, `std/jwt.yz`, `std/crypto/hash.yz`, `std/crypto/password.yz`, `std/config/api_config.yz`, `std/db/yazabase.yz` (and `postgres` / `mysql` / `mssql`)
- **Related Proposals:** `yaza-data-media-web-modules.md` (`std.web`), `yaza-ui-design-system.md` (YAZA UI / "YAZA Current")

---

## 1. Motivation & Problem Statement

Today, YAZA v1.0.0 ships production-verified starters for standalone backends:
- `yaza new --api <name>` (Layered REST API with YazaBase ACID SQL, JWT/RBAC, OpenAPI/Postman generation)
- `yaza new --microservice <name>` (Hexagonal Ports & Adapters with K8s probes, domain events, and async workers)
- `yaza new --minimal-api <name>` (Single-file lightweight service)

Meanwhile, `templates/fullstack/` is still an early placeholder stub whose README explicitly notes that API hosting and dynamic web rendering are not yet wired.

Teams building web products (admin dashboards, SaaS apps, internal portals, e-commerce backends, CMS platforms) frequently want **one cohesive fullstack monolith**—backend and frontend living inside **one YAZA project, one type system, and one compiled native binary**—without:
- splitting into two repos, two toolchains, and duplicate DTOs/validators,
- shipping hundreds of megabytes of `node_modules` or a JavaScript SSR runtime,
- paying stop-the-world Garbage Collector (GC) pauses under concurrent HTML rendering.

---

## 2. Core Architectural Thesis: Arena-SSR MVC + Hypermedia Fragments

Rather than copying legacy interpreted MVC frameworks or heavy SPA bundlers, YAZA's Fullstack Monolith combines six primitives unique to YAZA's architecture:

### 2.1 Zero-GC Arena-SSR Pipeline (`arena_scope`)
Every incoming browser or API request executes inside one bounded per-request `arena_scope` governed by `[api.resources]` (`yaza.toml`):

```text
TCP Socket (HTTP/1.1 Keep-Alive, 8 spawn workers)
  └─ arena_scope (256 KiB floor -> 1 MiB init -> 64 MiB ceiling, AQB)
       ├─ 1. Bounded HTTP + Cookie + Form/JSON Parsing
       ├─ 2. Security Middleware (Signed Session, CSRF Guard, Rate Limit)
       ├─ 3. MVC Controller Dispatch
       ├─ 4. Service & Model Layer (YazaBase ACID SQL / Postgres / MySQL / MSSQL)
       ├─ 5. Compiled View Renderer (Native LLVM HTML/Fragment Builder + std/html escaping)
       └─ 6. Wire Write (Web CSP + Security Headers + Keep-Alive)
  └─ Scope Exit -> 1-Step Pointer Rewind (0 GC pauses, 0 per-object free)
```

All temporary allocations created during SQL row decoding, model hydration, HTML escaping, and page assembly are reclaimed deterministically in one step the instant the HTTP response is flushed.

### 2.2 Compiled Type-Safe Views (Zero SSTI, XSS-Safe by Construction)
Traditional MVC engines parse string templates at runtime (risking Server-Side Template Injection and runtime template errors). In YAZA Fullstack:
- Views live in `src/views/**/*.yz` as strongly typed YAZA functions compiled to native LLVM machine code.
- If a controller passes the wrong model field or type to a view, `yaza check` rejects it at compile time.
- Every dynamic text node and attribute passes through `std/html.yz` (`html_escape`, `html_escape_attribute`), and every dynamic link passes `html_href_is_safe` (rejecting `javascript:`, protocol-relative URLs, control characters, and malformed authorities).

### 2.3 Tri-Mode Content Negotiation from One Controller
A single MVC controller action serves three client modes cleanly without duplicating business logic or schemas:

| Request Mode | Trigger | Response | Use Case |
| :--- | :--- | :--- | :--- |
| **Full SSR Page** | Normal browser navigation (`GET /products`) | `200 OK` `text/html; charset=utf-8` wrapped in `layouts/app.yz` (YAZA UI) | Initial load, SEO, direct links, works with JS disabled |
| **Hypermedia Fragment** | Header `X-Yaza-Fragment: <target-id>` or `Accept: text/vnd.yaza.fragment` | `200 OK` `text/html; charset=utf-8` containing **only the updated component fragment** | SPA-like instant UI updates (search, pagination, inline CRUD, modals) without full page reload |
| **JSON / YBIF API** | `Accept: application/json` or `/api/v1/*` route | `200/201` `application/json` (or `application/x-ybif`) | Mobile clients, external integrations, Postman/OpenAPI clients |

A tiny, optional, zero-dependency YAZA-owned script (`public/js/yaza-live.js`, ~2 KB, zero `eval`, strict CSP compliant) intercepts links/forms annotated with `data-yaza-target="#product-table"` and swaps the returned fragment into the DOM. If JavaScript is disabled or unavailable, every `<a href>` and `<form method="POST">` degrades cleanly to standard HTTP Post/Redirect/Get (`303 See Other`).

### 2.4 YAZA UI ("YAZA Current") Built-In Design System
Consistent with `yaza-ui-design-system.md`:
- Ships the YAZA-owned token stylesheet (`public/css/yaza-ui.css`) and reusable compiled view primitives (`src/views/components/`): AppShell, Topbar, Sidebar, MetricCard, DataTable, FormField, Button, Badge, Alert, EmptyState, Dialog.
- Semantic HTML first, keyboard accessible, responsive light/dark themes, zero third-party CSS/JS runtime dependencies.

### 2.5 Capability-Rooted Static Asset Serving (`std/fsroot.yz`)
All static assets under `./public` are served exclusively via `fs_root_open("./public")` and `fs_root_read_bytes(...)`:
- Mathematically prevents `../` path traversal, backslashes, NUL bytes, and symlink escapes at the OS boundary.
- Enforces an explicit MIME-type whitelist (`.css`, `.js`, `.svg`, `.png`, `.jpg`, `.webp`, `.woff2`, `.ico`, `.txt`, `.json`) — unknown extensions fail closed (`404`).
- Supports `ETag`, `If-None-Match` (`304 Not Modified`), bounded file size caps (e.g., 4 MiB default), and immutable/revalidating `Cache-Control`.

### 2.6 Web Security by Default (Fail-Closed)
Extending YAZA's API security guarantees to browser workloads:
- **Signed Session Cookies:** `yaza_session=<signed-token>; HttpOnly; SameSite=Lax; Path=/` (plus `Secure` when `YAZA_ENV=production`), signed with HMAC-SHA256 using `YAZA_AUTH_SECRET` (fail-closed startup if missing/weak in production) and verified in constant time.
- **CSRF Protection:** Synchronizer CSRF token bound to the active session cookie (`csrf_token` hidden input or `X-CSRF-Token` header), verified in constant time on every state-changing `POST`/`PUT`/`PATCH`/`DELETE` web route.
- **Strict Web CSP:** Separate from the JSON API `default-src 'none'` policy, HTML responses emit:
  ```http
  Content-Security-Policy: default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' data:; font-src 'self'; object-src 'none'; base-uri 'self'; form-action 'self'; frame-ancestors 'none'
  X-Content-Type-Options: nosniff
  X-Frame-Options: DENY
  Referrer-Policy: strict-origin-when-cross-origin
  ```
- **Hardware-Aware Resource Profiles (AQB):** Full support for `low_memory` (`128 MiB`), `balanced` (`512 MiB`), `throughput` (`2 GiB`), custom `yaza.toml` overrides, atomic `503` load-shedding, and `/healthz`, `/readyz`, `/metrics`.

---

## 3. Standard Library Additions (`std/web/`)

To keep framework primitives cleanly separated from application templates (per `yaza-data-media-web-modules.md` Rule #8), Phase 1 introduces a YAZA-owned `std/web/` module family:

| Module | Responsibility | Key Primitives |
| :--- | :--- | :--- |
| `std/web/response.yz` | Web HTTP response builders & header formatting | `web_html_ok`, `web_fragment_ok`, `web_redirect_see_other(location)`, `web_static_ok(mime, bytes, etag)`, `web_format_http_response(...)` with Web CSP, `Location`, `Set-Cookie`, `ETag` |
| `std/web/cookie.yz` | Cookie parsing & signed session cookies | `cookie_parse_header`, `cookie_get`, `session_sign_cookie`, `session_verify_cookie`, `cookie_set_header`, `cookie_clear_header` |
| `std/web/form.yz` | Bounded `application/x-www-form-urlencoded` parsing | `form_parse_urlencoded(body, max_fields)`, `form_get`, `form_url_decode` (rejects NUL/control bytes, capped at 64 KiB / 128 fields) |
| `std/web/csrf.yz` | Session-bound CSRF token generation & validation | `csrf_issue_token(session_id, secret)`, `csrf_verify_token(session_id, submitted, secret)` (constant-time HMAC check) |
| `std/web/static.yz` | Rooted static file server over `std/fsroot.yz` | `static_serve_rooted(public_root_fd, url_path, if_none_match)` with strict MIME whitelist & traversal/symlink rejection |
| `std/web/view.yz` | Safe compiled HTML composition helpers over `std/html.yz` | `el_text`, `el_attr`, `el_href`, `csrf_hidden_input`, `html_document_shell` |

Note on `std/http.yz`: `HttpRequest` should also extract the `Cookie`, `X-Yaza-Fragment`, `X-CSRF-Token`, and `If-None-Match` headers (preserving backwards compatibility with existing API/Microservice callers, or via a dedicated `WebRequest` wrapper parsed from raw headers in `std/web/request.yz` so existing `HttpRequest` struct layout stays 100% ABI-compatible with all existing examples and tests!).

> **Design Decision (Backwards Compatibility):** Keeping `std/http.yz`'s `HttpRequest` unchanged and adding `std/web/request.yz` (`web_parse_request(raw_bytes) -> Result[WebRequest, Text]`) guarantees zero breaking changes to `templates/api/`, `templates/microservice/`, and all existing `std/http` tests.

---

## 4. Target Project Layout (`yaza new --fullstack <name>`)

```text
my_app/
├── yaza.toml                         # [project], [api], [api.resources], [database], [web], [architecture]
├── README.md
├── Dockerfile
├── compose.yaml
├── .gitignore
├── .env.example
├── data/                             # YazaBase ACID SQL storage (provisioned via yaza db set-user)
├── public/                           # Capability-confined static assets (fs_root_open("./public"))
│   ├── css/
│   │   └── yaza-ui.css               # YAZA Current Design System tokens & components
│   └── js/
│       └── yaza-live.js              # Optional ~2 KB CSP-safe hypermedia fragment swapper
├── src/
│   ├── main.yz                       # Multi-worker TCP HTTP/1.1 server + arena_scope + YAZA_SELF_TEST=1
│   ├── app.yz                        # Module wiring & route table (Web MVC + Static + JSON API + Probes)
│   ├── config/
│   │   └── app_config.yz             # yaza.toml loader + AQB profile validation + auth secret guard
│   ├── middleware/
│   │   └── web_security.yz           # Signed cookie sessions, Argon2id login guard, CSRF enforcement
│   ├── models/
│   │   ├── task.yz                   # Domain entity & input validation
│   │   └── task_repository.yz        # Parameterized YazaBase SQL CRUD (pluggable to Postgres/MySQL/MSSQL)
│   ├── services/
│   │   └── task_service.yz           # Pure business logic (zero HTML/HTTP coupling)
│   ├── controllers/
│   │   ├── auth_controller.yz        # GET/POST /login, POST /logout (Argon2id + signed HttpOnly cookie)
│   │   ├── dashboard_controller.yz   # GET / (SSR overview), GET/POST/DELETE /tasks (SSR + Fragment + JSON)
│   │   └── health_controller.yz      # /healthz, /readyz, /metrics
│   └── views/
│       ├── layouts/
│       │   └── app_layout.yz         # Master HTML5 shell with YAZA UI sidebar, topbar, flash alerts
│       ├── components/
│       │   ├── ui_primitives.yz      # Escaped MetricCard, Badge, Alert, CSRF Form helpers
│       │   └── task_table.yz         # Reusable fragment swapped via X-Yaza-Fragment or full render
│       └── pages/
│           ├── login_page.yz         # Login screen
│           └── dashboard_page.yz     # Full dashboard composing layout + components
└── tests/
    └── fullstack-smoke.sh            # Live HTTP SSR + Fragment + CSRF + Cookie + Static traversal tests
```

### Layer Dependency Rules (`[architecture]` in `yaza.toml`)
Enforced via `yaza arch check`:
- `models` -> `std/db/*`, `std/csv`, `std/convert` (MUST NOT import `controllers`, `views`, `std/web`)
- `services` -> `models` (MUST NOT import `views`, `controllers`, `std/web`)
- `views` -> `models` (read-only view data), `std/web/view`, `std/html` (MUST NOT import `repositories`, `std/db/*`, `controllers`)
- `controllers` -> `services`, `views`, `middleware`, `std/web/*`
- `main` -> `app`, `config`, `std/net`, `std/sync`, `std/log`

---

## 5. Staged Implementation Roadmap

### Phase 1: `std/web/` Foundation + Security Unit Tests
- Implement `std/web/request.yz`, `std/web/response.yz`, `std/web/cookie.yz`, `std/web/form.yz`, `std/web/csrf.yz`, `std/web/static.yz`, and `std/web/view.yz`.
- Add focused validation script `scripts/validate-std-web.sh` running in debug, `-O2`, and `ASan/UBSan` modes covering:
  - HTML XSS escaping & `javascript:` href rejection
  - URL-encoded form decoding, field limits, control-byte rejection
  - Signed cookie tampering rejection & expiry checks
  - Constant-time CSRF verification (valid, missing, mismatched session)
  - Rooted static serving (valid `.css`/`.js`, `304` ETag, `../` traversal rejection, symlink rejection, disallowed extension rejection)

### Phase 2: Production `templates/fullstack/` MVC Monolith
- Replace the stub `templates/fullstack/` with the working single-binary MVC starter (Dashboard + Task/Catalog CRUD + YazaBase SQL + Argon2id Auth/Session + CSRF + YAZA UI + `yaza-live.js` progressive fragments + `/api/v1/*` JSON + `/healthz`/`/readyz`/`/metrics`).
- Support `YAZA_SELF_TEST=1` verifying full SSR HTML rendering, fragment responses, CSRF rejection (`403`), authenticated form POST (`303`), JSON API parity, and static asset serving.
- Verify live in a real browser (`http://127.0.0.1:8080`) with zero CSP console violations.

### Phase 3: CLI Integration & Architecture Guardrails
- Add `--mvc` alias alongside `--fullstack` in `compiler/llvm_cli.yz` (`yaza new --fullstack` / `yaza new --mvc`).
- Upgrade `yaza arch check` to parse `[architecture]` import rules across `src/` files and fail on layer violations (e.g., a View importing a Repository directly).
- Add web rules to `yaza audit --security` (flagging unescaped HTML assembly, missing CSRF middleware on web POST routes, or insecure cookie flags).

### Phase 4: CI, Sanitizers & Fullstack SSR Benchmarks
- Wire `scripts/validate-std-web.sh` and generated fullstack `YAZA_SELF_TEST=1` into `.github/workflows/release-matrix.yml`.
- Benchmark YAZA Fullstack Arena-SSR (`GET /` full HTML + SQLite/YazaBase query, `X-Yaza-Fragment` partial swap, Peak RSS, binary size) against comparable stacks (Go `html/template`, Rust Askama/Axum, Node/Next.js, PHP/Laravel) and publish raw reproducible data.

---

## 6. Acceptance Gates (Before Merging to `main`)

1. All existing core, database, API, microservice, and sanitizer suites remain 100% green.
2. `std/web` positive and adversarial security fixtures (XSS, CSRF, cookie forgery, `../` traversal, symlink escape, oversized form body) pass under normal, `-O2`, and `ASan/UBSan`.
3. `yaza new --fullstack demo_app` compiles cleanly (`yaza check`, `yaza build --release`), passes `YAZA_SELF_TEST=1`, and renders cleanly in a live browser with zero CSP errors and working progressive fragment swaps.
4. No third-party runtime dependencies are introduced.


---

## Addendum: First-Class Native `view` Syntax (Zero `list_push` Boilerplate)

To eliminate manual `list_push(p, "...")` and quote-escaping (`\"`) noise in view templates without introducing runtime template reflection, `compiler/core_hir.yz` natively supports `view` declarations:

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

- **`view <name>(<params>):`** compiles directly to `fn <name>(<params>) -> Text:` backed by an arena-scoped `TextList` + single-pass `text_join`.
- **`{{ expr }}`** auto-escapes via `view_escape(expr)` (XSS-safe by default).
- **`{! expr !}`** inserts explicit trusted HTML/child view components without double-escaping.
- **Line sigils**: lines starting with `<`, `{{`, `{!`, or `|` (plain text line) emit markup; all other indented lines (`let`, `var`, `set`, `if`/`elif`/`else`, `while`, `for`) compile as native statically-typed YAZA control flow with 2-space or 4-space HTML indentation freely supported.
