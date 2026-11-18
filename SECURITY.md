# Security Policy

## Reporting a vulnerability

Please **do not** open a public GitHub issue for security vulnerabilities in the YAZA compiler, C runtime, standard library, or YazaBase database/HTTP API.

Instead, report vulnerabilities privately through GitHub Security Advisories on `yazaprojectstudios/yaza-language` or via the security contact published at https://yazastudios.id/docs/security/.

Include:
- Affected component and version/commit SHA
- Minimal reproduction steps or proof of concept
- Potential impact and suggested remediation (if known)

## Security design principles

- **Fail-closed defaults**: No built-in default database passwords (`YAZABASE_USER` / `YAZABASE_PASSWORD` must be explicitly provisioned with `yaza db set-user`).
- **Strong token signing**: `YAZA_AUTH_SECRET` / `YAZABASE_API_SECRET` requires at least 32 characters (`HS256` JWT / `YWT` v1) with real Unix epoch expiry validation.
- **Argon2id password hashing**: Stored `.yaza_credentials` hashes use salted Argon2id (`libargon2`).
- **Loopback / TLS boundary**: Plain HTTP binding on non-loopback addresses is rejected unless explicitly fronted by a TLS-terminating reverse proxy (`YAZA_TRUST_TLS_PROXY=1`).
