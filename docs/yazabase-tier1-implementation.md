# YazaBase Tier 1: Implementation & Verification Gates

Status: **Tier 1 Verified Complete** on branch `yazabase-tier1-2-3` (26 Nov 2026). All measurements below were observed directly from `--release` (`-O2`) builds on Apple M5 (`darwin/arm64`) and verified across normal, `-O2`, and `ASan+UBSan` runs.

---

## 1. Delta WAL (`rec_type = 17`, `LEAF_SLOT_DELTA`) & Write Statement Plan Cache

- **Implemented & Verified:**
  - `yazabase/delta_wal_v1.yz` (`rec_type = 17`, magic `"YDL1"`): compact ordered slot-delta WAL records storing changed slot keys and before/after row payloads for `INSERT`, `UPDATE`, and `DELETE` whenever a leaf page already has a full-page image recorded in the current checkpoint window.
  - Full ARIES integration (`yazabase/wal_v1.yz`, `yazabase/checkpoint_v1.yz`, `yazabase/transaction_v1.yz`, `yazabase/group_commit_v1.yz`, `yazabase/replication_v1.yz`): REDO, UNDO, CLR compensation, streaming WAL replica replay, checkpoint window FPI tracking/reset, and rollback/reopen recovery handle both full-page (`1..16`) and `LEAF_SLOT_DELTA` (`17`) records transparently.
  - Wired across the active overlay & group-commit writers for `INSERT`, `UPDATE`, and `DELETE` (`sql_insert_query_v1.yz`, `sql_update_query_v1.yz`, `sql_delete_query_v1.yz`).
  - **Write Statement Plan Cache** (`yazabase/plan_cache_v1.yz`, `yazabase/session_v1.yz`): bounded 64-entry write plan cache covering parameterized `INSERT`, `UPDATE`, and `DELETE` alongside the 64-entry `SELECT` plan cache. Automatically invalidated on DDL (`CREATE TABLE`, `DROP TABLE`, `ALTER TABLE`, `CREATE INDEX`, `DROP INDEX`, `CREATE VIEW`, `DROP VIEW`) or `catalog_root` / `table_schema_version` change, while re-evaluating bind parameters, constraints, FK/CHECK rules, MVCC visibility, target rows, and page/WAL budgets fresh on every execution.
- **Measured WAL Footprint Reduction (100-statement Group Commit probe, `-O2`):**
  - 100 grouped `INSERT`s: **54,000 bytes** WAL total (540 B/row avg, 1 fsync, `61.48 ms`) vs ~3.28 MB with 1 full-page mutation record per statement (~60× smaller).
  - 100 grouped parameterized `UPDATE`s: **58,448 bytes** WAL total (~584 B/row avg, 1 fsync, `73.39 ms`), 99 write plan cache hits on 1 miss.
  - 100 grouped parameterized `DELETE`s: **58,448 bytes** WAL total (~584 B/row avg, 1 fsync, `64.66 ms`), 99 write plan cache hits on 1 miss.

## 2. Bidirectional Logical LSN Sync Engine (`Mobile <-> Server`)

- **Implemented & Verified:**
  - `yazabase/sync_engine_v1.yz`: logical change stream above the WAL commit boundary emitting `{origin_id, origin_seq, txn_id, commit_ts, schema_version, table_name, operation (1=UPSERT, 2=DELETE), pk_key_hex, row_json, row_version}`.
  - Handles `LEAF_SLOT_DELTA` (`17`), `LEAF_INSERT`, `LEAF_UPDATE`, `LEAF_DELETE`, and `NODE_SPLIT`, skips uncommitted/aborted and `CATALOG_ROOT_COMMIT` (`16`) DDL-only WAL spans without stalling the extraction cursor, and prevents ping-pong echo via `applied_local_txns` + defeated-outbox pruning.
  - Persists watermarks, row versions, delete tombstones (with bounded `yaza_db_sync_prune_tombstones_v1`), outbox, and echo-prevention state atomically per database UUID in `ydb-sync-<uuid>.state` (CRC32C-verified, fsync + atomic rename).
  - Deterministic conflict resolution policies (`SERVER_WINS = 1`, `CLIENT_WINS = 2`, `VERSION_LWW = 3` using `(row_version, origin_id)` tiebreak — never wall-clock only).
  - Fail-closed batch CRC32C and `schema_version` verification before any row mutation.
  - Authenticated HTTP endpoints wired into `yazabase/cluster_api_v1.yz` (`GET /api/v1/sync/status`, `GET /api/v1/sync/pull`, `POST /api/v1/sync/push` enforcing YWT auth, read-only replica rejection with HTTP 409, server-controlled conflict policy rejection with HTTP 403, and schema mismatch HTTP 409) plus mobile/embedded wrappers in `std/db/yazabase_embedded.yz`.

