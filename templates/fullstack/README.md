# {{project_name}} — YAZA Fullstack Monolith (Arena-SSR MVC + Hypermedia Fragments)

Single-binary **Backend + Frontend + Embedded YazaBase ACID SQL** monolith following **RFC 0009**:
- **Zero-GC Arena-SSR:** Every request executes inside a bounded `arena_scope` (`yaza.toml` `[api.resources]`), reclaimed in 1 step on exit.
- **Compiled Type-Safe Views (`src/views/`):** Native LLVM-compiled HTML components with `std/html.yz` XSS escaping and `html_href_is_safe` link checks.
- **Tri-Mode Controllers (`src/controllers/`):** Serve Full HTML SSR (`GET /`), instant Hypermedia Fragment swaps (`X-Yaza-Fragment` via zero-dep `public/yaza-live.js`), and JSON API (`/api/v1/tasks`) from the same MVC controller & service layer.
- **Web Security by Default:** YazaBase Argon2id authentication, HMAC-SHA256 signed `HttpOnly; SameSite=Lax` session cookies, session-bound CSRF synchronizer tokens, strict Web CSP (`script-src 'self'`), and confined static asset serving (`std/web/static`).

## Quick Start

1. Provision YazaBase credentials (used for both SQL storage & Argon2id web login):
```sh
mkdir -p data
yaza db set-user ./data admin 'AdminPass_2026_Secure'
export YAZABASE_USER=admin
export YAZABASE_PASSWORD='AdminPass_2026_Secure'
export YAZA_AUTH_SECRET="$(yaza secret token)"
```

2. Type-check, build, and run built-in self-test (SSR + Fragment + CSRF + Auth + Static + JSON API):
```sh
yaza check
yaza build --release src/main.yz -o {{project_name}}
YAZA_SELF_TEST=1 ./{{project_name}}
```

3. Start the live fullstack server:
```sh
./{{project_name}}
# Open http://127.0.0.1:8080/login (sign in with admin / AdminPass_2026_Secure)
```

## Architecture Layers
- `src/models/` — Domain entities (`TaskItem`) & parameterized YazaBase SQL repository
- `src/services/` — Pure business logic & metrics (no HTML/HTTP imports)
- `src/middleware/` — Fail-closed auth secret, signed session cookies, Argon2id verification, CSRF guard
- `src/views/` — Compiled YAZA UI layouts, pages, and partial fragments
- `src/controllers/` — Auth + Tri-mode Task/Dashboard MVC & JSON controllers
- `public/` — YAZA Current CSS (`yaza-ui.css`) & ~2 KB progressive fragment swapper (`yaza-live.js`)
