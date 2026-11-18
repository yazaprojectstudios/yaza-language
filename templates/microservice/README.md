# {{project_name}} (Cloud-Native Microservice)

Microservice berbasis **Hexagonal Architecture (Ports & Adapters)**, **YazaBase Embedded SQL Engine**, **Live HTTP/1.1 Keep-Alive Listener**, dan **Core 2 Async/Await (`spawn` & `await`)**.

## Struktur Arsitektur
- `src/domain/`: Entitas murni & Domain Events (`Order`, `OrderCreatedEvent`) tanpa ketergantungan I/O.
- `src/ports/`: Port Repository asinkron (`std/db/yazabase` parameterized SQL + `std/csv`).
- `src/adapters/`: Inbound HTTP adapter (`GET /healthz`, `/readyz`, `/metrics`, `GET /api/v1/orders`, `GET /api/v1/orders/:id`, `POST /api/v1/orders`).
- `src/main.yz`: Bootstrap HTTP/1.1 multi-worker server (`8` concurrent `spawn` accept workers + `arena_scope` per request) dan `YAZA_SELF_TEST=1` mode.

## Setup Database & Jalankan Layanan

1. Provision YazaBase credentials:
```bash
mkdir -p data
yaza db set-user ./data admin dev_secret_pass_change_me
export YAZABASE_USER=admin
export YAZABASE_PASSWORD=dev_secret_pass_change_me
```

2. Verifikasi type-check, build, dan self-test:
```bash
yaza check
yaza build --release src/main.yz -o {{project_name}}
YAZA_SELF_TEST=1 ./{{project_name}}
```

3. Jalankan live TCP HTTP/1.1 server:
```bash
yaza run
# Listening on http://0.0.0.0:8080
```

## Contoh Request Live
```bash
curl -sf http://127.0.0.1:8080/healthz
curl -sf http://127.0.0.1:8080/readyz
curl -sf http://127.0.0.1:8080/metrics
curl -sf http://127.0.0.1:8080/api/v1/orders
curl -sf -X POST http://127.0.0.1:8080/api/v1/orders \
  -H 'Content-Type: application/json' \
  -d '{"id":"ord_202","customer_id":"cust_7","total_cents":49000}'
```