## 3. Transparent Authenticated Page-Level & WAL/Backup Encryption-at-Rest

- **Implemented & Verified:**
  - `yazabase/page_crypto_v1.yz` & `yazabase/storage_v1.yz`: Encrypt-then-MAC over exact 16,384-byte `.yzdb` pages (HMAC-SHA256 CTR + 32-byte HMAC-SHA256 tag over `format_ver || db_uuid || page_id || key_epoch || nonce || ciphertext`), using only YAZA runtime audited crypto primitives and verified with `bytes_equal_constant_time` prior to decryption or page decoding.
  - Companion authenticated sidecar (`YDC1`, `ydb-crypt-<uuid>.meta`) preserves `.yzdb` 16 KiB page alignment; unencrypted databases (`flags == 1`) remain 100% compatible, while encrypted databases (`flags == 3`) fail closed on legacy unencrypted `yaza_db_storage_open_v1`.
  - Supports PBKDF2-SHA256 passphrases and raw 32-byte KMS/Keystore keys, authenticated `YDE1` envelope (`seal_blob`, `open_blob`, `seal_file`, `open_file`) for WAL segments, checkpoints, backups, and temporary files, zeroizes key material on close (`bytes_zeroize`), and supports crash-safe atomic key rotation (`rekey` pre-commit rollback & post-commit roll-forward recovery).

---

## Verified Validation Gates (26 Nov 2026, branch `yazabase-tier1-2-3`)

| Validation Script / Target | Modes Tested | Observed Result |
| --- | --- | --- |
| `scripts/validate-yazabase-page-crypto.sh` | normal, `-O2`, ASan+UBSan | `yazabase-page-crypto-v1-validation-ok` |
| `scripts/validate-yazabase-tier1.sh write` | normal, `-O2`, ASan+UBSan | `yazabase-tier1-write-smoke-validation-ok` |
| `scripts/validate-yazabase-tier1.sh all` | normal, `-O2`, ASan+UBSan | `yazabase-tier1-smoke-validation-ok` |
| `examples/yazabase-delta-wal-v1-smoke.yz` | `-O2` | `delta WAL smoke passed` |
| `examples/yazabase-delta-wal-storage-smoke.yz` | `-O2` | `delta WAL storage smoke passed` |
| `examples/yazabase-leaf-recovery-v1-smoke.yz` | `-O2` | `yazabase-leaf-recovery-v1-ok cases=6` |
| `scripts/validate-yazabase-phase-b.sh` | focused | `yazabase-phase-b-focused-validation-ok` |
| `scripts/validate-yazabase-phase-c.sh` | focused | `yazabase-phase-c-focused-validation-ok` |
| `scripts/validate-yazabase-session.sh` | focused | `yazabase-session-focused-validation-ok` |
| `scripts/validate-yazabase-alter-table.sh` | focused | `yazabase-alter-table-focused-validation-ok` |
| `scripts/validate-yazabase-secondary-index.sh` | focused | `yazabase-secondary-index-focused-validation-ok` |
| `scripts/validate-yazabase-unique-index.sh` | focused | `yazabase-unique-index-focused-validation-ok` |
| `scripts/validate-yazabase-sync-recovery.sh` | fsync/EIO injection | Exit 0 (all crash-boundary & WAL-tail checks passed) |
| `scripts/build-yazabase-cli.sh` + `validate-yazabase-cli-profile.sh` | `--release` | `yazabase-cli-profile-scalar-types-focused-validation-ok` |
| `yazabase-jdbc/build.sh` + `yazabase-jdbc/test/run.sh` | JVM | `ALL CHECKS PASSED` |

---

## Release (`-O2`) Benchmarks: Pre-Tier-1 Baseline vs Tier 1

Measured on Apple M5 (`darwin/arm64`), Clang `-O2`:

