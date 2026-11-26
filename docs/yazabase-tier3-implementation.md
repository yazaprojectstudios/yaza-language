# YazaBase Tier 3 Implementation & Tier 2+3 Benchmarks

YazaBase Tier 3 completes horizontal multi-shard routing and rebalancing, declarative schema diffs with crash-safe migration journals, and canonical binary `YJB1` JSONB storage with durable expression indexes on top of Tier 1 and Tier 2.

## Implemented Subsystems

### Tier 3.1: Horizontal Sharding & Scatter-Gather ([`yazabase/shard_router_v1.yz`](../yazabase/shard_router_v1.yz))
- **Hash & Contiguous Range Partitioning**:
  - `yaza_db_shard_new_hash_cluster_v1` (FNV-1a 31-bit modulo placement) and `yaza_db_shard_new_range_cluster_v1` (validating contiguous non-overlapping `[min, max)` partitions, failing closed on gaps or bounds errors).
  - `yaza_db_shard_locate_id_v1` resolves target shard IDs for both modes.
- **Lag-Aware Read/Write Routing**:
  - `yaza_db_shard_set_replica_v1` registers healthy/lag replica states per shard.
  - `yaza_db_shard_route_rw_v1` routes writes to the shard primary and reads to the lowest-lag healthy replica within `max_replica_lag_lsn`, falling back to primary or rejecting fail-closed when `allow_primary_fallback == false`.
- **Scatter-Gather & Live Hash Expansion**:
  - `yaza_db_shard_scatter_gather_v1` executes SQL projections across active shard sessions and merges distributed `COUNT`, `SUM`, `MIN`, `MAX`, and sorted top-`K` `ORDER BY ... LIMIT`.
  - `yaza_db_shard_live_expand_hash_v1` migrates affected keys from `N` shards to `N + 1` shards preserving column `type_id`s, deletes moved rows on source shards, bumps cluster `epoch`, and verifies total row conservation across all shards.

### Tier 3.2: Declarative Schema Diff & Crash-Safe Migration Journal ([`yazabase/schema_migration_v1.yz`](../yazabase/schema_migration_v1.yz))
- **Manifest Parsing & Live Catalog Diff**:
  - `yaza_db_mig_parse_manifest_v1` parses desired JSON table/column/index manifests via `jsonb_v1`.
  - `yaza_db_mig_plan_diff_v1` introspects `yaza_db_catalog_snapshot_load_with_roots_v1` and `yaza_db_index_catalog_rows_v1` (ignoring internal `sys_*` and `_ydb_*` catalog tables) to emit deterministic `CREATE_TABLE`, `ADD_COLUMN`, `ALTER_TYPE`, `DROP_COLUMN`, `CREATE_INDEX`, and `DROP_TABLE` steps.
- **Safety Classification & `YDM1` Journal**:
  - Classifies each operation as `SAFE_ONLINE`, `REQUIRES_BACKFILL`, or `DESTRUCTIVE_BLOCKED` (rejecting destructive changes unless `allow_destructive == true`).
  - `yaza_db_mig_write_journal_v1` and `yaza_db_mig_read_journal_v1` persist step state (`PENDING` -> `IN_PROGRESS` -> `APPLIED`) atomically in `schema-migration.journal` with CRC32C verification (`YDM1`).
  - `yaza_db_mig_apply_plan_v1` resumes cleanly after simulated mid-plan crashes, skipping `APPLIED` steps without re-execution.

### Tier 3.3: Native Canonical Binary JSONB (`YJB1`) & Expression Indexes ([`yazabase/jsonb_v1.yz`](../yazabase/jsonb_v1.yz))
- **Canonical Binary Encoding & Validation**:
  - Supports 7 JSON types (`Null`, `Bool`, `Int64`, `Float64`, `Text`, `Array`, `Object`) with sorted unique object keys so equivalent JSON inputs produce byte-for-byte identical encodings (`bytes_equal`).
  - `YJB1` 12-byte header (`magic`, `ver=1`, `flags=0`, CRC32C over payload); `yaza_db_jsonb_decode_v1` / `yaza_db_jsonb_validate_v1` reject duplicate keys, malformed syntax, non-strict key order, trailing bytes, and corrupted CRCs fail-closed.
- **Path Extraction, Containment & Functional Updates**:
  - `yaza_db_jsonb_extract_path_v1` (`->`), `yaza_db_jsonb_extract_path_text_v1` (`->>`), `yaza_db_jsonb_contains_v1` (`@>` nested object/array subset matching), and `yaza_db_jsonb_set_v1`.
- **Durable `.yzdb` Expression Index**:
  - `yaza_db_jsonb_index_init_v1`, `yaza_db_jsonb_index_upsert_v1` (`SELECT` check -> in-place `UPDATE` or `INSERT` respecting clustered PK tombstones), `yaza_db_jsonb_index_delete_v1`, and `yaza_db_jsonb_index_lookup_v1` backed by indexed `.yzdb` tables surviving close/reopen.

---

## Verified Tier 3 Validation Gates (`normal`, `-O2`, `ASan+UBSan`)

| Gate | Command | Modes Verified | Result |
| --- | --- | --- | --- |
| Tier 3.1 Sharding | `sh scripts/validate-yazabase-tier3-sharding.sh` | `normal`, `o2`, `asan` | `yazabase-tier3-sharding-validation-ok` |
| Tier 3.2 Schema Migrations | `sh scripts/validate-yazabase-tier3-migration.sh` | `normal`, `o2`, `asan` | `yazabase-tier3-migration-validation-ok` |
| Tier 3.3 Native JSONB `YJB1` | `sh scripts/validate-yazabase-tier3-jsonb.sh` | `normal`, `o2`, `asan` | `yazabase-tier3-jsonb-validation-ok` |

---

## Tier 2 + Tier 3 `-O2` Micro-Benchmarks ([`examples/yazabase-tier23-bench.yz`](../examples/yazabase-tier23-bench.yz))

Measured with Clang `-O2` on Apple M5 (`darwin/arm64`):

| Subsystem | Workload | Raw Output | Latency / Rate |
| --- | --- | --- | --- |
| Concurrent Engine (`Tier 2.1`) | 4 writer threads × 25 tx (`100` commits) | `commits=100 batches=50 fsyncs=50 total_ns=291475000` | `2.915 ms/commit` (~343 commits/s, 2.0× fsync batching) |
| Vector Search (`Tier 2.2`) | 200 × 16-dim vectors, 20 L2 top-5 queries | `flat_20_ns=3851000 hnsw_20_ns=11514000 flat_top1=77 hnsw_top1=77` | Flat `192.6 µs/q`, HNSW `575.7 µs/q` (exact top-1 match) |
| Full-Text Search (`Tier 2.2`) | 100 docs, 25 multi-term AND BM25 queries | `queries=25 topk=5 hits=5 total_ns=171000` | **`6.84 µs/query`** |
| JSONB Codec & Path (`Tier 3.3`) | 500× encode / `->>` extract / `@>` contains | `enc_500_ns=3134000 path_500_ns=987000 contains_500_ns=1437000` | `6.27 µs` enc / `1.97 µs` path / `2.87 µs` contains |
| JSONB Expr Index (`Tier 3.3`) | 20 SQL index lookups across 60 rows | `idx_lookup_20_ns=46349000 idx_hits=1` | `2.317 ms/lookup` |
