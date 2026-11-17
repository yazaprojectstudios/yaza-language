/**
 * YazaBase Node.js / TypeScript Driver
 */

export interface YazaBaseConfig {
  dirPath: string;
  dbName?: string;
  cliPath?: string;
}

export declare class YazaBaseError extends Error {
  constructor(message: string);
}

export declare class YazaBaseClient {
  readonly dirPath: string;
  readonly dbName: string;
  readonly cliPath: string;

  constructor(dirPath: string, dbName?: string, cliPath?: string | null);

  query<T = Record<string, any>>(sql: string): T[];
}

export declare function connect(
  dir: string,
  db?: string,
  cli?: string | null
): YazaBaseClient;
