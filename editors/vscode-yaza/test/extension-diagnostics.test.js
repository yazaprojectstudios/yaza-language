"use strict";

const assert = require("assert");
const fs = require("fs");
const path = require("path");
const Module = require("module");
const { fileURLToPath, pathToFileURL } = require("url");

const projectRoot = path.resolve(__dirname, "../../..");
const cliPath = path.join(projectRoot, "dist", "yaza");
assert.ok(fs.existsSync(cliPath), `expected built YAZA CLI at ${cliPath}`);

class FakeUri {
  constructor(fsPath) { this.fsPath = fsPath; this.scheme = "file"; }
  toString() { return pathToFileURL(this.fsPath).href; }
  static file(filePath) { return new FakeUri(path.resolve(filePath)); }
  static parse(value) { return new FakeUri(fileURLToPath(value)); }
}

class FakeRange {
  constructor(startLine, startCharacter, endLine, endCharacter) {
    this.start = { line: startLine, character: startCharacter };
    this.end = { line: endLine, character: endCharacter };
  }
}

class FakeDiagnostic {
  constructor(range, message, severity) { Object.assign(this, { range, message, severity }); }
}

const diagnosticEntries = new Map();
const listeners = {};
const noOp = () => ({ dispose() {} });
const fakeVscode = {
  Uri: FakeUri,
  Range: FakeRange,
  Position: class { constructor(line, character) { Object.assign(this, { line, character }); } },
  Location: class { constructor(uri, position) { Object.assign(this, { uri, position }); } },
  Diagnostic: FakeDiagnostic,
  DiagnosticSeverity: { Error: 0, Warning: 1, Information: 2 },
  CompletionItem: class { constructor(label, kind) { Object.assign(this, { label, kind }); } },
  CompletionItemKind: { Keyword: 14 },
  MarkdownString: class { constructor(value) { this.value = value; } },
  Hover: class { constructor(contents, range) { Object.assign(this, { contents, range }); } },
  DocumentSymbol: class { constructor(name, detail, kind, range, selectionRange) { Object.assign(this, { name, detail, kind, range, selectionRange }); } },
  SymbolKind: { Function: 11, Struct: 22, Enum: 13, Interface: 10, Class: 4 },
  TextEdit: { replace: (range, newText) => ({ range, newText }) },
  EndOfLine: { LF: 1, CRLF: 2 },
  languages: {
    createDiagnosticCollection() {
      return {
        set(uri, values) { diagnosticEntries.set(uri.toString(), values); },
        delete(uri) { diagnosticEntries.delete(uri.toString()); },
        dispose() {}
      };
    },
    registerDocumentFormattingEditProvider: noOp,
    registerCompletionItemProvider: noOp,
    registerHoverProvider: noOp,
    registerDocumentSymbolProvider: noOp,
    registerDefinitionProvider: noOp
  },
  workspace: {
    textDocuments: [],
    getConfiguration(section) {
      assert.strictEqual(section, "yaza");
      return { get(name, fallback) {
        if (name === "executablePath") return cliPath;
        if (name === "standardLibraryPath") return projectRoot;
        if (name === "diagnosticsTimeoutMs") return 10000;
        return fallback;
      } };
    },
    getWorkspaceFolder() { return { uri: FakeUri.file(projectRoot) }; },
    onDidOpenTextDocument(callback) { listeners.open = callback; return noOp(); },
    onDidChangeTextDocument(callback) { listeners.change = callback; return noOp(); },
    onDidSaveTextDocument(callback) { listeners.save = callback; return noOp(); },
    onDidCloseTextDocument(callback) { listeners.close = callback; return noOp(); }
  },
  window: {
    createOutputChannel() { return { appendLine() {}, show() {}, dispose() {} }; },
    showInformationMessage() {}
  },
  commands: { registerCommand: noOp }
};

const originalLoad = Module._load;
Module._load = function loadWithVscode(request, parent, isMain) {
  if (request === "vscode") return fakeVscode;
  return originalLoad.call(this, request, parent, isMain);
};

async function main() {
  const extension = require("../extension");
  extension.activate({ subscriptions: [] });

  const sourcePath = path.join(projectRoot, "tests", ".yaza-vscode-diagnostics-smoke.yz");
  const source = "start:\n    let message: Text = 123 # 🚀\n    say message\n";
  const lines = source.split(/\r?\n/);
  const document = {
    languageId: "yaza",
    version: 1,
    uri: FakeUri.file(sourcePath),
    eol: fakeVscode.EndOfLine.LF,
    lineCount: lines.length,
    getText() { return source; },
    lineAt(line) { return { text: lines[line] }; }
  };

  listeners.change({ document });
  const deadline = Date.now() + 15000;
  let reported = [];
  while (Date.now() < deadline) {
    reported = diagnosticEntries.get(document.uri.toString()) || [];
    if (reported.length > 0) break;
    await new Promise(resolve => setTimeout(resolve, 50));
  }

  assert.strictEqual(reported.length, 1, "compiler error from unsaved editor content should be published");
  assert.strictEqual(reported[0].code, "YZA1000");
  assert.strictEqual(reported[0].source, "YAZA");
  assert.strictEqual(reported[0].range.start.line, 1, "diagnostic line must map back from the temporary check file");
  assert.strictEqual(reported[0].range.start.character, 4, "statement range must skip leading indentation");
  assert.strictEqual(reported[0].range.end.character, lines[1].length, "statement range must cover the offending source line");

  const temporaryFiles = fs.readdirSync(path.dirname(sourcePath)).filter(name => name.startsWith(".yaza-vscode-check-"));
  assert.deepStrictEqual(temporaryFiles, [], "temporary unsaved-buffer check files must be removed");
  extension.deactivate();
  process.stdout.write("YAZA VS Code diagnostics: unsaved-buffer compiler errors map to the editor and temp files are removed.\n");
}

main().catch(error => {
  process.stderr.write(`${error.stack || error}\n`);
  process.exitCode = 1;
}).finally(() => {
  Module._load = originalLoad;
});
