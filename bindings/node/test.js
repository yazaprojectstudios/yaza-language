const { YazaBaseClient, connect } = require('./index');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

console.log('Testing YazaBase Node.js driver...');

const testDir = '/tmp/yazabase_node_test';
if (fs.existsSync(testDir)) {
  fs.rmSync(testDir, { recursive: true, force: true });
}

const client = connect(testDir, 'main', path.resolve(__dirname, '../../build/yazabase-cli'));

console.log('1. Creating table...');
client.query('CREATE TABLE users (id integer primary key, name text, email text);');

console.log('2. Inserting rows...');
client.query("INSERT INTO users VALUES (1, 'Alice Smith', 'alice@example.com');");
client.query("INSERT INTO users VALUES (2, 'Bob Jones', 'bob@acme.org');");
client.query("INSERT INTO users VALUES (3, 'Charlie Brown', 'charlie@gmail.com');");

console.log('3. Querying rows...');
const allUsers = client.query('SELECT * FROM users;');
console.log('Found users:', allUsers);
assert.strictEqual(allUsers.length, 3);
assert.strictEqual(allUsers[0].name, 'Alice Smith');

console.log('4. Testing LIKE query in Node.js...');
const likeUsers = client.query("SELECT * FROM users WHERE name LIKE 'Ali%';");
console.log('Found LIKE users:', likeUsers);
assert.strictEqual(likeUsers.length, 1);
assert.strictEqual(likeUsers[0].name, 'Alice Smith');

console.log('Node.js driver tests passed successfully!');
