# {{project_name}} API

This starter creates an HTTP API project backed by YazaBase. It includes product CRUD routes, health endpoints, environment-based credentials, token authentication, and OpenAPI/Postman generation.

## Run locally

From the generated project directory, provision the database credentials, then start the service:

```sh
mkdir -p data
yaza db set-user ./data admin '<unique-password>'

export YAZABASE_USER=admin
export YAZABASE_PASSWORD='<unique-password>'
export YAZA_AUTH_SECRET="$(yaza secret token)"
yaza run src/main.yz
```

Do not commit real passwords or token secrets. The `set-user` command takes its password as a command-line argument; use it only in a trusted local shell, and avoid reusing a production password.

The default listener is `127.0.0.1:8080`. Configuration lives in `yaza.toml`; secrets belong in environment variables or a secret manager.

## Generate API client files

```sh
yaza generate --openapi -o openapi.json
yaza generate --postman -o postman.json
```

Run `yaza --help` for the full CLI command list.
