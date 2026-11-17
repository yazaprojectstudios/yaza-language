"use strict";

const fs = require("fs");
const path = require("path");
const { spawn } = require("child_process");
const { formatYaza } = require("./formatter");

let vscode;
let diagnostics;
let output;
let sequence = 0;
const checks = new Map();
const MAX_CHECK_OUTPUT = 2 * 1024 * 1024;

const completionNames = [
  ["start", "Program entry block"], ["fn", "Declare a typed function"],
  ["shape", "Declare a product data type"], ["variant", "Declare a tagged variant"],
  ["impl", "Implement a conversion or interface"], ["if", "Start a conditional block"],
  ["elif", "Add a conditional branch"], ["else", "Add a fallback branch"],
  ["match", "Match every case of a variant"], ["for", "Iterate over a list or numeric range"],
  ["while", "Repeat while a Boolean condition is true"], ["return", "Return a value from a function"],
  ["let", "Declare an immutable binding"], ["var", "Declare a mutable binding"],
  ["set", "Update a mutable binding"], ["defer", "Register cleanup for scope exit"],
  ["assert", "Fail when a Boolean expression is false"], ["say", "Write a value to standard output"],
  ["Text", "UTF-8 text value"], ["Number", "YAZA integer numeric type"],
  ["Int32", "Checked 32-bit integer"], ["Float64", "64-bit floating-point number"],
  ["Boolean", "Boolean value"], ["TextList", "List of text values"],
  ["Bytes", "Byte sequence"], ["Option", "Explicitly optional value"],
  ["Result", "Success or error value"], ["async", "Declare an asynchronous function"],
  ["spawn", "Spawn an isolated parallel Task"], ["await", "Wait for a Task completion"],
  ["borrow", "Borrow a value without moving ownership"], ["mut", "Mutable borrow qualifier"],
  ["Task", "Concurrent task handle returned by spawn"], ["List", "Typed collection list"],
  ["controller", "Declare an API controller"], ["route", "Declare an HTTP route"],
  ["group", "Group HTTP routes under a prefix"]
];

const hoverText = new Map([
  ["if", "Starts a conditional block. The condition must have type `Boolean`."],
  ["elif", "Adds another Boolean condition to the current `if` chain."],
  ["else", "Adds the fallback block to the current `if` chain."],
  ["return", "Returns from the current function. YAZA requires a final top-level return for functions with a return type."],
  ["let", "Declares an immutable local binding."],
  ["var", "Declares a mutable local binding. Update it with `set name = value`."],
  ["set", "Updates a mutable binding declared with `var`."],
  ["match", "Checks all cases of a tagged variant, `Option`, or `Result`. Every case must be covered."],
  ["async", "Marks a function as asynchronous so callers can run it concurrently with `spawn`."],
  ["spawn", "Launches an asynchronous function on the YAZA task runtime with an isolated arena and returns `Task[T]`."],
  ["await", "Waits for a `Task[T]` handle produced by `spawn` and returns its result."],
  ["borrow", "Creates a non-owning reference (`borrow x` or `borrow mut x`)."],
  ["Task", "Concurrent execution handle returned by `spawn`."],
  ["Text", "UTF-8 text value."], ["Number", "YAZA integer numeric type."],
  ["Int32", "Checked 32-bit integer type."], ["Float64", "64-bit floating-point type."],
  ["Boolean", "Boolean value: `true` or `false`."], ["Option", "A value that is either `some(value)` or `none`."],
  ["Result", "A success value (`ok`) or an error value (`err`)."]
]);