| Benchmark / Metric | Baseline (25 Nov 2026) | Tier 1 (26 Nov 2026) | Delta / Notes |
| --- | --- | --- | --- |
| `point_select` warm median | `3,570 ns` (`min=3490`, `max=3710`) | `3,620 ns` (`min=3510`, `max=3660`) | Parity (+1.4%, `page_read=0`) |
| `bench_group_commit` solo 100 INSERTs | `83,375,000 ns` | `80,130,000 ns` | -3.9% faster |
| `bench_group_commit` grouped 100 INSERTs (1 fsync) | `74,643,000 ns` | **`59,614,000 ns`** | **-20.1% faster** (Delta WAL savings) |
| `bench_yazabase_enhancement` INSERT standalone p50 (500 tx) | `793,000 ns` | `789,000` / `796,000 ns` | Parity |
| `bench_yazabase_enhancement` INSERT batch p50 (10×50) | `850,000 ns` (`elapsed=26.94 ms`) | `867,000` / `853,000 ns` (`26.79 ms`) | Parity |
| `bench_yazabase_enhancement` UPDATE batch elapsed (10×50) | `12,277,000 ns` (`p50=1.098 ms`) | `11,847,000` / `11,976,000 ns` (`p50=1.083 / 1.069 ms`) | -2.5% to -3.5% faster (1 outlier run hit `15.08 ms` with `p95=3.83 ms` fsync spike) |
| `bench_yazabase_enhancement` DELETE batch elapsed (10×50) | `10,571,000 ns` (`p50=961 µs`) | `10,659,000` / `10,415,000 ns` (`p50=949 / 944 µs`) | Parity to -1.5% faster |
| `bench_yazabase_enhancement` SELECT p50 (1000 ops) | `98,000 ns` | `98,000 ns` | Exact parity |
| `bench_analytical_scan` `COUNT(*)` / `SUM(val)` (10k rows) | `111 µs` / `130 µs` | `111 µs` / `130 µs` | Exact parity |
| `bench_analytical_scan` `MAX(val)` / `COUNT_RANGE` | `384 µs` / `1,053 µs` | **`126 µs` / `372 µs`** | **3.0× / 2.8× faster** |
| `bench_analytical_scan` `POINT_1000` total | `7,819,000 ns` | **`4,468,000 ns`** | **1.75× faster** (`4.47 µs`/lookup) |

---

## Honest Remaining Boundaries (Out of Tier 1 Scope)

1. **Sync state encryption & outbox retention:** `ydb-sync-<uuid>.state` uses CRC32C integrity and atomic rename but is not yet wrapped inside the `YDE1` encrypted file envelope when encryption-at-rest is enabled, and the local `outbox` array should gain bounded cursor compaction/pruning once all known peers acknowledge past an LSN watermark.
2. **Per-table custom sync conflict resolvers:** `SERVER_WINS`, `CLIENT_WINS`, and `VERSION_LWW` are implemented globally per batch; per-table policy metadata and custom resolver callbacks remain future hardening.
3. **Exhaustive sync-boundary fault injection:** `validate-yazabase-sync-recovery.sh` tests crash recovery on single-node DML/DDL/overflow/capacity boundaries, and `validate-yazabase-page-crypto.sh` tests crash-injected pre/post-commit rekey recovery; an equivalent automated fsync-EIO crash harness specifically inside `yaza_db_sync_state_save_v1` and mid-delta-group-commit remains desirable before calling Tier 2/3 production-hardened.

---

## Tier Roadmap Status (Updated 26 Nov 2026)

| Roadmap item | Status | Implementation & Verification |
| --- | --- | --- |
| **Tier 1: Delta WAL, Sync Engine, Page Crypto** | **COMPLETE** | `yazabase-tier1-smoke-validation-ok` (`normal`, `o2`, `asan`); see sections 1–3 above. |
| **Tier 2.1: Concurrent server & group-commit writer** | **COMPLETE** | `yazabase-tier2-concurrent-validation-ok` (`normal`, `o2`, `asan`); see [`docs/yazabase-tier2-implementation.md`](./yazabase-tier2-implementation.md). |
| **Tier 2.2: Vector search & FTS (`BM25`)** | **COMPLETE** | `yazabase-tier2-search-validation-ok` (`normal`, `o2`, `asan`); see [`docs/yazabase-tier2-implementation.md`](./yazabase-tier2-implementation.md). |
| **Tier 2.3: PITR & CDC tail reader** | **COMPLETE** | `yazabase-tier2-pitr-cdc-validation-ok` (`normal`, `o2`, `asan`); see [`docs/yazabase-tier2-implementation.md`](./yazabase-tier2-implementation.md). |
| **Tier 3.1: Horizontal Sharding & Scatter-Gather** | **COMPLETE** | `yazabase-tier3-sharding-validation-ok` (`normal`, `o2`, `asan`); see [`docs/yazabase-tier3-implementation.md`](./yazabase-tier3-implementation.md). |
| **Tier 3.2: Declarative schema diff & online migration** | **COMPLETE** | `yazabase-tier3-migration-validation-ok` (`normal`, `o2`, `asan`); see [`docs/yazabase-tier3-implementation.md`](./yazabase-tier3-implementation.md). |
| **Tier 3.3: Native `JSONB` & expression indexes** | **COMPLETE** | `yazabase-tier3-jsonb-validation-ok` (`normal`, `o2`, `asan`); see [`docs/yazabase-tier3-implementation.md`](./yazabase-tier3-implementation.md). |
