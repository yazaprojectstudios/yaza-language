# YAZA

YAZA is a compiled programming language with an LLVM backend, a native command-line tool, and YazaBase, its embedded SQL database. The CLI can create API and service projects, run or build `.yz` programs, generate API client specifications, and manage a YazaBase database.

## Requirements

- macOS or Linux on x86-64 or ARM64
- Clang, `xz`, and `tar`
- Argon2 development files to build the YazaBase credential support (`libargon2-dev` on Debian/Ubuntu, or `argon2` with Homebrew)

Windows is not currently part of the release build matrix.

## Install from source

The repository includes compressed LLVM bootstrap files for the supported macOS and Linux targets. Build the native tools for the current machine:

```sh
./scripts/build-host.sh
export PATH="$PWD/build:$PATH"
yaza --version
```

On Ubuntu/Debian, install build dependencies first:

```sh
sudo apt-get install clang xz-utils libargon2-dev
```

On macOS with Homebrew:

```sh
brew install argon2
```

For a release build, download the archive matching the operating system and CPU architecture from the GitHub Releases page, extract it, and add its `bin/` directory to `PATH`. The release archive contains the matching runtime, standard library, and project templates.

## Run a program

Create `hello.yz`:

```yaza
start:
    say "Hello, world!"
```

Then run it:

```sh
yaza run hello.yz
```

Build an optimized executable:

```sh
yaza build --release hello.yz -o hello
./hello
```

## Create an API project

Generate the layered API starter:

```sh
yaza new --api catalog-api
cd catalog-api
```

Provision a YazaBase user and a token secret before starting the service. Use a unique password and keep both values out of source control:

```sh
mkdir -p data
yaza db set-user ./data admin '<unique-password>'
export YAZABASE_USER=admin
export YAZABASE_PASSWORD='<unique-password>'
export YAZA_AUTH_SECRET="$(yaza secret token)"
yaza run src/main.yz
```

The generated project defaults to YazaBase. For a local source checkout, set `YAZA_STDLIB_DIR`, `YAZA_RUNTIME_DIR`, and `YAZABASE_BIN` if the CLI cannot find the matching files automatically. For deployment, load secrets from the environment or a secret manager; do not commit them to `yaza.toml` or `.env`.

Generate API tools from a project:

```sh
yaza generate --openapi -o openapi.json
yaza generate --postman -o postman.json
```

## Use YazaBase

Open an in-memory SQL shell:

```sh
yazabase shell :memory:
```

Create a persistent database and execute SQL:

```sh
yazabase init ./data catalog
yazabase sql ./data catalog 'CREATE TABLE products (id BIGINT PRIMARY KEY, name TEXT)'
yazabase sql ./data catalog 'SELECT id, name FROM products'
```

Run `yaza --help` or `yazabase --help` for the full command list. Use `yaza audit --security` to scan a YAZA project configuration and source tree for common risky patterns.

## Development and branches

Run focused validation from the repository root:

```sh
sh scripts/validate-core2-features.sh
sh scripts/validate-yazabase-api-v1.sh
sh scripts/validate-api-profile-config.sh
```

Use short-lived pull-request branches from `main`: `feat/<topic>`, `fix/<topic>`, or `docs/<topic>`. Release builds are created from version tags such as `v1.0.1`; the release workflow publishes the artifacts built by its four-platform matrix.

## Repository scope

The compiler, runtime, standard library, YazaBase engine, project templates, bindings, editor support, and source examples are kept together so the CLI and release packages can be built and checked reproducibly. Generated binaries and build output belong in the ignored `build/` and `dist/` directories.
