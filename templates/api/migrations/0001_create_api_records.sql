CREATE TABLE IF NOT EXISTS yaza_api_records (
    resource TEXT NOT NULL,
    id TEXT NOT NULL,
    payload JSONB NOT NULL CHECK (jsonb_typeof(payload) = 'object'),
    created_at TIMESTAMPTZ NOT NULL DEFAULT clock_timestamp(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT clock_timestamp(),
    PRIMARY KEY (resource, id),
    CHECK (octet_length(payload::text) <= 49152)
);

CREATE INDEX IF NOT EXISTS yaza_api_records_updated_at_idx
    ON yaza_api_records (resource, updated_at DESC);
