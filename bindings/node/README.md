# @yaza/yazabase

Official Node.js / TypeScript driver for **YazaBase**, the hyper-velocity zero-GC embedded SQL engine built into the YAZA Programming Language toolchain.

## Features

- **Embedded Zero-GC Engine**: Runs directly in-process or via CLI IPC without heavy C++ native compilation errors.
- **Full ACID Transactions**: Strict serializability, write-ahead logging (WAL), and clustered B+Tree indexes.
- **Sub-Millisecond Queries**: Point lookups and narrow range scans in microseconds.
- **TypeScript Support**: First-class type declarations included (`index.d.ts`).

## Installation

```bash
npm install yazabase
# or yarn add yazabase
# or pnpm add yazabase
```

Ensure YAZA or YazaBase is installed:
```bash
curl -fsSL https://yazastudios.id/install.sh | sh
```

## Quick Start

```typescript
const { connect } = require('yazabase');

// Connect / create database in './data' with database name 'app'
const db = connect('./data', 'app');

// Create a table
db.query(`
  CREATE TABLE users (
    id integer primary key,
    name text,
    email text
  );
`);

// Insert records
db.query("INSERT INTO users VALUES (1, 'Alice Smith', 'alice@gmail.com');");
db.query("INSERT INTO users VALUES (2, 'Bob Jones', 'bob@acme.org');");

// Query records with LIKE / ILIKE pattern matching
const users = db.query("SELECT * FROM users WHERE name LIKE 'Ali%';");
console.log(users);
// Output: [ { id: '1', name: 'Alice Smith', email: 'alice@gmail.com' } ]
```

## API Reference

### `connect(dirPath: string, dbName?: string, cliPath?: string): YazaBaseClient`
Creates or opens a database directory.

### `client.query<T = any>(sql: string): T[]`
Executes an arbitrary SQL statement (DDL, DML, or SELECT) and returns an array of objects for SELECT statements.

## License
Apache-2.0
