# YAZA Language Support for VS Code

This local extension provides `.yz` syntax highlighting, four-space block indentation, bracket pairing, snippets, document formatting, compiler diagnostics, basic completion and hover help, document symbols, and same-file go-to-definition.

## Requirements

- VS Code 1.85 or newer.
- The native YAZA CLI available on VS Code's `PATH`, or its full path configured as `yaza.executablePath`.
- Set `yaza.standardLibraryPath` if the CLI cannot locate YAZA's standard library automatically.

The editor extension is JavaScript that calls the YAZA native CLI for checks. It does not replace or reimplement the YAZA compiler.

## Install locally

Install from the packaged `.vsix` downloaded from [GitHub Releases](https://github.com/yazaprojectstudios/yaza-language/releases/tag/v1.1.0):

```sh
code --install-extension yaza-language-support-1.1.0.vsix
```

Or copy this folder to:

```text
~/.vscode/extensions/yaza-language.yaza-language-support-1.1.0
```

Restart VS Code, then open a `.yz` file. For VS Code Insiders, use `~/.vscode-insiders/extensions/` instead.

To develop the extension, open `editors/vscode-yaza` as a folder and press `F5`. VS Code opens an Extension Development Host with the extension enabled.

## Editor features

- **Format Document:** use `Format Document` from the Command Palette. The formatter normalizes block indentation to the configured editor tab size and removes trailing whitespace. YAZA's default is four spaces.
- **Live diagnostics:** the extension runs `yaza check --format json` after edits, when a file opens, and when it is saved. It checks the unsaved buffer by writing a short-lived hidden `.yz` sibling file; it does not overwrite the user's source file. Checks are debounced and cancelled when newer edits arrive.
- **Completions and hover:** common YAZA keywords and built-in types have basic descriptions.
- **Navigation:** Outline symbols and same-file go-to-definition cover functions, shapes, variants, interfaces, and API layer declarations.
- **Snippets:** type `yaza-` to browse starters, including `yaza-classify` for early-return control flow.

Compiler diagnostics currently report the first compiler error per check. Errors found while compiling a source statement use a statement-level range; the CLI encodes character offsets as UTF-8 bytes and the extension converts them to VS Code's UTF-16 positions. Other failures may still use a line-level range. The extension does not yet provide cross-file semantic analysis, rename/refactor, or a YAZA language-server protocol implementation. The compiler's diagnostic interface is the foundation for those capabilities.

## Settings

- `yaza.executablePath`: CLI command or full executable path; defaults to `yaza`.
- `yaza.standardLibraryPath`: optional standard-library directory.
- `yaza.diagnosticsTimeoutMs`: per-check timeout in milliseconds; defaults to 10,000.

Run **YAZA: Check Current File** to request an immediate check. Use **YAZA: Show Language Output** to inspect CLI launch or diagnostic errors.

## Development check

```sh
npm test
```
