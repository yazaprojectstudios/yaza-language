"use strict";

function codeBeforeComment(line) {
  let inString = false;
  let escaped = false;
  for (let index = 0; index < line.length; index += 1) {
    const character = line[index];
    if (inString) {
      if (escaped) escaped = false;
      else if (character === "\\") escaped = true;
      else if (character === '"') inString = false;
    } else if (character === '"') {
      inString = true;
    } else if (character === "#") {
      return line.slice(0, index);
    }
  }
  return line;
}

function visualIndent(line, tabSize) {
  let width = 0;
  for (const character of line) {
    if (character === " ") width += 1;
    else if (character === "\t") width += tabSize - (width % tabSize);
    else break;
  }
  return width;
}

function blockHeader(line) {
  const code = codeBeforeComment(line).trim();
  if (!code.endsWith(":")) return false;
  return /^(?:start|api|(?:async\s+)?fn|view|shape|variant|impl|controller|service|repository|model|interface|if|elif|else|while|for|match)\b/.test(code) ||
    /^[A-Za-z_][A-Za-z0-9_]*(?:\([^\n]*\))?:$/.test(code);
}

function formatYaza(source, options = {}) {
  const tabSize = Math.max(2, Math.min(8, Number(options.tabSize) || 4));
  const indent = options.insertSpaces === false ? "\t" : " ".repeat(tabSize);
  const eol = options.eol || (source.includes("\r\n") ? "\r\n" : "\n");
  const blocks = [];
  const inputLines = source.replace(/\r\n?/g, "\n").split("\n");
  const outputLines = [];
  let inViewBlock = false;

  for (const rawLine of inputLines) {
    const content = rawLine.trim();
    if (content.length === 0) {
      outputLines.push("");
      continue;
    }

    const rawVisual = visualIndent(rawLine, tabSize);
    if (rawVisual === 0 && !content.startsWith("#")) {
      inViewBlock = /^view\s+/.test(content);
    }

    if (inViewBlock && rawVisual > 0) {
      outputLines.push(" ".repeat(rawVisual) + content);
      continue;
    }

    const sourceDepth = Math.round(rawVisual / tabSize);
    while (blocks.length > sourceDepth) blocks.pop();
    outputLines.push(indent.repeat(blocks.length) + content);
    if (blockHeader(content)) blocks.push(content);
  }

  while (outputLines.length > 0 && outputLines[outputLines.length - 1] === "") outputLines.pop();
  while (outputLines.length > 0 && outputLines[0] === "") outputLines.shift();
  return outputLines.length === 0 ? "" : outputLines.join(eol) + eol;
}

module.exports = { formatYaza };