function activate(context) {
  vscode = require("vscode");
  diagnostics = vscode.languages.createDiagnosticCollection("yaza");
  output = vscode.window.createOutputChannel("YAZA");
  context.subscriptions.push(diagnostics, output);

  context.subscriptions.push(vscode.languages.registerDocumentFormattingEditProvider("yaza", {
    provideDocumentFormattingEdits(document, options) {
      const formatted = formatYaza(document.getText(), { tabSize: options.tabSize, insertSpaces: options.insertSpaces, eol: document.eol === vscode.EndOfLine.CRLF ? "\r\n" : "\n" });
      const fullRange = new vscode.Range(document.positionAt(0), document.positionAt(document.getText().length));
      return [vscode.TextEdit.replace(fullRange, formatted)];
    }
  }));

  context.subscriptions.push(vscode.languages.registerCompletionItemProvider("yaza", {
    provideCompletionItems() {
      return completionNames.map(([name, detail]) => {
        const item = new vscode.CompletionItem(name, vscode.CompletionItemKind.Keyword);
        item.detail = detail;
        return item;
      });
    }
  }));

  context.subscriptions.push(vscode.languages.registerHoverProvider("yaza", {
    provideHover(document, position) {
      const range = document.getWordRangeAtPosition(position, /[A-Za-z_][A-Za-z0-9_]*/);
      if (!range) return undefined;
      const explanation = hoverText.get(document.getText(range));
      return explanation ? new vscode.Hover(new vscode.MarkdownString(explanation), range) : undefined;
    }
  }));

  context.subscriptions.push(vscode.languages.registerDocumentSymbolProvider("yaza", {
    provideDocumentSymbols(document) {
      const lines = document.getText().split(/\r?\n/);
      const declarations = [];
      const declarationPattern = /^(?:async\s+)?(fn|shape|variant|interface|controller|service|repository|model)\s+([A-Za-z_][A-Za-z0-9_]*)/;
      for (let line = 0; line < lines.length; line += 1) {
        const match = lines[line].match(declarationPattern);
        if (!match) continue;
        const kind = ({ fn: vscode.SymbolKind.Function, shape: vscode.SymbolKind.Struct, variant: vscode.SymbolKind.Enum,
          interface: vscode.SymbolKind.Interface, controller: vscode.SymbolKind.Class, service: vscode.SymbolKind.Class,
          repository: vscode.SymbolKind.Class, model: vscode.SymbolKind.Struct })[match[1]];
        const range = new vscode.Range(line, 0, line, lines[line].length);
        declarations.push(new vscode.DocumentSymbol(match[2], match[1], kind, range, range));
      }
      return declarations;
    }
  }));

  context.subscriptions.push(vscode.languages.registerDefinitionProvider("yaza", {
    provideDefinition(document, position) {
      const wordRange = document.getWordRangeAtPosition(position, /[A-Za-z_][A-Za-z0-9_]*/);
      if (!wordRange) return undefined;
      const word = document.getText(wordRange);
      const lines = document.getText().split(/\r?\n/);
      const declarationPattern = new RegExp("^(?:fn|shape|variant|interface|controller|service|repository|model)\\s+" + escapeRegExp(word) + "\\b");
      for (let line = 0; line < lines.length; line += 1) {
        if (declarationPattern.test(lines[line])) return new vscode.Location(document.uri, new vscode.Position(line, lines[line].indexOf(word)));
      }
      return undefined;
    }
  }));

  const checkCommand = vscode.commands.registerCommand("yaza.checkCurrentFile", () => {
    const document = vscode.window.activeTextEditor && vscode.window.activeTextEditor.document;
    if (document && document.languageId === "yaza") scheduleCheck(document, 0);
    else vscode.window.showInformationMessage("Open a YAZA .yz file to check it.");
  });
  const showOutputCommand = vscode.commands.registerCommand("yaza.showOutput", () => output.show(true));
  context.subscriptions.push(checkCommand, showOutputCommand);

  context.subscriptions.push(vscode.workspace.onDidOpenTextDocument(document => scheduleCheck(document, 250)));
  context.subscriptions.push(vscode.workspace.onDidChangeTextDocument(event => scheduleCheck(event.document, 350)));
  context.subscriptions.push(vscode.workspace.onDidSaveTextDocument(document => scheduleCheck(document, 0)));
  context.subscriptions.push(vscode.workspace.onDidCloseTextDocument(document => clearCheck(document)));
  for (const document of vscode.workspace.textDocuments) scheduleCheck(document, 250);
}

