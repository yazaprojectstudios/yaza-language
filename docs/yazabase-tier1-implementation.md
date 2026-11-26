# YazaBase Tier 1: implementation gates

Status: in progress. This file records the implementation boundary; none of the performance numbers below are release claims.

## 1. Delta WAL and write plans

The current working tree introduces `LEAF_SLOT_DELTA` (record type 17). Its versioned payload carries an operation, page ID, ordered keys, and before/after row payloads. Full-page records remain the fallback. The first mutation of a page in a checkpoint window needs a full image; subsequent overlay inserts may use a delta when the encoded record is smaller. Checkpoint must reset this per-window decision. Recovery, rollback, and replica replay must accept both record formats. Encode/decode/page-mutation and file-backed redo/replay/undo smoke tests pass locally. Startup recovery passes for committed and uncommitted deltas, including CLR and a second reopen. The Phase B integration fixture passes and inspects the written WAL to confirm that a 30-INSERT group commit emits type-17 records, every emitted delta follows a full image for its page within that batch, the rows persist on reopen, and the batch uses one WAL fsync. The active optimized writer is **overlay INSERT only**: UPDATE delta staging exists but the SQL group-commit path does not call it, and DELETE still uses the existing record format. Exhaustive sync-boundary fault injection and an equivalent UPDATE/DELETE integration path remain required before treating delta WAL as complete.

The current write path still parses and binds each statement. A write-plan cache must cache immutable syntax and schema-bound metadata, keyed by exact SQL plus schema identity. Parameter values, permissions, constraints, MVCC visibility, target rows, and WAL/page budgets must be evaluated anew on every execution. Invalidate on DDL and schema-root change. Measure parse/bind cost separately from commit and fsync. Target WAL reduction and transaction CPU figures are hypotheses until measured on identical workloads.

## 2. Mobile/server bidirectional synchronization

Physical WAL streaming is one-way replication of one database history. Two independently writable databases cannot safely apply each other's physical page records: page IDs, local LSNs, and transaction histories diverge. A local LSN is only a cursor within its origin's log.

Build a **logical change stream** above the WAL commit boundary. Each committed change needs stable origin ID, origin sequence, transaction ID, schema version, table/row identity, operation, and payload. Persist per-origin watermarks and idempotency keys on both peers. Push and pull only committed changes, with bounded batches and authenticated transport. Reject a schema mismatch instead of guessing. Define tombstone retention and conflict policy per table; last-write-wins based only on device wall clocks is unsafe under clock skew. Use explicit server authority or deterministic version ordering by default, with a custom resolver as an opt-in. Test duplicate delivery, replay after crash, offline divergence, deletion, schema migration, and concurrent edits before calling sync complete.

## 3. Encryption at rest

Encryption is disabled until a complete authenticated format and key lifecycle exist. A 16 KiB plaintext page cannot be replaced in place by a 16 KiB AEAD ciphertext **plus** nonce/tag without reserving authenticated metadata elsewhere. Define a new file-format version and encrypted-page envelope (or authenticated sidecar) before changing storage reads/writes. Authenticate before page decoding. Bind database UUID, page ID, format version, and generation into associated data; never reuse a nonce under a key. Encrypt WAL, checkpoints, backups, and temporary files as part of the same threat boundary. Define key IDs, KMS/Keystore integration, rotation, recovery after interrupted writes, and authenticated restore. CRC32C remains useful for accidental corruption but is not a substitute for AEAD. Do not ship an unaudited custom cipher.

## Exit criteria

1. Feature-specific tests pass, including crash recovery at every relevant sync boundary and mixed old/new WAL records.
2. Benchmarks report WAL bytes per mutation, p50/p95 write latency, CPU time, and fsync count against the same baseline, with raw samples.
3. Logical sync proves idempotent convergence across offline edits and restart.
4. Encrypted data, WAL, and backup reject wrong keys and tampering, and key rotation survives crash injection.

Current gate state: item 1 is partially covered for INSERT and recovery; items 2–4 are open. Tier 2 and Tier 3 must not be marked complete from Tier 1's codec or Phase B smoke tests.

## Following tiers: repository inventory

This is an implementation inventory, not a completion claim.

| Roadmap item | Existing foundation | Missing release gate |
| --- | --- | --- |
| Tier 2: concurrent server and group-commit writer | PG wire and HTTP accept loops, runtime tasks/channels, and batched group commit exist. | A bounded multi-connection event loop, one serialized writer queue, isolated read snapshots, backpressure, disconnect/cancellation behavior, and concurrent durability tests. |
| Tier 2: vector and full-text search | B+Tree, overflow pages, and SQL scalar types exist. | A versioned `VECTOR` format, distance functions, flat baseline, HNSW or equivalent index, tokenizer, inverted index, BM25 semantics, WAL/recovery integration, and recall/performance tests. |
| Tier 2: PITR and CDC | One-way physical WAL chunk streaming and snapshot export exist. | Retained segment archive, restore-to-LSN/time protocol, committed logical change feed, cursor/retention contracts, tamper/integrity checks, and restore tests. |
| Tier 3: declarative schema diff and online migration | Internal catalog v1-to-v2 migration and SQL DDL exist. | User schema manifest, deterministic diff, safety classification, migration journal, rollback/resume behavior, and concurrent reader/writer tests. |
| Tier 3: JSONB and expression/generated indexes | TEXT/BLOB storage and secondary B+Tree indexes exist. | Canonical binary JSON semantics, parser/path operators, expression dependency tracking, index maintenance on UPDATE/DELETE, schema versioning, and corruption tests. |
