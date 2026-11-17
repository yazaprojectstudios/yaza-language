# {{project_name}}

A minimal YAZA HTTP API starter. It keeps the listener and its simple routes in `src/main.yz`; it does not scaffold controllers, services, repositories, or database files.

## Run

```sh
yaza run
```

The server starts at `http://127.0.0.1:3000`. Use `yaza run --port 8080` to request another port. If a requested port is occupied, YAZA reports the next available port.

Try the root route:

```sh
curl http://127.0.0.1:3000/
```

The runtime also provides `/healthz`, `/readyz`, and `/metrics`.

## Generate API documents

```sh
yaza generate
```

This writes the Postman collection, OpenAPI 3.1 document, and Swagger-compatible JSON into `docs/api/`. Use `yaza generate --postman`, `yaza generate --openapi`, or `yaza generate --swagger` to generate one artifact.