function escapeRegExp(value) {
  return value.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}

function scheduleCheck(document, delay) {
  if (!vscode || document.languageId !== "yaza" || document.uri.scheme !== "file") return;
  const key = document.uri.toString();
  let state = checks.get(key);
  if (!state) {
    state = { generation: 0, timer: undefined, child: undefined, tempPath: undefined, publishedUris: new Set() };
    checks.set(key, state);
  }
  state.generation += 1;
  const generation = state.generation;
  if (state.timer) clearTimeout(state.timer);
  if (state.child) state.child.kill();
  state.timer = setTimeout(() => {
    state.timer = undefined;
    runCheck(document, key, state, generation).catch(error => {
      output.appendLine(`Diagnostics failed for ${document.uri.fsPath}: ${error.message}`);
    });
  }, delay);
}

async function runCheck(document, key, state, generation) {
  const filePath = document.uri.fsPath;
  const folder = vscode.workspace.getWorkspaceFolder(document.uri);
  const settings = vscode.workspace.getConfiguration("yaza", document.uri);
  const executable = settings.get("executablePath", "yaza");
  const tempPath = path.join(path.dirname(filePath), `.yaza-vscode-check-${process.pid}-${++sequence}.yz`);
  state.tempPath = tempPath;
  let activeChild;
  try {
    await fs.promises.writeFile(tempPath, document.getText(), { flag: "wx", mode: 0o600 });
    const env = { ...process.env };
    const standardLibraryPath = settings.get("standardLibraryPath", "").trim();
    if (standardLibraryPath) env.YAZA_STDLIB_DIR = standardLibraryPath;
    const result = await execute(executable, ["check", "--format", "json", tempPath], {
      cwd: folder ? folder.uri.fsPath : path.dirname(filePath), env,
      timeoutMs: settings.get("diagnosticsTimeoutMs", 10000),
      onChild: child => {
        activeChild = child;
        if (state.generation === generation) state.child = child;
        else child.kill();
      }
    });
    if (state.generation !== generation) return;
    const report = JSON.parse(result.stdout.trim());
    const ownDiagnostics = [];
    const otherDiagnostics = new Map();
    for (const item of report.diagnostics || []) {
      const uri = item.uri ? vscode.Uri.parse(item.uri) : document.uri;
      const rangeData = item.range && item.range.start;
      if (!rangeData) {
        ownDiagnostics.push(toDiagnostic(item, new vscode.Range(0, 0, 0, 0)));
        continue;
      }
      if (path.resolve(uri.fsPath) !== path.resolve(tempPath)) {
        if (uri.scheme === "file") {
          const line = Math.max(0, Math.min(rangeData.line, 1000000));
          const target = new vscode.Range(line, 0, line, 1);
          const diagnostic = toDiagnostic(item, target);
          const entries = otherDiagnostics.get(uri.toString()) || [];
          entries.push(diagnostic);
          otherDiagnostics.set(uri.toString(), entries);
        }
        continue;
      }
      const line = Math.max(0, Math.min(rangeData.line, Math.max(0, document.lineCount - 1)));
      const lineText = document.lineAt(line).text;
      let startCharacter = 0;
      let endCharacter = Math.min(1, lineText.length);
      if (item.range.precision === "statement" && item.range.end && item.range.end.line === rangeData.line) {
        startCharacter = utf8ByteColumnToUtf16(lineText, rangeData.character);
        endCharacter = utf8ByteColumnToUtf16(lineText, item.range.end.character);
      }
      const target = new vscode.Range(line, startCharacter, line, Math.max(startCharacter, endCharacter));
      ownDiagnostics.push(toDiagnostic(item, target));
    }
    for (const previousUri of state.publishedUris) diagnostics.delete(vscode.Uri.parse(previousUri));
    state.publishedUris.clear();
    diagnostics.set(document.uri, ownDiagnostics);
    state.publishedUris.add(document.uri.toString());
    for (const [uri, entries] of otherDiagnostics) diagnostics.set(vscode.Uri.parse(uri), entries);
    for (const uri of otherDiagnostics.keys()) state.publishedUris.add(uri);
    if (result.error && ownDiagnostics.length === 0 && otherDiagnostics.size === 0) {
      output.appendLine(`yaza check exited ${result.code}: ${result.error.message}`);
    }
  } catch (error) {
    if (state.generation === generation) {
      diagnostics.set(document.uri, [new vscode.Diagnostic(new vscode.Range(0, 0, 0, 0),
        `Could not run YAZA diagnostics: ${error.message}. Set yaza.executablePath if the CLI is not on VS Code's PATH.`, vscode.DiagnosticSeverity.Warning)]);
    }
  } finally {
    if (state.child === activeChild) state.child = undefined;
    try { await fs.promises.unlink(tempPath); } catch (_) { /* The temporary check file may already be gone. */ }
    if (state.tempPath === tempPath) state.tempPath = undefined;
    if (state.generation === generation && state.timer === undefined) checks.delete(key);
  }
}

