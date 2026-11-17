/**
 * YazaBase Node.js / TypeScript Driver
 * High-performance embedded database driver for Node.js, Express, Fastify, Next.js.
 */

const { execFileSync } = require('child_process');
const fs = require('fs');
const path = require('path');

class YazaBaseError extends Error {
  constructor(message) {
    super(message);
    this.name = 'YazaBaseError';
  }
}

class YazaBaseClient {
  constructor(dirPath, dbName = 'main', cliPath = null) {
    this.dirPath = path.resolve(dirPath);
    this.dbName = dbName;
    this.cliPath = cliPath || this._findCli();

    if (!fs.existsSync(this.dirPath)) {
      fs.mkdirSync(this.dirPath, { recursive: true });
    }
    const dbFile = path.join(this.dirPath, `${dbName}.ydb`);
    if (!fs.existsSync(dbFile)) {
      this._initDb();
    }
  }

  _findCli() {
    const candidates = [
      'yazabase',
      'yazabase-cli',
      path.join(process.env.HOME || '', '.yaza/bin/yazabase'),
      path.join(process.env.HOME || '', '.yazabase/bin/yazabase'),
      '/usr/local/bin/yazabase',
      path.resolve(__dirname, '../../build/yazabase-cli'),
    ];
    for (const c of candidates) {
      if (fs.existsSync(c)) {
        return c;
      }
    }
    return 'yazabase';
  }

  _initDb() {
    try {
      execFileSync(this.cliPath, ['init', this.dirPath, this.dbName], { encoding: 'utf8' });
    } catch (err) {
      throw new YazaBaseError(`Failed to initialize database: ${err.message}`);
    }
  }

  query(sql) {
    try {
      const output = execFileSync(this.cliPath, ['sql', '--csv', this.dirPath, this.dbName, sql], {
        encoding: 'utf8',
        maxBuffer: 64 * 1024 * 1024,
      }).trim();

      if (!output) return [];

      const lines = output.split(/\r?\n/).filter(Boolean);
      if (lines.length === 0) return [];
      const headers = lines[0].split(',');
      const rows = [];

      for (let i = 1; i < lines.length; i++) {
        const values = lines[i].split(',');
        const row = {};
        for (let j = 0; j < headers.length; j++) {
          const val = values[j];
          row[headers[j]] = val === '\\N' ? null : val;
        }
        rows.push(row);
      }
      return rows;
    } catch (err) {
      throw new YazaBaseError(`SQL execution error: ${err.stderr || err.message}`);
    }
  }
}

module.exports = {
  YazaBaseClient,
  YazaBaseError,
  connect: (dir, db, cli) => new YazaBaseClient(dir, db, cli),
};
