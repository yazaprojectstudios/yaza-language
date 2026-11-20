# {{project_name}} — YAZA Fullstack Monolith (Arena-SSR MVC + Hypermedia Fragments)

Single-binary **Backend + Frontend + Embedded YazaBase ACID SQL** monolith following **RFC 0009**:
- **Zero-GC Arena-SSR:** Every request executes inside a bounded `arena_scope` (`yaza.toml` `[api.resources]`), reclaimed in 1 step on exit.
- **Compiled Type-Safe Views (`src/views/`):** Native LLVM-compiled HTML components with `std/html.yz` XSS escaping and `html_href_is_safe` link checks.
- **Tri-Mode Controllers (`src/controllers/`):** Serve Full HTML SSR (`GET /`), instant Hypermedia Fragment swaps (`X-Yaza-Fragment` via zero-dep `public/yaza-live.js`), and JSON API (`/api/v1/tasks`) from the same MVC controller & service layer.
- **Web Security by Default:** Separate Argon2id web-admin credentials, HMAC-SHA256 signed `HttpOnly; SameSite=Lax` session cookies, session-bound CSRF tokens, login throttling, strict Web CSP (`script-src 'self'`), and confined static assets.

## Quick Start

1. Provision YazaBase service credentials and a separate web-admin account:
```sh
mkdir -p data
yaza db set-user ./data admin 'AdminPass_2026_Secure'
export YAZABASE_USER=admin
export YAZABASE_PASSWORD='AdminPass_2026_Secure'
export YAZA_AUTH_SECRET="$(yaza secret token)"
yaza build --release src/admin_setup.yz -o admin-setup
YAZA_WEB_ADMIN_USER=admin YAZA_WEB_ADMIN_PASSWORD='DifferentWebPassword_2026' ./admin-setup
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
# Open http://127.0.0.1:8080/login (web admin uses DifferentWebPassword_2026)
```

## Architecture Layers
- `src/models/` — Domain entities (`TaskItem`) & parameterized YazaBase SQL repository
- `src/services/` — Pure business logic & metrics (no HTML/HTTP imports)
- `src/middleware/` — Fail-closed auth secret, signed session cookies, Argon2id verification, CSRF guard
- `src/views/` — Compiled YAZA UI layouts, pages, and partial fragments
- `src/controllers/` — Auth + Tri-mode Task/Dashboard MVC & JSON controllers
- `public/` — YAZA Current CSS (`yaza-ui.css`) & ~2 KB progressive fragment swapper (`yaza-live.js`)

## HTTPS client

`std/http_client.yz` provides bounded `http_client_get`, `http_client_post_json`,
and `http_client_request`. TLS certificate and hostname verification are
mandatory. Only `https://` is accepted; redirects are returned to the caller
and responses are bounded (1–16 MiB). Pass bearer credentials through the
dedicated parameter, not the URL. macOS and Linux builds require libcurl;
the Windows build currently reports this transport as unavailable. For a
private CA, set `YAZA_TLS_CA_BUNDLE` to an absolute PEM bundle path; peer and
hostname verification remain enabled.

## Versioned migrations

`src/migrations/registry.yz` lists ordered, checksummed migrations. Build and
run `src/migrate.yz` when deploying a new version:

```sh
yaza build --release src/migrate.yz -o migrate
./migrate up
```

`./migrate down` rolls back the latest applied migration. The bundled first
migration creates `mail_jobs`; the second stores one-time account action tokens;
the third records admin email verification.
A rollback can delete queued messages or active reset links, so only
run it after reviewing the data. YazaBase currently commits each SQL
statement separately; migration metadata is marked `pending` or
`rolling_back` before DDL. If interrupted, the next run stops with a dirty
state and requires operator inspection and repair. Do not edit an applied
migration; add a new version instead.

## Email through a local SMTP relay or Google/Brevo

