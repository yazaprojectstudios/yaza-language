# Contributing to YAZA

Thank you for your interest in contributing to the YAZA programming language, runtime, standard library, and YazaBase engine.

## Getting started

1. Fork `yazaprojectstudios/yaza-language` and clone your fork locally.
2. Create a short-lived branch from `main` using `feat/<topic>`, `fix/<topic>`, or `docs/<topic>`.
3. Build the host toolchain:

```sh
sh scripts/build-host.sh
export PATH="$PWD/build:$PATH"
```

## Required checks before opening a pull request

Run the focused compiler, runtime, and YazaBase verification suites from the repository root:

```sh
sh scripts/validate-core2-features.sh
sh scripts/validate-yazabase-api-v1.sh
sh scripts/validate-api-profile-config.sh
YAZA_STDLIB_DIR="$PWD/std" build/yaza run examples/auth-jwt-ywt-rbac-smoke.yz
YAZA_TEST_CLI="$PWD/build/yaza" sh templates/api/tests/generation-smoke.sh
```

On Linux, also run the runtime C sanitizer checks when modifying `runtime/`:

```sh
CC=clang sh scripts/validate-runtime-v1-linux.sh
```

## Coding & security expectations

- Never commit secrets, passwords, private keys, `.env` files, or generated `build/` / `dist/` binaries.
- Preserve fail-closed security behavior in `std/db/yazabase`, `std/http`, `std/jwt`, `std/ywt`, and `yazabase/api_v1`.
- Keep changes focused and add/update tests in `examples/` or `runtime/tests/` for any behavioral change.
- All contributions are licensed under the Apache License, Version 2.0 (`LICENSE`).
