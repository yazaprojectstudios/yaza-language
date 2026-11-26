# YazaBase Tier 2 Implementation

YazaBase Tier 2 adds concurrent group-commit OLTP execution, native vector and full-text search indexes, and crash-safe point-in-time recovery (PITR) with logical change-data-capture (CDC) streaming on top of Tier 1 storage and cluster primitives.

## Implemented Subsystems

### Tier 2.1: Single-Owner Concurrent Engine & Group Commit ([`yazabase/concurrent_engine_v1.yz`](../yazabase/concurrent_engine_v1.yz))
- **Single-Owner Actor Architecture (`YazaDbConcurrentEngineV1`)**:
  - `yaza_db_concurrent_writer_loop_v1` owns `YazaDbSessionV1` exclusively on one thread, draining write queues, coalescing batches via `yaza_db_group_commit_execute_v1` (`wal_group_commit_sync_count`, `wal_group_commit_batch_count`), executing reads linearly (`SELECT` returning serialized JSON), and closing the session safely on shutdown (`yaza_db_session_close_v1`).
  - Cross-thread queues and result channels store data in program-arena primitive collections (`List[Text]`, `List[Boolean]`, `List[Number]`) and hex-encoded `YazaDbWalRingBufferV1`, preventing spawned-task arena lease hazards under `ASan+UBSan`.
  - Includes `tcp_pair_create()`/`tcp_pair_peer()`-verified worker dispatch (`yaza_db_concurrent_handle_connection_v1`) alongside production `tcp_listen`/`tcp_accept` loops.

### Tier 2.2: Native Vector Search & Full-Text Search ([`yazabase/vector_index_v1.yz`](../yazabase/vector_index_v1.yz), [`yazabase/fts_bm25_v1.yz`](../yazabase/fts_bm25_v1.yz))
- **Binary Vector Codec & Distance Metrics (`YDV1`)**:
  - Fixed-header binary encoding (`yaza_db_vector_encode_v1` / `yaza_db_vector_decode_v1`) with CRC32C payload verification and finite `Float64` / NaN rejection (`dim` 1..4096).
  - Supports Cosine, L2 Euclidean, and Inner Product metrics (`yaza_db_vector_distance_v1`).
- **Exact Flat & HNSW Graph Index**:
  - `yaza_db_vector_flat_search_v1` baseline exact search and `YazaDbHnswIndexV1` hierarchical navigable small-world search (`yaza_db_hnsw_new_v1`, `yaza_db_hnsw_insert_v1`, `yaza_db_hnsw_delete_v1`, `yaza_db_hnsw_search_v1`).
  - Durable `.yzdb` B+Tree persistence and reopen reload (`yaza_db_hnsw_persist_to_session_v1`, `yaza_db_hnsw_load_from_session_v1`).
- **Full-Text Search & Okapi BM25**:
  - Case-folding ASCII tokenizer with stopword filtering (`yaza_db_fts_tokenize_v1`), inverted posting lists (`YazaDbFtsIndexV1`), tombstone-safe document update/delete (`yaza_db_fts_index_doc_v1`), AND/OR BM25 scoring (`yaza_db_fts_search_bm25_v1`), and durable `.yzdb` table persistence (`yaza_db_fts_persist_to_session_v1`, `yaza_db_fts_load_from_session_v1`).

### Tier 2.3: Point-in-Time Recovery & Logical CDC ([`yazabase/pitr_cdc_v1.yz`](../yazabase/pitr_cdc_v1.yz), [`yazabase/cluster_api_v1.yz`](../yazabase/cluster_api_v1.yz))
- **PITR Chunked Base & WAL Archival (`yaza_db_pitr_archive_base_v1`, `yaza_db_pitr_archive_wal_chunk_v1`)**:
  - Archives clean checkpoint snapshots and base WAL segments in bounded 256 KiB chunks inside scoped 20 MiB arenas with sparse authenticated zero-hole descriptors (`Z` records) and optional `YDE1` AEAD encryption (`YDB1-PITR-BASE`, `YDB1-PITR-BWAL`, `YDB1-PITR-WAL`).
  - Protects ledgers and chunks with CRC32C integrity verification (`.base.meta`, `.base.parts`, `.base.wals`, `.chunks`, `.chunks.sig`).
- **Fail-Closed Two-Pass Restore (`yaza_db_pitr_restore_v1`)**:
  - Verifies archive ledgers, CRCs, and decryption keys before mutating the target root directory.
  - Supports restoration by `target_lsn` or `target_commit_ts` using LSN-continuous ARIES WAL replay followed by clean recovery reopen/checkpoint, excluding rolled-back and post-target transactions.
- **Logical CDC Tail Reader & Cluster HTTP Endpoints**:
  - `yaza_db_cdc_poll_v1` / `yaza_db_cdc_poll_to_json_v1` stream committed `UPSERT`/`DELETE` changes with durable JSON consumer cursors (`yaza_db_cdc_load_cursor_v1`, `yaza_db_cdc_save_cursor_v1`).
  - Wired into `yaza_api_route_request_cluster_v1` at `GET|POST /api/v1/cdc/poll` and `POST /api/v1/cdc/ack`.

## Verified Validation Gates (`normal`, `-O2`, `ASan+UBSan`)

| Gate | Command | Modes Verified | Result |
| --- | --- | --- | --- |
| Tier 2.1 Concurrent Engine | `sh scripts/validate-yazabase-tier2-concurrent.sh` | `normal`, `o2`, `asan` | `yazabase-tier2-concurrent-validation-ok` |
| Tier 2.2 Vector + FTS Search | `sh scripts/validate-yazabase-tier2-search.sh` | `normal`, `o2`, `asan` | `yazabase-tier2-search-validation-ok` |
| Tier 2.3 PITR & Logical CDC | `sh scripts/validate-yazabase-tier2-pitr-cdc.sh` | `normal`, `o2`, `asan` | `yazabase-tier2-pitr-cdc-validation-ok` |