`src/services/mail_service.yz` provides `mail_service_send(to, subject, body)`.
The default `YAZA_MAIL_PROVIDER=local` connects only to a trusted relay on
`127.0.0.1`; set `YAZA_MAIL_RELAY_PORT` (default `25`). Mailpit on port
`1025` is useful for development. The local MTA handles TLS, authentication,
retries, and outbound delivery.

On macOS and Linux builds with libcurl, set `YAZA_MAIL_PROVIDER=google` or
`brevo` to submit directly over mandatory, certificate-verified STARTTLS on
port `587`. Set `YAZA_MAIL_FROM`, `YAZA_MAIL_USERNAME`, and
`YAZA_MAIL_CREDENTIAL` through your secret manager. Google supports
`YAZA_MAIL_AUTH_MODE=oauth2` with a current access token; `password` uses
an app password where the account permits one. Brevo uses `password` with
its SMTP login and **SMTP key**. Windows builds currently report that secure
outbound transport is unavailable. Subject lines are printable ASCII; bodies
are UTF-8 and MIME Base64 encoded.
The same `YAZA_TLS_CA_BUNDLE` setting applies to direct SMTP when the provider
uses a private CA. Keep that bundle under deployment control.

Example environment values (load the credential from a secret manager):

| Provider | `YAZA_MAIL_PROVIDER` | `YAZA_MAIL_USERNAME` | `YAZA_MAIL_AUTH_MODE` | `YAZA_MAIL_CREDENTIAL` |
| --- | --- | --- | --- | --- |
| Google | `google` | Gmail/Workspace address | `oauth2` or `password` | Current OAuth2 access token or account app password |
| Brevo | `brevo` | Brevo SMTP login | `password` | Brevo SMTP key |

Google OAuth2 access tokens expire; the deployment must refresh them. A Google
app password is available only for eligible accounts with two-step verification.
Brevo requires an enabled transactional SMTP account and a verified sender.

```yaza
use "services/mail_service"

fn send_welcome(address: Text) -> Result[Boolean, Text]:
    return mail_service_send(address, "Welcome", "Your account is ready.")
```

For request handlers, enqueue mail in the YazaBase outbox and run a separate
worker process. The worker uses a file lock so one process claims jobs at a
time, retries failures with bounded backoff, and moves jobs to `failed` after
five attempts:

```sh
yaza build --release src/mail_worker.yz -o mail-worker
./mail-worker
```

```yaza
fn queue_welcome(address: Text) -> Result[Number, Text]:
    return mail_service_enqueue(address, "Welcome", "Your account is ready.")
```

The outbox provides **at-least-once submission**. A crash after SMTP accepts
the message but before the database marks it `sent` can produce a duplicate;
include an application message ID and handle duplicates where they matter.
The worker currently polls every 250 ms and uses one YazaBase CLI subprocess
per SQL statement. `mail_service_send` remains available for synchronous
tools. An SMTP success means the relay accepted the message, not that the
recipient received it.

## Metrics and account security

`/metrics` exposes request counts, coarse latency histogram, and p95/p99
bucket upper bounds. Access logs include a server-generated request ID, status,
and latency. Set `YAZA_TRACE_REQUESTS=1` to emit correlated controller,
service, and database timing spans for task routes. This is local structured
tracing; OTLP export and propagation across services are not implemented.
The MVC login permits five
attempts per account hash per 15 minutes, with a 256-key process-local table.
The limiter resets on process restart and cannot replace an edge rate limit.
Current MVC authentication is a **single configured admin account** separate
from the YazaBase service account. Configure `YAZA_ADMIN_EMAIL` and
`YAZA_PUBLIC_BASE_URL` to enable the `/forgot-password` email flow. Production
requires an HTTPS public base URL. The reset link is one-use, expires in 15
minutes, and changing the web-admin Argon2id verifier revokes old web sessions.
It does not change the database service password. The logged-in admin can
request email verification at `/email-verification`; a one-time link records
the configured address as verified. This single-admin flow is not a multi-user
registration system. Durable per-user sessions and general user management
need a separate account model.