function utf8ByteColumnToUtf16(lineText, byteColumn) {
  const bytes = Buffer.from(lineText, "utf8");
  const boundedColumn = Math.max(0, Math.min(Number(byteColumn) || 0, bytes.length));
  const prefix = bytes.subarray(0, boundedColumn).toString("utf8");
  return prefix.length;
}

function toDiagnostic(item, range) {
  const severity = item.severity === "warning" ? vscode.DiagnosticSeverity.Warning :
    item.severity === "information" ? vscode.DiagnosticSeverity.Information : vscode.DiagnosticSeverity.Error;
  const diagnostic = new vscode.Diagnostic(range, item.message || "YAZA compiler diagnostic", severity);
  diagnostic.source = "YAZA";
  diagnostic.code = item.code;
  return diagnostic;
}

function execute(command, args, options) {
  return new Promise((resolve, reject) => {
    let stdout = "";
    let stderr = "";
    let overflow = false;
    const child = spawn(command, args, { cwd: options.cwd, env: options.env, shell: false, windowsHide: true, stdio: ["ignore", "pipe", "pipe"] });
    options.onChild(child);
    const timeout = setTimeout(() => child.kill(), Math.max(1000, Number(options.timeoutMs) || 10000));
    child.stdout.on("data", chunk => {
      stdout += chunk.toString();
      if (stdout.length + stderr.length > MAX_CHECK_OUTPUT) { overflow = true; child.kill(); }
    });
    child.stderr.on("data", chunk => {
      stderr += chunk.toString();
      if (stdout.length + stderr.length > MAX_CHECK_OUTPUT) { overflow = true; child.kill(); }
    });
    child.on("error", error => { clearTimeout(timeout); reject(error); });
    child.on("close", (code, signal) => {
      clearTimeout(timeout);
      if (overflow) reject(new Error("diagnostic output exceeded the 2 MiB safety limit"));
      else if (signal === "SIGTERM" && code === null) reject(new Error("diagnostics were cancelled or timed out"));
      else if (stdout.trim().length === 0) reject(new Error(stderr.trim() || `compiler exited with status ${code}`));
      else resolve({ stdout, stderr, code });
    });
  });
}

function clearCheck(document) {
  const key = document.uri.toString();
  const state = checks.get(key);
  if (state) {
    state.generation += 1;
    if (state.timer) clearTimeout(state.timer);
    if (state.child) state.child.kill();
    if (state.tempPath) fs.promises.unlink(state.tempPath).catch(() => {});
    for (const uri of state.publishedUris) diagnostics.delete(vscode.Uri.parse(uri));
    checks.delete(key);
  }
  diagnostics.delete(document.uri);
}

function deactivate() {
  for (const state of checks.values()) {
    if (state.timer) clearTimeout(state.timer);
    if (state.child) state.child.kill();
    if (state.tempPath) fs.promises.unlink(state.tempPath).catch(() => {});
  }
  checks.clear();
}

module.exports = { activate, deactivate };
